#include "pianoroll.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

namespace loggx {
namespace { constexpr int keys = 64, ruler = 28, row = 17, high = 83, low = 36; constexpr double beatWidth = 55; }
PianoRoll::PianoRoll(Document* doc, QWidget* parent) : QWidget(parent), doc_(doc) {
    connect(doc_,&Document::changed,this,[this] { resize(sizeHint()); updateGeometry(); update(); });
}
void PianoRoll::setClip(const QString& id) { clip_ = id; note_ = -1; resize(sizeHint()); updateGeometry(); update(); }
QSize PianoRoll::sizeHint() const {
    const auto* c = doc_->project().findClip(clip_);
    return {int(keys+std::max(16.0,c ? c->duration*doc_->project().tempo/60 : 16)*beatWidth),ruler+(high-low+1)*row};
}
QRectF PianoRoll::noteRect(const Note& n) const {
    const auto* c = doc_->project().findClip(clip_);
    const double offset = c ? c->sourceIn*doc_->project().tempo/60 : 0;
    return QRectF(keys+(n.beat-offset)*beatWidth,ruler+(high-n.pitch)*row+2,n.length*beatWidth-2,row-4);
}
void PianoRoll::paintEvent(QPaintEvent*) {
    QPainter p(this); p.fillRect(rect(),QColor("#151820"));
    const auto* c = doc_->project().findClip(clip_);
    for (int pitch = low; pitch <= high; ++pitch) {
        const bool black = pitch%12 == 1 || pitch%12 == 3 || pitch%12 == 6 || pitch%12 == 8 || pitch%12 == 10;
        const int y = ruler+(high-pitch)*row;
        p.fillRect(keys,y,width()-keys,row,QColor(black ? "#151820" : "#1c2029"));
        p.fillRect(0,y,keys-1,row-1,QColor(black ? "#282e3b" : "#9da7ba"));
        if (pitch%12 == 0) { p.setPen(QColor("#10131a")); p.drawText(10,y+13,"C"+QString::number(pitch/12-1)); }
    }
    p.fillRect(0,0,width(),ruler,QColor("#222631"));
    for (int beat = 0; keys+beat*beatWidth < width(); ++beat) {
        const int x = int(keys+beat*beatWidth); p.setPen(QColor(beat%4 == 0 ? "#50586a" : "#303644")); p.drawLine(x,ruler,x,height());
        p.setPen(QColor("#aeb6c8")); if (beat%4 == 0) p.drawText(x+5,19,QString::number(beat/4+1));
    }
    if (!c) { p.setPen(QColor("#bdc5d7")); p.drawText(QRect(keys+35,ruler+30,600,70),Qt::AlignLeft|Qt::TextWordWrap,"Instrumentenclip in der Timeline doppelt anklicken oder ein neues Pattern erstellen."); return; }
    p.setClipRect(QRect(keys,ruler,width()-keys,height()-ruler));
    for (int i = 0; i < c->notes.size(); ++i) {
        const Note& note = i == note_ ? target_ : c->notes[i];
        p.setBrush(QColor::fromRgbF(float(0.48+0.22*note.velocity),float(0.38+0.15*note.velocity),0.8f)); p.setPen(QColor("#d3bcff")); p.drawRoundedRect(noteRect(note),3,3);
    }
    const double end = keys+c->duration*doc_->project().tempo/60*beatWidth;
    p.fillRect(QRectF(end,ruler,width()-end,height()-ruler),QColor(0,0,0,100));
}
void PianoRoll::mousePressEvent(QMouseEvent* e) {
    const auto* c = doc_->project().findClip(clip_);
    if (!c || e->position().x() < keys || e->position().y() < ruler) return;
    note_ = -1;
    for (int i = int(c->notes.size())-1; i >= 0; --i) if (noteRect(c->notes[i]).contains(e->position())) {
        if (e->button() == Qt::RightButton) { doc_->change("Note entfernen",[&](Project& p) { p.findClip(clip_)->notes.removeAt(i); }); return; }
        note_ = i; original_ = target_ = c->notes[i]; press_ = e->position();
        resizing_ = e->position().x() > noteRect(original_).right()-7; update(); return;
    }
    if (e->button() != Qt::LeftButton) return;
    const double localBeat = std::floor((e->position().x()-keys)/beatWidth*4)/4;
    if (localBeat >= c->duration*doc_->project().tempo/60) return;
    Note n; n.pitch = std::clamp(high-int(e->position().y()-ruler)/row,low,high);
    n.beat = localBeat+c->sourceIn*doc_->project().tempo/60;
    n.length = std::min(0.5,c->duration*doc_->project().tempo/60-localBeat); n.velocity = velocity_;
    if (n.length < 0.0625) return;
    doc_->change("Note hinzufügen",[&](Project& p) { p.findClip(clip_)->notes.append(n); });
}
void PianoRoll::mouseMoveEvent(QMouseEvent* e) {
    if (note_ < 0 || !(e->buttons() & Qt::LeftButton)) return;
    const double delta = std::round((e->position().x()-press_.x())/beatWidth*4)/4;
    if (resizing_) target_.length = std::max(0.25,original_.length+delta);
    else { target_.beat = std::max(0.0,original_.beat+delta); target_.pitch = std::clamp(original_.pitch-qRound((e->position().y()-press_.y())/row),low,high); }
    update();
}
void PianoRoll::mouseReleaseEvent(QMouseEvent*) {
    if (note_ >= 0) {
        const int index = note_; const Note value = target_; note_ = -1;
        doc_->change("Note bearbeiten",[&](Project& p) { auto* c = p.findClip(clip_); if (c && index < c->notes.size()) c->notes[index] = value; });
    }
}
}
