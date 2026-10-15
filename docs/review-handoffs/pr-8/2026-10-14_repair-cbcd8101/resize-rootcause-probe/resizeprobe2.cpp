#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickWindow w; QQuickWindow modal;
    w.resize(1280, 720);
    w.show(); w.requestActivate();
    QTimer t0; QObject::connect(&t0, &QTimer::timeout, [&]{
        std::printf("initial: %dx%d active=%d\n", w.width(), w.height(), int(w.isActive()));
        w.resize(760, 480);
    });
    t0.setSingleShot(true); t0.start(1000);
    QTimer t1; QObject::connect(&t1, &QTimer::timeout, [&]{
        std::printf("after760: %dx%d active=%d\n", w.width(), w.height(), int(w.isActive()));
        std::fflush(stdout);
        modal.resize(400, 300); modal.show(); modal.requestActivate();
    });
    t1.setSingleShot(true); t1.start(3000);
    QTimer t2; QObject::connect(&t2, &QTimer::timeout, [&]{
        std::printf("modal shown; main active=%d. hiding modal\n", int(w.isActive()));
        std::fflush(stdout);
        modal.hide();
        // Resize immediately after hide, exactly like the test loop does.
        w.resize(1280, 720);
    });
    t2.setSingleShot(true); t2.start(4500);
    QTimer t3; QObject::connect(&t3, &QTimer::timeout, [&]{
        std::printf("after1280: %dx%d active=%d\n", w.width(), w.height(), int(w.isActive()));
        std::fflush(stdout);
    });
    t3.setSingleShot(true); t3.start(10000);
    QTimer t4; QObject::connect(&t4, &QTimer::timeout, [&]{ app.quit(); });
    t4.setSingleShot(true); t4.start(11000);
    return app.exec();
}
