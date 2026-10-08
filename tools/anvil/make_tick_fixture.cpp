// make_tick_fixture.cpp — deterministic MP4 fixture with sub-microsecond
// frame spacing.
//
// The FFmpeg CLI cannot write frame timestamps closer than 1 microsecond:
// every retiming path quantizes through AV_TIME_BASE before the muxer sees
// the packets (measured on ffmpeg n9.0.2: -framerate 2000000, concat
// duration 0.0000005, and -c:v copy all land on exact microsecond
// boundaries). Native ticks that collide after microsecond rescaling — the
// regression input demanded by evaluator finding 4209783084 — therefore need
// a container written programmatically with an explicit fine timebase.
//
// Usage:
//   make_tick_fixture OUT.mp4 [ticks_per_frame=5] [timescale=10000000] [frames=4]
//
// Behavior: writes `frames` distinct MJPEG frames with presentation
// timestamps i*ticks_per_frame in stream timebase 1/timescale. With the
// defaults, adjacent frames sit 5 ticks = 0.5 us apart, so their microsecond
// rescaling (av_rescale_q, round-half-away-from-zero) collides while their
// native ticks stay distinct.
// distinct. Frame content is a deterministic gradient plus a frame-index
// bar so decoders cannot deduplicate identical pictures.
//
// Exit codes: 0 success, 1 usage error, 2 encode/mux failure. Prints the
// realized stream timebase and the written ticks to stdout (deterministic;
// no wall-clock or random content anywhere).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

