#include "mainwindow.h"
#include "theme_manager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QToolBar>
#include <QCoreApplication>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QApplication>
#include <QToolButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QClipboard>
#include <QTimer>
#include <QPainter>
#include <QGridLayout>
#include <QFrame>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QWindow>
#include <QRubberBand>
#include <QtMath>
#include <QInputMethod>
#include "pdfviewport.h"
#include "pagecache.h"

// ── Memory / cache configuration constants ──
static constexpr qint64 PAGE_CACHE_BUDGET_MB      = 96;
static constexpr qint64 THUMBNAIL_CACHE_BUDGET_MB  = 16;
static constexpr double THUMBNAIL_DPI              = 36.0;


class PdfScrollArea : public QScrollArea {
public:
    explicit PdfScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {
        setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(0, 0);
    }

    QSize sizeHint() const override {
        return QSize(900, 650);
    }

    QSize minimumSizeHint() const override {
        return QSize(0, 0);
    }
};

class BookmarkItemDelegate : public QStyledItemDelegate {
public:
    BookmarkItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        int depth = 0;
        QModelIndex p = index.parent();
        while (p.isValid()) {
            depth++;
            p = p.parent();
        }

        int indent = 12 + depth * 20;
        bool hasChildren = index.model()->hasChildren(index);
        bool isExpanded = false;
        if (auto *tree = qobject_cast<const QTreeView*>(opt.widget)) {
            isExpanded = tree->isExpanded(index);
        }

        painter->fillRect(opt.rect, QColor("#FFFFFF")); // Clear entire row to white

        QRect contentRect = opt.rect;
        contentRect.setLeft(opt.rect.left() + indent);

        // Leaf items must not have a fake expand/branch block.  Only rows that
        // really have children reserve space for a chevron.  This keeps the
        // selected background from painting over a left-side placeholder.
        if (!hasChildren) {
            contentRect.setLeft(opt.rect.left() + indent + 4);
        }

        // Draw custom selected/hover background ONLY over content rect
        if (opt.state & QStyle::State_Selected) {
            painter->fillRect(contentRect, QColor("#EFF6FF"));
        } else if (opt.state & QStyle::State_MouseOver) {
            painter->fillRect(contentRect, QColor("#F1F5F9"));
        }

        if (hasChildren) {
            QRect arrowRect(contentRect.left() + 4, opt.rect.top() + (opt.rect.height() - 16) / 2, 16, 16);
            QString iconPath = isExpanded ? ":/assets/chevron-down.svg" : ":/assets/chevron-right.svg";
            QIcon icon(iconPath);
            icon.paint(painter, arrowRect, Qt::AlignCenter);
            contentRect.setLeft(arrowRect.right() + 6);
        }

        // Draw Text
        painter->setPen(QColor(opt.state & QStyle::State_Selected ? "#2563EB" : "#374151"));
        QFont font = opt.font;
        if (opt.state & QStyle::State_Selected) font.setBold(true);
        painter->setFont(font);

        QRect textRect = contentRect;
        textRect.setLeft(textRect.left() + 4);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, opt.text);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(option.rect.width(), 32);
    }
};

const QList<double> ZOOM_PRESETS = {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0};

namespace {
void enableTextInputMethod(QWidget *widget) {
    if (!widget) return;
    widget->setAttribute(Qt::WA_InputMethodEnabled, true);
    widget->setFocusPolicy(Qt::StrongFocus);
    if (qobject_cast<QTextEdit*>(widget))
        widget->setInputMethodHints(Qt::ImhMultiLine);
    else
        widget->setInputMethodHints(Qt::ImhNone);
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("DiPDF - Modern Academic PDF Reader");
    setAcceptDrops(true);

    appSettings = new QSettings("DiPDFCorp", "DiPDFApp", this);
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onAiReplyFinished);

    // ── Create memory-bounded page caches ──
    m_pageCache      = new PageCache(PAGE_CACHE_BUDGET_MB * 1024 * 1024,
                                     QStringLiteral("MainPageCache"));
    m_thumbnailCache = new PageCache(THUMBNAIL_CACHE_BUDGET_MB * 1024 * 1024,
                                     QStringLiteral("ThumbnailCache"));

    createMainLayout();
    applyGlobalStyle();
    loadAiConfig();
    updateRecentFilesGrid();
}

MainWindow::~MainWindow() {
    // Clean up all open document tabs (frees Poppler::Document pointers).
    while (pdfTabWidget->count() > 0) {
        QWidget *w = pdfTabWidget->widget(0);
        if (w && w != plusTabWidget) {
            if (auto *sa = qobject_cast<QScrollArea*>(w)) {
                auto *doc = reinterpret_cast<Poppler::Document*>(
                    sa->property("popplerDoc").toLongLong());
                sa->setProperty("popplerDoc", QVariant());
                delete doc;
            }
        }
        pdfTabWidget->removeTab(0);
        if (w && w != plusTabWidget) delete w;
    }

    delete m_pageCache;
    delete m_thumbnailCache;
}

quintptr MainWindow::docIdFromScrollArea(const QScrollArea *sa) {
    if (!sa) return 0;
    return static_cast<quintptr>(sa->property("popplerDoc").toLongLong());
}

void MainWindow::applyGlobalStyle() {
    // ThemeManager::globalStyleSheet() applied in main.cpp — nothing extra needed
}

// ════════════════════════════════════════════════════════════════════════════
// MAIN LAYOUT
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createMainLayout() {
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Nav Sidebar (styled via #NavSidebar in global QSS) ──
    navSidebar = new QListWidget(this);
    navSidebar->setObjectName("NavSidebar");
    navSidebar->setFixedWidth(ThemeManager::NavSidebarW);
    navSidebar->setIconSize(QSize(22, 22));

    QListWidgetItem *logoItem = new QListWidgetItem(QIcon(":/assets/picture_as_pdf.svg"), " DiPDF");
    logoItem->setFont(ThemeManager::headingFont(16));
    logoItem->setForeground(QBrush(QColor(ThemeManager::TextPrimary)));
    logoItem->setFlags(Qt::NoItemFlags);
    navSidebar->addItem(logoItem);

    navSidebar->addItem(new QListWidgetItem(QIcon(":/assets/home.svg"),     "  Home"));
    navSidebar->addItem(new QListWidgetItem(QIcon(":/assets/history.svg"),  "  Recent"));
    navSidebar->addItem(new QListWidgetItem(QIcon(":/assets/star.svg"),     "  Favorite"));
    navSidebar->addItem(new QListWidgetItem(QIcon(":/assets/settings.svg"), "  Cài đặt"));
    navSidebar->setCurrentRow(1);

    mainStack = new QStackedWidget(this);
    mainStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainStack->setMinimumSize(0, 0);

    createHomeView();
    createRecentView();
    createFavoriteView();
    createSettingsView();

    // ── PDF Tab Widget ──
    pdfTabWidget = new QTabWidget(this);
    pdfTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pdfTabWidget->setMinimumSize(0, 0);
    pdfTabWidget->setTabsClosable(true);
    pdfTabWidget->setStyleSheet(ThemeManager::pdfTabBarStyle());
    pdfTabWidget->setElideMode(Qt::ElideRight);

    plusTabWidget = new QWidget();
    pdfTabWidget->addTab(plusTabWidget, "+");
    if (QTabBar *tabBar = pdfTabWidget->findChild<QTabBar *>()) {
        tabBar->setTabButton(0, QTabBar::RightSide, nullptr);
    }

    connect(pdfTabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closePdfTab);
    connect(pdfTabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (suppressPlusTabOpen) return;
        if (index >= 0 && pdfTabWidget->widget(index) == plusTabWidget) {
            if (pdfTabWidget->count() <= 1) return;
            int prev = -1;
            for (int i = 0; i < pdfTabWidget->count(); ++i) {
                if (pdfTabWidget->widget(i) != plusTabWidget) prev = i;
            }
            if (prev >= 0) pdfTabWidget->setCurrentIndex(prev);
            onOpenFileClicked();
        } else {
            onPdfTabChanged(index);
        }
    });

    mainStack->addWidget(homeView);      // 0
    mainStack->addWidget(recentView);    // 1
    mainStack->addWidget(favoriteView);  // 2
    mainStack->addWidget(settingsView);  // 3
    mainStack->addWidget(pdfTabWidget);  // 4

    mainLayout->addWidget(navSidebar);
    mainLayout->addWidget(mainStack);
    setCentralWidget(centralWidget);

    createReaderToolbar();
    createLeftSidebar();
    createRightSidebar();

    connect(navSidebar, &QListWidget::currentRowChanged, this, &MainWindow::onNavigationChanged);
}

