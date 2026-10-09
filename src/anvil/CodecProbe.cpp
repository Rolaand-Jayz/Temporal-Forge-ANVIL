// CodecProbe.cpp — in-process encode→decode MV-export measurement.
#include "CodecProbe.hpp"

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/motion_vector.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

namespace anvil {
namespace {

// Deterministic 64x64 YUV420P frame with a block that translates by one
// pixel per frame — enough signal for real encoders to emit block MVs.
AVFrame* makeSyntheticFrame(int width, int height, int frameIdx) {
    AVFrame* f = av_frame_alloc();
    f->format = AV_PIX_FMT_YUV420P;
    f->width = width;
    f->height = height;
    if (av_frame_get_buffer(f, 0) < 0) {
        av_frame_free(&f);
        return nullptr;
    }
    if (av_frame_make_writable(f) < 0) {
        av_frame_free(&f);
        return nullptr;
    }
    const int bx = (frameIdx * 2) % (width - 24);
    const int by = frameIdx % (height - 24);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool inBlock = x >= bx && x < bx + 16 && y >= by && y < by + 16;
            const uint8_t v = inBlock ? uint8_t(200 + (x + y) % 40)
                                      : uint8_t(40 + ((x / 8) + (y / 8) + frameIdx) % 60);
            f->data[0][y * f->linesize[0] + x] = v;
        }
    }
    for (int c = 1; c <= 2; ++c) {
        const int cw = width / 2, ch = height / 2;
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x)
                f->data[c][y * f->linesize[c] + x] = uint8_t(128 + ((x / 4) + frameIdx) % 20 - 10);
    }
    return f;
}

struct EncodedPackets {
    std::vector<AVPacket*> packets; // ref-counted; caller frees
    int expectedFrames = 0;
    bool ok = false;
};

EncodedPackets encodeSynthetic(const AVCodec* encoder, int width, int height, int frames) {
    EncodedPackets out;
    AVCodecContext* c = avcodec_alloc_context3(encoder);
    if (!c) return out;
    c->width = width;
    c->height = height;
    c->time_base = AVRational{1, 30};
    c->framerate = AVRational{30, 1};
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->gop_size = 30;
    c->max_b_frames = 0;
    if (avcodec_open2(c, encoder, nullptr) < 0) {
        avcodec_free_context(&c);
        return out;
    }

    auto drainPackets = [&]() -> bool {
        for (;;) {
            AVPacket* pkt = av_packet_alloc();
            if (!pkt) return false;
            const int rc = avcodec_receive_packet(c, pkt);
            if (rc == 0) {
                out.packets.push_back(pkt); // transferred ownership
                continue;
            }
            av_packet_free(&pkt);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) return true;
            return false;
        }
    };

    bool ok = true;
    for (int i = 0; i < frames && ok; ++i) {
        AVFrame* f = makeSyntheticFrame(width, height, i);
        if (!f) { ok = false; break; }
        f->pts = i;
        const int sendRc = avcodec_send_frame(c, f);
        av_frame_free(&f);
        if (sendRc < 0 || !drainPackets()) ok = false;
    }
    if (ok) {
        const int flushRc = avcodec_send_frame(c, nullptr);
        if (flushRc < 0 && flushRc != AVERROR_EOF) ok = false;
        if (ok && !drainPackets()) ok = false;
    }
    avcodec_free_context(&c);
    out.expectedFrames = frames;
    out.ok = ok && !out.packets.empty();
    return out;
}

// Decodes packets with MV export requested; counts frames carrying MV side data.
struct DecodeProbeResult {
    bool decoderAvailable = false;
    bool decodedOk = false;
    int framesWithMv = 0;
    int totalFrames = 0;
};

