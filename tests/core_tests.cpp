#include "core/project.h"
#include "core/document.h"
#include "core/audio.h"
#include "core/exportplan.h"
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>
#include <stdexcept>

using namespace loggx;
namespace {
Project fixture() {
    auto p = Project::blank(); Media m; m.path = "/media/clip.mp4"; m.name = "clip.mp4"; m.video = m.audio = true; m.duration = 20;
    p.media.append(m); Clip c; c.mediaId = m.id; c.duration = 10; c.start = 2; c.sourceIn = 1; p.tracks[0].clips.append(c); return p;
}
bool run(const QString& executable, const QStringList& args, QByteArray* bytes = nullptr) {
    QProcess process; process.start(executable,args);
    if (!process.waitForStarted(5000) || !process.waitForFinished(60000)) return false;
    if (bytes) *bytes = process.readAllStandardOutput();
    if (process.exitCode() != 0) qWarning().noquote() << process.readAllStandardError();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
}
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void projectRoundTrip() {
        QTemporaryDir dir; QVERIFY(dir.isValid()); auto p = fixture(); p.media[0].path = dir.filePath("source with spaces.mp4");
        p.tracks[0].clips[0].brightness = 0.2;
        const QString path = dir.filePath("project.loggx"); saveProject(p,path); auto loaded = loadProject(path);
        QCOMPARE(loaded.toJson(),p.toJson());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("source with spaces.mp4"));
    }
    void rejectMalformedProjects() {
        auto p = fixture(); p.tracks[0].clips[0].duration = -1; QVERIFY_EXCEPTION_THROWN(p.validate(),std::runtime_error);
        p = fixture(); p.tracks[0].clips[0].mediaId = "missing"; QVERIFY_EXCEPTION_THROWN(p.validate(),std::runtime_error);
        p = fixture(); p.tracks[0].clips[0].sourceIn = 19; QVERIFY_EXCEPTION_THROWN(p.validate(),std::runtime_error);
        auto json = fixture().toJson(); json["version"] = 99; QVERIFY_EXCEPTION_THROWN(Project::fromJson(json),std::runtime_error);
        json = fixture().toJson(); json["tempo"] = "fast"; QVERIFY_EXCEPTION_THROWN(Project::fromJson(json),std::runtime_error);
        p = fixture(); p.tracks[1].id = p.tracks[0].id; QVERIFY_EXCEPTION_THROWN(p.validate(),std::runtime_error);
        p = fixture(); p.tempo = std::nan(""); QVERIFY_EXCEPTION_THROWN(p.validate(),std::runtime_error);
    }
    void splitPreservesSourceAndUndo() {
        Document doc; doc.replace(fixture()); const auto id = doc.project().tracks[0].clips[0].id;
        QVERIFY(doc.split(id,6)); QCOMPARE(doc.project().tracks[0].clips.size(),2);
        const auto& a = doc.project().tracks[0].clips[0]; const auto& b = doc.project().tracks[0].clips[1];
        QCOMPARE(a.duration,4); QCOMPARE(b.start,6); QCOMPARE(b.sourceIn,5); QCOMPARE(b.duration,6); QVERIFY(a.id != b.id);
        doc.history()->undo(); QCOMPARE(doc.project().tracks[0].clips.size(),1); QCOMPARE(doc.project().findClip(id)->duration,10);
        doc.history()->redo(); QCOMPARE(doc.project().tracks[0].clips.size(),2);
    }
    void trimAndInvalidEditAreAtomic() {
        Document doc; doc.replace(fixture()); const auto id = doc.project().tracks[0].clips[0].id;
        QVERIFY(doc.trim(id,3,9)); QCOMPARE(doc.project().findClip(id)->sourceIn,2); QCOMPARE(doc.project().findClip(id)->duration,6);
        const auto before = doc.project().toJson(); QSignalSpy errors(&doc,&Document::rejected);
        QVERIFY(!doc.trim(id,-5,9)); QCOMPARE(errors.count(),1); QCOMPARE(doc.project().toJson(),before);
    }
    void rippleAcrossTracks() {
        Document doc; auto p = fixture(); auto later = p.tracks[0].clips[0]; later.id = newId(); later.start = 15; p.tracks[0].clips.append(later);
        Clip pattern; pattern.start = 15; pattern.duration = 2; p.tracks[2].clips.append(pattern); doc.replace(p);
        QVERIFY(doc.remove(p.tracks[0].clips[0].id,true)); QCOMPARE(doc.project().tracks[0].clips[0].start,5); QCOMPARE(doc.project().tracks[2].clips[0].start,5);
        doc.history()->undo(); QCOMPARE(doc.project().tracks[2].clips[0].start,15);
    }
    void moveAndDuplicate() {
        Document doc; auto p = fixture(); Track video; video.name = "Video 2"; p.tracks.append(video); doc.replace(p);
        const auto id = p.tracks[0].clips[0].id;
        QVERIFY(!doc.move(id,1,0)); QVERIFY(doc.move(id,3,8)); QCOMPARE(doc.project().tracks[0].clips.size(),0);
        QVERIFY(doc.duplicate(id)); QCOMPARE(doc.project().tracks[3].clips.size(),2); QCOMPARE(doc.project().tracks[3].clips[1].start,18);
    }
    void fadesAndSnap() {
        Clip c; c.duration = 4; c.fadeIn = 1; c.fadeOut = 1;
        QCOMPARE(clipEnvelope(c,0),0); QCOMPARE(clipEnvelope(c,0.5),0.5); QCOMPARE(clipEnvelope(c,2),1); QCOMPARE(clipEnvelope(c,3.5),0.5);
        QCOMPARE(snapTime(0.76,120,true,30),1.0); QCOMPARE(snapTime(-1,120,true,30),0.0); QCOMPARE(timecode(61.5,30),QString("00:01:01:15"));
    }
    void instrumentPanMuteAndSplit() {
        auto p = Project::blank(); Clip clip; clip.duration = 2; clip.notes.append({60,0,4,0.8}); p.tracks[2].clips.append(clip); p.tracks[2].pan = -1;
        auto sample = instrumentSample(p,0.137); QVERIFY(std::abs(sample.left) > 0.001f); QCOMPARE(sample.right,0.0f);
        Document doc; doc.replace(p); QVERIFY(doc.split(clip.id,0.5)); auto after = instrumentSample(doc.project(),0.737); auto before = instrumentSample(p,0.737);
        QVERIFY(std::abs(after.left-before.left) < 0.00001f);
        p.tracks[2].mute = true; QCOMPARE(instrumentSample(p,0.137).left,0.0f);
        p.tracks[2].mute = false; p.tracks[0].solo = true; QCOMPARE(instrumentSample(p,0.137).left,0.0f);
    }
    void wavAndCancellation() {
        QTemporaryDir dir; auto p = Project::blank(); Clip c; c.duration = 0.1; c.notes.append({60,0,1,1}); p.tracks[2].clips.append(c);
        const auto path = dir.filePath("notes.wav"); writeInstrumentWav(p,path);
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); auto bytes = f.readAll(); QCOMPARE(bytes.left(4),QByteArray("RIFF")); QCOMPARE(bytes.size(),44+4800*4);
        std::atomic_bool cancelled(true); QVERIFY_EXCEPTION_THROWN(writeInstrumentWav(p,dir.filePath("cancel.wav"),&cancelled),std::runtime_error); QVERIFY(!QFile::exists(dir.filePath("cancel.wav")));
    }
    void exportPlanDoesNotInterpolateFilenames() {
        auto p = fixture(); p.media[0].path = "C:/media/a';[movie] & file.mp4";
        auto plan = makeExportPlan(p,{},"result.mp4"); QVERIFY(plan.arguments.contains(p.media[0].path)); QVERIFY(!plan.graph.contains(p.media[0].path));
        QVERIFY(plan.arguments.contains("-n")); QVERIFY(plan.graph.contains("trim=start=1.000000")); QVERIFY(plan.graph.contains("adelay=96000S"));
        QVERIFY_EXCEPTION_THROWN(makeExportPlan(Project::blank(),{},"out.mp4"),std::runtime_error);
        ExportSettings s; s.width = 7680; s.height = 4320; QVERIFY_EXCEPTION_THROWN(makeExportPlan(p,s,"out.mp4"),std::runtime_error);
    }
    void ffmpegIntegration() {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg"); if (ffmpeg.isEmpty()) QSKIP("FFmpeg is not available; CI Linux installs it.");
        QTemporaryDir dir; const auto media = dir.filePath("red source.mp4");
        QVERIFY(run(ffmpeg,{"-v","error","-f","lavfi","-i","color=c=red:s=160x90:r=10:d=1","-f","lavfi","-i","sine=frequency=440:duration=1","-c:v","mpeg4","-c:a","aac","-shortest",media}));
        auto p = Project::blank(); p.fps = 10; Media m; m.path = media; m.video = m.audio = true; m.duration = 1; p.media.append(m);
        Clip c; c.mediaId = m.id; c.start = 0.5; c.duration = 0.5; c.sourceIn = 0.25; p.tracks[0].clips.append(c);
        Clip music; music.duration = 1; music.notes.append({60,0,2,0.8}); p.tracks[2].clips.append(music);
        const auto wav = dir.filePath("notes.wav"); writeInstrumentWav(p,wav);
        ExportSettings settings; settings.width = 160; settings.height = 90; settings.fps = 10;
        const auto output = dir.filePath("output.mp4"); const auto plan = makeExportPlan(p,settings,output,wav);
        QVERIFY(run(ffmpeg,plan.arguments)); QVERIFY(QFileInfo(output).size() > 500);
        QByteArray pixels;
        QVERIFY(run(ffmpeg,{"-v","error","-ss","0.7","-i",output,"-frames:v","1","-f","rawvideo","-pix_fmt","rgb24","pipe:1"},&pixels));
        QCOMPARE(pixels.size(),160*90*3); QVERIFY(quint8(pixels[0]) > 180); QVERIFY(quint8(pixels[1]) < 60);
        QVERIFY(run(ffmpeg,{"-v","error","-ss","0.1","-i",output,"-frames:v","1","-f","rawvideo","-pix_fmt","rgb24","pipe:1"},&pixels));
        QCOMPARE(pixels.size(),160*90*3); QVERIFY(quint8(pixels[0]) < 20);
        settings.audioOnly = true; const auto audio = makeExportPlan(p,settings,dir.filePath("mix.wav"),wav); QVERIFY(run(ffmpeg,audio.arguments));
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