// ════════════════════════════════════════════════════════════════════════════
// HOME VIEW
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createHomeView() {
    homeView = new QWidget(this);
    homeView->setStyleSheet(QString("background-color: %1;").arg(ThemeManager::Surface));
    QVBoxLayout *layout = new QVBoxLayout(homeView);
    layout->setContentsMargins(48, 40, 48, 40);

    QLabel *greeting = new QLabel("Chào mừng bạn đến với DiPDF Reader", this);
    greeting->setStyleSheet(ThemeManager::homeGreetingStyle());
    greeting->setFont(ThemeManager::headingFont(24));

    QLabel *subGreeting = new QLabel("Đọc PDF nhanh chóng, gọn nhẹ và hiệu quả", this);
    subGreeting->setStyleSheet(ThemeManager::homeSubGreetingStyle());

    layout->addWidget(greeting, 0, Qt::AlignHCenter);
    layout->addWidget(subGreeting, 0, Qt::AlignHCenter);

    QWidget *cardsContainer = new QWidget(this);
    cardsContainer->setMaximumWidth(1200);
    cardsContainer->setMinimumWidth(0);
    cardsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *topCardsLayout = new QHBoxLayout(cardsContainer);
    topCardsLayout->setContentsMargins(0, 0, 0, 0);
    topCardsLayout->setSpacing(ThemeManager::Sp24);

    // ── Primary open button ──
    QPushButton *openFileCard = new QPushButton("📂  Mở file PDF", this);
    openFileCard->setFixedHeight(96);
    openFileCard->setMinimumWidth(300);
    openFileCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    openFileCard->setCursor(Qt::PointingHandCursor);
    openFileCard->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; border-radius: %2px; "
                "font-size: 17px; font-weight: bold; }"
                "QPushButton:hover { background-color: %3; }")
        .arg(ThemeManager::Primary)
        .arg(ThemeManager::RadiusLarge)
        .arg(ThemeManager::PrimaryHover)
    );
    connect(openFileCard, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);

    // ── Drop zone ──
    QPushButton *dropZoneCard = new QPushButton("☁️  Kéo thả file vào đây\nhoặc click để chọn file", this);
    dropZoneCard->setFixedHeight(96);
    dropZoneCard->setMinimumWidth(300);
    dropZoneCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dropZoneCard->setCursor(Qt::PointingHandCursor);
    dropZoneCard->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; "
                "border: 2px dashed %3; border-radius: %4px; "
                "font-size: 15px; font-weight: 600; }"
                "QPushButton:hover { background-color: %5; border-color: %6; }")
        .arg(ThemeManager::SurfaceMuted)
        .arg(ThemeManager::TextLabel)
        .arg(ThemeManager::BorderStrong)
        .arg(ThemeManager::RadiusLarge)
        .arg(ThemeManager::PrimarySoft)
        .arg(ThemeManager::Primary)
    );
    connect(dropZoneCard, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);

    topCardsLayout->addWidget(openFileCard, 1);
    topCardsLayout->addWidget(dropZoneCard, 1);
    layout->addWidget(cardsContainer);
    layout->addSpacing(ThemeManager::Sp24);

    // ── Recent files header ──
    QHBoxLayout *recentHeader = new QHBoxLayout();
    QLabel *recentLabel = new QLabel("🕒 Đọc gần đây", this);
    recentLabel->setStyleSheet(ThemeManager::homeSectionTitleStyle());

    QPushButton *clearAllBtn = new QPushButton("Xóa tất cả", this);
    clearAllBtn->setStyleSheet(ThemeManager::dangerButtonStyle());
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    connect(clearAllBtn, &QPushButton::clicked, this, [this](){
        appSettings->setValue("recent_files", QStringList());
        updateRecentFilesGrid();
    });

    recentHeader->addWidget(recentLabel);
    recentHeader->addStretch();
    recentHeader->addWidget(clearAllBtn);
    layout->addLayout(recentHeader);

    recentFilesHomeGrid = new QListWidget(this);
    recentFilesHomeGrid->setStyleSheet(ThemeManager::fileListStyle());
    layout->addWidget(recentFilesHomeGrid);
}

// ════════════════════════════════════════════════════════════════════════════
// RECENT VIEW
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createRecentView() {
    recentView = new QWidget(this);
    recentView->setStyleSheet(QString("background-color: %1;").arg(ThemeManager::Surface));
    QVBoxLayout *layout = new QVBoxLayout(recentView);
    layout->setContentsMargins(32, 24, 32, 24);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("All Recent Files", this);
    title->setStyleSheet(ThemeManager::settingsTitleStyle());

    QPushButton *clearAllBtn = new QPushButton("Xóa tất cả", this);
    clearAllBtn->setStyleSheet(ThemeManager::dangerButtonStyle());
    clearAllBtn->setCursor(Qt::PointingHandCursor);
    connect(clearAllBtn, &QPushButton::clicked, this, [this](){
        appSettings->setValue("recent_files", QStringList());
        updateRecentFilesGrid();
    });

    header->addWidget(title);
    header->addStretch();
    header->addWidget(clearAllBtn);

    allRecentFilesList = new QListWidget(this);
    allRecentFilesList->setStyleSheet(ThemeManager::fileListStyle());

    layout->addLayout(header);
    layout->addWidget(allRecentFilesList);
}

// ════════════════════════════════════════════════════════════════════════════
// FAVORITE VIEW
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createFavoriteView() {
    favoriteView = new QWidget(this);
    favoriteView->setStyleSheet(QString("background-color: %1;").arg(ThemeManager::Surface));
    QVBoxLayout *layout = new QVBoxLayout(favoriteView);
    layout->setContentsMargins(32, 24, 32, 24);

    QHBoxLayout *header = new QHBoxLayout();
    QLabel *title = new QLabel("Favorite Files", this);
    title->setStyleSheet(ThemeManager::settingsTitleStyle());

    QPushButton *clearAllFavBtn = new QPushButton("Xóa tất cả", this);
    clearAllFavBtn->setStyleSheet(ThemeManager::dangerButtonStyle());
    clearAllFavBtn->setCursor(Qt::PointingHandCursor);
    connect(clearAllFavBtn, &QPushButton::clicked, this, [this](){
        appSettings->setValue("favorite_files", QStringList());
        updateRecentFilesGrid();
    });

    header->addWidget(title);
    header->addStretch();
    header->addWidget(clearAllFavBtn);

    favoriteFilesList = new QListWidget(this);
    favoriteFilesList->setStyleSheet(ThemeManager::fileListStyle());

    layout->addLayout(header);
    layout->addWidget(favoriteFilesList);
}

// ════════════════════════════════════════════════════════════════════════════
// SETTINGS VIEW
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createSettingsView() {
    settingsView = new QWidget(this);
    settingsView->setStyleSheet(QString("background-color: %1;").arg(ThemeManager::Surface));
    QVBoxLayout *contentLayout = new QVBoxLayout(settingsView);
    contentLayout->setContentsMargins(40, 32, 40, 32);
    contentLayout->setAlignment(Qt::AlignTop);

    QLabel *title = new QLabel("OpenAI Compatible", this);
    title->setStyleSheet(ThemeManager::settingsTitleStyle());
    title->setFont(ThemeManager::headingFont(20));

    QLabel *sub = new QLabel("Dùng cho chức năng dịch và hỏi đáp tài liệu", this);
    sub->setStyleSheet(ThemeManager::settingsSubtitleStyle());

    apiUrlInput = new QLineEdit(this);
    apiKeyInput = new QLineEdit(this);
    modelNameInput = new QLineEdit(this);

    apiKeyInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    enableTextInputMethod(apiUrlInput);
    enableTextInputMethod(modelNameInput);
    // Keep the API key field as a password field, but still allow IME when users
    // paste/type non-ASCII local keys or provider names.
    enableTextInputMethod(apiKeyInput);

    apiUrlInput->setStyleSheet(ThemeManager::settingsInputStyle());
    apiKeyInput->setStyleSheet(ThemeManager::settingsInputStyle());
    modelNameInput->setStyleSheet(ThemeManager::settingsInputStyle());

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *testBtn = new QPushButton("  Kiểm tra kết nối", this);
    testBtn->setIcon(QIcon(":/assets/sync.svg"));
    testBtn->setIconSize(QSize(18, 18));
    testBtn->setCursor(Qt::PointingHandCursor);
    testBtn->setStyleSheet(ThemeManager::secondaryButtonStyle());

    QPushButton *saveBtn = new QPushButton("  Lưu cấu hình", this);
    saveBtn->setIcon(QIcon(":/assets/save.svg"));
    saveBtn->setIconSize(QSize(18, 18));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(ThemeManager::primaryButtonStyle());
    
    connect(testBtn, &QPushButton::clicked, this, [this, testBtn](){
        QString urlStr = apiUrlInput->text().trimmed();
        QString apiKey = apiKeyInput->text().trimmed();
        QString modelName = modelNameInput->text().trimmed();

        testBtn->setText("  Đang kiểm tra...");
        testBtn->setEnabled(false);

        QUrl url(urlStr);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!apiKey.isEmpty()) {
            request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
        }

        QJsonObject rootObj;
        rootObj["model"] = modelName;
        rootObj["max_tokens"] = 1;
        QJsonArray messages;
        QJsonObject sysMsg;
        sysMsg["role"] = "system"; sysMsg["content"] = "test connection";
        messages.append(sysMsg);
        rootObj["messages"] = messages;

        QNetworkReply *reply = networkManager->post(request, QJsonDocument(rootObj).toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply, testBtn]() {
            if (reply->error() == QNetworkReply::NoError) {
                QMessageBox::information(this, "Thành công", "Kết nối tới API OpenAI Compatible thành công!");
            } else {
                QByteArray data = reply->readAll();
                QMessageBox::warning(this, "Lỗi kết nối", QString("Không thể kết nối API.\nHTTP Code: %1\n\nNội dung:\n%2")
                                  .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                                  .arg(QString::fromUtf8(data).left(500)));
            }
            testBtn->setText("  Kiểm tra kết nối");
            testBtn->setEnabled(true);
            reply->deleteLater();
        });
    });
    
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveAiConfig);

    btnLayout->addWidget(testBtn);
    btnLayout->addWidget(saveBtn);
    btnLayout->addStretch();

    contentLayout->addWidget(title);
    contentLayout->addWidget(sub);
    contentLayout->addSpacing(ThemeManager::Sp8);

    QLabel *l1 = new QLabel("Base URL", this);
    l1->setStyleSheet(ThemeManager::settingsLabelStyle());
    contentLayout->addWidget(l1);
    contentLayout->addWidget(apiUrlInput);

    QLabel *l2 = new QLabel("API Key", this);
    l2->setStyleSheet(ThemeManager::settingsLabelStyle());
    contentLayout->addWidget(l2);
    contentLayout->addWidget(apiKeyInput);

    QLabel *l3 = new QLabel("Model", this);
    l3->setStyleSheet(ThemeManager::settingsLabelStyle());
    contentLayout->addWidget(l3);
    contentLayout->addWidget(modelNameInput);

    contentLayout->addSpacing(ThemeManager::Sp24);
    contentLayout->addLayout(btnLayout);
    contentLayout->addStretch();
}

