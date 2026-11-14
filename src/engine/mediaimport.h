#pragma once
#include "core/project.h"
#include <QObject>
#include <QFutureWatcher>

namespace loggx {
struct ImportResult { QVector<Media> media; QStringList errors; };
QString findTool(const QString& name);
ImportResult probeMedia(const QStringList& paths, const QString& probe);
class MediaImporter : public QObject {
    Q_OBJECT
public:
    explicit MediaImporter(QObject* parent = nullptr);
    bool busy() const { return watcher.isRunning(); }
    void importFiles(const QStringList& paths);
signals:
    void complete(const loggx::ImportResult& result);
private:
    QFutureWatcher<ImportResult> watcher;
};
}
