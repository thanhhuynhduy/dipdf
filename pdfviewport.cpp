#include "pdfviewport.h"
#include "theme_manager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QtMath>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════════════════════

PdfViewport::PdfViewport(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    m_renderTimer.setSingleShot(true);
    connect(&m_renderTimer, &QTimer::timeout, this, &PdfViewport::doRender);

    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

PdfViewport::~PdfViewport() = default;

// ════════════════════════════════════════════════════════════════════════════
// Document Management
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::setDocument(Poppler::Document *doc, PageCache *cache,
                              double zoom, double dpr)
{
    clearDocument();

    m_doc       = doc;
    m_pageCache = cache;
    m_zoom      = zoom;
    m_dpr       = dpr;
    m_docId     = reinterpret_cast<quintptr>(doc);

    if (!doc) return;

    // Query all page sizes — each Poppler::Page is created and immediately
    // released.  Only the lightweight QSizeF is retained per page.
    const int numPages = doc->numPages();
    m_pages.resize(numPages);
    for (int i = 0; i < numPages; ++i) {
        std::unique_ptr<Poppler::Page> page = doc->page(i);
        if (page) {
            m_pages[i].baseSize = page->pageSizeF();
        } else {
            m_pages[i].baseSize = QSizeF(595.0, 842.0); // default A4
        }
    }

    rebuildLayout();
}

void PdfViewport::clearDocument()
{
    m_doc       = nullptr;
    m_pageCache = nullptr;
    m_docId     = 0;
    m_pages.clear();
    m_totalHeight  = 0;
    m_contentWidth = 0;
    m_currentPage  = 0;

    m_selectionPage = -1;
    m_selectionStart = m_selectionEnd = QPoint();
    m_selectedText.clear();
    m_textBoxes.clear();
    m_textBoxesPage = -1;
    m_isSelecting   = false;
    m_isDragging    = false;

    setFixedSize(0, 0);
}

// ════════════════════════════════════════════════════════════════════════════
// Zoom
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::setZoom(double zoom, double dpr)
{
    m_zoom = zoom;
    m_dpr  = dpr;

    // Invalidate text selection (coordinates are zoom-dependent)
    m_selectionPage = -1;
    m_selectionStart = m_selectionEnd = QPoint();
    m_selectedText.clear();
    m_textBoxes.clear();
    m_textBoxesPage = -1;

    rebuildLayout();
    update();
}

// ════════════════════════════════════════════════════════════════════════════
// Layout — compute pixel sizes and y-offsets for all pages at current zoom
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::rebuildLayout()
{
    int pageContentWidth = 0;
    m_totalHeight = PAGE_MARGIN; // top margin

    for (int i = 0; i < static_cast<int>(m_pages.size()); ++i) {
        auto &p = m_pages[i];
        p.pixelWidth  = qMax(1, qCeil(p.baseSize.width()  * m_zoom));
        p.pixelHeight = qMax(1, qCeil(p.baseSize.height() * m_zoom));
        p.yOffset     = m_totalHeight;

        m_totalHeight += p.pixelHeight;
        if (i + 1 < static_cast<int>(m_pages.size()))
            m_totalHeight += PAGE_SPACING;

        pageContentWidth = qMax(pageContentWidth, p.pixelWidth);
    }

    m_totalHeight += PAGE_MARGIN; // bottom margin
    m_contentWidth = pageContentWidth + 2 * PAGE_MARGIN;

    syncContentWidth();
}

void PdfViewport::syncContentWidth()
{
    int vpWidth = 0;
    if (auto *sa = scrollArea()) {
        if (sa->viewport())
            vpWidth = sa->viewport()->width();
    }

    int desired = qMax(m_contentWidth, qMax(1, vpWidth));
    int h       = qMax(1, m_totalHeight);

    if (width() != desired || height() != h) {
        setFixedSize(desired, h);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Page queries
// ════════════════════════════════════════════════════════════════════════════

int PdfViewport::pageYOffset(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= static_cast<int>(m_pages.size()))
        return 0;
    return m_pages[pageIndex].yOffset;
}

QRect PdfViewport::pageRect(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_pages.size()))
        return QRect();
    const auto &p = m_pages[index];
    // Center page horizontally within the content width
    int x = qMax(PAGE_MARGIN, (width() - p.pixelWidth) / 2);
    return QRect(x, p.yOffset, p.pixelWidth, p.pixelHeight);
}