// ════════════════════════════════════════════════════════════════════════════
// READER TOOLBAR — Centered [Navigation] [Zoom] groups
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createReaderToolbar() {
    readerToolbar = new QToolBar("Reader Options", this);
    readerToolbar->setMovable(false);
    readerToolbar->setStyleSheet(ThemeManager::toolbarStyle());
    readerToolbar->setIconSize(QSize(18, 18));

    QWidget *leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    readerToolbar->addWidget(leftSpacer);

    // ── Page Navigation Group (compact, fixed-width) ──
    QWidget *navGroup = new QWidget(this);
    navGroup->setStyleSheet(ThemeManager::toolbarGroupStyle());
    navGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout *navLayout = new QHBoxLayout(navGroup);
    navLayout->setContentsMargins(4, 3, 4, 3);
    navLayout->setSpacing(2);

    QPushButton *prevBtn = new QPushButton("◀", this);
    prevBtn->setFixedSize(32, 32);
    prevBtn->setCursor(Qt::PointingHandCursor);
    prevBtn->setToolTip("Trang trước");
    prevBtn->setStyleSheet(ThemeManager::toolbarGroupButtonStyle());

    pageInput = new QLineEdit("0", this);
    pageInput->setFixedWidth(64);
    pageInput->setAlignment(Qt::AlignCenter);
    pageInput->setAttribute(Qt::WA_InputMethodEnabled, false);
    pageInput->setStyleSheet(
        QString("QLineEdit { border: 1px solid %1; border-radius: 6px; padding: 4px; "
                "background: %2; font-size: 13px; font-weight: 600; "
                "color: %3; min-height: 14px; }"
                "QLineEdit:focus { border-color: %4; }")
        .arg(ThemeManager::Border)
        .arg(ThemeManager::Surface)
        .arg(ThemeManager::TextPrimary)
        .arg(ThemeManager::Primary)
    );
    connect(pageInput, &QLineEdit::returnPressed, this, &MainWindow::jumpToPage);

    totalPagesLabel = new QLabel("/ 0", this);
    totalPagesLabel->setStyleSheet(
        QString("color: %1; font-weight: 600; font-size: 13px; background: transparent; padding: 0 4px;")
        .arg(ThemeManager::TextSecondary)
    );

    QPushButton *nextBtn = new QPushButton("▶", this);
    nextBtn->setFixedSize(32, 32);
    nextBtn->setCursor(Qt::PointingHandCursor);
    nextBtn->setToolTip("Trang sau");
    nextBtn->setStyleSheet(ThemeManager::toolbarGroupButtonStyle());

    connect(prevBtn, &QPushButton::clicked, this, [this]() {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            auto *viewport = qobject_cast<PdfViewport*>(sa->widget());
            if (viewport) {
                int target = viewport->currentVisiblePage() - 1;
                if (target >= 0) {
                    sa->verticalScrollBar()->setValue(viewport->pageYOffset(target));
                    updateToolbarDisplay();
                    scheduleThumbnailRenderAround(target);
                }
            }
        }
    });

    connect(nextBtn, &QPushButton::clicked, this, [this]() {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
            auto *viewport = qobject_cast<PdfViewport*>(sa->widget());
            if (doc && viewport) {
                int target = viewport->currentVisiblePage() + 1;
                if (target < doc->numPages()) {
                    sa->verticalScrollBar()->setValue(viewport->pageYOffset(target));
                    updateToolbarDisplay();
                    scheduleThumbnailRenderAround(target);
                }
            }
        }
    });


    navLayout->addWidget(prevBtn);
    navLayout->addWidget(pageInput);
    navLayout->addWidget(totalPagesLabel);
    navLayout->addWidget(nextBtn);
    readerToolbar->addWidget(navGroup);

    // ── Separator ──
    QWidget *sep = new QWidget(this);
    sep->setFixedWidth(ThemeManager::Sp12);
    readerToolbar->addWidget(sep);

    // ── Zoom Group (compact, fixed-width) ──
    QWidget *zoomGroup = new QWidget(this);
    zoomGroup->setStyleSheet(ThemeManager::toolbarGroupStyle());
    zoomGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout *zoomLayout = new QHBoxLayout(zoomGroup);
    zoomLayout->setContentsMargins(4, 3, 4, 3);
    zoomLayout->setSpacing(2);

    QPushButton *zoomOutBtn = new QPushButton("−", this);
    zoomOutBtn->setFixedSize(32, 32);
    zoomOutBtn->setCursor(Qt::PointingHandCursor);
    zoomOutBtn->setToolTip("Thu nhỏ");
    zoomOutBtn->setStyleSheet(ThemeManager::toolbarGroupButtonStyle());
    connect(zoomOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);

    zoomLabel = new QLabel("100%", this);
    zoomLabel->setFixedWidth(52);
    zoomLabel->setAlignment(Qt::AlignCenter);
    zoomLabel->setStyleSheet(
        QString("font-weight: 600; color: %1; font-size: 13px; background: transparent;")
        .arg(ThemeManager::TextPrimary)
    );

    QPushButton *zoomInBtn = new QPushButton("+", this);
    zoomInBtn->setFixedSize(32, 32);
    zoomInBtn->setCursor(Qt::PointingHandCursor);
    zoomInBtn->setToolTip("Phóng to");
    zoomInBtn->setStyleSheet(ThemeManager::toolbarGroupButtonStyle());
    connect(zoomInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);

    zoomLayout->addWidget(zoomOutBtn);
    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addWidget(zoomInBtn);
    readerToolbar->addWidget(zoomGroup);

    QWidget *rightSpacer = new QWidget(this);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    readerToolbar->addWidget(rightSpacer);

    // ── Navigation signals (logic preserved exactly) ──
    connect(prevBtn, &QPushButton::clicked, this, [this]() {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            int page = sa->property("currentPage").toInt();
            if (page > 0) {
                QWidget *container = sa->widget();
                if (container && container->layout()) {
                    QWidget *pageWidget = container->layout()->itemAt(page - 1)->widget();
                    if (pageWidget) sa->verticalScrollBar()->setValue(pageWidget->y());
                }
            }
        }
    });
    connect(nextBtn, &QPushButton::clicked, this, [this]() {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            int page = sa->property("currentPage").toInt();
            auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
            if (doc && page < doc->numPages() - 1) {
                QWidget *container = sa->widget();
                if (container && container->layout()) {
                    QWidget *pageWidget = container->layout()->itemAt(page + 1)->widget();
                    if (pageWidget) sa->verticalScrollBar()->setValue(pageWidget->y());
                }
            }
        }
    });

    addToolBar(Qt::TopToolBarArea, readerToolbar);
    readerToolbar->hide();
}

