#include "ui/window.h"
#include <QApplication>
#include <QFont>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QPixmap>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("LoggX");
    QCoreApplication::setApplicationName("LoggX Studio");
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window,QColor("#171a22"));
    palette.setColor(QPalette::WindowText,QColor("#dfe4ef"));
    palette.setColor(QPalette::Base,QColor("#111318"));
    palette.setColor(QPalette::AlternateBase,QColor("#242a36"));
    palette.setColor(QPalette::Text,QColor("#dfe4ef"));
    palette.setColor(QPalette::Button,QColor("#2a2f3b"));
    palette.setColor(QPalette::ButtonText,QColor("#dfe4ef"));
    palette.setColor(QPalette::Highlight,QColor("#9c75c9"));
    palette.setColor(QPalette::HighlightedText,QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText,QColor("#929caf"));
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor("#737d90"));
    palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor("#737d90"));
    app.setPalette(palette);
    app.setFont(QFont("Segoe UI",10));
    const bool smoke = app.arguments().size() >= 3 && app.arguments().at(1) == "--smoke-test";
    const auto data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data);
    QLockFile lock(data+"/studio.lock");
    if (!smoke && !lock.tryLock(0)) {
        QMessageBox::information(nullptr,"LoggX Studio","LoggX Studio läuft bereits. Bitte das bestehende Fenster verwenden.");
        return 1;
    }
    loggx::Window window; window.show();
    if (smoke) {
        if (app.arguments().size() > 3) window.openPath(app.arguments().at(3));
        QTimer::singleShot(1500,&window,[&] { app.exit(window.grab().save(app.arguments().at(2)) ? 0 : 2); });
    } else if (app.arguments().size() > 1) window.openPath(app.arguments().at(1));
    return app.exec();
}
