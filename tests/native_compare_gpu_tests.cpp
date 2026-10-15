// PR8-F0002: exercise the real decoder -> EASU -> native Qt scene graph.
// No fake engine or texture: record the VkImages selected by the production
// VideoSurfaceItem on Qt's render thread. Missing display/device returns 77;
// missing paired input on a functioning device is a failure, never a skip.
#include "core/PlaybackEngine.hpp"
#include "render/VulkanContext.hpp"
#include "ui/VideoSurfaceItem.hpp"

#include <QGuiApplication>
#include <QDir>
#include <QImage>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QVulkanInstance>
#include <QtQuick/qsgtexture_platform.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace temporal_forge;

class ObservedSurface : public VideoSurfaceItem {
public:
    using VideoSurfaceItem::VideoSurfaceItem;
    std::atomic<VkImage> left{VK_NULL_HANDLE}, right{VK_NULL_HANDLE};
    std::atomic<unsigned> updates{0};
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override {
        auto* result = VideoSurfaceItem::updatePaintNode(old, data);
        auto image = [](QSGNode* n) {
            if (!n) return VkImage(VK_NULL_HANDLE);
            auto* texture = static_cast<QSGSimpleTextureNode*>(n)->texture();
            auto* native = texture ? texture->nativeInterface<
                QNativeInterface::QSGVulkanTexture>() : nullptr;
            return native ? native->nativeImage() : VkImage(VK_NULL_HANDLE);
        };
        left.store(image(result && result->firstChild() ? result->firstChild() : result));
        right.store(image(result && result->firstChild() ? result->lastChild() : nullptr));
        ++updates;
        return result;
    }
};

template<class Predicate> bool waitFor(Predicate predicate, int timeout = 10000) {
    for (int elapsed = 0; elapsed < timeout; elapsed += 10) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

int main(int argc, char** argv) {
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
        std::puts("SKIP: native Qt comparison requires a display and Vulkan device");
        return 77;
    }
    if (argc < 2) return 1;
    // Software decode makes the opt-in CPU reconstruction reachable, while
    // upload, spatial scaling and Qt presentation still execute on Vulkan.
    qputenv("TFORGE_DISABLE_HW_DECODE", "1");
    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QVulkanInstance instance;
    instance.setApiVersion(QVersionNumber(1, 3, 0));
    if (!instance.create()) return 77;
    VulkanContext vk;
    if (!vk.init(false, instance.vkInstance())) return 77;
    PlaybackEngine engine;
    engine.setVulkanHandles(vk.physical(), vk.device(), vk.queue(),
                           vk.queueFamily(), vk.queueFamily(), vk.instance());
    engine.setFsr4Enabled(false);
    QQmlEngine qml;
    qml.rootContext()->setContextProperty("playback", &engine);
    QQuickWindow window;
    window.setVulkanInstance(&instance);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        vk.physical(), vk.device(), int(vk.queueFamily())));
    window.resize(320, 240);
    auto* surface = new ObservedSurface(window.contentItem());
    QQmlEngine::setContextForObject(surface, qml.rootContext());
    surface->setSize(QSizeF(320, 240));
    surface->setRawOnly(true);
    window.show();
    int failures = 0;
    auto check = [&](bool success, const char* description) {
        std::printf("%s: %s\n", success ? "OK" : "FAIL", description);
        if (!success) ++failures;
    };
    VkImage raw = VK_NULL_HANDLE, enhanced = VK_NULL_HANDLE;
    uint32_t rawW = 0, rawH = 0, outW = 0, outH = 0;
    auto pairReady = [&] {
        return engine.fsr4NativeOutput(enhanced, outW, outH) &&
               engine.fsr4RawOutput(raw, rawW, rawH);
    };
    auto repaint = [&](VkImage wantedLeft, VkImage wantedRight) {
        const unsigned before = surface->updates.load();
        surface->requestUpdate();
        window.update();
        return waitFor([&] {
            return surface->updates.load() > before &&
                   surface->left.load() == wantedLeft &&
                   surface->right.load() == wantedRight;
        });
    };
    engine.openUrl(QUrl::fromLocalFile(QString::fromUtf8(argv[1])));
    const bool optIn = std::getenv("ANVIL_LIVE_PRE_FSR") != nullptr;
    const bool ready = waitFor([&] {
        (void)engine.advanceRenderFrame();
        return pairReady() && (!optIn || engine.anvilLiveStage() == "reconstructing");
    });
    check(ready, "EASU-only publishes a valid paired source/output");
    engine.pause();
    // Drain any decode already in progress before observing the frozen pair.
    waitFor([] { return false; }, 100);
    if (pairReady()) {
        check(raw != enhanced && outW == rawW * 2 && outH == rawH * 2,
              "source is a distinct native-resolution VkImage, not EASU output");
        check(repaint(raw, VK_NULL_HANDLE), "rawOnly selects the decoder/control image");
        const QImage rawPixels = window.grabWindow();
        surface->setRawOnly(false);
        check(repaint(enhanced, VK_NULL_HANDLE), "enhanced-only selects spatial output");
        const QImage enhancedPixels = window.grabWindow();
        check(!rawPixels.isNull() && !enhancedPixels.isNull() && rawPixels != enhancedPixels,
              "actual Qt-presented raw and EASU pixels differ for the frozen frame");
        if (const char* directory = std::getenv("TFORGE_TEST_CAPTURE_DIR")) {
            const QDir output(QString::fromUtf8(directory));
            check(QDir().mkpath(output.path()) &&
                  rawPixels.save(output.filePath("native-raw.png")) &&
                  enhancedPixels.save(output.filePath("native-easu.png")),
                  "save actual Qt GPU readback evidence");
        }
        surface->setCompareActive(true);
        surface->setCompareRawOnLeft(true);
        check(repaint(raw, enhanced), "comparison orientation: source/output");
        surface->setCompareRawOnLeft(false);
        check(repaint(enhanced, raw), "comparison orientation: output/source");
        const auto stage = engine.anvilLiveStage();
        check(optIn ? stage == "reconstructing" : stage == "off",
              "published pixel stage matches default-off or reconstructed opt-in frame");
        // Invalidate a previously populated raw cache while enhanced output
        // still exists. The mode setter withdraws publication synchronously.
        surface->setCompareActive(false);
        surface->setRawOnly(true);
        check(repaint(raw, VK_NULL_HANDLE), "raw cache populated before mode switch");
        engine.setFsr4Enabled(true);
        check(!engine.fsr4RawOutput(raw, rawW, rawH), "mode switch withdraws raw publication");
        check(repaint(VK_NULL_HANDLE, VK_NULL_HANDLE),
              "missing raw publication clears the node and stale cache");
        engine.setFsr4Enabled(false);
        // The decoder may have reached EOF while the Qt checks ran. Seek
        // through the public API to require a fresh publication on recovery.
        engine.seekUs(0);
        engine.play();
        check(waitFor([&] {
            (void)engine.advanceRenderFrame();
            return pairReady();
        }), "EASU pair recovers after FSR/EASU switch");
        engine.pause();
        waitFor([] { return false; }, 100);
        check(pairReady() && repaint(raw, VK_NULL_HANDLE), "raw pane recovers with the new pair");
    }
    engine.close();
    check(repaint(VK_NULL_HANDLE, VK_NULL_HANDLE), "close invalidates prior raw texture");
    window.hide();
    window.releaseResources();
    std::printf("native_compare_gpu_tests: %d failures; device=%s\n",
                failures, vk.info().name.c_str());
    return failures ? 1 : 0;
}