// ════════════════════════════════════════════════════════════════════════════
// LEFT SIDEBAR — Thumbnails + Bookmarks + Collapse
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createLeftSidebar() {
    leftDock = new QDockWidget(this);
    leftDock->setTitleBarWidget(new QWidget());
    leftDock->setMinimumWidth(ThemeManager::LeftPanelMinW);
    leftDock->setMaximumWidth(ThemeManager::LeftPanelMaxW);
    leftDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    QWidget *leftWrapper = new QWidget(leftDock);
    QVBoxLayout *leftWrapperLayout = new QVBoxLayout(leftWrapper);
    leftWrapperLayout->setContentsMargins(0, 0, 0, 0);
    leftWrapperLayout->setSpacing(0);

    // ── Panel Header ──
    QWidget *leftHeader = new QWidget(leftWrapper);
    leftHeader->setFixedHeight(ThemeManager::PanelHeaderH);
    leftHeader->setStyleSheet(ThemeManager::panelHeaderStyle());
    QHBoxLayout *leftHeaderLayout = new QHBoxLayout(leftHeader);
    leftHeaderLayout->setContentsMargins(ThemeManager::Sp12, 0, ThemeManager::Sp8, 0);

    QLabel *leftTitle = new QLabel("📑 Tài liệu", leftHeader);
    leftTitle->setStyleSheet(ThemeManager::panelTitleStyle());

    leftCollapseBtn = new QPushButton("«", leftHeader);
    leftCollapseBtn->setStyleSheet(ThemeManager::collapseButtonStyle());
    leftCollapseBtn->setCursor(Qt::PointingHandCursor);
    leftCollapseBtn->setToolTip("Thu gọn panel trái");
    connect(leftCollapseBtn, &QPushButton::clicked, this, &MainWindow::toggleLeftPanel);

    leftHeaderLayout->addWidget(leftTitle);
    leftHeaderLayout->addStretch();
    leftHeaderLayout->addWidget(leftCollapseBtn);
    leftWrapperLayout->addWidget(leftHeader);

    // ── Custom Full-Width Tab Bar for Left Panel ──
    QWidget *leftTabBarWidget = new QWidget(leftWrapper);
    leftTabBarWidget->setStyleSheet(ThemeManager::customTabBarContainerStyle());
    QHBoxLayout *leftTabBarLayout = new QHBoxLayout(leftTabBarWidget);
    leftTabBarLayout->setContentsMargins(0, 0, 0, 0);
    leftTabBarLayout->setSpacing(0);

    QPushButton *tabThumbBtn = new QPushButton("Thumbnail", leftTabBarWidget);
    QPushButton *tabBmBtn = new QPushButton("Bookmark", leftTabBarWidget);
    for (auto btn : {tabThumbBtn, tabBmBtn}) {
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(ThemeManager::customTabButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        leftTabBarLayout->addWidget(btn);
    }
    tabThumbBtn->setChecked(true);
    leftWrapperLayout->addWidget(leftTabBarWidget);

    // ── Tab widget (hidden default tab bar) ──
    leftSidebarTabs = new QTabWidget(leftWrapper);
    leftSidebarTabs->tabBar()->hide();
    leftSidebarTabs->setStyleSheet("QTabWidget::pane { border: none; background: #FFFFFF; }");

    connect(tabThumbBtn, &QPushButton::clicked, this, [this, tabThumbBtn, tabBmBtn]() {
        leftSidebarTabs->setCurrentIndex(0);
        tabThumbBtn->setChecked(true); tabBmBtn->setChecked(false);
    });
    connect(tabBmBtn, &QPushButton::clicked, this, [this, tabThumbBtn, tabBmBtn]() {
        leftSidebarTabs->setCurrentIndex(1);
        tabThumbBtn->setChecked(false); tabBmBtn->setChecked(true);
    });
    
    // Connect original index-changed in case it changes programmatically
    connect(leftSidebarTabs, &QTabWidget::currentChanged, this, [tabThumbBtn, tabBmBtn](int index) {
        tabThumbBtn->setChecked(index == 0);
        tabBmBtn->setChecked(index == 1);
    });

    // ── Thumbnail list ──
    thumbnailListWidget = new QListWidget(this);
    thumbnailListWidget->setViewMode(QListView::IconMode);
    thumbnailListWidget->setMovement(QListView::Static);
    thumbnailListWidget->setResizeMode(QListView::Adjust);
    thumbnailListWidget->setGridSize(QSize(170, 250));
    thumbnailListWidget->setItemAlignment(Qt::AlignHCenter);
    thumbnailListWidget->setStyleSheet(ThemeManager::thumbnailListStyle());
    connect(thumbnailListWidget, &QListWidget::currentRowChanged, this, [this](int row){
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            if (row >= 0) renderCurrentPage(sa, row);
        }
    });

    // ── Bookmark tree + empty state ──
    QWidget *bookmarkContainer = new QWidget(this);
    QVBoxLayout *bookmarkLayout = new QVBoxLayout(bookmarkContainer);
    bookmarkLayout->setContentsMargins(0, 0, 0, 0);
    bookmarkLayout->setSpacing(0);

    bookmarkTreeView = new QTreeView(this);
    bookmarkTreeView->setHeaderHidden(true);
    bookmarkTreeView->setIndentation(0);
    bookmarkTreeView->setItemDelegate(new BookmarkItemDelegate(this));
    bookmarkModel = new QStandardItemModel(this);
    bookmarkTreeView->setModel(bookmarkModel);
    bookmarkTreeView->setStyleSheet(ThemeManager::bookmarkTreeStyle());
    connect(bookmarkTreeView, &QTreeView::clicked, this, &MainWindow::onBookmarkClicked);

    bookmarkEmptyLabel = new QLabel(this);
    bookmarkEmptyLabel->setText("🔖\nChưa có bookmark\n\nBookmark trong PDF sẽ\nhiển thị ở đây.");
    bookmarkEmptyLabel->setAlignment(Qt::AlignCenter);
    bookmarkEmptyLabel->setWordWrap(true);
    bookmarkEmptyLabel->setStyleSheet(ThemeManager::emptyStateStyle());

    bookmarkLayout->addWidget(bookmarkTreeView);
    bookmarkLayout->addWidget(bookmarkEmptyLabel);
    bookmarkEmptyLabel->hide();

    leftSidebarTabs->addTab(thumbnailListWidget, "Thumbnail");
    leftSidebarTabs->addTab(bookmarkContainer,   "Bookmark");
    leftWrapperLayout->addWidget(leftSidebarTabs);

    leftDock->setWidget(leftWrapper);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);
    leftDock->hide();

    // ── Floating expand button ──
    leftExpandBtn = new QPushButton("»", this);
    leftExpandBtn->setStyleSheet(ThemeManager::collapseButtonStyle());
    leftExpandBtn->setCursor(Qt::PointingHandCursor);
    leftExpandBtn->setToolTip("Mở rộng panel trái");
    leftExpandBtn->setFixedSize(28, 56);
    leftExpandBtn->hide();
    connect(leftExpandBtn, &QPushButton::clicked, this, &MainWindow::toggleLeftPanel);
}

