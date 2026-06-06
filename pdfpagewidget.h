#ifndef PDFPAGEWIDGET_H
#define PDFPAGEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <memory>
#include <vector>
#include <poppler/qt6/poppler-qt6.h>

class PdfPageWidget : public QWidget {
    Q_OBJECT

public:
    explicit PdfPageWidget(QWidget *parent = nullptr);
    ~PdfPageWidget() override;

    void setPagePixmap(const QPixmap &pixmap);
    void setPopplerPage(std::unique_ptr<Poppler::Page> page, double zoom);
    
    QPixmap getPixmap() const { return m_pixmap; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;


protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPixmap m_pixmap;
    std::unique_ptr<Poppler::Page> m_page;
    double m_zoom;
    std::vector<std::unique_ptr<Poppler::TextBox>> m_textBoxes;

    bool m_isSelecting;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    
    // For storing selected text
    QString m_selectedText;

    void updateSelection();
    QRectF pointsToPixels(const QRectF &rect) const;
};

#endif // PDFPAGEWIDGET_H
