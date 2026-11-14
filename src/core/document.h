#pragma once
#include "project.h"
#include <QObject>
#include <QUndoStack>
#include <functional>

namespace loggx {
class Document : public QObject {
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);
    const Project& project() const { return project_; }
    QUndoStack* history() { return &history_; }
    bool change(const QString& name, const std::function<void(Project&)>& edit);
    void replace(Project p);
    bool split(const QString& clip, double at);
    bool remove(const QString& clip, bool ripple = false);
    bool duplicate(const QString& clip);
    bool move(const QString& clip, int track, double at);
    bool trim(const QString& clip, double left, double right);
signals:
    void changed();
    void rejected(const QString& reason);
private:
    friend class EditCommand;
    Project project_ = Project::blank();
    QUndoStack history_;
};
}