// ════════════════════════════════════════════════════════════════════════════
// RIGHT SIDEBAR — Study Panel (Search, Notes, AI Chat)
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::createRightSidebar() {
    rightDock = new QDockWidget(this);
    rightDock->setTitleBarWidget(new QWidget());
    rightDock->setMinimumWidth(ThemeManager::RightPanelMinW);
    rightDock->setMaximumWidth(ThemeManager::RightPanelMaxW);
    rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    QWidget *rightWrapper = new QWidget(rightDock);
    QVBoxLayout *rightWrapperLayout = new QVBoxLayout(rightWrapper);
    rightWrapperLayout->setContentsMargins(0, 0, 0, 0);
    rightWrapperLayout->setSpacing(0);

    // ── Panel Header (same height/style as left) ──
    QWidget *rightHeader = new QWidget(rightWrapper);
    rightHeader->setFixedHeight(ThemeManager::PanelHeaderH);
    rightHeader->setStyleSheet(ThemeManager::panelHeaderStyle());
    QHBoxLayout *rightHeaderLayout = new QHBoxLayout(rightHeader);
    rightHeaderLayout->setContentsMargins(ThemeManager::Sp8, 0, ThemeManager::Sp12, 0);

    rightCollapseBtn = new QPushButton("»", rightHeader);
    rightCollapseBtn->setStyleSheet(ThemeManager::collapseButtonStyle());
    rightCollapseBtn->setCursor(Qt::PointingHandCursor);
    rightCollapseBtn->setToolTip("Thu gọn panel phải");
    connect(rightCollapseBtn, &QPushButton::clicked, this, &MainWindow::toggleRightPanel);

    QLabel *rightTitle = new QLabel("📚 Công cụ học", rightHeader);
    rightTitle->setStyleSheet(ThemeManager::panelTitleStyle());

    rightHeaderLayout->addWidget(rightCollapseBtn);
    rightHeaderLayout->addWidget(rightTitle);
    rightHeaderLayout->addStretch();
    rightWrapperLayout->addWidget(rightHeader);

    // ── Custom Full-Width Tab Bar for Right Panel ──
    QWidget *rightTabBarWidget = new QWidget(rightWrapper);
    rightTabBarWidget->setStyleSheet(ThemeManager::customTabBarContainerStyle());
    QHBoxLayout *rightTabBarLayout = new QHBoxLayout(rightTabBarWidget);
    rightTabBarLayout->setContentsMargins(0, 0, 0, 0);
    rightTabBarLayout->setSpacing(0);

    QPushButton *tabSearchBtn = new QPushButton("Tìm kiếm", rightTabBarWidget);
    QPushButton *tabNotesBtn = new QPushButton("Ghi chú", rightTabBarWidget);
    QPushButton *tabAiBtn = new QPushButton("AI Chat", rightTabBarWidget);
    for (auto btn : {tabSearchBtn, tabNotesBtn, tabAiBtn}) {
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(ThemeManager::customTabButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        rightTabBarLayout->addWidget(btn);
    }
    tabAiBtn->setChecked(true);
    rightWrapperLayout->addWidget(rightTabBarWidget);

    // ── Tab widget (hidden default tab bar) ──
    QTabWidget *rightTabs = new QTabWidget(rightWrapper);
    rightTabs->tabBar()->hide();
    rightTabs->setStyleSheet("QTabWidget::pane { border: none; background: #FFFFFF; }");

    connect(tabSearchBtn, &QPushButton::clicked, this, [rightTabs, tabSearchBtn, tabNotesBtn, tabAiBtn]() {
        rightTabs->setCurrentIndex(0);
        tabSearchBtn->setChecked(true); tabNotesBtn->setChecked(false); tabAiBtn->setChecked(false);
    });
    connect(tabNotesBtn, &QPushButton::clicked, this, [rightTabs, tabSearchBtn, tabNotesBtn, tabAiBtn]() {
        rightTabs->setCurrentIndex(1);
        tabSearchBtn->setChecked(false); tabNotesBtn->setChecked(true); tabAiBtn->setChecked(false);
    });
    connect(tabAiBtn, &QPushButton::clicked, this, [rightTabs, tabSearchBtn, tabNotesBtn, tabAiBtn]() {
        rightTabs->setCurrentIndex(2);
        tabSearchBtn->setChecked(false); tabNotesBtn->setChecked(false); tabAiBtn->setChecked(true);
    });

    connect(rightTabs, &QTabWidget::currentChanged, this, [tabSearchBtn, tabNotesBtn, tabAiBtn](int index) {
        tabSearchBtn->setChecked(index == 0);
        tabNotesBtn->setChecked(index == 1);
        tabAiBtn->setChecked(index == 2);
    });

    int pad = ThemeManager::Sp16;
    int gap = ThemeManager::Sp8;

    // ── TAB 1: Search ──
    QWidget *searchTab = new QWidget();
    QVBoxLayout *searchLayout = new QVBoxLayout(searchTab);
    searchLayout->setContentsMargins(pad, pad, pad, pad);
    searchLayout->setSpacing(gap);

    searchInput = new QLineEdit(this);
    enableTextInputMethod(searchInput);
    searchInput->setPlaceholderText("🔎 Tìm kiếm trong PDF...");
    searchInput->setStyleSheet(ThemeManager::searchInputStyle());
    connect(searchInput, &QLineEdit::returnPressed, this, &MainWindow::performSearch);

    QPushButton *searchBtn = new QPushButton("Tìm kiếm", this);
    searchBtn->setStyleSheet(ThemeManager::primaryButtonStyle());
    searchBtn->setCursor(Qt::PointingHandCursor);
    connect(searchBtn, &QPushButton::clicked, this, &MainWindow::performSearch);

    searchResultList = new QListWidget(this);
    searchResultList->setStyleSheet(ThemeManager::resultListStyle());
    connect(searchResultList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item){
        int targetPage = item->data(Qt::UserRole).toInt();
        if (targetPage >= 0) {
            if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
                renderCurrentPage(sa, targetPage);
                scheduleThumbnailRenderAround(targetPage);
            }
        }
    });

    searchEmptyLabel = new QLabel(this);
    searchEmptyLabel->setText("🔍\nChưa có kết quả\n\nNhập từ khóa để tìm trong tài liệu.");
    searchEmptyLabel->setAlignment(Qt::AlignCenter);
    searchEmptyLabel->setWordWrap(true);
    searchEmptyLabel->setStyleSheet(ThemeManager::emptyStateStyle());

    QLabel *searchResultLabel = new QLabel("Kết quả:", this);
    searchResultLabel->setStyleSheet(ThemeManager::settingsLabelStyle());

    searchLayout->addWidget(searchInput);
    searchLayout->addWidget(searchBtn);
    searchLayout->addSpacing(ThemeManager::Sp4);
    searchLayout->addWidget(searchResultLabel);
    searchLayout->addWidget(searchEmptyLabel);
    searchLayout->addWidget(searchResultList);

    // ── TAB 2: Notes ──
    QWidget *notesTab = new QWidget();
    QVBoxLayout *notesLayout = new QVBoxLayout(notesTab);
    notesLayout->setContentsMargins(pad, pad, pad, pad);
    notesLayout->setSpacing(gap);

    noteInput = new QTextEdit(this);
    enableTextInputMethod(noteInput);
    noteInput->setPlaceholderText("Thêm ghi chú cho trang hiện hành...");
    noteInput->setMaximumHeight(100);

    QPushButton *saveNoteBtn = new QPushButton("📝 Lưu ghi chú", this);
    saveNoteBtn->setStyleSheet(ThemeManager::primaryButtonStyle());
    saveNoteBtn->setCursor(Qt::PointingHandCursor);
    connect(saveNoteBtn, &QPushButton::clicked, this, &MainWindow::saveNote);

    QLabel *notesLabel = new QLabel("Ghi chú đã lưu:", this);
    notesLabel->setStyleSheet(ThemeManager::settingsLabelStyle());

    notesList = new QListWidget(this);
    notesList->setStyleSheet(ThemeManager::resultListStyle());
    notesList->setWordWrap(true);

    notesEmptyLabel = new QLabel(this);
    notesEmptyLabel->setText("📝\nChưa có ghi chú\n\nChọn đoạn văn hoặc trang hiện tại\nđể thêm ghi chú.");
    notesEmptyLabel->setAlignment(Qt::AlignCenter);
    notesEmptyLabel->setWordWrap(true);
    notesEmptyLabel->setStyleSheet(ThemeManager::emptyStateStyle());

    notesLayout->addWidget(noteInput);
    notesLayout->addWidget(saveNoteBtn);
    notesLayout->addSpacing(ThemeManager::Sp4);
    notesLayout->addWidget(notesLabel);
    notesLayout->addWidget(notesEmptyLabel);
    notesLayout->addWidget(notesList);

    // ── TAB 3: AI Chat ──
    QWidget *aiTab = new QWidget();
    QVBoxLayout *aiLayout = new QVBoxLayout(aiTab);
    aiLayout->setContentsMargins(pad, pad, pad, pad);
    aiLayout->setSpacing(gap);

    // Welcome / empty state
    aiEmptyLabel = new QLabel(this);
    aiEmptyLabel->setText("🤖  Hỏi về tài liệu này\n\nBạn có thể yêu cầu tóm tắt, dịch\nhoặc giải thích nội dung PDF.");
    aiEmptyLabel->setAlignment(Qt::AlignCenter);
    aiEmptyLabel->setWordWrap(true);
    aiEmptyLabel->setStyleSheet(ThemeManager::emptyStateStyle());

    // Quick actions
    QLabel *quickLabel = new QLabel("Gợi ý nhanh:", this);
    quickLabel->setStyleSheet(ThemeManager::settingsLabelStyle());

    QWidget *quickActionsWidget = new QWidget(this);
    QGridLayout *quickGrid = new QGridLayout(quickActionsWidget);
    quickGrid->setContentsMargins(0, 0, 0, 0);
    quickGrid->setSpacing(ThemeManager::Sp8);

    auto makeQuickBtn = [this](const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, this);
        btn->setStyleSheet(ThemeManager::quickActionButtonStyle());
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, text](){
            aiInputArea->setText(text);
            aiInputArea->setFocus(Qt::OtherFocusReason);
            if (QGuiApplication::inputMethod()) {
                QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
            }
        });
        return btn;
    };

    quickGrid->addWidget(makeQuickBtn("Tóm tắt trang này"),    0, 0);
    quickGrid->addWidget(makeQuickBtn("Dịch đoạn đã chọn"),    0, 1);
    quickGrid->addWidget(makeQuickBtn("Giải thích thuật ngữ"),  1, 0);
    quickGrid->addWidget(makeQuickBtn("Tạo flashcards"),        1, 1);

    // AI Result area
    aiResultArea = new QTextEdit(this);
    aiResultArea->setReadOnly(true);
    aiResultArea->setStyleSheet(
        QString("QTextEdit { background-color: %1; border: 1px solid %2; "
                "border-radius: 8px; padding: 12px; color: %3; font-size: 13px; }")
        .arg(ThemeManager::SurfaceMuted)
        .arg(ThemeManager::Border)
        .arg(ThemeManager::TextLabel)
    );
    aiResultArea->setAttribute(Qt::WA_InputMethodEnabled, false);

    // Input composer
    aiInputArea = new QTextEdit(this);
    enableTextInputMethod(aiInputArea);
    aiInputArea->setPlaceholderText(QStringLiteral("\U0001F4AC Nh\u1EADp c\u00E2u h\u1ECFi ho\u1EB7c n\u1ED9i dung c\u1EA7n d\u1ECBch..."));
    aiInputArea->setMaximumHeight(90);

    QHBoxLayout *aiBtnLayout = new QHBoxLayout();
    aiBtnLayout->setSpacing(ThemeManager::Sp8);

    QPushButton *transBtn = new QPushButton(" Dịch", this);
    transBtn->setIcon(QIcon(":/assets/translate.svg"));
    transBtn->setStyleSheet(ThemeManager::secondaryButtonStyle());
    transBtn->setCursor(Qt::PointingHandCursor);

    QPushButton *chatBtn = new QPushButton(" Chat AI", this);
    chatBtn->setIcon(QIcon(":/assets/chat.svg"));
    chatBtn->setStyleSheet(ThemeManager::primaryButtonStyle());
    chatBtn->setCursor(Qt::PointingHandCursor);

    connect(transBtn, &QPushButton::clicked, this, [this](){ handleAiAction(true); });
    connect(chatBtn, &QPushButton::clicked, this, [this](){ handleAiAction(false); });

    aiBtnLayout->addWidget(transBtn);
    aiBtnLayout->addWidget(chatBtn);

    aiLayout->addWidget(aiEmptyLabel);
    aiLayout->addWidget(quickLabel);
    aiLayout->addWidget(quickActionsWidget);
    aiLayout->addSpacing(ThemeManager::Sp4);
    aiLayout->addWidget(aiResultArea, 1);
    aiLayout->addWidget(aiInputArea);
    aiLayout->addLayout(aiBtnLayout);

    // ── Add tabs ──
    rightTabs->addTab(searchTab, "🔍 Tìm kiếm");
    rightTabs->addTab(notesTab,  "📝 Ghi chú");
    rightTabs->addTab(aiTab,     "🤖 AI Chat");
    rightTabs->setCurrentIndex(2);

    rightWrapperLayout->addWidget(rightTabs);

    rightDock->setWidget(rightWrapper);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);
    rightDock->hide();

    // ── Floating expand button ──
    rightExpandBtn = new QPushButton("«", this);
    rightExpandBtn->setStyleSheet(ThemeManager::collapseButtonStyle());
    rightExpandBtn->setCursor(Qt::PointingHandCursor);
    rightExpandBtn->setToolTip("Mở rộng panel phải");
    rightExpandBtn->setFixedSize(28, 56);
    rightExpandBtn->hide();
    connect(rightExpandBtn, &QPushButton::clicked, this, &MainWindow::toggleRightPanel);
}

// ════════════════════════════════════════════════════════════════════════════
// SEARCH LOGIC
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::performSearch() {
    searchResultList->clear();
    QString query = searchInput->text().trimmed();
    if (query.isEmpty()) return;

    auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget());
    if (!sa) return;
    auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
    if (!doc) return;

    searchResultList->addItem("Đang quét tài liệu...");
    QCoreApplication::processEvents();

    int matchCount = 0;
    for (int i = 0; i < doc->numPages(); ++i) {
        std::unique_ptr<Poppler::Page> p = doc->page(i);
        if (p) {
            QList<QRectF> matches = p->search(query, Poppler::Page::IgnoreCase);
            if (!matches.isEmpty()) {
                QListWidgetItem *item = new QListWidgetItem(
                    QString("Trang %1: Tìm thấy %2 kết quả").arg(i + 1).arg(matches.size()));
                item->setData(Qt::UserRole, i);
                item->setForeground(QBrush(QColor(ThemeManager::Primary)));
                searchResultList->addItem(item);
                matchCount += matches.size();
            }
        }
        if (i % 5 == 0) QCoreApplication::processEvents();
    }

    delete searchResultList->takeItem(0);

    if (matchCount == 0) {
        searchResultList->addItem("Không tìm thấy kết quả nào.");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// NOTES LOGIC
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::saveNote() {
    QString noteText = noteInput->toPlainText().trimmed();
    if (noteText.isEmpty()) return;

    auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget());
    if (!sa) return;

    int currentPage = sa->property("currentPage").toInt();
    QString fileName = pdfTabWidget->tabText(pdfTabWidget->currentIndex());
    QString key = "notes_" + fileName;

    QStringList currentNotes = appSettings->value(key).toStringList();
    QString noteEntry = QString("📌 Trang %1:\n%2").arg(currentPage + 1).arg(noteText);
    currentNotes.append(noteEntry);

    appSettings->setValue(key, currentNotes);
    noteInput->clear();
    loadNotesForCurrentFile();
}

