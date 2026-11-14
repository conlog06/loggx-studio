#pragma once
#include "core/document.h"
#include <QWidget>

namespace loggx {
class PianoRoll : public QWidget {
    Q_OBJECT
public:
    explicit PianoRoll(Document*, QWidget* parent = nullptr);
    void setClip(const QString& id);
    void setVelocity(double velocity) { velocity_ = velocity; }
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    QRectF noteRect(const Note&) const;
    Document* doc_; QString clip_;
    int note_ = -1;
    bool resizing_ = false;
    QPointF press_;
    Note original_, target_;
    double velocity_ = 0.8;
};
}
