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
#include <QLibraryInfo>



int main(int argc, char *argv[]) {
#if defined(Q_OS_LINUX)
    // On Wayland, forcing XCB can break input methods like fcitx5.
    // If the old XCB workaround is needed for window decorations, use DIPDF_FORCE_XCB=1.
    // IME module fallbacks are now handled inside the AppImage's AppRun script
    // or by the user's native desktop environment variables.
    if (qEnvironmentVariableIsSet("DIPDF_FORCE_XCB")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
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

#if defined(Q_OS_LINUX)
    // ── IME diagnostic output (visible with QT_LOGGING_RULES="*.debug=true") ──
    qDebug("[DiPDF-IME] QT_IM_MODULE    = %s", qgetenv("QT_IM_MODULE").constData());
    qDebug("[DiPDF-IME] XMODIFIERS      = %s", qgetenv("XMODIFIERS").constData());
    qDebug("[DiPDF-IME] QT_QPA_PLATFORM = %s", qgetenv("QT_QPA_PLATFORM").constData());
    qDebug("[DiPDF-IME] GTK_IM_MODULE   = %s", qgetenv("GTK_IM_MODULE").constData());
    qDebug("[DiPDF-IME] SDL_IM_MODULE   = %s", qgetenv("SDL_IM_MODULE").constData());
    qDebug("[DiPDF-IME] QGuiApplication::platformName() = %s", QGuiApplication::platformName().toUtf8().constData());
    qDebug("[DiPDF-IME] QLibraryInfo::path(QLibraryInfo::PluginsPath) = %s", QLibraryInfo::path(QLibraryInfo::PluginsPath).toUtf8().constData());
    qDebug("[DiPDF-IME] Qt plugin path  = %s",
           QCoreApplication::libraryPaths().join(";").toUtf8().constData());
    qDebug("[DiPDF-IME] inputMethod()   = %p", static_cast<void*>(app.inputMethod()));
#endif

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