void MainWindow::loadNotesForCurrentFile() {
    if (!notesList) return;
    notesList->clear();

    if (pdfTabWidget->count() == 0) return;

    QString fileName = pdfTabWidget->tabText(pdfTabWidget->currentIndex());
    QString key = "notes_" + fileName;
    const QStringList currentNotes = appSettings->value(key).toStringList();

    for (const QString &note : currentNotes) {
        QListWidgetItem *item = new QListWidgetItem(note);
        item->setForeground(QBrush(QColor(ThemeManager::TextLabel)));
        notesList->addItem(item);
    }

    if (notesEmptyLabel) {
        notesEmptyLabel->setVisible(currentNotes.isEmpty());
        notesList->setVisible(!currentNotes.isEmpty());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// NAVIGATION
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::onNavigationChanged(int index) {
    if (leftExpandBtn) leftExpandBtn->hide();
    if (rightExpandBtn) rightExpandBtn->hide();

    if (index == 1) {
        mainStack->setCurrentIndex(0);
        navSidebar->show(); readerToolbar->hide(); leftDock->hide(); rightDock->hide();
    } else if (index == 2) {
        mainStack->setCurrentIndex(1);
        navSidebar->show(); readerToolbar->hide(); leftDock->hide(); rightDock->hide();
    } else if (index == 3) {
        mainStack->setCurrentIndex(2);
        navSidebar->show(); readerToolbar->hide(); leftDock->hide(); rightDock->hide();
    } else if (index == 4) {
        mainStack->setCurrentIndex(3);
        navSidebar->show(); readerToolbar->hide(); leftDock->hide(); rightDock->hide();
    } else if (index == 5) {
        navSidebar->hide(); readerToolbar->show(); leftDock->show(); rightDock->show();
        mainStack->setCurrentIndex(4);
    }
}

void MainWindow::onOpenFileClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Select PDF Document", "", "PDF Files (*.pdf)");
    if (!path.isEmpty()) openPdfFile(path);
}

// ════════════════════════════════════════════════════════════════════════════
// PDF FILE OPEN + CANVAS
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::openPdfFile(const QString &filePath) {
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(filePath);
    if (!doc || doc->isLocked()) {
        QMessageBox::critical(this, "Error", "Failed to load document via Poppler native backend.");
        return;
    }

    doc->setRenderHint(Poppler::Document::Antialiasing, true);
    doc->setRenderHint(Poppler::Document::TextAntialiasing, true);

    // A new document starts from the default preset instead of inheriting a
    // previous tab's zoom value.
    currentZoom = 1.0;

    QScrollArea *scrollArea = new PdfScrollArea(this);
    scrollArea->setWidgetResizable(false);
    scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea->setMinimumSize(0, 0);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    scrollArea->setStyleSheet(ThemeManager::scrollAreaCanvasStyle());

    // Match the canvas background on the scroll area viewport so there's
    // no visible gap when the viewport is wider than the document.
    if (scrollArea->viewport()) {
        scrollArea->viewport()->setStyleSheet(
            QString("background: %1;").arg(ThemeManager::CanvasBg));
    }

    Poppler::Document* rawDocPtr = doc.release();
    scrollArea->setProperty("popplerDoc", QVariant::fromValue(reinterpret_cast<qlonglong>(rawDocPtr)));
    scrollArea->setProperty("zoomFactor", currentZoom);

    // ── Create virtualized viewport (NO per-page widgets) ──
    // PdfViewport stores only lightweight metadata (32 bytes/page) and
    // paints visible pages directly from the LRU cache.
    PdfViewport *viewport = new PdfViewport(scrollArea);
    viewport->setDocument(rawDocPtr, m_pageCache, currentZoom, devicePixelRatioF());
    scrollArea->setWidget(viewport);

    connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, scrollArea]() {
        renderVisiblePages(scrollArea);
    });

    // Update toolbar when PdfViewport detects
    // the current page changed (e.g. while scrolling).
    connect(viewport, &PdfViewport::currentPageChanged, this, [this](int page) {
        updateToolbarDisplay();
    });

    QFileInfo info(filePath);
    int idx = pdfTabWidget->insertTab(pdfTabWidget->count() - 1, scrollArea, info.fileName());
    addFileToRecent(filePath);
    pdfTabWidget->setCurrentIndex(idx);

    onNavigationChanged(5);

    // Deferred render — only the first visible pages will actually be rendered.
    QTimer::singleShot(50, this, [this, scrollArea]() {
        renderVisiblePages(scrollArea);
    });
}

// ════════════════════════════════════════════════════════════════════════════
// RENDER PAGES — Cache-backed, with LRU eviction
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::renderVisiblePages(QScrollArea *scrollArea) {
    if (!scrollArea) return;
    auto *viewport = qobject_cast<PdfViewport*>(scrollArea->widget());
    if (!viewport || !viewport->document()) return;

    const int scrollY       = scrollArea->verticalScrollBar()->value();
    const int viewportHeight = scrollArea->viewport()->height();
    viewport->renderVisiblePages(scrollY, viewportHeight);
}

void MainWindow::renderCurrentPage(QScrollArea *scrollArea, int pageIndex) {
    if (!scrollArea) return;
    auto *viewport = qobject_cast<PdfViewport*>(scrollArea->widget());
    if (!viewport || pageIndex < 0 || pageIndex >= viewport->pageCount()) return;

    scrollArea->verticalScrollBar()->setValue(viewport->pageYOffset(pageIndex));
    renderVisiblePages(scrollArea);
    updateToolbarDisplay();
}

// ════════════════════════════════════════════════════════════════════════════
// EVENT FILTER — PdfViewport handles drag-to-scroll and text selection
// internally, so no container interception is needed.
// ════════════════════════════════════════════════════════════════════════════

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    Q_UNUSED(obj);
    Q_UNUSED(event);
    return QMainWindow::eventFilter(obj, event);
}

// ════════════════════════════════════════════════════════════════════════════
// THUMBNAILS — Lazy loading: only render visible items + small buffer
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::renderSidebarThumbnails(Poppler::Document* doc, QListWidget* listWidget) {
    listWidget->clear();
    if (!doc) return;
    int total = doc->numPages();

    listWidget->blockSignals(true);
    listWidget->setIconSize(QSize(140, 200));
    listWidget->setSpacing(6);

    // Shared placeholder — Qt implicit sharing means only 1 pixmap in memory.
    static QPixmap sharedPlaceholder;
    if (sharedPlaceholder.isNull()) {
        sharedPlaceholder = QPixmap(140, 200);
        sharedPlaceholder.fill(QColor(ThemeManager::Surface));
    }
    QIcon placeholderIcon(sharedPlaceholder);

    for (int i = 0; i < total; ++i) {
        QListWidgetItem *item = new QListWidgetItem(placeholderIcon, QString::number(i + 1), listWidget);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        listWidget->addItem(item);
    }
    listWidget->blockSignals(false);

    // Stop existing thumbnail timer
    if (thumbnailTimer) {
        thumbnailTimer->stop();
        thumbnailTimer->deleteLater();
        thumbnailTimer = nullptr;
    }

    // Disconnect old scroll connection
    if (m_thumbScrollConn) {
        disconnect(m_thumbScrollConn);
        m_thumbScrollConn = {};
    }

    // Debounce timer for lazy thumbnail rendering
    thumbnailTimer = new QTimer(this);
    thumbnailTimer->setSingleShot(true);
    thumbnailTimer->setInterval(80);
    connect(thumbnailTimer, &QTimer::timeout, this, &MainWindow::renderVisibleThumbnailsNow);

    // Connect list scroll to debounce timer
    m_thumbScrollConn = connect(listWidget->verticalScrollBar(), &QScrollBar::valueChanged,
                                this, [this]() {
        if (thumbnailTimer) thumbnailTimer->start();
    });

    // Initial render of visible thumbnails (after layout settles)
    QTimer::singleShot(50, this, &MainWindow::renderVisibleThumbnailsNow);
}

void MainWindow::renderVisibleThumbnailsNow() {
    if (!thumbnailListWidget) return;
    auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget());
    if (!sa) return;
    auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
    if (!doc) return;

    const int total = doc->numPages();

    // ── Robust visible-range detection using visualItemRect() ──
    const QRect vpRect = thumbnailListWidget->viewport()->rect();
    int firstVisible = -1, lastVisible = -1;

    for (int i = 0; i < total; ++i) {
        QListWidgetItem *item = thumbnailListWidget->item(i);
        if (!item) continue;
        QRect itemRect = thumbnailListWidget->visualItemRect(item);
        if (itemRect.intersects(vpRect)) {
            if (firstVisible < 0) firstVisible = i;
            lastVisible = i;
        } else if (firstVisible >= 0) {
            break;
        }
    }

    if (firstVisible < 0) {
        int cur = thumbnailListWidget->currentRow();
        if (cur < 0) cur = 0;
        firstVisible = qMax(0, cur - 4);
        lastVisible  = qMin(total - 1, cur + 4);
    }

    firstVisible = qMax(0, firstVisible - 3);
    lastVisible  = qMin(total - 1, lastVisible + 3);

    renderThumbnailRange(firstVisible, lastVisible);
}

