#include "document.h"
#include <QUndoCommand>
#include <algorithm>
#include <stdexcept>

namespace loggx {
class EditCommand : public QUndoCommand {
public:
    EditCommand(Document* d, Project before, Project after, const QString& text)
        : QUndoCommand(text), doc(d), old(std::move(before)), next(std::move(after)) {}
    void undo() override { doc->project_ = old; emit doc->changed(); }
    void redo() override { doc->project_ = next; emit doc->changed(); }
private:
    Document* doc; Project old, next;
};
Document::Document(QObject* parent) : QObject(parent), history_(this) {}
bool Document::change(const QString& name, const std::function<void(Project&)>& edit) {
    try {
        Project next = project_; edit(next); next.validate();
        if (next.toJson() == project_.toJson()) return false;
        history_.push(new EditCommand(this,project_,std::move(next),name)); return true;
    } catch (const std::exception& e) { emit rejected(QString::fromUtf8(e.what())); return false; }
}
void Document::replace(Project p) { p.validate(); project_ = std::move(p); history_.clear(); emit changed(); }
bool Document::split(const QString& clip, double at) {
    return change("Clip teilen",[&](Project& p) {
        auto* t = p.trackForClip(clip); auto* c = p.findClip(clip);
        if (!c || at <= c->start + 0.001 || at >= c->end() - 0.001) return;
        Clip right = *c; right.id = newId(); right.start = at;
        right.duration = c->end()-at; right.sourceIn += at-c->start;
        right.fadeIn = 0; right.fadeOut = std::min(right.fadeOut,right.duration);
        c->duration = at-c->start; c->fadeOut = 0; c->fadeIn = std::min(c->fadeIn,c->duration);
        t->clips.append(right);
    });
}
bool Document::remove(const QString& clip, bool ripple) {
    return change(ripple ? "Ripple-Löschen" : "Clip löschen",[&](Project& p) {
        auto* t = p.trackForClip(clip); const auto* c = p.findClip(clip); if (!c) return;
        const double end = c->end(), length = c->duration;
        t->clips.erase(std::remove_if(t->clips.begin(),t->clips.end(),[&](const Clip& x) { return x.id == clip; }),t->clips.end());
        if (ripple) for (auto& lane : p.tracks) for (auto& x : lane.clips) if (x.start >= end-0.0001) x.start -= length;
    });
}
bool Document::duplicate(const QString& clip) {
    return change("Clip duplizieren",[&](Project& p) {
        const auto* c = p.findClip(clip); if (!c) return;
        Clip copy = *c; copy.id = newId(); copy.start = c->end(); p.trackForClip(clip)->clips.append(copy);
    });
}
bool Document::move(const QString& clip, int track, double at) {
    return change("Clip verschieben",[&](Project& p) {
        auto* from = p.trackForClip(clip); auto* c = p.findClip(clip);
        if (!c || track < 0 || track >= p.tracks.size()) return;
        if (from->kind != p.tracks[track].kind) throw std::runtime_error("Bitte denselben Spurtyp wählen.");
        Clip copy = *c; copy.start = std::max(0.0,at);
        from->clips.erase(std::remove_if(from->clips.begin(),from->clips.end(),[&](const Clip& x) { return x.id == clip; }),from->clips.end());
        p.tracks[track].clips.append(copy);
    });
}
bool Document::trim(const QString& clip, double left, double right) {
    return change("Clip trimmen",[&](Project& p) {
        auto* c = p.findClip(clip); if (!c) return;
        c->sourceIn += left-c->start; c->start = left; c->duration = right-left;
        c->fadeIn = std::min(c->fadeIn,c->duration); c->fadeOut = std::min(c->fadeOut,c->duration);
    });
}
}
