#include "project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace loggx {
namespace {
void require(bool ok, const QString& message) {
    if (!ok) throw std::runtime_error(message.toStdString());
}
bool range(double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; }
double number(const QJsonObject& o, const char* k, double fallback) {
    const auto v = o.value(QLatin1String(k));
    require(v.isUndefined() || v.isDouble(), QStringLiteral("Ungültige Zahl: ") + QLatin1String(k));
    return v.toDouble(fallback);
}
QString kindName(TrackKind k) {
    return k == TrackKind::Video ? "video" : k == TrackKind::Audio ? "audio" : "instrument";
}
QJsonArray array(const QJsonObject& o, const char* key) {
    require(o.value(QLatin1String(key)).isArray(), QStringLiteral("Liste fehlt: ") + QLatin1String(key));
    return o.value(QLatin1String(key)).toArray();
}
}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
Project Project::blank() {
    Project p;
    Track video; video.name = "Video 1";
    Track audio; audio.name = "Audio 1"; audio.kind = TrackKind::Audio;
    Track music; music.name = "Synth 1"; music.kind = TrackKind::Instrument;
    p.tracks = {video, audio, music};
    return p;
}
double Project::duration() const {
    double d = 0;
    for (const auto& t : tracks) for (const auto& c : t.clips) d = std::max(d, c.end());
    return d;
}
const Media* Project::findMedia(const QString& id) const {
    for (const auto& m : media) if (m.id == id) return &m;
    return nullptr;
}
const Clip* Project::findClip(const QString& id) const {
    for (const auto& t : tracks) for (const auto& c : t.clips) if (c.id == id) return &c;
    return nullptr;
}
Clip* Project::findClip(const QString& id) {
    for (auto& t : tracks) for (auto& c : t.clips) if (c.id == id) return &c;
    return nullptr;
}
Track* Project::trackForClip(const QString& id) {
    for (auto& t : tracks) for (auto& c : t.clips) if (c.id == id) return &t;
    return nullptr;
}
bool Project::audible(const Track& track) const {
    const bool solo = std::any_of(tracks.begin(), tracks.end(), [](const Track& t) { return t.solo; });
    return !track.mute && (!solo || track.solo);
}
QJsonObject Project::toJson(const QString& directory) const {
    QJsonArray assets, lanes;
    for (const auto& m : media) {
        const QString path = directory.isEmpty() ? m.path : QDir(directory).relativeFilePath(m.path);
        assets.append(QJsonObject{{"id",m.id},{"path",path},{"name",m.name},{"duration",m.duration},
            {"video",m.video},{"audio",m.audio},{"width",m.width},{"height",m.height}});
    }
    for (const auto& t : tracks) {
        QJsonArray clips;
        for (const auto& c : t.clips) {
            QJsonArray notes;
            for (const auto& n : c.notes) notes.append(QJsonObject{{"pitch",n.pitch},{"beat",n.beat},
                {"length",n.length},{"velocity",n.velocity}});
            clips.append(QJsonObject{{"id",c.id},{"mediaId",c.mediaId},{"name",c.name},
                {"start",c.start},{"duration",c.duration},{"sourceIn",c.sourceIn},{"gain",c.gain},
                {"fadeIn",c.fadeIn},{"fadeOut",c.fadeOut},{"brightness",c.brightness},
                {"contrast",c.contrast},{"saturation",c.saturation},{"notes",notes}});
        }
        lanes.append(QJsonObject{{"id",t.id},{"name",t.name},{"kind",kindName(t.kind)},
            {"instrument",t.instrument},{"gain",t.gain},{"pan",t.pan},{"mute",t.mute},
            {"solo",t.solo},{"clips",clips}});
    }
    return {{"format","loggx"},{"version",1},{"name",name},{"tempo",tempo},
        {"width",width},{"height",height},{"fps",fps},{"media",assets},{"tracks",lanes}};
}
Project Project::fromJson(const QJsonObject& o, const QString& directory) {
    require(o.value("format") == "loggx" && o.value("version").toInt() == 1,
        "Unbekanntes Projektformat oder nicht unterstützte Version.");
    Project p;
    p.name = o.value("name").toString("Unbenannt");
    p.tempo = number(o,"tempo",120);
    p.width = o.value("width").toInt(1920); p.height = o.value("height").toInt(1080);
    p.fps = o.value("fps").toInt(30);
    for (const auto& v : array(o,"media")) {
        const auto a = v.toObject(); Media m;
        m.id = a.value("id").toString(); m.name = a.value("name").toString();
        const auto path = a.value("path").toString();
        require(!path.isEmpty(), "Leerer Medienpfad.");
        m.path = QFileInfo(path).isAbsolute() ? QDir::cleanPath(path) : QDir(directory).absoluteFilePath(path);
        m.duration = number(a,"duration",0); m.video = a.value("video").toBool();
        m.audio = a.value("audio").toBool(); m.width = a.value("width").toInt(); m.height = a.value("height").toInt();
        p.media.append(m);
    }
    for (const auto& v : array(o,"tracks")) {
        const auto a = v.toObject(); Track t;
        t.id = a.value("id").toString(); t.name = a.value("name").toString();
        const auto kind = a.value("kind").toString();
        require(kind == "video" || kind == "audio" || kind == "instrument", "Unbekannter Spurtyp.");
        t.kind = kind == "video" ? TrackKind::Video : kind == "audio" ? TrackKind::Audio : TrackKind::Instrument;
        t.instrument = a.value("instrument").toString("synth");
        t.gain = number(a,"gain",1); t.pan = number(a,"pan",0);
        t.mute = a.value("mute").toBool(); t.solo = a.value("solo").toBool();
        for (const auto& cv : array(a,"clips")) {
            const auto b = cv.toObject(); Clip c;
            c.id = b.value("id").toString(); c.mediaId = b.value("mediaId").toString(); c.name = b.value("name").toString();
            c.start = number(b,"start",0); c.duration = number(b,"duration",4);
            c.sourceIn = number(b,"sourceIn",0); c.gain = number(b,"gain",1);
            c.fadeIn = number(b,"fadeIn",0); c.fadeOut = number(b,"fadeOut",0);
            c.brightness = number(b,"brightness",0); c.contrast = number(b,"contrast",1); c.saturation = number(b,"saturation",1);
            for (const auto& nv : array(b,"notes")) {
                const auto n = nv.toObject();
                c.notes.append({n.value("pitch").toInt(-1),number(n,"beat",0),number(n,"length",1),number(n,"velocity",0.8)});
            }
            t.clips.append(c);
        }
        p.tracks.append(t);
    }
    p.validate(); return p;
}
void Project::validate() const {
    require(range(tempo,20,300), "Tempo muss zwischen 20 und 300 BPM liegen.");
    require(width >= 16 && width <= 7680 && height >= 16 && height <= 4320 && !(width%2) && !(height%2), "Ungültige Bildgröße.");
    require(fps >= 1 && fps <= 120, "Ungültige Bildrate.");
    QSet<QString> ids;
    auto id = [&](const QString& s) { require(!s.isEmpty() && !ids.contains(s), "Fehlende oder doppelte ID."); ids.insert(s); };
    for (const auto& m : media) { id(m.id); require(range(m.duration,0.001,86400) && (m.video || m.audio), "Ungültige Mediendauer oder Medienart."); }
    for (const auto& t : tracks) {
        id(t.id); require(range(t.gain,0,4) && range(t.pan,-1,1), "Ungültiger Mixerwert.");
        require(t.instrument == "synth" || t.instrument == "bass" || t.instrument == "drums", "Unbekanntes Instrument.");
        for (const auto& c : t.clips) {
            id(c.id);
            require(range(c.start,0,86400) && range(c.duration,0.001,86400) && c.end() <= 86400 && range(c.sourceIn,0,86400), "Ungültiger Clip-Zeitraum.");
            require(range(c.gain,0,4) && range(c.fadeIn,0,c.duration) && range(c.fadeOut,0,c.duration), "Ungültiger Clip-Pegel oder Fade.");
            require(range(c.brightness,-1,1) && range(c.contrast,0,2) && range(c.saturation,0,2), "Ungültige Farbkorrektur.");
            if (t.kind != TrackKind::Instrument) {
                const auto* m = findMedia(c.mediaId);
                require(m != nullptr, "Clip verweist auf unbekanntes Medium.");
                require(c.sourceIn + c.duration <= m->duration + 0.002, "Clip überschreitet das Quellmedium.");
                require(t.kind != TrackKind::Video || m->video, "Audiodatei auf Videospur.");
                require(t.kind != TrackKind::Audio || m->audio, "Medium ohne Ton auf Audiospur.");
                require(c.notes.isEmpty(), "Noten sind nur auf Instrumentenspuren erlaubt.");
            } else require(c.mediaId.isEmpty(), "Instrumentenclip darf kein Medium referenzieren.");
            for (const auto& n : c.notes) require(n.pitch >= 0 && n.pitch <= 127 && range(n.beat,0,500000) && range(n.length,0.0625,500000) && range(n.velocity,0,1), "Ungültige MIDI-Note.");
        }
    }
}
Project loadProject(const QString& path) {
    QFile f(path); require(f.open(QIODevice::ReadOnly), f.errorString());
    require(f.size() <= 32*1024*1024, "Projektdatei ist größer als 32 MB.");
    QJsonParseError e; const auto j = QJsonDocument::fromJson(f.readAll(),&e);
    require(e.error == QJsonParseError::NoError && j.isObject(), "Beschädigte JSON-Projektdatei: " + e.errorString());
    return Project::fromJson(j.object(),QFileInfo(path).absolutePath());
}
void saveProject(const Project& p, const QString& path) {
    p.validate(); QSaveFile f(path); require(f.open(QIODevice::WriteOnly),f.errorString());
    const auto bytes = QJsonDocument(p.toJson(QFileInfo(path).absolutePath())).toJson();
    require(f.write(bytes) == bytes.size(),f.errorString()); require(f.commit(),f.errorString());
}
QString timecode(double seconds, int fps) {
    qint64 frames = qRound64(std::max(0.0,seconds) * fps);
    return QString("%1:%2:%3:%4").arg(frames / (fps*3600),2,10,QChar('0'))
        .arg((frames/(fps*60))%60,2,10,QChar('0')).arg((frames/fps)%60,2,10,QChar('0')).arg(frames%fps,2,10,QChar('0'));
}
double snapTime(double seconds, double tempo, bool beats, int fps) {
    const double step = beats ? 60.0 / tempo : 1.0 / fps;
    return std::max(0.0,std::round(seconds/step)*step);
}
}
