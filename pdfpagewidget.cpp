#include "pdfpagewidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QStyleOption>
#include <QSizePolicy>

PdfPageWidget::PdfPageWidget(QWidget *parent)
    : QWidget(parent), m_zoom(1.0), m_isSelecting(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

PdfPageWidget::~PdfPageWidget() = default;

void PdfPageWidget::setPagePixmap(const QPixmap &pixmap) {
    m_pixmap = pixmap;
    updateGeometry();
    update();
}

QSize PdfPageWidget::sizeHint() const {
    if (width() > 0 && height() > 0) return size();
    if (!m_pixmap.isNull()) return m_pixmap.size();
    return QWidget::sizeHint();
}

QSize PdfPageWidget::minimumSizeHint() const {
    return sizeHint();
}

void PdfPageWidget::setPopplerPage(std::unique_ptr<Poppler::Page> page, double zoom) {
    m_page = std::move(page);
    m_zoom = zoom;
    if (m_page) {
        m_textBoxes = m_page->textList();
    } else {
        m_textBoxes.clear();
    }
}

QRectF PdfPageWidget::pointsToPixels(const QRectF &rect) const {
    return QRectF(rect.x() * m_zoom, rect.y() * m_zoom, rect.width() * m_zoom, rect.height() * m_zoom);
}

void PdfPageWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setClipRect(rect());
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), Qt::white);
    if (!m_pixmap.isNull()) {
        painter.drawPixmap(rect(), m_pixmap, m_pixmap.rect());
    }

    if (!m_textBoxes.empty()) {
        if (m_selectionStart != m_selectionEnd) {
            auto findClosestBox = [&](QPoint pt) -> int {
                int bestIdx = -1;
                double minDist = 1e9;
                for (size_t i = 0; i < m_textBoxes.size(); ++i) {
                    QRectF pxRect = pointsToPixels(m_textBoxes[i]->boundingBox());
                    if (pxRect.contains(pt)) return i;
                    
                    double dx = pxRect.center().x() - pt.x();
                    double dy = pxRect.center().y() - pt.y();
                    double d = dx*dx + dy*dy * 5.0; // Penalize vertical distance to stick to lines
                    if (d < minDist) {
                        minDist = d;
                        bestIdx = i;
                    }
                }
                if (minDist > 10000.0 * m_zoom) return -1; // roughly 100 pixels squared
                return bestIdx;
            };

            int startIndex = findClosestBox(m_selectionStart);
            int endIndex = findClosestBox(m_selectionEnd);

            if (startIndex != -1 && endIndex != -1) {
                if (startIndex > endIndex) std::swap(startIndex, endIndex);

                painter.setBrush(QColor(0, 120, 215, 80)); // Semi-transparent blue
                painter.setPen(Qt::NoPen);
                m_selectedText.clear();

                for (int i = startIndex; i <= endIndex; ++i) {
                    const auto &box = m_textBoxes[i];
                    QRectF pxRect = pointsToPixels(box->boundingBox());
                    painter.drawRect(pxRect);
                    m_selectedText += box->text();
                    if (box->hasSpaceAfter() && i != endIndex) {
                        m_selectedText += " ";
                    } else if (i < endIndex) {
                        // If there's no space, check if the next box is on a new line
                        QRectF nextRect = pointsToPixels(m_textBoxes[i+1]->boundingBox());
                        if (qAbs(nextRect.y() - pxRect.y()) > pxRect.height() * 0.5) {
                            m_selectedText += "\n";
                        }
                    }
                }
            } else {
                m_selectedText.clear();
            }
        } else {
            m_selectedText.clear();
        }
    }
}

void PdfPageWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Check if click is near any text box
        bool clickedOnText = false;
        QPoint clickPos = event->pos();
        for (const auto &box : m_textBoxes) {
            QRectF pxRect = pointsToPixels(box->boundingBox());
            // Expand hit area slightly
            if (pxRect.adjusted(-5, -5, 5, 5).contains(clickPos)) {
                clickedOnText = true;
                break;
            }
        }

        if (clickedOnText) {
            m_isSelecting = true;
            m_selectionStart = clickPos;
            m_selectionEnd = clickPos;
            update();
            event->accept();
            return;
        }
    }
    event->ignore(); // Allow bubbling to parent for drag-scroll
}

void PdfPageWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_isSelecting) {
        m_selectionEnd = event->pos();
        update();
        event->accept();
    } else {
        event->ignore();
    }
}

void PdfPageWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        m_selectionEnd = event->pos();
        update();
        event->accept();
    } else {
        event->ignore();
    }
}

void PdfPageWidget::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Copy)) {
        if (!m_selectedText.isEmpty()) {
            QApplication::clipboard()->setText(m_selectedText.trimmed());
        }
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}
