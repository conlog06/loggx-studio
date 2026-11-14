#include "exportplan.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace loggx {
namespace {
QString f(double v) { return QString::number(v,'f',6); }
QString label(const QString& s) { return "["+s+"]"; }
}
bool hasInstruments(const Project& p) {
    for (const auto& t : p.tracks) if (t.kind == TrackKind::Instrument && p.audible(t))
        for (const auto& c : t.clips) if (!c.notes.isEmpty()) return true;
    return false;
}
ExportPlan makeExportPlan(const Project& p, const ExportSettings& s, const QString& destination, const QString& instrumentWav) {
    p.validate();
    if (p.duration() <= 0) throw std::runtime_error("Die Timeline ist leer.");
    if (s.width < 16 || s.height < 16 || s.width > 1920 || s.height > 1920 || s.width*s.height > 1920*1080
        || s.width%2 || s.height%2 || s.fps < 1 || s.fps > 60) throw std::runtime_error("Der Prototyp exportiert maximal 1080p bei 60 fps.");
    const QStringList encoders = {"libx264","h264_nvenc","h264_qsv","h264_amf","h264_videotoolbox"};
    if (!s.audioOnly && !encoders.contains(s.encoder)) throw std::runtime_error("Nicht unterstützter Encoder.");
    if (hasInstruments(p) && instrumentWav.isEmpty()) throw std::runtime_error("Gerenderte Instrumentenspur fehlt.");
    ExportPlan plan; plan.arguments = {"-hide_banner","-nostdin","-n","-progress","pipe:1","-nostats"};
    QStringList graph, audios;
    QString video = "base";
    if (!s.audioOnly) graph << QString("color=c=black:s=%1x%2:r=%3:d=%4[base]").arg(s.width).arg(s.height).arg(s.fps).arg(f(p.duration()));
    int input = 0, serial = 0;
    // Bottom lanes are composited first. Within a lane the latest starting clip wins.
    for (int lane = int(p.tracks.size())-1; lane >= 0; --lane) {
        const auto& track = p.tracks[lane];
        if (track.kind == TrackKind::Instrument) continue;
        auto clips = track.clips;
        std::stable_sort(clips.begin(),clips.end(),[](const Clip& a, const Clip& b) { return a.start < b.start; });
        for (const auto& c : clips) {
            const auto* m = p.findMedia(c.mediaId);
            const bool visual = !s.audioOnly && track.kind == TrackKind::Video && !track.mute;
            const bool sound = p.audible(track) && m->audio;
            if (!visual && !sound) continue;
            plan.arguments << "-i" << m->path;
            const QString stem = QString::number(serial++);
            if (visual) {
                const QString v = "v"+stem, combined = "layer"+stem;
                graph << QString("[%1:v:0]trim=start=%2:duration=%3,setpts=PTS-STARTPTS+%4/TB,scale=%5:%6:force_original_aspect_ratio=decrease,pad=%5:%6:(ow-iw)/2:(oh-ih)/2,setsar=1,eq=brightness=%7:contrast=%8:saturation=%9[%10]")
                    .arg(input).arg(f(c.sourceIn)).arg(f(c.duration)).arg(f(c.start)).arg(s.width).arg(s.height)
                    .arg(f(c.brightness)).arg(f(c.contrast)).arg(f(c.saturation)).arg(v);
                graph << label(video)+label(v)+"overlay=eof_action=pass:repeatlast=0:enable='gte(t,"+f(c.start)+")*lt(t,"+f(c.end())+")'"+label(combined);
                video = combined;
            }
            if (sound) {
                const QString a = "a"+stem;
                QString filter = QString("[%1:a:0]atrim=start=%2:duration=%3,asetpts=PTS-STARTPTS,aformat=sample_rates=48000:channel_layouts=stereo,volume=%4,pan=stereo|c0=%5*c0|c1=%6*c1")
                    .arg(input).arg(f(c.sourceIn)).arg(f(c.duration)).arg(f(c.gain*track.gain))
                    .arg(f(std::sqrt(1-track.pan))).arg(f(std::sqrt(1+track.pan)));
                if (c.fadeIn > 0) filter += ",afade=t=in:st=0:d="+f(c.fadeIn);
                if (c.fadeOut > 0) filter += ",afade=t=out:st="+f(c.duration-c.fadeOut)+":d="+f(c.fadeOut);
                filter += ",adelay="+QString::number(qRound64(c.start*48000))+"S:all=1"+label(a);
                graph << filter; audios << a;
            }
            ++input;
        }
    }
    if (!instrumentWav.isEmpty()) {
        plan.arguments << "-i" << instrumentWav;
        graph << QString("[%1:a:0]anull[instruments]").arg(input++); audios << "instruments";
    }
    if (audios.isEmpty()) graph << "anullsrc=r=48000:cl=stereo,atrim=duration="+f(p.duration())+"[mix]";
    else {
        QString sources; for (const auto& a : audios) sources += label(a);
        graph << sources+"amix=inputs="+QString::number(audios.size())+":normalize=0:dropout_transition=0,alimiter=limit=0.95:level=false:latency=true,apad,atrim=duration="+f(p.duration())+"[mix]";
    }
    plan.graph = graph.join(';');
    plan.arguments << "-filter_complex" << plan.graph;
    if (!s.audioOnly) {
        plan.arguments << "-map" << label(video) << "-c:v" << s.encoder << "-pix_fmt" << "yuv420p" << "-r" << QString::number(s.fps);
        if (s.encoder == "libx264") plan.arguments << "-crf" << "20" << "-preset" << "medium";
        else plan.arguments << "-b:v" << "12M";
        plan.arguments << "-movflags" << "+faststart";
    }
    plan.arguments << "-map" << "[mix]" << "-c:a" << (s.audioOnly ? "pcm_s24le" : "aac");
    if (!s.audioOnly) plan.arguments << "-b:a" << "256k";
    plan.arguments << "-ar" << "48000" << "-t" << f(p.duration()) << destination;
    return plan;
}
}
