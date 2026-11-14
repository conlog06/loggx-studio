#include "playback.h"
#include "core/audio.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace loggx {
class NoteStream : public QIODevice {
public:
    NoteStream(Project p, double at, int sampleRate) : project(std::move(p)), frame(qRound64(at*sampleRate)), rate(sampleRate) { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 65536+QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 length) override {
        const qint64 count = length / qint64(sizeof(float)*2);
        for (qint64 i = 0; i < count; ++i) {
            const auto value = instrumentSample(project,double(frame++)/rate);
            const float pair[2] = {value.left,value.right};
            std::memcpy(data+i*8,pair,8);
        }
        return count*8;
    }
    qint64 writeData(const char*,qint64) override { return -1; }
private:
    const Project project; qint64 frame; int rate;
};
QImage gradeImage(const QImage& source, double brightness, double contrast, double saturation) {
    if (source.isNull() || (brightness == 0 && contrast == 1 && saturation == 1)) return source;
    QImage image = source.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const double r = qRed(line[x])/255.0, g = qGreen(line[x])/255.0, b = qBlue(line[x])/255.0;
            const double luma = r*0.2126+g*0.7152+b*0.0722;
            auto channel = [&](double c) { return qRound(std::clamp(((luma+(c-luma)*saturation)-0.5)*contrast+0.5+brightness,0.0,1.0)*255); };
            line[x] = qRgb(channel(r),channel(g),channel(b));
        }
    }
    return image;
}
Playback::Playback(QObject* parent) : QObject(parent) {
    timer_.setInterval(16); timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_,&QTimer::timeout,this,[this] {
        position_ = base_+clock_.elapsed()/1000.0;
        if (position_ >= project_.duration()) { pause(); seek(0); return; }
        sync(false); emit positionChanged(position_);
    });
    connect(&videoSink_,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame& frame) {
        const auto* c = project_.findClip(visibleClip_);
        if (!c) return;
        auto image = frame.toImage();
        if (image.width() > 1280) image = image.scaledToWidth(1280,Qt::SmoothTransformation);
        emit frameReady(gradeImage(image,c->brightness,c->contrast,c->saturation));
    });
}
Playback::~Playback() { pause(); }
void Playback::setProject(const Project& p) {
    const bool resume = playing_; pause();
    for (auto& player : players_) { delete player.player; delete player.audio; }
    players_.clear(); visibleClip_.clear(); project_ = p;
    seek(std::min(position_,project_.duration()));
    if (resume) play();
}
void Playback::seek(double seconds) {
    position_ = std::clamp(seconds,0.0,std::max(0.0,project_.duration()));
    base_ = position_; clock_.restart(); sync(true);
    if (playing_) startSynth();
    emit positionChanged(position_);
}
void Playback::play() {
    if (playing_ || project_.duration() <= 0) return;
    if (position_ >= project_.duration()) position_ = 0;
    playing_ = true; base_ = position_; clock_.restart(); sync(true); startSynth(); timer_.start(); emit playingChanged(true);
}
void Playback::pause() {
    timer_.stop();
    for (auto& p : players_) p.player->pause();
    if (synth_) synth_->stop(); synth_.reset(); stream_.reset();
    if (playing_) { playing_ = false; emit playingChanged(false); }
}
void Playback::toggle() { playing_ ? pause() : play(); }
void Playback::startSynth() {
    if (synth_) synth_->stop(); synth_.reset(); stream_.reset();
    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) return;
    QAudioFormat format; format.setSampleRate(48000); format.setChannelCount(2); format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format)) { emit error("Audiogerät unterstützt kein Stereo-Float mit 48 kHz. Instrumente bleiben in der Vorschau stumm."); return; }
    stream_ = std::make_unique<NoteStream>(project_,position_,48000);
    synth_ = std::make_unique<QAudioSink>(device,format);
    synth_->setBufferSize(8192); synth_->start(stream_.get());
}
void Playback::sync(bool seeking) {
    QString visible;
    QVector<const Clip*> active;
    QVector<const Track*> lanes;
    for (const auto& track : project_.tracks) {
        if (track.kind == TrackKind::Instrument) continue;
        const Clip* top = nullptr;
        for (const auto& c : track.clips) if (c.start <= position_ && c.end() > position_) {
            if (track.kind == TrackKind::Video && !track.mute && (!top || c.start >= top->start)) top = &c;
            active << &c; lanes << &track;
        }
        if (visible.isEmpty() && top) visible = top->id;
    }
    if (visible != visibleClip_) {
        visibleClip_ = visible;
        for (auto& p : players_) p.player->setVideoSink(nullptr);
        emit frameReady(QImage());
    }
    for (auto it = players_.begin(); it != players_.end();) {
        const bool used = std::any_of(active.begin(),active.end(),[&](const Clip* c) { return c->id == it->clip; });
        if (!used) { delete it->player; delete it->audio; it = players_.erase(it); }
        else ++it;
    }
    for (int i = 0; i < active.size(); ++i) {
        const auto& c = *active[i]; const auto& t = *lanes[i]; const auto* m = project_.findMedia(c.mediaId);
        auto it = std::find_if(players_.begin(),players_.end(),[&](const Player& p) { return p.clip == c.id; });
        bool created = false;
        if (it == players_.end()) {
            auto* player = new QMediaPlayer(this); auto* audio = new QAudioOutput(this);
            player->setAudioOutput(audio);
            connect(player,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString& message) { emit error(message); });
            players_.push_back({c.id,player,audio}); it = std::prev(players_.end()); created = true;
            if (c.id == visibleClip_) player->setVideoSink(&videoSink_);
            player->setSource(QUrl::fromLocalFile(m->path));
            connect(player,&QMediaPlayer::mediaStatusChanged,this,[this,player,id=c.id](QMediaPlayer::MediaStatus status) {
                if (status != QMediaPlayer::LoadedMedia) return;
                const auto* clip = project_.findClip(id); if (!clip) return;
                player->setPosition(qRound64((clip->sourceIn+position_-clip->start)*1000));
                if (playing_) player->play();
            });
        }
        if (c.id == visibleClip_ && it->player->videoSink() != &videoSink_) it->player->setVideoSink(&videoSink_);
        const double volume = project_.audible(t) ? t.gain*clipEnvelope(c,position_-c.start) : 0;
        it->audio->setVolume(float(std::clamp(volume,0.0,1.0)));
        const auto expected = qRound64((c.sourceIn+position_-c.start)*1000);
        if (seeking || created || std::abs(it->player->position()-expected) > 180) it->player->setPosition(expected);
        if (playing_ && it->player->playbackState() != QMediaPlayer::PlayingState) it->player->play();
    }
}
}