void MainWindow::renderThumbnailRange(int first, int last) {
    if (!thumbnailListWidget) return;
    auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget());
    if (!sa) return;
    auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
    if (!doc) return;

    const int total = doc->numPages();
    const quintptr docId = reinterpret_cast<quintptr>(doc);
    first = qMax(0, first);
    last  = qMin(total - 1, last);

    for (int i = first; i <= last; ++i) {
        QListWidgetItem *item = thumbnailListWidget->item(i);
        if (!item) continue;

        if (item->data(Qt::UserRole + 1).toBool()) continue;

        if (m_thumbnailCache) {
            PageCacheKey key = makeCacheKey(docId, i, THUMBNAIL_DPI, 1.0);
            QPixmap cached = m_thumbnailCache->get(key);
            if (!cached.isNull()) {
                item->setIcon(QIcon(cached));
                item->setData(Qt::UserRole + 1, true);
                continue;
            }
        }

        std::unique_ptr<Poppler::Page> page = doc->page(i);
        if (page) {
            QImage img = page->renderToImage(THUMBNAIL_DPI, THUMBNAIL_DPI);
            if (!img.isNull()) {
                QPixmap pix = QPixmap::fromImage(std::move(img))
                                  .scaled(140, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPixmap canvas(140, 200);
                canvas.fill(Qt::white);
                QPainter p(&canvas);
                int x = (140 - pix.width()) / 2;
                int y = (200 - pix.height()) / 2;
                p.drawPixmap(x, y, pix);
                p.end();
                item->setIcon(QIcon(canvas));
                item->setData(Qt::UserRole + 1, true);

                if (m_thumbnailCache) {
                    PageCacheKey key = makeCacheKey(docId, i, THUMBNAIL_DPI, 1.0);
                    m_thumbnailCache->insert(key, canvas);
                }
            }
        }
    }
}

void MainWindow::scheduleThumbnailRenderAround(int pageIndex) {
    if (!thumbnailListWidget) return;
    const int total = thumbnailListWidget->count();
    if (pageIndex < 0 || pageIndex >= total) return;

    thumbnailListWidget->blockSignals(true);
    thumbnailListWidget->setCurrentRow(pageIndex);
    thumbnailListWidget->scrollToItem(
        thumbnailListWidget->item(pageIndex),
        QAbstractItemView::PositionAtCenter);
    thumbnailListWidget->blockSignals(false);

    renderThumbnailRange(pageIndex - 4, pageIndex + 4);

    QTimer::singleShot(0, this, &MainWindow::renderVisibleThumbnailsNow);
}

// ════════════════════════════════════════════════════════════════════════════
// BOOKMARKS
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::loadBookmarksHierarchy(Poppler::Document* doc) {
    bookmarkModel->clear();
    QStandardItem *rootItem = bookmarkModel->invisibleRootItem();
    for (const Poppler::OutlineItem &item : doc->outline()) {
        parseOutlineNode(item, rootItem);
    }

    bool hasBookmarks = bookmarkModel->rowCount() > 0;
    if (bookmarkEmptyLabel) {
        bookmarkEmptyLabel->setVisible(!hasBookmarks);
        bookmarkTreeView->setVisible(hasBookmarks);
    }
}

void MainWindow::parseOutlineNode(const Poppler::OutlineItem &item, QStandardItem *parentItem) {
    QStandardItem *newItem = new QStandardItem(item.name());
    newItem->setEditable(false);

    auto dest = item.destination();
    if (dest) {
        newItem->setData(dest->pageNumber(), Qt::UserRole);
    } else {
        newItem->setData(-1, Qt::UserRole);
    }

    parentItem->appendRow(newItem);
    for (const Poppler::OutlineItem &child : item.children()) {
        parseOutlineNode(child, newItem);
    }
}

void MainWindow::onBookmarkClicked(const QModelIndex &index) {
    if (bookmarkModel->hasChildren(index)) {
        if (bookmarkTreeView->isExpanded(index)) {
            bookmarkTreeView->collapse(index);
        } else {
            bookmarkTreeView->expand(index);
        }
    }

    int targetPage = bookmarkModel->itemFromIndex(index)->data(Qt::UserRole).toInt();
    if (targetPage >= 0) {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
            renderCurrentPage(sa, targetPage);
            scheduleThumbnailRenderAround(targetPage);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// AI CHAT (LOGIC UNCHANGED)
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::handleAiAction(bool isTranslate) {
    QString prompt = aiInputArea->toPlainText().trimmed();
    if (prompt.isEmpty()) return;

    aiResultArea->setText("AI Engine is working, please wait...");

    QString urlStr = appSettings->value("api_url", "http://127.0.0.1:52625/v1/chat/completions").toString();
    QString apiKey = appSettings->value("api_key", "lm-studio").toString();
    QString modelName = appSettings->value("api_model", "translategemma:4b").toString();

    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    }

    QString systemRole = isTranslate ? "You are a professional translator. Translate text to natural Vietnamese. Only output the translation."
                                     : "You are a helpful assistant. Reply concisely in Vietnamese.";

    QJsonObject rootObj;
    rootObj["model"] = modelName;
    rootObj["temperature"] = isTranslate ? 0.3 : 0.7;

    QJsonArray messages;
    QJsonObject sysMsg, userMsg;
    sysMsg["role"] = "system"; sysMsg["content"] = systemRole;
    userMsg["role"] = "user"; userMsg["content"] = prompt;
    messages.append(sysMsg);
    messages.append(userMsg);
    rootObj["messages"] = messages;

    networkManager->post(request, QJsonDocument(rootObj).toJson());
}

void MainWindow::onAiReplyFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject rootObj = doc.object();
        
        if (rootObj.contains("choices")) {
            QString output = rootObj.value("choices").toArray().at(0).toObject()
                                 .value("message").toObject()
                                 .value("content").toString();
            aiResultArea->setText(output.trimmed());
        } else {
            aiResultArea->setText("Lỗi định dạng phản hồi từ API:\n" + QString::fromUtf8(data));
        }
    } else {
        QByteArray data = reply->readAll();
        aiResultArea->setText(QString("Lỗi kết nối: %1\nHTTP Code: %2\nNội dung: %3")
                                  .arg(reply->errorString())
                                  .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                                  .arg(QString::fromUtf8(data)));
    }
    reply->deleteLater();
}

