#include "mediaimport.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <cmath>

namespace loggx {
QString findTool(const QString& name) {
    const QString configured = QSettings().value("tools/"+name).toString();
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable()) return configured;
    QString filename = name;
#ifdef Q_OS_WIN
    filename += ".exe";
#endif
    const QString adjacent = QCoreApplication::applicationDirPath()+"/"+filename;
    if (QFileInfo(adjacent).isExecutable()) return adjacent;
    return QStandardPaths::findExecutable(name);
}
ImportResult probeMedia(const QStringList& paths, const QString& probe) {
    ImportResult result;
    if (probe.isEmpty()) { result.errors << "FFprobe fehlt. Unter Einstellungen die FFmpeg-Werkzeuge auswählen."; return result; }
    for (const auto& path : paths) {
        const QFileInfo file(path);
        if (!file.isFile()) { result.errors << path+": Datei nicht gefunden."; continue; }
        QProcess process;
        process.start(probe,{"-v","error","-show_format","-show_streams","-of","json",file.absoluteFilePath()});
        if (!process.waitForStarted(5000) || !process.waitForFinished(30000)) {
            process.kill(); process.waitForFinished(3000); result.errors << file.fileName()+": Medienprüfung hat das Zeitlimit überschritten."; continue;
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            result.errors << file.fileName()+": "+QString::fromUtf8(process.readAllStandardError()).left(500); continue;
        }
        const auto o = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        Media m; m.path = file.absoluteFilePath(); m.name = file.fileName();
        m.duration = o.value("format").toObject().value("duration").toString().toDouble();
        for (const auto& value : o.value("streams").toArray()) {
            const auto stream = value.toObject(); const QString type = stream.value("codec_type").toString();
            if (type == "video" && !stream.value("disposition").toObject().value("attached_pic").toInt()) {
                m.video = true; m.width = stream.value("width").toInt(); m.height = stream.value("height").toInt();
            }
            if (type == "audio") m.audio = true;
            m.duration = std::max(m.duration,stream.value("duration").toString().toDouble());
        }
        if (!(m.video || m.audio) || !std::isfinite(m.duration) || m.duration <= 0 || m.duration > 86400) {
            result.errors << file.fileName()+": Kein unterstütztes Video/Audio mit gültiger Dauer (max. 24 Stunden)."; continue;
        }
        result.media << m;
    }
    return result;
}
MediaImporter::MediaImporter(QObject* parent) : QObject(parent) {
    connect(&watcher,&QFutureWatcher<ImportResult>::finished,this,[this] { emit complete(watcher.result()); });
}
void MediaImporter::importFiles(const QStringList& paths) {
    if (busy()) return;
    const auto probe = findTool("ffprobe");
    watcher.setFuture(QtConcurrent::run([paths,probe] { return probeMedia(paths,probe); }));
}
}
