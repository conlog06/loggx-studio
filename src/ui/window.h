#pragma once
#include "core/document.h"
#include "engine/playback.h"
#include "engine/mediaimport.h"
#include "engine/renderqueue.h"
#include <QMainWindow>
#include <QTimer>

class QLabel; class QListWidget; class QLineEdit; class QDoubleSpinBox; class QComboBox;
class QStackedWidget; class QProgressBar; class QPushButton; class QVBoxLayout; class QTabBar;
namespace loggx {
class Timeline; class PianoRoll;
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(QWidget* parent = nullptr);
    void openPath(const QString& path);
protected:
    void closeEvent(QCloseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void buildUi();
    QWidget* mediaPage();
    QWidget* colorPage();
    QWidget* musicPage();
    QWidget* exportPage();
    QWidget* inspector();
    void refresh();
    void refreshInspector();
    void refreshMixer();
    void showFrame(const QImage& image);
    void changeClip(const QString&, const std::function<void(Clip&)>&);
    void importFiles(QStringList paths = {});
    void addMedia(const QString& id);
    void addTrack(TrackKind kind);
    void addPattern(int track, double at);
    void editPattern(const QString& id);
    void newProject();
    bool save(bool choose = false);
    bool confirmDiscard();
    void settings();
    void enqueueExport();
    void autosave();
    QString recoveryPath() const;
    void recover();
    Document document_;
    Playback playback_;
    MediaImporter importer_;
    RenderQueue renders_;
    QTimer autosaveTimer_;
    QString path_;
    bool refreshing_ = false;
    int currentWorkspace_ = 1;
    Timeline* timeline_ = nullptr;
    PianoRoll* piano_ = nullptr;
    QLabel *preview_ = nullptr, *title_ = nullptr, *time_ = nullptr, *selectionTitle_ = nullptr, *patternTitle_ = nullptr;
    QListWidget *media_ = nullptr, *jobs_ = nullptr;
    QLineEdit* search_ = nullptr;
    QDoubleSpinBox *tempo_ = nullptr, *start_ = nullptr, *duration_ = nullptr, *sourceIn_ = nullptr, *gain_ = nullptr, *fadeIn_ = nullptr, *fadeOut_ = nullptr;
    QDoubleSpinBox *brightness_ = nullptr, *contrast_ = nullptr, *saturation_ = nullptr;
    QComboBox *instrument_ = nullptr, *preset_ = nullptr, *encoder_ = nullptr;
    QStackedWidget* workspace_ = nullptr;
    QTabBar* tabs_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton* play_ = nullptr;
    QVBoxLayout* mixer_ = nullptr;
    QImage lastFrame_;
};
}
