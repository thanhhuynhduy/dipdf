#include "mainwindow.h"
#include "theme_manager.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QInputMethod>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTimer>
#include <QEvent>
#include <QPointer>
#include <QWidget>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QPixmapCache>

namespace {
class InputMethodWakeupFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::FocusIn && event->type() != QEvent::MouseButtonPress) {
            return QObject::eventFilter(watched, event);
        }

        QWidget *widget = qobject_cast<QWidget*>(watched);
        const bool isTextEditor = qobject_cast<QLineEdit*>(watched)
                                  || qobject_cast<QTextEdit*>(watched)
                                  || qobject_cast<QPlainTextEdit*>(watched);
        if (!widget || !isTextEditor) {
            return QObject::eventFilter(watched, event);
        }

        widget->setAttribute(Qt::WA_InputMethodEnabled, true);
        widget->setInputMethodHints(Qt::ImhNone);

        QPointer<QWidget> safeWidget(widget);
        QTimer::singleShot(0, [safeWidget]() {
            if (!safeWidget) return;
            if (QInputMethod *im = QGuiApplication::inputMethod()) {
                im->update(Qt::ImQueryAll);
                im->show();
            }
        });

        return QObject::eventFilter(watched, event);
    }
};
}

int main(int argc, char *argv[]) {
#if defined(Q_OS_LINUX)
    // Force XCB instead of Wayland for AppImage builds.
    // On GNOME/Wayland, a Qt Wayland AppImage can run without client-side
    // decorations, which makes the minimize/maximize/close buttons disappear.
    // Launch with DIPDF_KEEP_QPA_PLATFORM=1 to keep the user's platform choice.
    if (!qEnvironmentVariableIsSet("DIPDF_KEEP_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }

    // This app targets fcitx5 Vietnamese input on Linux. Force Qt to load the
    // fcitx Qt input-context plugin before QApplication is created. If you ever
    // need another IME, launch with DIPDF_KEEP_IM_MODULE=1.
    if (!qEnvironmentVariableIsSet("DIPDF_KEEP_IM_MODULE")) {
        qputenv("QT_IM_MODULE", QByteArrayLiteral("fcitx"));
        qputenv("XMODIFIERS", QByteArrayLiteral("@im=fcitx"));
        qputenv("GTK_IM_MODULE", QByteArrayLiteral("fcitx"));
        qputenv("SDL_IM_MODULE", QByteArrayLiteral("fcitx"));
    }
#endif

    QCoreApplication::setApplicationName("DiPDF");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("DiPDF");
    QGuiApplication::setDesktopFileName("DiPDF");

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);

    // Cap Qt's global pixmap cache to 10 MB (default is 10 MB on desktop,
    // but explicit is safer).  Large PDF page pixmaps are managed by our
    // own PageCache, so the global cache does not need to be large.
    QPixmapCache::setCacheLimit(10240); // 10 MB in KB

    InputMethodWakeupFilter inputMethodWakeupFilter(&app);
    app.installEventFilter(&inputMethodWakeupFilter);
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set both application and top-level window icons.
    // The app icon is used by dialogs; the window icon is what most Linux
    // desktops show in the taskbar/window switcher.
    const QIcon appIcon(":/assets/icon.png");
    app.setWindowIcon(appIcon);

    QQmlApplicationEngine engine;

    // Load bundled fonts
    QFontDatabase::addApplicationFont(":/assets/Quicksand-Regular.ttf");
    QFontDatabase::addApplicationFont(":/assets/Quicksand-Medium.ttf");
    QFontDatabase::addApplicationFont(":/assets/Quicksand-Bold.ttf");
    QFontDatabase::addApplicationFont(":/assets/MaterialSymbolsOutlined.ttf");

    // Apply unified theme from ThemeManager
    app.setStyleSheet(ThemeManager::globalStyleSheet());

    // Proactively initialize the platform input method. This helps fcitx5 show
    // preedit/candidate UI consistently after the first editable widget gains focus.
    if (app.inputMethod()) {
        app.inputMethod()->update(Qt::ImQueryAll);
    }

    MainWindow w;
    w.resize(1200, 800);
    w.showMaximized();

    if (argc > 1) { w.openPdfFile(argv[1]); }

    return app.exec();
}