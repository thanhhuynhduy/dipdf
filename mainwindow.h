#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QTreeView>
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QDockWidget>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardItemModel>
#include <QSettings>
#include <QObject>
#include <QString>
#include <poppler-qt6.h>

class PdfViewport;

class PageCache;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void openPdfFile(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onNavigationChanged(int index);
    void onOpenFileClicked();
    void saveAiConfig();
    void loadAiConfig();
    void handleAiAction(bool isTranslate);
    void onAiReplyFinished(QNetworkReply* reply);
    void closePdfTab(int index);
    void onPdfTabChanged(int index);
    void jumpToPage();
    void zoomIn();
    void zoomOut();
    void onBookmarkClicked(const QModelIndex &index);

    // Slots mới cho Tìm kiếm và Ghi chú
    void performSearch();
    void saveNote();
    void loadNotesForCurrentFile();

    // Collapse panel slots
    void toggleLeftPanel();
    void toggleRightPanel();

private:
    void createMainLayout();
    void createHomeView();
    void createRecentView();
    void createFavoriteView();
    void createSettingsView();
    void applyGlobalStyle();
    void createReaderToolbar();
    void createLeftSidebar();
    void createRightSidebar();

    void updateRecentFilesGrid();
    void addFileToRecent(const QString &filePath);
    void renderSidebarThumbnails(Poppler::Document* doc, QListWidget* listWidget);
    void renderVisibleThumbnailsNow();
    void renderThumbnailRange(int first, int last);
    void scheduleThumbnailRenderAround(int pageIndex);
    void loadBookmarksHierarchy(Poppler::Document* doc);
    void parseOutlineNode(const Poppler::OutlineItem &item, QStandardItem *parentItem);
    void renderCurrentPage(QScrollArea *scrollArea, int pageIndex);
    void renderVisiblePages(QScrollArea *scrollArea);
    void applyZoomToCurrentDocument(double newZoom);
    void updateToolbarDisplay();

    // Helper: get document ID for cache keys from a scroll area.
    static quintptr docIdFromScrollArea(const QScrollArea *sa);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QListWidget *navSidebar;
    QStackedWidget *mainStack;
    QTabWidget *pdfTabWidget;

    QWidget *homeView;
    QWidget *recentView;
    QWidget *favoriteView;
    QWidget *settingsView;
    QListWidget *allRecentFilesList;
    QListWidget *favoriteFilesList;
    QListWidget *recentFilesHomeGrid;

    QLineEdit *apiUrlInput;
    QLineEdit *apiKeyInput;
    QLineEdit *modelNameInput;
    QSettings *appSettings;

    QToolBar *readerToolbar;
    QLineEdit *pageInput;
    QLabel *totalPagesLabel;
    QLabel *zoomLabel;
    double currentZoom = 1.0;

    QDockWidget *leftDock;
    QTabWidget *leftSidebarTabs;
    QWidget *plusTabWidget;
    QListWidget *thumbnailListWidget;
    QTreeView *bookmarkTreeView;
    QStandardItemModel *bookmarkModel;

    QDockWidget *rightDock;
    QTextEdit *aiInputArea;
    QTextEdit *aiResultArea;
    QNetworkAccessManager *networkManager;

    // Các thành phần giao diện mới cho Search và Notes
    QLineEdit *searchInput;
    QListWidget *searchResultList;
    QTextEdit *noteInput;
    QListWidget *notesList;

    // ── UI Modernization: collapse buttons & empty states ──
    QPushButton *leftCollapseBtn;
    QPushButton *rightCollapseBtn;
    QPushButton *leftExpandBtn;   // Floating expand button khi panel collapsed
    QPushButton *rightExpandBtn;

    // Empty state labels (hiển thị khi content rỗng)
    QLabel *bookmarkEmptyLabel;
    QLabel *searchEmptyLabel;
    QLabel *notesEmptyLabel;
    QLabel *aiEmptyLabel;

    QTimer *thumbnailTimer = nullptr;
    bool suppressPlusTabOpen = false;
    QMetaObject::Connection m_thumbScrollConn;  // lazy thumbnail scroll connection

    // ── Memory-optimized page caches ──
    PageCache *m_pageCache      = nullptr;  //  96 MB budget for rendered pages
    PageCache *m_thumbnailCache = nullptr;  //  16 MB budget for sidebar thumbnails
};

#endif // MAINWINDOW_H