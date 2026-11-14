#pragma once
#include "core/document.h"
#include <QWidget>

namespace loggx {
class Timeline : public QWidget {
    Q_OBJECT
public:
    explicit Timeline(Document* doc, QWidget* parent = nullptr);
    void setPosition(double seconds);
    void setZoom(double pixelsPerSecond);
    void setBeatSnap(bool enabled) { beatSnap_ = enabled; update(); }
    void setSelected(const QString& id);
    QString selected() const { return selected_; }
    int selectedTrack() const { return selectedTrack_; }
    QSize sizeHint() const override;
signals:
    void seek(double seconds);
    void selectionChanged(const QString& id);
    void trackSelected(int index);
    void editPattern(const QString& id);
    void createPattern(int track, double seconds);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    QRectF clipRect(const Clip&, int lane) const;
    double secondsAt(double x) const;
    Document* doc_;
    double zoom_ = 48, position_ = 0;
    QString selected_;
    int selectedTrack_ = 0, dragMode_ = 0, targetTrack_ = 0;
    QPointF press_, current_;
    Clip original_;
    double targetStart_ = 0, targetEnd_ = 0;
    bool beatSnap_ = true, dragging_ = false;
};
}
