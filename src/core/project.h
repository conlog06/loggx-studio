#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace loggx {
QString newId();
enum class TrackKind { Video, Audio, Instrument };
struct Note {
    int pitch = 60;
    double beat = 0.0, length = 1.0, velocity = 0.8;
};
struct Media {
    QString id = newId(), path, name;
    double duration = 0;
    bool video = false, audio = false;
    int width = 0, height = 0;
};
struct Clip {
    QString id = newId(), mediaId, name = QStringLiteral("Clip");
    double start = 0, duration = 4, sourceIn = 0, gain = 1;
    double fadeIn = 0, fadeOut = 0;
    double brightness = 0, contrast = 1, saturation = 1;
    QVector<Note> notes;
    double end() const { return start + duration; }
};
struct Track {
    QString id = newId(), name;
    TrackKind kind = TrackKind::Video;
    QString instrument = QStringLiteral("synth");
    double gain = 1, pan = 0;
    bool mute = false, solo = false;
    QVector<Clip> clips;
};
struct Project {
    QString name = QStringLiteral("Unbenannt");
    double tempo = 120;
    int width = 1920, height = 1080, fps = 30;
    QVector<Media> media;
    QVector<Track> tracks;
    static Project blank();
    double duration() const;
    const Media* findMedia(const QString& id) const;
    const Clip* findClip(const QString& id) const;
    Clip* findClip(const QString& id);
    Track* trackForClip(const QString& id);
    bool audible(const Track& track) const;
    QJsonObject toJson(const QString& directory = {}) const;
    static Project fromJson(const QJsonObject&, const QString& directory = {});
    void validate() const;
};
Project loadProject(const QString& path);
void saveProject(const Project&, const QString& path);
QString timecode(double seconds, int fps = 30);
double snapTime(double seconds, double tempo, bool beats, int fps);
}
