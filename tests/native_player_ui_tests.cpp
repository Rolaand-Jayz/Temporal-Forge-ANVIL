// Full production Main.qml integration on a real shared Vulkan device.
// No replacement QML, fake playback object, or fabricated GPU result.
#include "anvil/BuildProvenance.hpp"
#include "core/PlaybackEngine.hpp"
#include "render/VulkanContext.hpp"
#include "ui/CompareController.hpp"
#include "ui/DebugOverlayModel.hpp"
#include "ui/FsrController.hpp"
#include "ui/PostProcessController.hpp"
#include "ui/ScreenCaptureController.hpp"
#include "ui/VideoSettingsController.hpp"
#include "ui/VideoSurfaceItem.hpp"
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickWindow>
#include <QVulkanInstance>
#include <QtTest/QTest>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <thread>
// Linker wrapping affects the application's static Vulkan calls only, not
// Qt's renderer. Successful calls always reach the actual Vulkan driver.
// This deliberately injects a submission error, never a successful fake frame.
namespace fault {
std::atomic<bool> armed{false}, presentation{false}, afterNeural{false};
std::atomic<int> entered{0}, released{0}, failed{0};
std::mutex mutex;
std::condition_variable cv;
std::set<VkCommandBuffer> neuralBuffers;
std::atomic<unsigned> successfulNeuralSubmits{0};
void release(int count) {
    released.store(count);
    cv.notify_all();
}
} // namespace fault
extern "C" void __real_vkCmdWriteTimestamp(VkCommandBuffer,
                                           VkPipelineStageFlagBits, VkQueryPool,
                                           uint32_t);
extern "C" void __wrap_vkCmdWriteTimestamp(VkCommandBuffer command,
                                           VkPipelineStageFlagBits stage,
                                           VkQueryPool pool, uint32_t query) {
    {
        std::lock_guard lock(fault::mutex);
        fault::neuralBuffers.insert(command);
    }
    __real_vkCmdWriteTimestamp(command, stage, pool, query);
}
extern "C" VkResult __real_vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*,
                                         VkFence);
