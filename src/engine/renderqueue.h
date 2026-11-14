#pragma once
#include "core/exportplan.h"
#include <QObject>
#include <QFutureWatcher>
#include <atomic>
#include <memory>
#include <deque>

namespace loggx {
struct RenderJob { Project project; ExportSettings settings; QString destination; };
class RenderQueue : public QObject {
    Q_OBJECT
public:
    explicit RenderQueue(QObject* parent = nullptr);
    ~RenderQueue() override;
    void enqueue(RenderJob job);
    void cancel();
    bool busy() const { return active_; }
signals:
    void progress(const QString& name, int percent, const QString& stage);
    void finished(const QString& path, const QString& error);
    void queueChanged(int remaining);
private:
    void next();
    std::deque<RenderJob> jobs_;
    QFutureWatcher<QString> watcher_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    QString current_;
    bool active_ = false;
};
}