DecodeProbeResult decodeProbe(const AVCodec* decoder, const EncodedPackets& enc) {
    DecodeProbeResult out;
    if (!decoder) return out;
    out.decoderAvailable = true;
    AVCodecContext* c = avcodec_alloc_context3(decoder);
    if (!c) return out;
    c->flags2 |= AV_CODEC_FLAG2_EXPORT_MVS;
    c->thread_count = 1;
    if (avcodec_open2(c, decoder, nullptr) < 0) {
        avcodec_free_context(&c);
        return out;
    }
    AVFrame* f = av_frame_alloc();
    if (!f) {
        avcodec_free_context(&c);
        return out;
    }

    auto drainFrames = [&]() -> bool {
        for (;;) {
            const int rc = avcodec_receive_frame(c, f);
            if (rc == 0) {
                ++out.totalFrames;
                if (av_frame_get_side_data(f, AV_FRAME_DATA_MOTION_VECTORS))
                    ++out.framesWithMv;
                av_frame_unref(f);
                continue;
            }
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) return true;
            return false;
        }
    };

    bool ok = true;
    for (AVPacket* pkt : enc.packets) {
        if (avcodec_send_packet(c, pkt) < 0 || !drainFrames()) {
            ok = false;
            break;
        }
    }
    if (ok) {
        const int flushRc = avcodec_send_packet(c, nullptr);
        if (flushRc < 0 && flushRc != AVERROR_EOF) ok = false;
        if (ok && !drainFrames()) ok = false;
    }
    // A capability probe is valid only if the synthetic stream decoded
    // completely. Partial decode must not be reported as "decoded ok".
    out.decodedOk = ok && enc.expectedFrames > 0
        && out.totalFrames == enc.expectedFrames;
    av_frame_free(&f);
    avcodec_free_context(&c);
    return out;
}

const AVCodec* firstAvailableEncoder(AVCodecID id, std::initializer_list<const char*> names,
                                     std::string& usedName) {
    for (const char* n : names) {
        if (const AVCodec* e = avcodec_find_encoder_by_name(n)) {
            usedName = n;
            return e;
        }
    }
    if (const AVCodec* e = avcodec_find_encoder(id)) {
        usedName = e->name ? e->name : avcodec_get_name(id);
        return e;
    }
    return nullptr;
}

} // namespace

std::vector<Manifest::CodecCapability> probeCodecCapabilities() {
    constexpr int kW = 64, kH = 64, kFrames = 8;
    std::vector<Manifest::CodecCapability> out;

    struct Spec {
        const char* codec;
        AVCodecID id;
        std::initializer_list<const char*> encoders;
    };
    const Spec specs[] = {
        {"h264", AV_CODEC_ID_H264, {"libx264", "libopenh264"}},
        {"hevc", AV_CODEC_ID_HEVC, {"libx265", "libsvthevc"}},
        {"av1", AV_CODEC_ID_AV1, {"libsvtav1", "libaom-av1"}},
    };

    for (const Spec& spec : specs) {
        Manifest::CodecCapability cap;
        cap.codec = spec.codec;

        std::string encName;
        const AVCodec* encoder =
            firstAvailableEncoder(spec.id, spec.encoders, encName);
        cap.encoderAvailable = encoder != nullptr;
        cap.encoder = encoder ? encName : "absent";

        const AVCodec* decoder = avcodec_find_decoder(spec.id);
        cap.decoderAvailable = decoder != nullptr;

        if (encoder && decoder) {
            EncodedPackets enc = encodeSynthetic(encoder, kW, kH, kFrames);
            if (enc.ok) {
                DecodeProbeResult dp = decodeProbe(decoder, enc);
                cap.probeTotalFrames = dp.totalFrames;
                cap.probeMvFrames = dp.framesWithMv;
                cap.decodeProbePassed = dp.decodedOk;
                cap.mvExportProven = dp.decodedOk && dp.framesWithMv > 0;
                if (!dp.decodedOk) {
                    cap.note = "synthetic decode probe failed or produced no frames; "
                               "MV capability unproven";
                } else if (!cap.mvExportProven) {
                    cap.note = "decoded ok but no AV_FRAME_DATA_MOTION_VECTORS "
                               "side data observed from this software decoder";
                }
            } else {
                cap.note = "synthetic encode failed; capability unproven";
            }
            for (AVPacket* p : enc.packets) av_packet_free(&p);
        } else {
            cap.note = encoder ? "decoder absent" : "encoder absent; capability unproven";
        }
        out.push_back(cap);
    }
    return out;
}

} // namespace anvil