extern "C" VkResult __wrap_vkQueueSubmit(VkQueue queue, uint32_t count,
                                         const VkSubmitInfo* submits,
                                         VkFence fence) {
    bool neural = false;
    {
        std::lock_guard lock(fault::mutex);
        for (uint32_t i = 0; i < count; ++i)
            for (uint32_t j = 0; j < submits[i].commandBufferCount; ++j)
                neural |= fault::neuralBuffers.contains(
                    submits[i].pCommandBuffers[j]);
    }
    const bool selected = fault::presentation.load()
                              ? (!neural && fault::afterNeural.exchange(false))
                              : neural;
    if (fault::armed.load() && selected) {
        const int ticket = ++fault::entered;
        if (ticket <= 2) {
            std::unique_lock lock(fault::mutex);
            fault::cv.wait(lock,
                           [&] { return fault::released.load() >= ticket; });
            if (ticket == 1) {
                ++fault::failed;
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
        }
    }
    const auto result = __real_vkQueueSubmit(queue, count, submits, fence);
    if (neural && result == VK_SUCCESS) {
        ++fault::successfulNeuralSubmits;
        if (fault::armed.load())
            fault::afterNeural.store(true);
    }
    return result;
}
using namespace temporal_forge;
template <class P>
bool until(P predicate, int timeout = 15000) {
    for (int elapsed = 0; elapsed < timeout; elapsed += 10) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}
int main(int argc, char** argv) {
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY"))
        return 77;
    if (argc < 2)
        return 1;
    qputenv("TFORGE_DISABLE_HW_DECODE", "1");
    const bool rejectPresentation =
        std::getenv("TFORGE_TEST_REJECT_PRESENTATION") != nullptr;
    fault::presentation.store(rejectPresentation);
    if (rejectPresentation)
        qputenv("TFORGE_FSR4_DISABLE_FUSED_PRESENTATION", "1");
    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QVulkanInstance instance;
    instance.setApiVersion(QVersionNumber(1, 3, 0));
    if (!instance.create())
        return 77;
    VulkanContext vk;
    if (!vk.init(false, instance.vkInstance()))
        return 77;
    PlaybackEngine engine;
    engine.setVulkanHandles(vk.physical(), vk.device(), vk.queue(),
                            vk.queueFamily(), vk.queueFamily(), vk.instance());
    FsrController fsr(&engine);
    CompareController compare;
    VideoSettingsController video;
    Settings settings;
    video.setSettings(&settings);
    DebugOverlayModel debug;
    PostProcessController post;
    ScreenCaptureController capture;
    const bool neural = std::getenv("TFORGE_TEST_NEURAL") != nullptr;
    const bool opted = std::getenv("ANVIL_LIVE_PRE_FSR") != nullptr;
    const QString wantedStage =
        std::getenv("TFORGE_TEST_EXPECT_STAGE")
            ? QString::fromUtf8(std::getenv("TFORGE_TEST_EXPECT_STAGE"))
            : (opted ? QStringLiteral("reconstructing")
                     : QStringLiteral("off"));
    fsr.setPreset(neural ? UpscalePreset::Quality : UpscalePreset::Off);
    qmlRegisterType<VideoSurfaceItem>("TemporalForge", 1, 0, "VideoSurface");
    QQmlApplicationEngine qml;
    auto* ctx = qml.rootContext();
    ctx->setContextProperty("playback", &engine);
    ctx->setContextProperty("fsr", &fsr);
    ctx->setContextProperty("videoSettings", &video);
    ctx->setContextProperty("compare", &compare);
    ctx->setContextProperty("debug", &debug);
    ctx->setContextProperty("postProcess", &post);
    ctx->setContextProperty("capture", &capture);
    qml.load(QUrl::fromLocalFile(
        QStringLiteral(TFORGE_SOURCE_ROOT "/resources/qml/Main.qml")));
    if (qml.rootObjects().isEmpty())
        return 1;
    auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().first());
    if (!window)
        return 1;
    window->setVulkanInstance(&instance);
    window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        vk.physical(), vk.device(), int(vk.queueFamily())));
    window->show();
    window->requestActivate();
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::printf("%s: %s\n", ok ? "OK" : "FAIL", name);
        if (!ok)
            ++failures;
    };
    check(std::string(anvil::buildprov::gitSha).size() == 40 &&
              std::string(anvil::buildprov::origin) == "build_generated",
          "actual full-player consumer links generated build provenance");
    auto named = [&](const char* name) -> QObject* {
        return window->findChild<QObject*>(QString::fromUtf8(name));
    };
    auto text = [&](const char* name) {
        auto* o = named(name);
        return o ? o->property("text").toString() : QString();
    };
    check(named("compareLeftLabel") && named("compareRightLabel") &&
              named("publishedFrameHud") && named("publishedFrameSettings"),
          "real Main.qml compare/HUD/settings objects instantiated");
    engine.openUrl(QUrl::fromLocalFile(QString::fromUtf8(argv[1])));
    const bool published = until([&] {
        VkImage image;
        uint32_t w, h;
        return engine.fsr4NativeOutput(image, w, h) &&
               engine.anvilLiveStage() == wantedStage &&
               (!neural || engine.lastFsr4DispatchMs() > 0);
    });
    check(published, "actual requested EASU/neural GPU frame published");
    if (neural && published)
        fault::armed.store(true);
    engine.pause();
    until([] { return false; }, 150);
    check(fsr.sourceWidth() == 64 && fsr.sourceHeight() == 48,
          "controller reports the actual loaded source dimensions");
    window->requestActivate();
    until([&] { return window->isActive(); }, 2000);
    QTest::keyClick(window, Qt::Key_C);
    check(until([&] { return compare.active(); }, 2000),
          "production C shortcut enables A/B compare");
    const auto expected = wantedStage == "reconstructing"
                              ? QStringLiteral("POST-ANVIL")
                              : QStringLiteral("SOURCE");
    const auto stage = engine.anvilLiveStage();
    // A resize request is not a realized size: on KDE Wayland a resize
    // issued while the window is still inactive — for example right after a
    // modal dialog hides — is silently dropped by the platform, which is how
    // both width cases previously produced byte-identical captures. Activate
    // the window first, keep re-issuing the request across transient
    // compositor states, and fail loudly when the delivered geometry never
    // arrives instead of snapshotting an unverified width claim.
    auto resizeConfirmed = [](QQuickWindow* w, int width, int height) {
        w->requestActivate();
        QTest::qWaitForWindowActive(w, 2000);
        for (int waited = 0; waited < 5000; waited += 50) {
            if (w->width() == width && w->height() == height)
                return true;
            if (waited % 500 == 0)
                w->resize(width, height);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        return w->width() == width && w->height() == height;
    };
    QImage mainCaptures[2];
    int mainCaptureCount = 0;
    for (int width : {760, 1280}) {
        check(resizeConfirmed(window, width, width == 760 ? 480 : 720),
              "compositor realizes the requested player window geometry");
        std::printf("realized player window: %dx%d logical (DPR %.2f)\n",
                    window->width(), window->height(),
                    window->devicePixelRatio());
        window->setProperty("chromeVisible", true);
        until([] { return false; }, 150);
        check(text("compareLeftLabel") == expected &&
                  text("compareRightLabel") == "PLAYER OUTPUT",
              "actual comparison labels match source/control stage");
        check(wantedStage == "off"
                  ? text("publishedFrameHud").contains("FSR")
                  : text("publishedFrameHud")
                        .contains("ANVIL " + wantedStage.toUpper()),
              "actual HUD reflects published stage");
        check(wantedStage == "off"
                  ? text("publishedFrameSettings").contains("off (opt in")
                  : text("publishedFrameSettings")
                        .contains(wantedStage == "standby"
                                      ? "waiting to reconstruct"
                                      : wantedStage),
              "actual Settings text reflects published stage");
        compare.swapSides();
        until([] { return false; }, 100);
        check(text("compareLeftLabel") == "PLAYER OUTPUT" &&
                  text("compareRightLabel") == expected,
              "actual QML pane labels follow swapped orientation");
        if (const char* dir = std::getenv("TFORGE_TEST_CAPTURE_DIR")) {
            QDir output(QString::fromUtf8(dir));
            QDir().mkpath(output.path());
            const auto pixels = window->grabWindow();
            check(!pixels.isNull() &&
                      pixels.save(output.filePath(
                          QString("main-%1-swapped.png").arg(width))),
                  "capture real full-player comparison window");
            if (mainCaptureCount < 2)
                mainCaptures[mainCaptureCount++] = pixels;
        }
        compare.swapSides();
        auto* dialog = qobject_cast<QQuickWindow*>(named("playerSettings"));
        if (dialog) {
            dialog->setProperty("currentTab", 2);
            QMetaObject::invokeMethod(dialog, "show");
            until([&] { return dialog->isVisible(); }, 2000);
            // Exercise the Settings window itself at the same supported
            // widths (760 is its minimumWidth) instead of snapshotting the
            // fixed default-size dialog twice.
            check(resizeConfirmed(dialog, width, width == 760 ? 560 : 680),
                  "compositor realizes the requested settings window geometry");
            std::printf("realized settings window: %dx%d logical\n",
                        dialog->width(), dialog->height());
            until([] { return false; }, 150);
            auto* banner =
                qobject_cast<QQuickItem*>(named("experimentalBackendWarning"));
            auto* warningTitle = qobject_cast<QQuickItem*>(
                named("experimentalBackendWarningTitle"));
            auto* warningBody = qobject_cast<QQuickItem*>(
                named("experimentalBackendWarningBody"));
            check(banner && warningTitle && warningBody &&
                      banner->isVisible() && banner->width() > 0,
                  "experimental backend warning banner is laid out and visible");
            if (banner && warningTitle && warningBody) {
                const qreal contentLeft = 181; // 180px sidebar + 1px divider
                const auto titlePos = warningTitle->mapToItem(
                    dialog->contentItem(), QPointF(0, 0));
                const auto bodyPos = warningBody->mapToItem(
                    dialog->contentItem(), QPointF(0, 0));
                check(titlePos.x() >= contentLeft &&
                          titlePos.x() + warningTitle->width() <=
                              dialog->width() - 4 &&
                          bodyPos.x() >= contentLeft &&
                          bodyPos.x() + warningBody->width() <=
                              dialog->width() - 4,
                      "warning text bounds stay inside the settings content "
                      "viewport");
                check(warningTitle->width() >=
                              warningTitle->implicitWidth() - 0.5 &&
                          warningBody->width() >=
                              warningBody->implicitWidth() - 0.5,
                      "warning title and explanation fit untruncated on "
                      "their lines at this width");
            }
            if (const char* dir = std::getenv("TFORGE_TEST_CAPTURE_DIR")) {
                auto image = dialog->grabWindow();
                check(!image.isNull() &&
                          image.save(
                              QDir(QString::fromUtf8(dir))
                                  .filePath(
                                      QString("settings-%1.png").arg(width))),
                      "capture actual Settings upscaler status");
            }
            dialog->hide();
        }
    }
    if (mainCaptureCount == 2)
        check(mainCaptures[0] != mainCaptures[1],
              "the 760px and 1280px player captures are distinct layouts");
    if (neural && published) {
        window->setProperty("chromeVisible", true);
        const auto before = window->grabWindow();
        const QRect region(before.width() / 4, before.height() / 3, 32, 32);
        // Resume the real QML timer so queued metadata can retire and the
        // next neural dispatch can reach the injected submission boundary.
        engine.play();
        const bool pending = until([] { return fault::entered.load() >= 1; },
                                   10000);
        check(pending,
              rejectPresentation
                  ? "hold actual post-neural presentation submission"
                  : "hold an actual timestamped neural graph queue submission");
        if (pending) {
            until([] { return false; }, 100);
            check(engine.anvilLiveStage() == stage &&
                      text("compareLeftLabel") == expected,
                  "pending neural work retains the displayed image stage and "
                  "QML label");
            check(window->grabWindow().copy(region) == before.copy(region),
                  "pending neural work does not replace the displayed "
                  "raw/control pixels");
        }
        fault::release(1);
        const bool rejected = until(
            [] {
                return fault::failed.load() == 1 && fault::entered.load() >= 2;
            },
            10000);
        check(rejected, "real neural submission rejects injected Vulkan error "
                        "and reaches retry");
        if (rejected) {
            until([] { return false; }, 100);
            check(engine.anvilLiveStage() == stage &&
                      text("compareLeftLabel") == expected,
                  "failed neural submission cannot mislabel old pixels SOURCE");
            check(window->grabWindow().copy(region) == before.copy(region),
                  "failed neural submission retains the paired previous "
                  "control pixels");
        }
        const unsigned priorSubmits = fault::successfulNeuralSubmits.load();
        fault::release(2);
        fault::armed.store(false);
        check(until(
                  [&] {
                      return fault::successfulNeuralSubmits.load() >
                                 priorSubmits &&
                             engine.lastFsr4DispatchMs() > 0 &&
                             engine.anvilLiveStage() == stage;
                  },
                  10000),
              "real driver accepts the neural retry after rejected submission");
        engine.setFsr4Enabled(false);
        check(until([&] {
                  VkImage raw, output;
                  uint32_t rw, rh, ow, oh;
                  return engine.fsr4RawOutput(raw, rw, rh) &&
                         engine.fsr4NativeOutput(output, ow, oh) &&
                         raw != output && ow == rw * 2 && oh == rh * 2;
              }),
              "actual neural-to-EASU switch publishes a fresh paired spatial "
              "frame");
        const unsigned beforeNeuralMode = fault::successfulNeuralSubmits.load();
        fsr.setPreset(UpscalePreset::Quality);
        check(until([&] {
                  VkImage raw, output;
                  uint32_t rw, rh, ow, oh;
                  return fault::successfulNeuralSubmits.load() >
                             beforeNeuralMode &&
                         engine.fsr4Active() &&
                         engine.lastFsr4DispatchMs() > 0 &&
                         engine.fsr4RawOutput(raw, rw, rh) &&
                         engine.fsr4NativeOutput(output, ow, oh) && ow > rw * 2;
              }),
              "actual EASU-to-neural switch resumes successful dispatch");
        engine.pause();
    }
    if (!neural) {
        // Withdraw a previously published pair without producing a new frame.
        // The raw pane must repaint even when advanceRenderFrame cannot advance.
        engine.setFsr4Enabled(!engine.fsr4Enabled());
        until([] { return false; }, 100);
        check(engine.anvilLiveStage() == stage,
              "pending mode change does not relabel the prior published stage");
        const auto withdrawn = window->grabWindow();
        bool blank = !withdrawn.isNull();
        for (int y = withdrawn.height() / 2;
             y < withdrawn.height() / 2 + 8 && blank; ++y)
            for (int x = withdrawn.width() / 4; x < withdrawn.width() / 4 + 8;
                 ++x) {
                auto color = withdrawn.pixelColor(x, y);
                if (color.red() + color.green() + color.blue() > 10)
                    blank = false;
            }
        check(blank,
              "paused full Main.qml raw pane clears withdrawn publication");
    }
    if (!neural) {
        // Verify the actual keyboard-to-preset binding without requiring a
        // neural asset provision for this selection-only regression.
        fsr.setBackend(BackendKind::SpatialFallback);
        fsr.setPreset(UpscalePreset::Quality);
        const UpscalePreset presets[] = {
            UpscalePreset::Off, UpscalePreset::Quality, UpscalePreset::Balanced,
            UpscalePreset::Performance, UpscalePreset::UltraPerformance};
        for (int i = 0; i < 5; ++i) {
            // Closing Settings can deliver a delayed focus transition. Each
            // window-scoped shortcut requires the real main window active.
            QCoreApplication::processEvents();
            window->requestActivate();
            check(QTest::qWaitForWindowActive(window, 2000),
                  "main window is active for the window-scoped preset key");
            QTest::keyClick(window, static_cast<Qt::Key>(Qt::Key_1 + i),
                            Qt::ControlModifier);
            check(
                until([&] { return fsr.preset() == presets[i]; }, 1000),
                "production Ctrl+1..5 shortcut selects its advertised preset");
        }
    }
    engine.close();
    window->hide();
    window->releaseResources();
    std::printf("injected failure target: %s\n",
                rejectPresentation ? "post-neural presentation"
                                   : "neural graph");
    std::printf("native_player_ui_tests: %d failures; mode=%s device=%s\n",
                failures, neural ? "neural" : "EASU", vk.info().name.c_str());
    return failures ? 1 : 0;
}
