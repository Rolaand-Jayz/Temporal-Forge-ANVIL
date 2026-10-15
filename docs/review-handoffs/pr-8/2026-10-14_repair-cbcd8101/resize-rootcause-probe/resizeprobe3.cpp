#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QTest>
#include <cstdio>
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickWindow w; QQuickWindow modal;
    w.resize(1280, 720);
    w.show(); w.requestActivate();
    QTimer t1; QObject::connect(&t1, &QTimer::timeout, [&]{
        w.resize(760, 480);
        modal.resize(400, 300); modal.show(); modal.requestActivate();
    });
    t1.setSingleShot(true); t1.start(1500);
    QTimer t2; QObject::connect(&t2, &QTimer::timeout, [&]{
        modal.hide();
        // Fixed strategy: activate first, then resize.
        w.requestActivate();
    });
    t2.setSingleShot(true); t2.start(3500);
    QTimer t3; QObject::connect(&t3, &QTimer::timeout, [&]{
        std::printf("active=%d, now resizing\n", int(w.isActive()));
        std::fflush(stdout);
        w.resize(1280, 720);
    });
    t3.setSingleShot(true); t3.start(4000);
    QTimer t4; QObject::connect(&t4, &QTimer::timeout, [&]{
        std::printf("after1280: %dx%d active=%d\n", w.width(), w.height(), int(w.isActive()));
        std::fflush(stdout);
        app.quit();
    });
    t4.setSingleShot(true); t4.start(6500);
    return app.exec();
}