// ════════════════════════════════════════════════════════════════════════════
// DRAG & DROP
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        QString localPath = url.toLocalFile();
        if (localPath.endsWith(".pdf", Qt::CaseInsensitive)) {
            openPdfFile(localPath);
            break;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// AI CONFIG (LOGIC UNCHANGED)
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::loadAiConfig() {
    apiUrlInput->setText(appSettings->value("api_url", "http://127.0.0.1:52625/v1/chat/completions").toString());
    apiKeyInput->setText(appSettings->value("api_key", "lm-studio").toString());
    modelNameInput->setText(appSettings->value("api_model", "translategemma:4b").toString());
}

void MainWindow::saveAiConfig() {
    appSettings->setValue("api_url", apiUrlInput->text().trimmed());
    appSettings->setValue("api_key", apiKeyInput->text().trimmed());
    appSettings->setValue("api_model", modelNameInput->text().trimmed());
    QMessageBox::information(this, "Success", "Local AI Configuration saved successfully!");
}

// ════════════════════════════════════════════════════════════════════════════
// RECENT FILES MANAGEMENT
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::addFileToRecent(const QString &filePath) {
    QStringList files = appSettings->value("recent_files").toStringList();
    files.removeAll(filePath);
    files.insert(0, filePath);
    if (files.size() > 10) files = files.mid(0, 10);
    appSettings->setValue("recent_files", files);
    updateRecentFilesGrid();
}

void MainWindow::updateRecentFilesGrid() {
    if (recentFilesHomeGrid) recentFilesHomeGrid->clear();
    if (allRecentFilesList) allRecentFilesList->clear();
    if (favoriteFilesList) favoriteFilesList->clear();

    const QStringList files = appSettings->value("recent_files").toStringList();
    const QStringList favFiles = appSettings->value("favorite_files").toStringList();

    auto createRowWidget = [this](const QString &path, const QFileInfo &info, bool isFavoriteList) -> QWidget* {
        QWidget *rowWidget = new QWidget(this);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(ThemeManager::Sp12, ThemeManager::Sp8, ThemeManager::Sp12, ThemeManager::Sp8);

        QLabel *iconLabel = new QLabel(this);
        iconLabel->setFixedSize(44, 54);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(QIcon(":/assets/pdf-colored.svg").pixmap(QSize(36, 44)));

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setSpacing(2);
        QLabel *titleLabel = new QLabel(info.fileName(), this);
        titleLabel->setStyleSheet(ThemeManager::fileRowTitleStyle());
        QLabel *pathLabel = new QLabel(info.path(), this);
        pathLabel->setStyleSheet(ThemeManager::fileRowPathStyle());
        textLayout->addWidget(titleLabel);
        textLayout->addWidget(pathLabel);

        QHBoxLayout *actionsLayout = new QHBoxLayout();
        actionsLayout->setSpacing(ThemeManager::Sp8);

        QPushButton *openBtn = new QPushButton("Mở", this);
        openBtn->setStyleSheet(ThemeManager::fileRowOpenBtnStyle());
        openBtn->setCursor(Qt::PointingHandCursor);
        connect(openBtn, &QPushButton::clicked, this, [this, path]() { openPdfFile(path); });

        bool isCurrentlyFav = appSettings->value("favorite_files").toStringList().contains(path);
        QPushButton *favBtn = new QPushButton(isCurrentlyFav ? "★ Đã thích" : "☆ Yêu thích", this);
        favBtn->setStyleSheet(ThemeManager::fileRowFavBtnStyle());
        favBtn->setCursor(Qt::PointingHandCursor);
        connect(favBtn, &QPushButton::clicked, this, [this, path, isCurrentlyFav]() {
            QStringList favList = appSettings->value("favorite_files").toStringList();
            if (isCurrentlyFav) {
                favList.removeAll(path);
            } else {
                if (!favList.contains(path)) favList.prepend(path);
            }
            appSettings->setValue("favorite_files", favList);
            updateRecentFilesGrid();
        });

        QPushButton *delBtn = new QPushButton("Xóa", this);
        delBtn->setStyleSheet(ThemeManager::fileRowDeleteBtnStyle());
        delBtn->setCursor(Qt::PointingHandCursor);
        connect(delBtn, &QPushButton::clicked, this, [this, path, isFavoriteList]() {
            if (isFavoriteList) {
                QStringList favList = appSettings->value("favorite_files").toStringList();
                favList.removeAll(path);
                appSettings->setValue("favorite_files", favList);
            } else {
                QStringList recentList = appSettings->value("recent_files").toStringList();
                recentList.removeAll(path);
                appSettings->setValue("recent_files", recentList);
            }
            updateRecentFilesGrid();
        });

        actionsLayout->addWidget(openBtn);
        actionsLayout->addWidget(favBtn);
        actionsLayout->addWidget(delBtn);

        rowLayout->addWidget(iconLabel);
        rowLayout->addLayout(textLayout, 1);
        rowLayout->addLayout(actionsLayout);

        return rowWidget;
    };

    for (const QString &path : files) {
        QFileInfo info(path);
        if (!info.exists()) continue;

        if (recentFilesHomeGrid) {
            QListWidgetItem *homeItem = new QListWidgetItem(recentFilesHomeGrid);
            homeItem->setSizeHint(QSize(0, 76));
            recentFilesHomeGrid->setItemWidget(homeItem, createRowWidget(path, info, false));
        }

        if (allRecentFilesList) {
            QListWidgetItem *recentItem = new QListWidgetItem(allRecentFilesList);
            recentItem->setSizeHint(QSize(0, 76));
            allRecentFilesList->setItemWidget(recentItem, createRowWidget(path, info, false));
        }
    }

    for (const QString &path : favFiles) {
        QFileInfo info(path);
        if (!info.exists()) continue;

        if (favoriteFilesList) {
            QListWidgetItem *favItem = new QListWidgetItem(favoriteFilesList);
            favItem->setSizeHint(QSize(0, 76));
            favoriteFilesList->setItemWidget(favItem, createRowWidget(path, info, true));
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// PDF TABS
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::closePdfTab(int index) {
    if (index < 0 || index >= pdfTabWidget->count()) return;
    QWidget *w = pdfTabWidget->widget(index);
    if (!w || w == plusTabWidget) return;

    int documentTabCount = 0;
    for (int i = 0; i < pdfTabWidget->count(); ++i) {
        if (pdfTabWidget->widget(i) != plusTabWidget) {
            ++documentTabCount;
        }
    }
    const bool closingLastDocument = (documentTabCount <= 1);

    suppressPlusTabOpen = true;
    if (auto *sa = qobject_cast<QScrollArea*>(w)) {
        auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
        const quintptr docId = reinterpret_cast<quintptr>(doc);
        sa->setProperty("popplerDoc", QVariant());
        // ── Free all cached data for this document ──
        if (m_pageCache)      m_pageCache->clearForDocument(docId);
        if (m_thumbnailCache) m_thumbnailCache->clearForDocument(docId);
        if (doc) delete doc;
    }

    pdfTabWidget->removeTab(index);
    delete w;
    suppressPlusTabOpen = false;

    if (closingLastDocument) {
        if (thumbnailTimer) {
            thumbnailTimer->stop();
        }
        thumbnailListWidget->clear();
        bookmarkModel->clear();
        pageInput->clear();
        totalPagesLabel->clear();
        zoomLabel->setText("100%");
        currentZoom = 1.0;

        pdfTabWidget->setMinimumSize(0, 0);
        pdfTabWidget->updateGeometry();
        mainStack->setMinimumSize(0, 0);
        mainStack->updateGeometry();

        navSidebar->setCurrentRow(1);
        mainStack->setCurrentIndex(0);
        navSidebar->show();
        readerToolbar->hide();
        leftDock->hide();
        rightDock->hide();

        if (QScreen *screen = windowHandle() ? windowHandle()->screen() : QApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            if (width() > available.width() || height() > available.height()) {
                resize(qMin(width(), available.width() - 80), qMin(height(), available.height() - 80));
                move(available.center() - rect().center());
            }
        }
        return;
    }

    // Activate a real remaining document tab, never the plus tab.
    for (int i = qMin(index, pdfTabWidget->count() - 1); i >= 0; --i) {
        if (pdfTabWidget->widget(i) != plusTabWidget) {
            pdfTabWidget->setCurrentIndex(i);
            onPdfTabChanged(i);
            return;
        }
    }
    for (int i = 0; i < pdfTabWidget->count(); ++i) {
        if (pdfTabWidget->widget(i) != plusTabWidget) {
            pdfTabWidget->setCurrentIndex(i);
            onPdfTabChanged(i);
            return;
        }
    }
}

void MainWindow::onPdfTabChanged(int index) {
    if (index >= 0) {
        if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->widget(index))) {
            auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
            if (doc) {
                currentZoom = sa->property("zoomFactor").isValid()
                                  ? sa->property("zoomFactor").toDouble()
                                  : 1.0;
                renderSidebarThumbnails(doc, thumbnailListWidget);
                loadBookmarksHierarchy(doc);
                updateToolbarDisplay();
                renderVisiblePages(sa);
            }
        }
    }
}

void MainWindow::updateToolbarDisplay() {
    if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
        auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
        auto *viewport = qobject_cast<PdfViewport*>(sa->widget());
        if (doc && viewport) {
            int current = viewport->currentVisiblePage();
            pageInput->setText(QString::number(current + 1));
            totalPagesLabel->setText(QString("/ %1").arg(doc->numPages()));
            zoomLabel->setText(QString("%1%").arg(qRound(currentZoom * 100)));
        }
    }
}

void MainWindow::jumpToPage() {
    int target = pageInput->text().toInt() - 1;
    if (auto *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget())) {
        auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
        auto *viewport = qobject_cast<PdfViewport*>(sa->widget());
        if (doc && viewport && target >= 0 && target < doc->numPages()) {
            sa->verticalScrollBar()->setValue(viewport->pageYOffset(target));
            updateToolbarDisplay();
            scheduleThumbnailRenderAround(target);
        }
    }
}

void MainWindow::applyZoomToCurrentDocument(double newZoom) {
    QScrollArea *sa = qobject_cast<QScrollArea*>(pdfTabWidget->currentWidget());
    if (!sa) return;

    auto *doc = reinterpret_cast<Poppler::Document*>(sa->property("popplerDoc").toLongLong());
    auto *viewport = qobject_cast<PdfViewport*>(sa->widget());
    if (!doc || !viewport) return;

    int currentPage = viewport->currentVisiblePage();
    currentPage = qBound(0, currentPage, doc->numPages() - 1);

    // Keep the current page as the anchor. Ratio-based restoring can land in a
    // blank gap after page heights change, so page anchoring is more reliable.
    currentZoom = newZoom;
    sa->setProperty("zoomFactor", currentZoom);

    // Immediately evict old-zoom pixmaps to free RAM!
    const quintptr docId = reinterpret_cast<quintptr>(doc);
    if (m_pageCache) {
        int keepZoomPermille = qRound(currentZoom * 1000);
        m_pageCache->clearForDocumentExceptZoom(docId, keepZoomPermille);
    }

    // Apply zoom to viewport which will recalculate geometry
    viewport->setDocument(doc, m_pageCache, currentZoom, devicePixelRatioF());

    // Jump to same page anchor
    sa->verticalScrollBar()->setValue(viewport->pageYOffset(currentPage));

    renderVisiblePages(sa);
    updateToolbarDisplay();
    viewport->update();
    sa->viewport()->update();
}

void MainWindow::zoomIn() {
    double nextZoom = currentZoom;
    for (double z : ZOOM_PRESETS) {
        if (z > currentZoom + 0.01) { nextZoom = z; break; }
    }
    if (qAbs(nextZoom - currentZoom) < 0.01) return;
    applyZoomToCurrentDocument(nextZoom);
}

void MainWindow::zoomOut() {
    double prevZoom = currentZoom;
    for (int i = ZOOM_PRESETS.size() - 1; i >= 0; --i) {
        if (ZOOM_PRESETS[i] < currentZoom - 0.01) { prevZoom = ZOOM_PRESETS[i]; break; }
    }
    if (qAbs(prevZoom - currentZoom) < 0.01) return;
    applyZoomToCurrentDocument(prevZoom);
}

// ════════════════════════════════════════════════════════════════════════════
// COLLAPSE/EXPAND SIDE PANELS
// ════════════════════════════════════════════════════════════════════════════

void MainWindow::toggleLeftPanel() {
    if (leftDock->isVisible()) {
        leftDock->hide();
        if (leftExpandBtn) {
            leftExpandBtn->show();
            leftExpandBtn->move(0, readerToolbar->height() + 40);
        }
    } else {
        leftDock->show();
        if (leftExpandBtn) leftExpandBtn->hide();
    }
}

void MainWindow::toggleRightPanel() {
    if (rightDock->isVisible()) {
        rightDock->hide();
        if (rightExpandBtn) {
            rightExpandBtn->show();
            rightExpandBtn->move(width() - rightExpandBtn->width(), readerToolbar->height() + 40);
        }
    } else {
        rightDock->show();
        if (rightExpandBtn) rightExpandBtn->hide();
    }
}