namespace {

bool parseLong(const char* s, long long& out) {
    if (!s || !*s) return false;
    char* end = nullptr;
    out = std::strtoll(s, &end, 10);
    return end && !*end;
}

// Deterministic 8-bit luma gradient with an index bar (frame % 8 rows of
// bright pixels near the top) so each frame's pixels differ.
void fillFrame(AVFrame* f, int index) {
    for (int y = 0; y < f->height; ++y)
        for (int x = 0; x < f->width; ++x)
            f->data[0][size_t(y) * f->linesize[0] + x] =
                static_cast<uint8_t>((x * 7 + y * 13) & 0xff);
    const int barY = 2 + (index % 8) * 3;
    for (int y = barY; y < barY + 2 && y < f->height; ++y)
        for (int x = 0; x < f->width; ++x)
            f->data[0][size_t(y) * f->linesize[0] + x] = 240;
    for (int y = 0; y < f->height / 2; ++y)
        for (int x = 0; x < f->width / 2; ++x) {
            f->data[1][size_t(y) * f->linesize[1] + x] = 128;
            f->data[2][size_t(y) * f->linesize[2] + x] = 128;
        }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 5) {
        std::fprintf(stderr,
                     "usage: %s OUT.mp4 [ticks_per_frame=5] [timescale=10000000] "
                     "[frames=4]\n",
                     argv[0]);
        return 1;
    }
    const char* outPath = argv[1];
    long long ticksPerFrame = 5, timescale = 10000000, frames = 4;
    if (argc >= 3 && (!parseLong(argv[2], ticksPerFrame) || ticksPerFrame < 1)) {
        std::fprintf(stderr, "make_tick_fixture: ticks_per_frame must be >= 1\n");
        return 1;
    }
    if (argc >= 4 && (!parseLong(argv[3], timescale) || timescale < 1000000)) {
        std::fprintf(stderr,
                     "make_tick_fixture: timescale must be >= 1000000 (finer than "
                     "microseconds is the point of this fixture)\n");
        return 1;
    }
    if (argc >= 5 && (!parseLong(argv[4], frames) || frames < 2 || frames > 64)) {
        std::fprintf(stderr, "make_tick_fixture: frames must be within [2,64]\n");
        return 1;
    }

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec) {
        std::fprintf(stderr, "make_tick_fixture: mjpeg encoder not found\n");
        return 2;
    }
    AVCodecContext* enc = avcodec_alloc_context3(codec);
    if (!enc) return 2;
    enc->width = 32;
    enc->height = 32;
    enc->pix_fmt = AV_PIX_FMT_YUVJ420P;
    enc->time_base = AVRational{1, static_cast<int>(timescale)};
    enc->global_quality = 2; // high quality, deterministic input content
    if (avcodec_open2(enc, codec, nullptr) < 0) {
        std::fprintf(stderr, "make_tick_fixture: avcodec_open2 failed\n");
        avcodec_free_context(&enc);
        return 2;
    }

    AVFormatContext* oc = nullptr;
    if (avformat_alloc_output_context2(&oc, nullptr, nullptr, outPath) < 0 || !oc) {
        std::fprintf(stderr, "make_tick_fixture: output context alloc failed\n");
        avcodec_free_context(&enc);
        return 2;
    }
    AVStream* st = avformat_new_stream(oc, nullptr);
    if (!st || avcodec_parameters_from_context(st->codecpar, enc) < 0) {
        std::fprintf(stderr, "make_tick_fixture: stream setup failed\n");
        avcodec_free_context(&enc);
        avformat_free_context(oc);
        return 2;
    }
    st->time_base = AVRational{1, static_cast<int>(timescale)};
    if (!(oc->oformat->flags & AVFMT_NOFILE)
        && avio_open(&oc->pb, outPath, AVIO_FLAG_WRITE) < 0) {
        std::fprintf(stderr, "make_tick_fixture: avio_open failed for %s\n", outPath);
        avcodec_free_context(&enc);
        avformat_free_context(oc);
        return 2;
    }
    if (avformat_write_header(oc, nullptr) < 0) {
        std::fprintf(stderr, "make_tick_fixture: write_header failed\n");
        avcodec_free_context(&enc);
        avformat_free_context(oc);
        return 2;
    }
    // The muxer may have adjusted the stream timebase; packets are fed in the
    // requested 1/timescale base and rescaled to whatever the muxer chose so
    // the written ticks are exact in the container's realized timebase.
    const AVRational requested = AVRational{1, static_cast<int>(timescale)};

    AVFrame* frame = av_frame_alloc();
    frame->format = enc->pix_fmt;
    frame->width = enc->width;
    frame->height = enc->height;
    if (av_frame_get_buffer(frame, 0) < 0) {
        std::fprintf(stderr, "make_tick_fixture: frame buffer alloc failed\n");
        av_frame_free(&frame);
        avcodec_free_context(&enc);
        avformat_free_context(oc);
        return 2;
    }

    int exitCode = 0;
    for (int i = 0; i < static_cast<int>(frames) && exitCode == 0; ++i) {
        fillFrame(frame, i);
        frame->pts = static_cast<int64_t>(i) * ticksPerFrame;
        if (avcodec_send_frame(enc, frame) < 0) {
            std::fprintf(stderr, "make_tick_fixture: send_frame failed\n");
            exitCode = 2;
            break;
        }
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(enc, pkt) == 0) {
            // The muxer derives each sample's stts duration from the next
            // packet's pts; the trailing sample would get duration 0 and the
            // demuxer would clip it as lying outside the track duration
            // (measured: N-1 frames readable). Every packet therefore carries
            // an explicit duration.
            pkt->duration = static_cast<int64_t>(ticksPerFrame);
            av_packet_rescale_ts(pkt, requested, st->time_base);
            pkt->stream_index = st->index;
            if (av_interleaved_write_frame(oc, pkt) < 0) {
                std::fprintf(stderr, "make_tick_fixture: write_frame failed\n");
                exitCode = 2;
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    if (exitCode == 0) {
        avcodec_send_frame(enc, nullptr); // flush
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(enc, pkt) == 0) {
            pkt->duration = static_cast<int64_t>(ticksPerFrame);
            av_packet_rescale_ts(pkt, requested, st->time_base);
            pkt->stream_index = st->index;
            if (av_interleaved_write_frame(oc, pkt) < 0) {
                std::fprintf(stderr, "make_tick_fixture: flush write failed\n");
                exitCode = 2;
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    if (exitCode == 0 && av_write_trailer(oc) < 0) {
        std::fprintf(stderr, "make_tick_fixture: write_trailer failed\n");
        exitCode = 2;
    }
    std::printf("timebase %d/%d ticks_per_frame %lld frames %lld\n",
                st->time_base.num, st->time_base.den, ticksPerFrame, frames);

    av_frame_free(&frame);
    avcodec_free_context(&enc);
    if (!(oc->oformat->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);
    avformat_free_context(oc);
    return exitCode;
}