// Binary search: find the page whose yOffset is <= y.
int PdfViewport::pageAtY(int y) const
{
    if (m_pages.empty()) return -1;
    if (y < m_pages[0].yOffset) return 0;

    int lo = 0;
    int hi = static_cast<int>(m_pages.size()) - 1;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if (m_pages[mid].yOffset <= y)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

QPair<int,int> PdfViewport::visiblePageRange(int top, int height) const
{
    if (m_pages.empty()) return {0, -1};

    int first = pageAtY(qMax(0, top));
    int last  = pageAtY(qMin(m_totalHeight - 1, top + height));

    return { qMax(0, first),
             qMin(static_cast<int>(m_pages.size()) - 1, last) };
}

// ════════════════════════════════════════════════════════════════════════════
// Rendering — pre-render visible pages into the cache
// Called by MainWindow from the scroll handler, NOT from paintEvent.
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::renderVisiblePages(int scrollY, int viewportHeight)
{
    if (!m_doc || !m_pageCache || m_pages.empty()) return;

    // Ensure content width stays in sync with viewport
    syncContentWidth();

    m_lastScrollY = scrollY;
    m_lastViewportHeight = viewportHeight;

    // Update current page tracking synchronously for fast toolbar response
    int centerY = scrollY + viewportHeight / 2;
    int newPage = pageAtY(centerY);
    if (newPage >= 0 && newPage != m_currentPage) {
        m_currentPage = newPage;
        emit currentPageChanged(m_currentPage);
    }

    // Schedule debounced rendering (e.g., ~1 frame)
    m_renderTimer.start(16);
}

void PdfViewport::doRender()
{
    if (!m_doc || !m_pageCache || m_pages.empty()) return;

    auto [first, last] = visiblePageRange(m_lastScrollY - RENDER_MARGIN,
                                           m_lastViewportHeight + 2 * RENDER_MARGIN);

    int pagesRendered = 0;
    bool moreNeeded = false;

    for (int i = first; i <= last; ++i) {
        PageCacheKey key = makeCacheKey(m_docId, i, m_zoom, m_dpr);
        QPixmap cached = m_pageCache->get(key);
        if (!cached.isNull()) continue; // already cached

        // Render budget: at most 2 new pages per tick to avoid freezing UI
        if (pagesRendered >= 2) {
            moreNeeded = true;
            break;
        }

        // Render from Poppler
        std::unique_ptr<Poppler::Page> page = m_doc->page(i);
        if (!page) continue;

        double res = std::round(72.0 * m_zoom * m_dpr);

        // Clamp to MAX_RENDER_DIM
        double maxDim = qMax(m_pages[i].baseSize.width(),
                             m_pages[i].baseSize.height());
        if (maxDim > 0.0) {
            double maxRes = static_cast<double>(MAX_RENDER_DIM) * 72.0 / maxDim;
            res = qMin(res, maxRes);
        }

        QImage img = page->renderToImage(res, res);
        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(std::move(img));
            m_pageCache->insert(key, pix);
        }
        pagesRendered++;
    }

    if (pagesRendered > 0) {
        update(); // schedule repaint if anything was rendered
    }
    
    if (moreNeeded) {
        m_renderTimer.start(10); // schedule next batch quickly
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Paint — draw only pages visible in the clip rect
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const QRect clip = event->rect();

    // Canvas background
    painter.fillRect(clip, QColor(ThemeManager::CanvasBg));

    if (m_pages.empty() || !m_pageCache) return;

    auto [first, last] = visiblePageRange(clip.top(), clip.height());

    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    for (int i = first; i <= last; ++i) {
        QRect pr = pageRect(i);
        if (!pr.intersects(clip)) continue;

        // Optional: draw a subtle shadow/border around the page
        painter.setPen(QColor(0, 0, 0, 30));
        painter.drawRect(pr.adjusted(-1, -1, 0, 0));

        PageCacheKey key = makeCacheKey(m_docId, i, m_zoom, m_dpr);
        QPixmap cached = m_pageCache->get(key);

        if (!cached.isNull()) {
            painter.fillRect(pr, Qt::white);
            painter.drawPixmap(pr, cached, cached.rect());
        } else {
            // Placeholder
            painter.fillRect(pr, QColor(0xF3, 0xF4, 0xF6));
            painter.setPen(QColor(0x9C, 0xA3, 0xAF));
            QFont f = painter.font();
            f.setPixelSize(13);
            painter.setFont(f);
            painter.drawText(pr, Qt::AlignCenter, QStringLiteral("Loading…"));
        }
    }

    // Text selection overlay
    drawTextSelection(painter);
}

// ════════════════════════════════════════════════════════════════════════════
// Scroll area accessor
// ════════════════════════════════════════════════════════════════════════════

QScrollArea *PdfViewport::scrollArea() const
{
    // Inside QScrollArea, the widget is a child of the viewport widget.
    QWidget *vp = parentWidget();
    if (vp)
        return qobject_cast<QScrollArea *>(vp->parentWidget());
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════════════
// Hit testing
// ════════════════════════════════════════════════════════════════════════════

int PdfViewport::hitTestPage(QPoint pos) const
{
    int page = pageAtY(pos.y());
    if (page < 0 || page >= static_cast<int>(m_pages.size())) return -1;
    QRect pr = pageRect(page);
    if (pr.contains(pos)) return page;
    return -1;
}

QPointF PdfViewport::pageLocalPoint(QPoint viewportPos, int pageIndex) const
{
    QRect pr = pageRect(pageIndex);
    return QPointF(viewportPos.x() - pr.x(), viewportPos.y() - pr.y());
}

// ════════════════════════════════════════════════════════════════════════════
// Text selection — lazy text-box loading
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::ensureTextBoxes(int pageIndex)
{
    if (m_textBoxesPage == pageIndex && !m_textBoxes.empty())
        return;
    m_textBoxes.clear();
    m_textBoxesPage = pageIndex;

    if (m_doc && pageIndex >= 0 && pageIndex < m_doc->numPages()) {
        std::unique_ptr<Poppler::Page> page = m_doc->page(pageIndex);
        if (page)
            m_textBoxes = page->textList();
    }
}

QRectF PdfViewport::pointsToPixels(const QRectF &rect) const
{
    return QRectF(rect.x() * m_zoom, rect.y() * m_zoom,
                  rect.width() * m_zoom, rect.height() * m_zoom);
}

void PdfViewport::drawTextSelection(QPainter &painter)
{
    if (m_selectionPage < 0 || m_textBoxes.empty()) return;
    if (m_selectionStart == m_selectionEnd) {
        m_selectedText.clear();
        return;
    }

    QRect pr = pageRect(m_selectionPage);
    QPoint localStart = m_selectionStart - pr.topLeft();
    QPoint localEnd   = m_selectionEnd   - pr.topLeft();

    auto findClosestBox = [&](QPoint pt) -> int {
        int bestIdx = -1;
        double minDist = 1e9;
        for (size_t i = 0; i < m_textBoxes.size(); ++i) {
            QRectF pxRect = pointsToPixels(m_textBoxes[i]->boundingBox());
            if (pxRect.contains(pt)) return static_cast<int>(i);

            double dx = pxRect.center().x() - pt.x();
            double dy = pxRect.center().y() - pt.y();
            double d  = dx * dx + dy * dy * 5.0;
            if (d < minDist) {
                minDist = d;
                bestIdx = static_cast<int>(i);
            }
        }
        if (minDist > 10000.0 * m_zoom) return -1;
        return bestIdx;
    };

    int startIndex = findClosestBox(localStart);
    int endIndex   = findClosestBox(localEnd);

    if (startIndex == -1 || endIndex == -1) {
        m_selectedText.clear();
        return;
    }
    if (startIndex > endIndex) std::swap(startIndex, endIndex);

    painter.setBrush(QColor(0, 120, 215, 80));
    painter.setPen(Qt::NoPen);
    m_selectedText.clear();

    for (int i = startIndex; i <= endIndex; ++i) {
        const auto &box = m_textBoxes[i];
        QRectF pxRect = pointsToPixels(box->boundingBox());
        // Translate from page-local to viewport coordinates
        pxRect.translate(pr.topLeft());
        painter.drawRect(pxRect);

        m_selectedText += box->text();
        if (box->hasSpaceAfter() && i != endIndex) {
            m_selectedText += QLatin1Char(' ');
        } else if (i < endIndex) {
            QRectF nextRect = pointsToPixels(m_textBoxes[i + 1]->boundingBox());
            if (qAbs(nextRect.y() - (pxRect.y() - pr.y())) > pxRect.height() * 0.5)
                m_selectedText += QLatin1Char('\n');
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Mouse events — text selection + drag-to-scroll
// ════════════════════════════════════════════════════════════════════════════

void PdfViewport::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int page = hitTestPage(event->pos());
        if (page >= 0) {
            ensureTextBoxes(page);
            QPointF localPos = pageLocalPoint(event->pos(), page);

            // Check if click is near any text box
            bool nearText = false;
            for (const auto &box : m_textBoxes) {
                QRectF pxRect = pointsToPixels(box->boundingBox());
                if (pxRect.adjusted(-5, -5, 5, 5).contains(localPos)) {
                    nearText = true;
                    break;
                }
            }

            if (nearText) {
                m_isSelecting    = true;
                m_selectionPage  = page;
                m_selectionStart = event->pos();
                m_selectionEnd   = event->pos();
                setCursor(Qt::IBeamCursor);
                update();
                event->accept();
                return;
            }
        }

        // Not near text → drag-to-scroll
        m_isDragging  = true;
        m_lastDragPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    event->ignore();
}

void PdfViewport::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting) {
        m_selectionEnd = event->pos();
        update();
        event->accept();
    } else if (m_isDragging) {
        QPoint currentPos = event->globalPosition().toPoint();
        QPoint delta = currentPos - m_lastDragPos;
        m_lastDragPos = currentPos;

        if (auto *sa = scrollArea()) {
            QScrollBar *hBar = sa->horizontalScrollBar();
            QScrollBar *vBar = sa->verticalScrollBar();
            hBar->setValue(hBar->value() - delta.x());
            vBar->setValue(vBar->value() - delta.y());
        }
        event->accept();
    } else {
        event->ignore();
    }
}

void PdfViewport::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isSelecting) {
            m_isSelecting  = false;
            m_selectionEnd = event->pos();
            setCursor(Qt::ArrowCursor);
            update();
            event->accept();
        } else if (m_isDragging) {
            m_isDragging = false;
            setCursor(Qt::ArrowCursor);
            event->accept();
        }
    } else {
        event->ignore();
    }
}

void PdfViewport::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy)) {
        if (!m_selectedText.isEmpty()) {
            QApplication::clipboard()->setText(m_selectedText.trimmed());
        }
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}
