#include "renderqueue.h"
#include "mediaimport.h"
#include "core/audio.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <exception>
#include <algorithm>

namespace loggx {
RenderQueue::RenderQueue(QObject* parent) : QObject(parent) {
    connect(&watcher_,&QFutureWatcher<QString>::finished,this,[this] {
        active_ = false; emit finished(current_,watcher_.result()); next();
    });
}
RenderQueue::~RenderQueue() { cancel(); watcher_.waitForFinished(); }
void RenderQueue::cancel() { jobs_.clear(); if (cancelled_) cancelled_->store(true); emit queueChanged(active_ ? 1 : 0); }
void RenderQueue::enqueue(RenderJob job) { jobs_.push_back(std::move(job)); emit queueChanged(int(jobs_.size())+(active_ ? 1 : 0)); next(); }
void RenderQueue::next() {
    if (active_ || jobs_.empty()) { emit queueChanged(int(jobs_.size())+(active_ ? 1 : 0)); return; }
    RenderJob job = jobs_.front(); jobs_.pop_front(); active_ = true; current_ = job.destination;
    cancelled_ = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = cancelled_; const QString executable = findTool("ffmpeg");
    auto report = [this,path=job.destination](int value, const QString& stage) {
        QMetaObject::invokeMethod(this,[this,path,value,stage] { emit progress(path,value,stage); },Qt::QueuedConnection);
    };
    watcher_.setFuture(QtConcurrent::run([job,executable,cancelled,report]() -> QString {
        try {
            if (executable.isEmpty()) return "FFmpeg fehlt. Unter Einstellungen den Programmpfad setzen.";
            if (QFileInfo::exists(job.destination)) return "Zieldatei existiert bereits. Bitte einen neuen Dateinamen wählen.";
            for (const auto& t : job.project.tracks) for (const auto& c : t.clips) {
                if (t.kind == TrackKind::Instrument) continue;
                const auto* m = job.project.findMedia(c.mediaId);
                if (!m || !QFileInfo::exists(m->path)) return "Quelldatei fehlt: "+(m ? m->path : c.mediaId);
            }
            const QFileInfo destination(job.destination);
            QTemporaryDir temp(destination.absolutePath()+"/.loggx-render-XXXXXX");
            if (!temp.isValid()) return "Temporärer Renderordner kann nicht erstellt werden.";
            QString notes;
            if (hasInstruments(job.project)) {
                notes = temp.filePath("instruments.wav");
                writeInstrumentWav(job.project,notes,cancelled.get(),[&](double fraction) { report(int(fraction*20),"Instrumente rendern"); });
            }
            if (cancelled->load()) return "Export abgebrochen.";
            const QString output = temp.filePath(job.settings.audioOnly ? "render.wav" : "render.mp4");
            const auto plan = makeExportPlan(job.project,job.settings,output,notes);
            QProcess process; process.start(executable,plan.arguments);
            if (!process.waitForStarted(5000)) return "FFmpeg konnte nicht gestartet werden: "+process.errorString();
            QByteArray diagnostics, progressBuffer;
            while (process.state() != QProcess::NotRunning) {
                process.waitForReadyRead(100);
                diagnostics += process.readAllStandardError(); diagnostics = diagnostics.right(12000);
                progressBuffer += process.readAllStandardOutput();
                int newline;
                while ((newline = progressBuffer.indexOf('\n')) >= 0) {
                    const auto line = progressBuffer.left(newline).trimmed(); progressBuffer.remove(0,newline+1);
                    if (line.startsWith("out_time_us=")) {
                        const double seconds = line.mid(12).toDouble()/1000000;
                        report(std::clamp(20+int(seconds/job.project.duration()*79),20,99),"Video und Ton exportieren");
                    }
                }
                if (cancelled->load()) { process.kill(); process.waitForFinished(5000); return "Export abgebrochen."; }
            }
            diagnostics += process.readAllStandardError();
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
                return "FFmpeg-Export fehlgeschlagen. Encoder und Quelldateien prüfen.\n"+QString::fromUtf8(diagnostics.right(4000));
            if (cancelled->load()) return "Export abgebrochen.";
            if (!QFile::rename(output,job.destination)) return "Export fertig, aber die Zieldatei konnte nicht gespeichert werden. Dateiname oder Berechtigungen prüfen.";
            report(100,"Fertig"); return {};
        } catch (const std::exception& e) { return QString::fromUtf8(e.what()); }
    }));
}
}
