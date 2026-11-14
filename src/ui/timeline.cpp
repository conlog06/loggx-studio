#include "timeline.h"
#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

namespace loggx {
namespace { constexpr int header = 164, ruler = 38, row = 70;
QColor trackColor(TrackKind kind) { return kind == TrackKind::Video ? QColor("#5e9de6") : kind == TrackKind::Audio ? QColor("#52c4a7") : QColor("#b695ed"); }
}
Timeline::Timeline(Document* doc, QWidget* parent) : QWidget(parent), doc_(doc) {
    setMinimumHeight(275); setMouseTracking(true); setFocusPolicy(Qt::StrongFocus);
    connect(doc_,&Document::changed,this,[this] {
        if (!doc_->project().findClip(selected_)) selected_.clear();
        selectedTrack_ = std::clamp(selectedTrack_,0,std::max(0,int(doc_->project().tracks.size())-1));
        resize(sizeHint()); updateGeometry(); update();
    });
}
QSize Timeline::sizeHint() const { return {int(header+std::max(30.0,doc_->project().duration()+8)*zoom_),ruler+row*int(doc_->project().tracks.size())+20}; }
void Timeline::setPosition(double seconds) { position_ = seconds; update(); }
void Timeline::setZoom(double zoom) { zoom_ = zoom; resize(sizeHint()); updateGeometry(); update(); }
void Timeline::setSelected(const QString& id) { selected_ = id; emit selectionChanged(id); update(); }
QRectF Timeline::clipRect(const Clip& c, int lane) const { return {header+c.start*zoom_,ruler+lane*row+9,c.duration*zoom_,row-18}; }
double Timeline::secondsAt(double x) const { return snapTime((x-header)/zoom_,doc_->project().tempo,beatSnap_,doc_->project().fps); }
void Timeline::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(),QColor("#111318"));
    const auto& project = doc_->project();
    painter.fillRect(0,0,width(),ruler,QColor("#20232c"));
    double step = beatSnap_ ? 60.0/project.tempo : 1.0;
    while (step*zoom_ < 26) step *= 2;
    for (double t = 0; header+t*zoom_ < width(); t += step) {
        const int x = int(header+t*zoom_);
        painter.setPen(QColor("#292d37")); painter.drawLine(x,ruler,x,height());
        painter.setPen(QColor("#7f8798"));
        const QString caption = beatSnap_ ? QString::number(int(std::round(t*project.tempo/60))+1) : QString::number(t,'f',0)+"s";
        painter.drawText(x+5,23,caption);
    }
    for (int lane = 0; lane < project.tracks.size(); ++lane) {
        const auto& track = project.tracks[lane]; const int y = ruler+lane*row;
        painter.fillRect(0,y,header,row,QColor(lane == selectedTrack_ ? "#2a2e3a" : "#1b1e26"));
        painter.fillRect(0,y+10,3,row-20,trackColor(track.kind));
        painter.setPen(QColor("#e1e4ed")); painter.drawText(QRect(13,y+7,140,25),Qt::AlignVCenter,track.name);
        painter.setPen(QColor("#8d95a6")); painter.drawText(13,y+51,track.kind == TrackKind::Video ? "VIDEO" : track.kind == TrackKind::Audio ? "AUDIO" : track.instrument.toUpper());
        for (int button = 0; button < 2; ++button) {
            const QRect area(113+button*23,y+36,20,21);
            const bool active = button == 0 ? track.mute : track.solo;
            painter.setBrush(active ? QColor("#ed945f") : QColor("#343a47")); painter.setPen(Qt::NoPen); painter.drawRoundedRect(area,4,4);
            painter.setPen(active ? QColor("#101217") : QColor("#c0c6d5")); painter.drawText(area,Qt::AlignCenter,button == 0 ? "M" : "S");
        }
        painter.setPen(QColor("#2a2e38")); painter.drawLine(0,y+row,width(),y+row);
        for (const auto& clip : track.clips) {
            const auto area = clipRect(clip,lane);
            painter.setClipRect(QRect(header,ruler,width()-header,height()-ruler));
            QColor color = trackColor(track.kind); if (track.mute) color = color.darker(180);
            QLinearGradient gradient(area.topLeft(),area.bottomLeft()); gradient.setColorAt(0,color.darker(140)); gradient.setColorAt(1,color.darker(210));
            painter.setBrush(gradient); painter.setPen(QPen(clip.id == selected_ ? QColor("#ffffff") : color,clip.id == selected_ ? 2 : 1));
            painter.drawRoundedRect(area.adjusted(1,1,-1,-1),6,6);
            painter.setClipRect(area.adjusted(6,2,-6,-2));
            painter.setPen(QColor("#f2f5fa")); painter.drawText(area.adjusted(8,4,-4,-25),Qt::AlignVCenter,clip.name);
            painter.setPen(color.lighter(150));
            if (track.kind == TrackKind::Instrument) {
                for (const auto& note : clip.notes) {
                    const double x = area.x()+(note.beat*60/project.tempo-clip.sourceIn)*zoom_;
                    const double w = note.length*60/project.tempo*zoom_;
                    painter.fillRect(QRectF(x,area.bottom()-8-(note.pitch%12)*1.3,std::max(2.0,w-2),2.5),color.lighter(145));
                }
            } else painter.drawText(area.adjusted(8,24,-4,0),Qt::AlignVCenter,timecode(clip.duration,project.fps));
            painter.setClipping(false);
        }
    }
    if (dragging_) {
        Clip ghost = original_; ghost.start = targetStart_; ghost.duration = targetEnd_-targetStart_;
        painter.setPen(QPen(QColor("#fca970"),2,Qt::DashLine)); painter.setBrush(QColor(250,164,98,35));
        painter.drawRoundedRect(clipRect(ghost,targetTrack_),6,6);
    }
    const double playhead = header+position_*zoom_;
    painter.setPen(QPen(QColor("#ff9d62"),2)); painter.drawLine(QPointF(playhead,0),QPointF(playhead,height()));
    painter.setBrush(QColor("#ff9d62")); painter.drawPolygon(QPolygonF{QPointF(playhead-5,0),QPointF(playhead+5,0),QPointF(playhead,9)});
    painter.fillRect(0,0,header,ruler,QColor("#20232c")); painter.setPen(QColor("#a2aabd")); painter.drawText(13,24,"SPUREN  /  "+QString::number(project.tracks.size()));
}
void Timeline::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    setFocus(); press_ = current_ = e->position(); dragging_ = false; dragMode_ = 0;
    if (e->position().y() < ruler) { emit seek(secondsAt(e->position().x())); return; }
    const int lane = int(e->position().y()-ruler)/row;
    if (lane < 0 || lane >= doc_->project().tracks.size()) return;
    selectedTrack_ = targetTrack_ = lane; emit trackSelected(lane);
    if (e->position().x() < header) {
        if (e->position().y() >= ruler+lane*row+36 && e->position().y() <= ruler+lane*row+57 && e->position().x() >= 113) {
            const bool mute = e->position().x() < 136;
            doc_->change(mute ? "Mute umschalten" : "Solo umschalten",[&](Project& p) { if (mute) p.tracks[lane].mute = !p.tracks[lane].mute; else p.tracks[lane].solo = !p.tracks[lane].solo; });
        }
        update(); return;
    }
    const auto& clips = doc_->project().tracks[lane].clips;
    for (auto it = clips.crbegin(); it != clips.crend(); ++it) {
        const auto r = clipRect(*it,lane);
        if (!r.contains(e->position())) continue;
        original_ = *it; targetStart_ = it->start; targetEnd_ = it->end();
        dragMode_ = e->position().x() < r.left()+7 ? 2 : e->position().x() > r.right()-7 ? 3 : 1;
        setSelected(it->id); return;
    }
    setSelected({}); emit seek(secondsAt(e->position().x()));
}
void Timeline::mouseMoveEvent(QMouseEvent* e) {
    current_ = e->position();
    if (!(e->buttons() & Qt::LeftButton) || !dragMode_) return;
    if (!dragging_ && (current_-press_).manhattanLength() < 4) return;
    dragging_ = true;
    const double delta = (current_.x()-press_.x())/zoom_;
    const auto& p = doc_->project();
    if (dragMode_ == 1) {
        targetStart_ = snapTime(original_.start+delta,p.tempo,beatSnap_,p.fps); targetEnd_ = targetStart_+original_.duration;
        targetTrack_ = std::clamp(int(current_.y()-ruler)/row,0,int(p.tracks.size())-1);
    } else if (dragMode_ == 2) {
        const double min = std::max(0.0,original_.start-original_.sourceIn);
        targetStart_ = std::clamp(snapTime(original_.start+delta,p.tempo,beatSnap_,p.fps),min,original_.end()-1.0/p.fps);
    } else {
        const auto* m = p.findMedia(original_.mediaId);
        const double max = m ? original_.start+m->duration-original_.sourceIn : 86400;
        targetEnd_ = std::clamp(snapTime(original_.end()+delta,p.tempo,beatSnap_,p.fps),original_.start+1.0/p.fps,max);
    }
    update();
}
void Timeline::mouseReleaseEvent(QMouseEvent*) {
    if (dragging_) {
        if (dragMode_ == 1) doc_->move(original_.id,targetTrack_,targetStart_);
        else doc_->trim(original_.id,targetStart_,targetEnd_);
    }
    dragMode_ = 0; dragging_ = false; update();
}
void Timeline::mouseDoubleClickEvent(QMouseEvent* e) {
    const int lane = int(e->position().y()-ruler)/row;
    if (e->position().x() < header || e->position().y() < ruler || lane < 0 || lane >= doc_->project().tracks.size()) return;
    if (doc_->project().tracks[lane].kind != TrackKind::Instrument) return;
    for (const auto& c : doc_->project().tracks[lane].clips) if (clipRect(c,lane).contains(e->position())) { emit editPattern(c.id); return; }
    emit createPattern(lane,secondsAt(e->position().x()));
}
}
