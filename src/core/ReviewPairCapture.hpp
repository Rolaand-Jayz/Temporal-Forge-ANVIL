// Controlled paired capture at one verified ANVIL→FSR4 decoder timestamp.
// Never fabricates or labels a bypass frame as reconstructed.
#pragma once

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QFileDevice>
#include <QString>
#include <QSaveFile>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}

namespace anvil {
struct ReviewFrameMeta {
    uint64_t frameIndex = 0;
    int64_t ptsUs = -1;
    int64_t ptsTicks = -1;
    int tbNum = 0;
    int tbDen = 0;
    int sourceW = 0;
    int sourceH = 0;
    double anvilStageMs = 0;
    double fsrDispatchMs = 0;
    std::string sourceId;
    std::string gitSha;
};

// All capture inputs are the ACTUAL output of the same player frame:
// raw = post-ANVIL YUV GPU upload/color-conversion, pre-FSR;
// fsr = fully dispatched post-FSR presentation image.
// Both are copied under the dispatch lock, not a UI screenshot.
class ReviewPairCapture {
public:
    ReviewPairCapture(std::filesystem::path dir, std::set<uint64_t> frames,
                      QByteArray attestKey = {})
        : dir_(std::move(dir)), requested_(std::move(frames)),
          attestKey_(std::move(attestKey)) {}

    static ReviewPairCapture fromEnvironment() {
        const char* dir = std::getenv("ANVIL_REVIEW_CAPTURE_DIR");
        const char* ids = std::getenv("ANVIL_REVIEW_FRAMES");
        std::set<uint64_t> frames;
        if (!dir || !*dir || !ids || !*ids) return {{}, {}};
        std::string input(ids);
        size_t start = 0;
        while (start < input.size() && frames.size() < 64) {
            const size_t end = input.find(',', start);
            const std::string part = input.substr(start, end == std::string::npos
                                                           ? end : end - start);
            uint64_t index = 0;
            const auto parsed = std::from_chars(part.data(), part.data()+part.size(), index);
            if (parsed.ec != std::errc{} || parsed.ptr != part.data()+part.size())
                return {{}, {}};  // malformed selection cannot silently capture another frame
            frames.insert(index);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        if (frames.empty() || frames.size() > 64) return {{}, {}};
        const char* keyFile = std::getenv("ANVIL_REVIEW_ATTEST_KEY_FILE");
        if (!keyFile || !*keyFile) return {{}, {}};
        QFileInfo keyInfo(QString::fromLocal8Bit(keyFile));
        QFileInfo dirInfo(QString::fromLocal8Bit(dir));
        if (!keyInfo.isFile() || keyInfo.isSymLink() ||
            keyInfo.absoluteFilePath().startsWith(dirInfo.absoluteFilePath() + QDir::separator()) ||
            (keyInfo.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
                                       QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther)))
            return {{}, {}};
        QFile keyFileHandle(keyInfo.absoluteFilePath());
        if (!keyFileHandle.open(QIODevice::ReadOnly)) return {{}, {}};
        const QByteArray key = keyFileHandle.readAll();
        if (key.size() < 32 || key.size() > 4096) return {{}, {}};
        return {dir, std::move(frames), key};
    }

    bool enabled() const { return !dir_.empty() && !requested_.empty() && attestKey_.size() >= 32; }
    bool wants(uint64_t frame) const {
        return enabled() && requested_.contains(frame) && !completed_.contains(frame);
    }

