#ifndef PDFVIEWPORT_H
#define PDFVIEWPORT_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include <QScrollBar>
#include <memory>
#include <vector>
#include <poppler/qt6/poppler-qt6.h>
#include "pagecache.h"

class PageCache;

// ════════════════════════════════════════════════════════════════════════════
// PdfViewport — A single virtualized widget that replaces N per-page widgets.
//
// Instead of creating one QWidget per PDF page (which costs ~2–4 KB each in
// Qt infrastructure, and forces a container widget as tall as the entire
// document), PdfViewport stores only lightweight metadata (32 bytes/page)
// and paints visible pages directly from the LRU PageCache.
//
// Features:
// • O(log N) visible-page detection via binary search on precomputed offsets
// • Text selection (lazy text-box loading from Poppler on first click)
// • Drag-to-scroll (when not clicking on text)
// • Max render dimension clamping (8192×8192) for safety
// ════════════════════════════════════════════════════════════════════════════

class PdfViewport : public QWidget {
    Q_OBJECT

public:
    explicit PdfViewport(QWidget *parent = nullptr);
    ~PdfViewport() override;

    // ── Document management ──
    void setDocument(Poppler::Document *doc, PageCache *cache,
                     double zoom, double dpr);
    void clearDocument();

    // ── Zoom ──
    void setZoom(double zoom, double dpr);
    double zoom() const { return m_zoom; }

    // ── Pre-render visible pages into cache (call from scroll handler) ──
    void renderVisiblePages(int scrollY, int viewportHeight);

    // ── Page queries ──
    int  pageCount()        const { return static_cast<int>(m_pages.size()); }
    int  currentVisiblePage() const { return m_currentPage; }
    int  pageYOffset(int pageIndex) const;
    int  totalHeight()      const { return m_totalHeight; }
    int  contentWidth()     const { return m_contentWidth; }

    // ── Text selection ──
    QString selectedText()  const { return m_selectedText; }

    // ── Document reference ──
    Poppler::Document *document() const { return m_doc; }
    quintptr docId()        const { return m_docId; }

signals:
    void currentPageChanged(int page);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // ── Lightweight per-page metadata (32 bytes each) ──
    struct PageInfo {
        QSizeF baseSize;   // Page size in Poppler points (72 DPI)
        int    yOffset;    // Y position in pixels at current zoom
        int    pixelWidth; // Width in pixels at current zoom
        int    pixelHeight;// Height in pixels at current zoom
    };

    // Layout
    void rebuildLayout();
    void syncContentWidth();
    QPair<int,int> visiblePageRange(int top, int height) const;
    QRect pageRect(int index) const;
    int   pageAtY(int y) const;

    // Hit testing
    int     hitTestPage(QPoint pos) const;
    QPointF pageLocalPoint(QPoint viewportPos, int pageIndex) const;

    // Text selection
    void   ensureTextBoxes(int pageIndex);
    QRectF pointsToPixels(const QRectF &rect) const;
    void   drawTextSelection(QPainter &painter);

    // Scroll area accessor
    QScrollArea *scrollArea() const;

    // ── State ──
    Poppler::Document *m_doc       = nullptr;
    PageCache         *m_pageCache = nullptr;
    double  m_zoom    = 1.0;
    double  m_dpr     = 1.0;
    quintptr m_docId  = 0;

    std::vector<PageInfo> m_pages;
    int m_totalHeight  = 0;
    int m_contentWidth = 0;
    int m_currentPage  = 0;

    // Spacing / safety constants
    static constexpr int PAGE_SPACING    = 20;
    static constexpr int PAGE_MARGIN     = 24;
    static constexpr int MAX_RENDER_DIM  = 8192;
    static constexpr int RENDER_MARGIN   = 300;

    // Text selection state
    bool   m_isSelecting    = false;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    int    m_selectionPage  = -1;
    QString m_selectedText;
    std::vector<std::unique_ptr<Poppler::TextBox>> m_textBoxes;
    int    m_textBoxesPage  = -1;

    // Drag-to-scroll state
    bool   m_isDragging  = false;
    QPoint m_lastDragPos;
};

#endif // PDFVIEWPORT_H
