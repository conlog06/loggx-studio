#include "window.h"
#include "timeline.h"
#include "pianoroll.h"
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QUndoView>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace loggx {
namespace {
QLabel* heading(const QString& text) { auto* l = new QLabel(text); l->setObjectName("sectionHeading"); return l; }
QLabel* help(const QString& text) { auto* l = new QLabel(text); l->setWordWrap(true); l->setObjectName("muted"); return l; }
QPushButton* button(const QString& text, const std::function<void()>& callback) {
    auto* b = new QPushButton(text); QObject::connect(b,&QPushButton::clicked,b,callback); return b;
}
QDoubleSpinBox* number(double lo, double hi, double step, const QString& suffix = {}) {
    auto* spin = new QDoubleSpinBox; spin->setRange(lo,hi); spin->setSingleStep(step); spin->setDecimals(2);
    spin->setSuffix(suffix); spin->setKeyboardTracking(false); return spin;
}
QScrollArea* makeScrollArea(QWidget* widget, bool resize = true) {
    auto* area = new QScrollArea; area->setWidget(widget); area->setWidgetResizable(resize); area->setFrameShape(QFrame::NoFrame); return area;
}
const char* studioStyle = R"(
QMainWindow,QDialog { background:#111318; color:#e5e8f0; }
QWidget { color:#dfe4ef; font-size:12px; }
QMenuBar,QMenu { background:#1b1e26; color:#dfe4ef; }
QMenu::item:selected,QMenuBar::item:selected { background:#373d4b; }
QPushButton { background:#2a2f3b; border:1px solid #3a4150; border-radius:6px; padding:7px 12px; }
QPushButton:hover { background:#3a4252; border-color:#677084; }
QPushButton:pressed { background:#4b5364; }
QPushButton:disabled { color:#656d7d; }
QPushButton#primary { background:#ef9863; color:#17191f; border-color:#ef9863; font-weight:700; }
QPushButton#primary:hover { background:#ffaf7d; }
QLineEdit,QDoubleSpinBox,QComboBox { background:#171b23; border:1px solid #39404e; border-radius:5px; padding:6px; selection-background-color:#74618e; }
QComboBox QAbstractItemView { background:#252a36; selection-background-color:#505a70; }
QListWidget,QUndoView { background:#171a22; border:0; outline:none; }
QListWidget::item { padding:12px; border-bottom:1px solid #282e39; }
QListWidget::item:selected { background:#3a3549; border-left:3px solid #ef9863; }
QLabel#muted { color:#929caf; line-height:1.5; }
QLabel#sectionHeading { color:#f0f3fa; font-size:15px; font-weight:600; padding:4px 0; }
QLabel#brand { font-size:20px; font-weight:800; letter-spacing:1px; }
QLabel#preview { background:#090b10; color:#718096; border:1px solid #2b303d; border-radius:10px; }
QLabel#timecode { font-family:Consolas,monospace; font-size:16px; color:#f2af83; }
QGroupBox { border:1px solid #333a48; border-radius:8px; margin-top:14px; padding-top:12px; }
QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 5px; color:#aab4c6; }
QScrollArea { background:#14171e; border:0; }
QScrollBar:horizontal { height:12px; background:#13161d; }
QScrollBar:vertical { width:12px; background:#13161d; }
QScrollBar::handle { background:#414a5d; border-radius:5px; min-width:25px; min-height:25px; }
QScrollBar::add-line,QScrollBar::sub-line { width:0;height:0; }
QSplitter::handle { background:#313744; }
QTabBar::tab { background:#1b1e27; border-top:2px solid transparent; color:#929daf; padding:13px 27px; font-size:13px; }
QTabBar::tab:selected { color:#ffb383; border-top-color:#f0a16e; background:#2b2930; }
QStatusBar { color:#929caf; background:#171a21; }
QSlider::groove:horizontal { height:5px; background:#373e4e; border-radius:2px; }
QSlider::handle:horizontal { width:13px; margin:-4px 0; border-radius:6px; background:#efab7d; }
QProgressBar { border:1px solid #3a4252; border-radius:5px; text-align:center; height:18px; background:#181c25; }
QProgressBar::chunk { background:#9471c6; border-radius:4px; }
QToolTip { background:#303746; color:#eef2fa; border:1px solid #526079; }
)";
}
Window::Window(QWidget* parent) : QMainWindow(parent), document_(this), playback_(this), importer_(this), renders_(this) {
    setStyleSheet(QString::fromUtf8(studioStyle)); setAcceptDrops(true); setMinimumSize(1060,720); resize(1480,940);
    buildUi();
    connect(&document_,&Document::changed,this,[this] { playback_.setProject(document_.project()); refresh(); });
    connect(document_.history(),&QUndoStack::cleanChanged,this,[this] { refresh(); });
    connect(&document_,&Document::rejected,this,[this](const QString& text) { QMessageBox::warning(this,"Änderung nicht möglich",text); refreshInspector(); });
    connect(&playback_,&Playback::positionChanged,this,[this](double time) { timeline_->setPosition(time); time_->setText(timecode(time,document_.project().fps)); });
    connect(&playback_,&Playback::frameReady,this,&Window::showFrame);
    connect(&playback_,&Playback::playingChanged,this,[this](bool playing) { play_->setText(playing ? "Pause" : "Wiedergabe"); });
    connect(&playback_,&Playback::error,this,[this](const QString& text) { statusBar()->showMessage(text,10000); });
    connect(&importer_,&MediaImporter::complete,this,[this](const ImportResult& result) {
        document_.change("Medien importieren",[&](Project& p) {
            for (const auto& media : result.media) {
                const bool exists = std::any_of(p.media.begin(),p.media.end(),[&](const Media& old) { return old.path == media.path; });
                if (!exists) p.media.append(media);
            }
        });
        statusBar()->showMessage(QString::number(result.media.size())+" Medien geprüft. Doppelklick fügt sie zur Timeline hinzu.",9000);
        if (!result.errors.isEmpty()) QMessageBox::warning(this,"Medienimport",result.errors.join("\n\n"));
    });
    connect(&renders_,&RenderQueue::progress,this,[this](const QString& name,int percent,const QString& stage) { progress_->setValue(percent); statusBar()->showMessage(QFileInfo(name).fileName()+" · "+stage); });
    connect(&renders_,&RenderQueue::finished,this,[this](const QString& path,const QString& error) {
        jobs_->addItem((error.isEmpty() ? "FERTIG  " : "FEHLER  ")+QFileInfo(path).fileName());
        if (!error.isEmpty()) QMessageBox::warning(this,"Export",error);
        else statusBar()->showMessage("Export gespeichert: "+path,12000);
    });
    connect(&renders_,&RenderQueue::queueChanged,this,[this](int remaining) { progress_->setFormat(remaining ? "%p% · "+QString::number(remaining)+" Auftrag/Aufträge" : "Bereit"); });
    autosaveTimer_.setInterval(60000); connect(&autosaveTimer_,&QTimer::timeout,this,&Window::autosave); autosaveTimer_.start();
    refresh(); playback_.setProject(document_.project());
    QTimer::singleShot(0,this,[this] { if (QCoreApplication::arguments().size() == 1) recover(); });
}
void Window::buildUi() {
    auto* file = menuBar()->addMenu("Datei");
    file->addAction("Neues Projekt",QKeySequence::New,this,&Window::newProject);
    file->addAction("Projekt öffnen …",QKeySequence::Open,this,[this] { const auto path = QFileDialog::getOpenFileName(this,"Projekt öffnen",{},"LoggX-Projekt (*.loggx)"); if (!path.isEmpty()) openPath(path); });
    file->addAction("Speichern",QKeySequence::Save,this,[this] { save(); });
    file->addAction("Speichern unter …",QKeySequence::SaveAs,this,[this] { save(true); });
    file->addAction("Projekt umbenennen …",this,[this] {
        bool ok; const auto name = QInputDialog::getText(this,"Projektname","Name",QLineEdit::Normal,document_.project().name,&ok);
        if (ok && !name.trimmed().isEmpty()) document_.change("Projekt umbenennen",[&](Project& p) { p.name = name.trimmed(); });
    });
    file->addSeparator(); file->addAction("Medien importieren …",QKeySequence("Ctrl+I"),this,[this] { importFiles(); });
    file->addAction("Fehlendes Medium neu verknüpfen …",this,[this] {
        auto* item = media_->currentItem(); if (!item) return;
        const auto id = item->data(Qt::UserRole).toString(); const auto path = QFileDialog::getOpenFileName(this,"Originalmedium auswählen");
        if (path.isEmpty()) return;
        // Relinking is explicit and must keep the original identity; mismatches are checked by the user.
        document_.change("Medium neu verknüpfen",[&](Project& p) { for (auto& m : p.media) if (m.id == id) m.path = path; });
    });
    file->addSeparator(); file->addAction("Einstellungen …",this,&Window::settings);
    auto* edit = menuBar()->addMenu("Bearbeiten");
    auto* undo = document_.history()->createUndoAction(this,"Rückgängig"); undo->setShortcut(QKeySequence::Undo); edit->addAction(undo);
    auto* redo = document_.history()->createRedoAction(this,"Wiederholen"); redo->setShortcuts(QKeySequence::Redo); edit->addAction(redo);
    edit->addAction("Clip teilen",QKeySequence("Ctrl+B"),this,[this] { document_.split(timeline_->selected(),playback_.position()); });
    edit->addAction("Clip duplizieren",QKeySequence("Ctrl+D"),this,[this] { document_.duplicate(timeline_->selected()); });
    edit->addAction("Clip löschen",QKeySequence::Delete,this,[this] { document_.remove(timeline_->selected()); });
    edit->addAction("Ripple-Löschen (alle Spuren)",QKeySequence("Shift+Delete"),this,[this] { document_.remove(timeline_->selected(),true); });
    edit->addAction("Verlauf …",this,[this] { auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle("Bearbeitungsverlauf"); auto* l = new QVBoxLayout(dialog); l->addWidget(new QUndoView(document_.history())); dialog->resize(400,500); dialog->show(); });
    auto* info = menuBar()->addMenu("Hilfe");
    info->addAction("Erste Schritte",this,[this] { QMessageBox::information(this,"LoggX Studio · Erste Schritte","1. Unter Windows sind FFmpeg und FFprobe im Paket enthalten. Auf macOS und Linux können sie unter Einstellungen ausgewählt werden.\n2. Videos oder Audio importieren; per Doppelklick zur Timeline hinzufügen.\n3. Clips ziehen, an den Rändern trimmen und mit Strg+B am Abspielkopf teilen.\n4. Instrumentenspur wählen und Pattern erstellen. Noten links hinzufügen, rechts löschen.\n5. Projekt speichern und unter Export rendern.\n\nLeertaste: Wiedergabe · Strg+S: Speichern · Strg+Z: Rückgängig\n\nVersion 0.1 ist ein technischer Prototyp. Vorschau bis 1280 Pixel Breite; Export bis 1080p. Plugin-Hosting, KI und 8K folgen später."); });
    info->addAction("Projekt auf GitHub",this,[] { QDesktopServices::openUrl(QUrl("https://github.com/conlog06/loggx-studio")); });
    auto* root = new QWidget; auto* layout = new QVBoxLayout(root); layout->setContentsMargins(14,8,14,0); layout->setSpacing(10); setCentralWidget(root);
    auto* top = new QHBoxLayout;
    auto* logo = new QLabel("LOGG<span style='color:#f0a16e'>X</span> <span style='font-weight:400;color:#98a3b7'>STUDIO</span>"); logo->setObjectName("brand"); top->addWidget(logo);
    top->addSpacing(24); title_ = new QLabel; title_->setObjectName("muted"); top->addWidget(title_); top->addStretch();
    top->addWidget(button("Importieren",[this] { importFiles(); })); top->addWidget(button("Speichern",[this] { save(); }));
    auto* exportButton = button("Exportieren",[this] { tabs_->setCurrentIndex(4); }); exportButton->setObjectName("primary"); top->addWidget(exportButton); layout->addLayout(top);
    auto* mainSplit = new QSplitter(Qt::Vertical);
    auto* upper = new QSplitter(Qt::Horizontal);
    workspace_ = new QStackedWidget;
    workspace_->addWidget(mediaPage());
    auto* cut = new QWidget; auto* cl = new QVBoxLayout(cut); cl->addWidget(heading("Dein Schnitt. Dein Sound."));
    cl->addWidget(help("Video und Musik teilen sich eine Zeitachse. Wähle einen Clip für die Bearbeitung im Inspector."));
    cl->addSpacing(15); cl->addWidget(button("+ Videospur",[this] { addTrack(TrackKind::Video); })); cl->addWidget(button("+ Audiospur",[this] { addTrack(TrackKind::Audio); })); cl->addWidget(button("+ Instrumentenspur",[this] { addTrack(TrackKind::Instrument); }));
    cl->addSpacing(15); cl->addWidget(heading("Schnellstart")); cl->addWidget(help("Clips verschieben: ziehen\nTrimmen: linken/rechten Rand ziehen\nTeilen: Strg+B\nDuplizieren: Strg+D\nRipple-Löschen: Umschalt+Entf\n\nM = Stumm · S = Solo\nTaktzahlen markieren die Beats.")); cl->addStretch(); workspace_->addWidget(cut);
    workspace_->addWidget(colorPage()); workspace_->addWidget(musicPage()); workspace_->addWidget(exportPage());
    workspace_->setMinimumWidth(255); upper->addWidget(workspace_);
    auto* monitor = new QWidget; auto* ml = new QVBoxLayout(monitor); ml->setContentsMargins(8,0,8,0);
    auto* monitorHeader = new QHBoxLayout; monitorHeader->addWidget(heading("PROGRAMM")); monitorHeader->addStretch(); monitorHeader->addWidget(help("Vorschau · max. 1280 px breit")); ml->addLayout(monitorHeader);
    preview_ = new QLabel("Medien importieren oder ein Musik-Pattern erstellen"); preview_->setObjectName("preview"); preview_->setAlignment(Qt::AlignCenter); preview_->setWordWrap(true); preview_->setMinimumSize(340,210); preview_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding); ml->addWidget(preview_,1);
    auto* transport = new QHBoxLayout; time_ = new QLabel("00:00:00:00"); time_->setObjectName("timecode"); transport->addWidget(time_); transport->addStretch();
    transport->addWidget(button("|◀",[this] { playback_.seek(0); })); play_ = button("Wiedergabe",[this] { playback_.toggle(); }); transport->addWidget(play_);
    transport->addWidget(button("Stopp",[this] { playback_.pause(); playback_.seek(0); })); ml->addLayout(transport); upper->addWidget(monitor);
    auto* ins = inspector(); ins->setMinimumWidth(235); ins->setMaximumWidth(300); upper->addWidget(ins); upper->setSizes({300,800,250}); mainSplit->addWidget(upper);
    auto* timelinePanel = new QWidget; auto* tl = new QVBoxLayout(timelinePanel); tl->setContentsMargins(0,0,0,0);
    auto* tools = new QHBoxLayout; tools->addWidget(heading("ARRANGEMENT")); tools->addSpacing(12);
    tools->addWidget(button("Teilen",[this] { document_.split(timeline_->selected(),playback_.position()); })); tools->addWidget(button("Löschen",[this] { document_.remove(timeline_->selected()); }));
    auto* snap = new QCheckBox("Beat-Snap"); snap->setChecked(true); tools->addWidget(snap);
    tools->addStretch(); tools->addWidget(help("TEMPO")); tempo_ = number(20,300,1," BPM"); tempo_->setValue(120); tempo_->setFixedWidth(110); tools->addWidget(tempo_);
    auto* zoom = new QSlider(Qt::Horizontal); zoom->setRange(10,180); zoom->setValue(48); zoom->setFixedWidth(120); tools->addWidget(help("Zoom")); tools->addWidget(zoom); tl->addLayout(tools);
    timeline_ = new Timeline(&document_); tl->addWidget(makeScrollArea(timeline_,false)); mainSplit->addWidget(timelinePanel); mainSplit->setSizes({480,300}); layout->addWidget(mainSplit,1);
    tabs_ = new QTabBar; tabs_->setExpanding(true); tabs_->addTab("01  Medien"); tabs_->addTab("02  Schnitt"); tabs_->addTab("03  Farbe"); tabs_->addTab("04  Musik"); tabs_->addTab("05  Export"); layout->addWidget(tabs_);
    connect(tabs_,&QTabBar::currentChanged,this,[this](int index) { currentWorkspace_ = index; workspace_->setCurrentIndex(index); }); tabs_->setCurrentIndex(1);
    connect(zoom,&QSlider::valueChanged,timeline_,[this](int value) { timeline_->setZoom(value); });
    connect(snap,&QCheckBox::toggled,timeline_,&Timeline::setBeatSnap);
    connect(timeline_,&Timeline::seek,&playback_,&Playback::seek);
    connect(timeline_,&Timeline::selectionChanged,this,[this] { refreshInspector(); });
    connect(timeline_,&Timeline::editPattern,this,&Window::editPattern);
    connect(timeline_,&Timeline::createPattern,this,&Window::addPattern);
    connect(tempo_,&QDoubleSpinBox::editingFinished,this,[this] { if (!refreshing_) document_.change("Tempo ändern",[this](Project& p) { p.tempo = tempo_->value(); }); });
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space),this); connect(space,&QShortcut::activated,&playback_,&Playback::toggle);
    statusBar()->showMessage("Bereit · LoggX Studio 0.1 · Lokale Projekte, kein Konto erforderlich");
}
QWidget* Window::mediaPage() {
    auto* page = new QWidget; auto* l = new QVBoxLayout(page); l->addWidget(heading("MEDIENPOOL"));
    search_ = new QLineEdit; search_->setPlaceholderText("Medien suchen …"); l->addWidget(search_); media_ = new QListWidget; l->addWidget(media_,1);
    l->addWidget(button("+ Medien importieren",[this] { importFiles(); })); l->addWidget(help("Doppelklick: an der Abspielposition einfügen. Dateien können auch ins Fenster gezogen werden."));
    connect(media_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item) { addMedia(item->data(Qt::UserRole).toString()); });
    connect(search_,&QLineEdit::textChanged,this,[this](const QString& filter) { for (int i = 0; i < media_->count(); ++i) media_->item(i)->setHidden(!media_->item(i)->text().contains(filter,Qt::CaseInsensitive)); });
    return page;
}
QWidget* Window::inspector() {
    auto* panel = new QWidget; auto* l = new QVBoxLayout(panel); l->addWidget(heading("INSPECTOR")); selectionTitle_ = help("Kein Clip ausgewählt"); l->addWidget(selectionTitle_);
    auto* form = new QFormLayout; form->setLabelAlignment(Qt::AlignLeft); form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    start_ = number(0,86400,0.1," s"); duration_ = number(0.001,86400,0.1," s"); duration_->setDecimals(3);
    sourceIn_ = number(0,86400,0.1," s"); gain_ = number(0,4,0.05," ×"); fadeIn_ = number(0,86400,0.1," s"); fadeOut_ = number(0,86400,0.1," s");
    form->addRow("Start",start_); form->addRow("Dauer",duration_); form->addRow("Quell-In",sourceIn_); form->addRow("Lautstärke",gain_); form->addRow("Audio-Fade in",fadeIn_); form->addRow("Audio-Fade out",fadeOut_); l->addLayout(form);
    connect(start_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Start ändern",[this](Clip& c) { c.start = start_->value(); }); });
    connect(duration_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Dauer ändern",[this](Clip& c) { c.duration = duration_->value(); c.fadeIn = std::min(c.fadeIn,c.duration); c.fadeOut = std::min(c.fadeOut,c.duration); }); });
    connect(sourceIn_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Quell-In ändern",[this](Clip& c) { c.sourceIn = sourceIn_->value(); }); });
    connect(gain_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Clip-Pegel ändern",[this](Clip& c) { c.gain = gain_->value(); }); });
    connect(fadeIn_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Fade ändern",[this](Clip& c) { c.fadeIn = fadeIn_->value(); }); });
    connect(fadeOut_,&QDoubleSpinBox::editingFinished,this,[this] { changeClip("Fade ändern",[this](Clip& c) { c.fadeOut = fadeOut_->value(); }); });
    l->addSpacing(10); l->addWidget(button("Clip umbenennen",[this] { const auto* c = document_.project().findClip(timeline_->selected()); if (!c) return; bool ok; const auto name = QInputDialog::getText(this,"Clip umbenennen","Name",QLineEdit::Normal,c->name,&ok); if (ok && !name.trimmed().isEmpty()) changeClip("Clip umbenennen",[&](Clip& clip) { clip.name = name.trimmed(); }); }));
    l->addWidget(help("Audio-Fades wirken auf den Ton. Farbkorrekturen findest du im Arbeitsbereich Farbe.")); l->addStretch(); return makeScrollArea(panel);
}
QWidget* Window::colorPage() {
    auto* page = new QWidget; auto* l = new QVBoxLayout(page); l->addWidget(heading("BASIS-FARBKORREKTUR")); l->addWidget(help("Videoclip auswählen und Bild anpassen. Die Vorschau zeigt eine Näherung; der Export verwendet FFmpeg eq."));
    auto* form = new QFormLayout; brightness_ = number(-1,1,0.05); contrast_ = number(0,2,0.05); saturation_ = number(0,2,0.05);
    form->addRow("Helligkeit",brightness_); form->addRow("Kontrast",contrast_); form->addRow("Sättigung",saturation_); l->addLayout(form);
    for (auto* spin : {brightness_,contrast_,saturation_}) connect(spin,&QDoubleSpinBox::editingFinished,this,[this] {
        changeClip("Farbkorrektur",[this](Clip& c) { c.brightness = brightness_->value(); c.contrast = contrast_->value(); c.saturation = saturation_->value(); });
    });
    l->addWidget(button("Farbe zurücksetzen",[this] { changeClip("Farbe zurücksetzen",[](Clip& c) { c.brightness = 0; c.contrast = c.saturation = 1; }); }));
    l->addStretch(); return page;
}
QWidget* Window::musicPage() {
    auto* page = new QWidget; auto* l = new QVBoxLayout(page); l->addWidget(heading("MUSIKSTUDIO"));
    l->addWidget(button("+ Neues Pattern (4 Takte)",[this] {
        int track = timeline_->selectedTrack();
        if (track < 0 || track >= document_.project().tracks.size() || document_.project().tracks[track].kind != TrackKind::Instrument) {
            track = -1; for (int i = 0; i < document_.project().tracks.size(); ++i) if (document_.project().tracks[i].kind == TrackKind::Instrument) { track = i; break; }
            if (track < 0) { addTrack(TrackKind::Instrument); track = int(document_.project().tracks.size())-1; }
        }
        addPattern(track,playback_.position());
    }));
    patternTitle_ = help("Kein Pattern geöffnet"); l->addWidget(patternTitle_);
    instrument_ = new QComboBox; instrument_->addItem("Synthesizer","synth"); instrument_->addItem("Bass","bass"); instrument_->addItem("Drum-Machine","drums"); l->addWidget(instrument_);
    connect(instrument_,&QComboBox::activated,this,[this] {
        const auto id = timeline_->selected(); document_.change("Instrument wechseln",[&](Project& p) { auto* track = p.trackForClip(id); if (track && track->kind == TrackKind::Instrument) track->instrument = instrument_->currentData().toString(); });
    });
    auto* openPiano = button("Piano-Roll öffnen",[this] { const auto id = timeline_->selected(); if (document_.project().findClip(id)) editPattern(id); }); l->addWidget(openPiano);
    auto* mixerPanel = new QWidget; mixer_ = new QVBoxLayout(mixerPanel); mixer_->setContentsMargins(0,0,0,0); l->addWidget(heading("MIXER")); l->addWidget(makeScrollArea(mixerPanel),1);
    l->addWidget(help("Panorama wird für Instrumente live und für alle Spuren im Export angewandt. Medienvorschau: Pegel auf 100 % begrenzt, ohne Panorama."));
    return page;
}
QWidget* Window::exportPage() {
    auto* page = new QWidget; auto* l = new QVBoxLayout(page); l->addWidget(heading("EXPORT & RENDER-QUEUE"));
    preset_ = new QComboBox; preset_->addItems({"YouTube · 1920 × 1080","TikTok / Reels · 1080 × 1920","Quadratisch · 1080 × 1080","Audio · WAV 24-bit / 48 kHz"}); l->addWidget(preset_);
    encoder_ = new QComboBox; encoder_->addItem("H.264 · Software (libx264)","libx264"); encoder_->addItem("H.264 · NVIDIA NVENC","h264_nvenc"); encoder_->addItem("H.264 · Intel QSV","h264_qsv"); encoder_->addItem("H.264 · AMD AMF","h264_amf"); encoder_->addItem("H.264 · Apple VideoToolbox","h264_videotoolbox"); l->addWidget(encoder_);
    l->addWidget(help("Der gewählte Encoder muss in deiner FFmpeg-Installation verfügbar sein. Das gesamte Arrangement inklusive Musik wird exportiert. Bildformat: einpassen mit schwarzen Rändern."));
    auto* render = button("Zur Render-Queue hinzufügen",[this] { enqueueExport(); }); render->setObjectName("primary"); l->addWidget(render);
    progress_ = new QProgressBar; progress_->setValue(0); progress_->setFormat("Bereit"); l->addWidget(progress_); jobs_ = new QListWidget; l->addWidget(jobs_,1);
    l->addWidget(button("Abbrechen & Queue leeren",[this] { renders_.cancel(); })); return page;
}
void Window::refresh() {
    refreshing_ = true;
    const auto& project = document_.project(); const bool dirty = !document_.history()->isClean();
    setWindowTitle(project.name+(dirty ? " *" : "")+" — LoggX Studio"); title_->setText(project.name+(dirty ? "  ·  Ungespeichert" : "")); tempo_->setValue(project.tempo);
    const QString selected = media_->currentItem() ? media_->currentItem()->data(Qt::UserRole).toString() : QString();
    media_->clear();
    for (const auto& m : project.media) {
        const bool exists = QFileInfo::exists(m.path);
        auto* item = new QListWidgetItem((exists ? (m.video ? "VIDEO  " : "AUDIO  ") : "FEHLT  ")+m.name+"\n"+timecode(m.duration,project.fps),media_);
        item->setData(Qt::UserRole,m.id); item->setToolTip(m.path); item->setHidden(!m.name.contains(search_->text(),Qt::CaseInsensitive));
        if (m.id == selected) media_->setCurrentItem(item);
    }
    refreshing_ = false; refreshInspector(); refreshMixer();
}
void Window::refreshInspector() {
    if (!timeline_) return;
    refreshing_ = true;
    const auto* c = document_.project().findClip(timeline_->selected());
    const Track* track = nullptr;
    for (const auto& t : document_.project().tracks) for (const auto& clip : t.clips) if (clip.id == timeline_->selected()) track = &t;
    selectionTitle_->setText(c ? c->name : "Kein Clip ausgewählt");
    for (auto* spin : {start_,duration_,sourceIn_,gain_,fadeIn_,fadeOut_}) spin->setEnabled(c != nullptr);
    for (auto* spin : {brightness_,contrast_,saturation_}) spin->setEnabled(track && track->kind == TrackKind::Video);
    if (c) {
        start_->setValue(c->start); duration_->setValue(c->duration); sourceIn_->setValue(c->sourceIn); gain_->setValue(c->gain);
        fadeIn_->setValue(c->fadeIn); fadeOut_->setValue(c->fadeOut); brightness_->setValue(c->brightness); contrast_->setValue(c->contrast); saturation_->setValue(c->saturation);
    }
    instrument_->setEnabled(track && track->kind == TrackKind::Instrument);
    if (track && track->kind == TrackKind::Instrument) { instrument_->setCurrentIndex(instrument_->findData(track->instrument)); patternTitle_->setText(c->name); }
    refreshing_ = false;
}
void Window::refreshMixer() {
    while (auto* item = mixer_->takeAt(0)) { if (item->widget()) item->widget()->deleteLater(); delete item; }
    for (const auto& t : document_.project().tracks) {
        const auto id = t.id;
        auto* box = new QGroupBox(t.name); auto* l = new QFormLayout(box);
        auto* volume = new QSlider(Qt::Horizontal); volume->setRange(0,200); volume->setValue(qRound(t.gain*100)); volume->setToolTip("Lautstärke in Prozent"); l->addRow(QString::number(qRound(t.gain*100))+" %",volume);
        auto* pan = new QSlider(Qt::Horizontal); pan->setRange(-100,100); pan->setValue(qRound(t.pan*100)); pan->setToolTip("Links / Rechts"); l->addRow("Pan",pan);
        connect(volume,&QSlider::sliderReleased,this,[this,id,volume] { document_.change("Mixer-Pegel",[&](Project& p) { for (auto& track : p.tracks) if (track.id == id) track.gain = volume->value()/100.0; }); });
        connect(pan,&QSlider::sliderReleased,this,[this,id,pan] { document_.change("Panorama",[&](Project& p) { for (auto& track : p.tracks) if (track.id == id) track.pan = pan->value()/100.0; }); });
        auto* controls = new QWidget; auto* hl = new QHBoxLayout(controls); hl->setContentsMargins(0,0,0,0);
        auto* mute = new QCheckBox("Mute"); mute->setChecked(t.mute); auto* solo = new QCheckBox("Solo"); solo->setChecked(t.solo); hl->addWidget(mute); hl->addWidget(solo); l->addRow(controls);
        connect(mute,&QCheckBox::toggled,this,[this,id](bool on) { document_.change("Mute",[&](Project& p) { for (auto& track : p.tracks) if (track.id == id) track.mute = on; }); });
        connect(solo,&QCheckBox::toggled,this,[this,id](bool on) { document_.change("Solo",[&](Project& p) { for (auto& track : p.tracks) if (track.id == id) track.solo = on; }); });
        mixer_->addWidget(box);
    }
    mixer_->addStretch();
}
void Window::changeClip(const QString& name, const std::function<void(Clip&)>& edit) {
    if (refreshing_) return;
    const auto id = timeline_->selected(); document_.change(name,[&](Project& p) { if (auto* c = p.findClip(id)) edit(*c); });
}
void Window::showFrame(const QImage& image) {
    lastFrame_ = image;
    if (image.isNull()) { preview_->setPixmap(QPixmap()); preview_->setText("LOGGX STUDIO\n\nKein Videoclip an dieser Position"); }
    else preview_->setPixmap(QPixmap::fromImage(image).scaled(preview_->size()-QSize(6,6),Qt::KeepAspectRatio,Qt::SmoothTransformation));
}
void Window::resizeEvent(QResizeEvent* e) { QMainWindow::resizeEvent(e); if (preview_ && !lastFrame_.isNull()) showFrame(lastFrame_); }
void Window::importFiles(QStringList paths) {
    if (importer_.busy()) { statusBar()->showMessage("Medienimport läuft bereits …",5000); return; }
    if (paths.isEmpty()) paths = QFileDialog::getOpenFileNames(this,"Medien importieren",{},"Medien (*.mp4 *.mov *.mkv *.webm *.m4v *.avi *.wav *.aiff *.aif *.flac *.mp3 *.m4a *.ogg);;Alle Dateien (*)");
    if (paths.isEmpty()) return;
    tabs_->setCurrentIndex(0); statusBar()->showMessage("Medien werden geprüft …"); importer_.importFiles(paths);
}
void Window::addMedia(const QString& id) {
    const auto* m = document_.project().findMedia(id); if (!m) return;
    const Media media = *m; const auto kind = m->video ? TrackKind::Video : TrackKind::Audio;
    QString newClip;
    document_.change("Clip einfügen",[&](Project& p) {
        int lane = timeline_->selectedTrack();
        if (lane < 0 || lane >= p.tracks.size() || p.tracks[lane].kind != kind) {
            lane = -1; for (int i = 0; i < p.tracks.size(); ++i) if (p.tracks[i].kind == kind) { lane = i; break; }
            if (lane < 0) { Track track; track.kind = kind; track.name = kind == TrackKind::Video ? "Video" : "Audio"; p.tracks.append(track); lane = int(p.tracks.size())-1; }
        }
        Clip c; c.mediaId = media.id; c.name = media.name; c.duration = media.duration; c.start = playback_.position(); newClip = c.id; p.tracks[lane].clips.append(c);
    });
    timeline_->setSelected(newClip); tabs_->setCurrentIndex(1);
}
void Window::addTrack(TrackKind kind) {
    document_.change("Spur hinzufügen",[&](Project& p) { Track t; t.kind = kind; t.name = (kind == TrackKind::Video ? "Video " : kind == TrackKind::Audio ? "Audio " : "Instrument ")+QString::number(p.tracks.size()+1); p.tracks.append(t); });
}
void Window::addPattern(int track, double at) {
    QString id;
    document_.change("Pattern erstellen",[&](Project& p) { if (track < 0 || track >= p.tracks.size() || p.tracks[track].kind != TrackKind::Instrument) return; Clip c; c.name = "Neues Pattern"; c.start = at; c.duration = 16*60/p.tempo; id = c.id; p.tracks[track].clips.append(c); });
    if (!id.isEmpty()) { timeline_->setSelected(id); editPattern(id); }
}
void Window::editPattern(const QString& id) {
    bool valid = false; for (const auto& t : document_.project().tracks) if (t.kind == TrackKind::Instrument) for (const auto& c : t.clips) if (c.id == id) valid = true;
    if (!valid) return;
    timeline_->setSelected(id); tabs_->setCurrentIndex(3);
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle("Piano-Roll · "+document_.project().findClip(id)->name); dialog->resize(1000,650);
    auto* l = new QVBoxLayout(dialog); l->addWidget(help("Klick: Note setzen · Ziehen: verschieben · Rechter Rand: Länge · Rechtsklick: löschen · Raster: 1/16\nDrums: C = Kick, C# = Snare, D = Hi-Hat (Muster wiederholt sich alle 3 Halbtöne)."));
    auto* piano = new PianoRoll(&document_); piano->setClip(id); auto* area = makeScrollArea(piano,false); l->addWidget(area,1);
    auto* controls = new QHBoxLayout; controls->addWidget(button("Wiedergabe / Pause",[this] { playback_.toggle(); })); controls->addWidget(help("Neue Noten · Velocity"));
    auto* velocity = new QSlider(Qt::Horizontal); velocity->setRange(1,100); velocity->setValue(80); controls->addWidget(velocity); l->addLayout(controls);
    connect(velocity,&QSlider::valueChanged,piano,[piano](int value) { piano->setVelocity(value/100.0); });
    dialog->show(); area->verticalScrollBar()->setValue(20*17);
}
bool Window::save(bool choose) {
    QString destination = path_;
    if (destination.isEmpty() || choose) destination = QFileDialog::getSaveFileName(this,"Projekt speichern",path_.isEmpty() ? "Mein-Projekt.loggx" : path_,"LoggX-Projekt (*.loggx)");
    if (destination.isEmpty()) return false;
    if (!destination.endsWith(".loggx",Qt::CaseInsensitive)) destination += ".loggx";
    try { saveProject(document_.project(),destination); path_ = destination; document_.history()->setClean(); QFile::remove(recoveryPath()); statusBar()->showMessage("Projekt gespeichert",5000); refresh(); return true; }
    catch (const std::exception& e) { QMessageBox::critical(this,"Speichern fehlgeschlagen",QString::fromUtf8(e.what())); return false; }
}
bool Window::confirmDiscard() {
    if (document_.history()->isClean()) return true;
    const auto answer = QMessageBox::question(this,"Ungespeicherte Änderungen","Änderungen am aktuellen Projekt speichern?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if (answer == QMessageBox::Save) return save();
    return answer == QMessageBox::Discard;
}
void Window::newProject() {
    if (importer_.busy()) { QMessageBox::information(this,"Import läuft","Bitte den laufenden Medienimport abwarten."); return; }
    if (!confirmDiscard()) return;
    playback_.pause(); QFile::remove(recoveryPath()); path_.clear(); document_.replace(Project::blank());
}
void Window::openPath(const QString& path) {
    if (importer_.busy()) { QMessageBox::information(this,"Import läuft","Bitte den laufenden Medienimport abwarten."); return; }
    try {
        Project p = loadProject(path); if (!confirmDiscard()) return; playback_.pause();
        path_ = QFileInfo(path).absoluteFilePath(); document_.replace(std::move(p)); playback_.seek(0);
        int missing = 0; for (const auto& m : document_.project().media) if (!QFileInfo::exists(m.path)) ++missing;
        if (missing) QMessageBox::warning(this,"Medien fehlen",QString::number(missing)+" Quelldateien fehlen. Wähle sie im Medienpool und verknüpfe sie über Datei → Fehlendes Medium neu verknüpfen.");
    } catch (const std::exception& e) { QMessageBox::critical(this,"Projekt nicht geöffnet",QString::fromUtf8(e.what())); }
}
void Window::settings() {
    QDialog dialog(this); dialog.setWindowTitle("FFmpeg-Werkzeuge"); auto* l = new QVBoxLayout(&dialog);
    l->addWidget(help("FFmpeg und FFprobe werden für Import und Export benötigt. Wähle vorhandene Programme oder füge ihren Ordner zum PATH hinzu. Das Windows-Paket findet die mitgelieferten Programme automatisch."));
    auto* form = new QFormLayout; auto* ffmpeg = new QLineEdit(findTool("ffmpeg")); auto* ffprobe = new QLineEdit(findTool("ffprobe"));
    for (const auto& pair : {qMakePair(QString("ffmpeg"),ffmpeg),qMakePair(QString("ffprobe"),ffprobe)}) {
        auto* row = new QWidget; auto* h = new QHBoxLayout(row); h->setContentsMargins(0,0,0,0); h->addWidget(pair.second);
        h->addWidget(button("…",[&,field=pair.second] { const auto f = QFileDialog::getOpenFileName(&dialog,"Programm auswählen"); if (!f.isEmpty()) field->setText(f); })); form->addRow(pair.first,row);
    }
    l->addLayout(form); auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); l->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.resize(680,240);
    if (dialog.exec() == QDialog::Accepted) {
        for (const auto& path : {ffmpeg->text(),ffprobe->text()}) if (!path.isEmpty() && !QFileInfo(path).isExecutable()) { QMessageBox::warning(this,"Pfad ungültig","Die ausgewählte Programmdatei ist nicht ausführbar."); return; }
        QSettings prefs; prefs.setValue("tools/ffmpeg",ffmpeg->text()); prefs.setValue("tools/ffprobe",ffprobe->text());
    }
}
void Window::enqueueExport() {
    if (document_.project().duration() <= 0) { QMessageBox::information(this,"Export","Füge zuerst Clips oder ein Musik-Pattern zur Timeline hinzu."); return; }
    ExportSettings s; const int preset = preset_->currentIndex(); s.audioOnly = preset == 3; s.fps = std::min(document_.project().fps,60); s.encoder = encoder_->currentData().toString();
    if (preset == 1) { s.width = 1080; s.height = 1920; } else if (preset == 2) s.width = s.height = 1080;
    QString path = QFileDialog::getSaveFileName(this,"Exportziel",s.audioOnly ? "LoggX-Mix.wav" : "LoggX-Video.mp4",s.audioOnly ? "WAV (*.wav)" : "MP4 (*.mp4)"); if (path.isEmpty()) return;
    const QString extension = s.audioOnly ? ".wav" : ".mp4"; if (!path.endsWith(extension,Qt::CaseInsensitive)) path += extension;
    if (QFileInfo::exists(path)) { QMessageBox::information(this,"Datei existiert","Bitte einen neuen Dateinamen wählen. Bestehende Exporte werden geschützt."); return; }
    jobs_->addItem("WARTET  "+QFileInfo(path).fileName()); renders_.enqueue({document_.project(),s,path});
}
QString Window::recoveryPath() const {
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); QDir().mkpath(dir);
    return dir+"/recovery.autosave.loggx";
}
void Window::autosave() {
    if (document_.history()->isClean()) return;
    try { saveProject(document_.project(),recoveryPath()); statusBar()->showMessage("Wiederherstellungskopie gespeichert",2500); }
    catch (const std::exception& e) { statusBar()->showMessage("Autosave fehlgeschlagen: "+QString::fromUtf8(e.what()),10000); }
}
void Window::recover() {
    if (!QFileInfo::exists(recoveryPath())) return;
    const auto answer = QMessageBox::question(this,"Projekt wiederherstellen","Eine automatische Sicherung wurde gefunden. Wiederherstellen?",QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        try { document_.replace(loadProject(recoveryPath())); document_.history()->resetClean(); path_.clear(); }
        catch (const std::exception& e) { QMessageBox::warning(this,"Wiederherstellung fehlgeschlagen",QString::fromUtf8(e.what())); }
    }
}
void Window::closeEvent(QCloseEvent* e) {
    if (renders_.busy()) {
        if (QMessageBox::question(this,"Export läuft","Export abbrechen und LoggX schließen?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No) != QMessageBox::Yes) { e->ignore(); return; }
    }
    if (!confirmDiscard()) { e->ignore(); return; }
    renders_.cancel(); playback_.pause(); QFile::remove(recoveryPath()); e->accept();
}
void Window::dragEnterEvent(QDragEnterEvent* e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void Window::dropEvent(QDropEvent* e) {
    QStringList files; for (const auto& url : e->mimeData()->urls()) if (url.isLocalFile()) files << url.toLocalFile();
    if (files.size() == 1 && files[0].endsWith(".loggx",Qt::CaseInsensitive)) openPath(files[0]); else if (!files.isEmpty()) importFiles(files);
}
}
