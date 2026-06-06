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

    QApplication app(argc, argv);
    InputMethodWakeupFilter inputMethodWakeupFilter(&app);
    app.installEventFilter(&inputMethodWakeupFilter);
    app.setStyle(QStyleFactory::create("Fusion"));

    // ✅ set icon cho app
    app.setWindowIcon(QIcon(":/assets/icon.png"));

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