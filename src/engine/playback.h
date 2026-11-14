#pragma once
#include "core/project.h"
#include <QAudioOutput>
#include <QAudioSink>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>
#include <QVideoSink>
#include <memory>
#include <vector>

namespace loggx {
QImage gradeImage(const QImage& image, double brightness, double contrast, double saturation);
class NoteStream;
class Playback : public QObject {
    Q_OBJECT
public:
    explicit Playback(QObject* parent = nullptr);
    ~Playback() override;
    void setProject(const Project& project);
    void seek(double seconds);
    void play();
    void pause();
    void toggle();
    bool playing() const { return playing_; }
    double position() const { return position_; }
signals:
    void positionChanged(double seconds);
    void playingChanged(bool playing);
    void frameReady(const QImage& frame);
    void error(const QString& message);
private:
    struct Player { QString clip; QMediaPlayer* player; QAudioOutput* audio; };
    void sync(bool seek);
    void startSynth();
    Project project_;
    std::vector<Player> players_;
    QVideoSink videoSink_;
    QTimer timer_;
    QElapsedTimer clock_;
    double position_ = 0, base_ = 0;
    bool playing_ = false;
    QString visibleClip_;
    std::unique_ptr<NoteStream> stream_;
    std::unique_ptr<QAudioSink> synth_;
};
}