    // Returns false without publishing a partial manifest on any failure.
    bool save(const ReviewFrameMeta& meta,
              const std::vector<uint8_t>& raw, uint32_t rawW, uint32_t rawH,
              const std::vector<uint8_t>& fsr, uint32_t fsrW, uint32_t fsrH,
              QString* error = nullptr) {
        auto fail = [&](const QString& reason) {
            if (error) *error = reason;
            return false;
        };
        if (!wants(meta.frameIndex)) return fail("Frame not requested or already captured");
        if (meta.ptsUs < 0 || meta.sourceW != static_cast<int>(rawW) ||
            meta.sourceH != static_cast<int>(rawH) ||
            rawW == 0 || rawH == 0 || fsrW == 0 || fsrH == 0 ||
            fsrW > 16384 || fsrH > 16384 || rawW > 16384 || rawH > 16384 ||
            raw.size() != static_cast<size_t>(rawW)*rawH*4 ||
            fsr.size() != static_cast<size_t>(fsrW)*fsrH*4)
            return fail("Invalid dimensions, timestamp, or readback byte count");

        // The causal live subset is NOT the immutable offline ANVIL baseline.
        // Lanczos aligns presentation grids, not reconstruction methods.
        std::vector<uint8_t> baseline(static_cast<size_t>(fsrW)*fsrH*4);
        SwsContext* scaler = sws_getContext(
            static_cast<int>(rawW), static_cast<int>(rawH), AV_PIX_FMT_RGBA,
            static_cast<int>(fsrW), static_cast<int>(fsrH), AV_PIX_FMT_RGBA,
            SWS_LANCZOS, nullptr, nullptr, nullptr);
        if (!scaler) return fail("Could not initialize spatial viewing scaler");
        const uint8_t* inputs[4] = {raw.data(), nullptr, nullptr, nullptr};
        const int inputStride[4] = {static_cast<int>(rawW)*4, 0, 0, 0};
        uint8_t* outputs[4] = {baseline.data(), nullptr, nullptr, nullptr};
        const int outputStride[4] = {static_cast<int>(fsrW)*4, 0, 0, 0};
        const int lines = sws_scale(scaler, inputs, inputStride, 0,
                                    static_cast<int>(rawH), outputs, outputStride);
        sws_freeContext(scaler);
        if (lines != static_cast<int>(fsrH))
            return fail("Spatial presentation conversion incomplete");

        const QString folder = QString("frame_%1").arg(meta.frameIndex, 6, 10, QLatin1Char('0'));
        const QString root = QString::fromStdString(dir_.string());
        const QString target = QDir(root).filePath(folder);
        if (QFileInfo::exists(QDir(target).filePath("manifest.json")))
            return fail("A manifest already exists for this frame; refusing overwrite");
        if (!QDir().mkpath(target)) return fail("Could not create output directory");

        QImage imageA(baseline.data(), static_cast<int>(fsrW), static_cast<int>(fsrH),
                      static_cast<int>(fsrW)*4, QImage::Format_RGBA8888);
        QImage imageB(fsr.data(), static_cast<int>(fsrW), static_cast<int>(fsrH),
                      static_cast<int>(fsrW)*4, QImage::Format_RGBA8888);
        const QString pathA = QDir(target).filePath("anvil_causal_live_control.png");
        const QString pathB = QDir(target).filePath("anvil_causal_live_fsr41.png");
        if (!imageA.save(pathA, "PNG") || !imageB.save(pathB, "PNG")) {
            QFile::remove(pathA); QFile::remove(pathB);
            return fail("Could not write both lossless PNG images");
        }
        const auto sha = [](const QString& path) -> QString {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return {};
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (!hash.addData(&file)) return {};
            return QString::fromLatin1(hash.result().toHex());
        };
        const QString shaA = sha(pathA), shaB = sha(pathB);
        if (shaA.isEmpty() || shaB.isEmpty()) {
            QFile::remove(pathA); QFile::remove(pathB);
            return fail("Could not hash capture images");
        }
        auto arm = [](const char* id, const QString& image, const QString& digest,
                      const char* label, const char* stage, const char* status) {
            return QJsonObject{
                {"id", id}, {"image", image}, {"sha256", digest},
                {"label", label}, {"stage", stage}, {"status", status}};
        };
        QJsonObject manifest{
            {"schema", "anvil-visual-pair-v2"},
            {"frame_index", static_cast<qint64>(meta.frameIndex)},
            {"pts_us", static_cast<qint64>(meta.ptsUs)},
            {"pts_ticks", static_cast<qint64>(meta.ptsTicks)},
            {"tb_num", meta.tbNum}, {"tb_den", meta.tbDen},
            {"source_width", meta.sourceW}, {"source_height", meta.sourceH},
            {"output_width", static_cast<int>(fsrW)},
            {"output_height", static_cast<int>(fsrH)},
            {"source_id", QString::fromStdString(meta.sourceId)},
            {"git_sha", QString::fromStdString(meta.gitSha)},
            {"anvil_stage_ms", meta.anvilStageMs},
            {"fsr_dispatch_ms", meta.fsrDispatchMs},
            {"verified_same_frame", true},
            {"capture_attestation", "producer-hmac-sha256-v1"},
            {"anvil_live_profile", "causal_cpu_previous_frame_v1"},
            {"live_control_presentation", "GPU YUV-to-RGBA then Lanczos display-grid alignment"},
            {"a", arm("live-control", "anvil_causal_live_control.png", shaA,
                "ANVIL causal live subset v1 — control", "causal_cpu_previous_frame_v1 + Lanczos display alignment", "control")},
            {"b", arm("live-fsr41", "anvil_causal_live_fsr41.png", shaB,
                "ANVIL causal live subset v1 + FSR 4.1 — tryout",
                "causal_cpu_previous_frame_v1 → FSR4.1 RE → presentation", "tryout")}};
        const QByteArray payload = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
        const QByteArray signature = QMessageAuthenticationCode::hash(
            payload, attestKey_, QCryptographicHash::Sha256).toHex();
        QSaveFile seal(QDir(target).filePath("capture.hmac"));
        if (!seal.open(QIODevice::WriteOnly) ||
            seal.write(signature) != signature.size() || !seal.commit()) {
            seal.cancelWriting();
            QFile::remove(pathA); QFile::remove(pathB);
            return fail("Could not commit authenticated capture seal");
        }
        QSaveFile out(QDir(target).filePath("manifest.json"));
        if (!out.open(QIODevice::WriteOnly)) {
            QFile::remove(pathA); QFile::remove(pathB);
            QFile::remove(QDir(target).filePath("capture.hmac"));
            return fail("Could not open manifest for atomic write");
        }
        if (out.write(payload) != payload.size() || !out.commit()) {
            out.cancelWriting();
            QFile::remove(pathA); QFile::remove(pathB);
            QFile::remove(QDir(target).filePath("capture.hmac"));
            return fail("Could not commit manifest");
        }
        completed_.insert(meta.frameIndex);
        return true;
    }
private:
    std::filesystem::path dir_;
    std::set<uint64_t> requested_;
    std::set<uint64_t> completed_;
    QByteArray attestKey_;
};
} // namespace anvil
