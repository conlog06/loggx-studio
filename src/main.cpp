#include "ui/window.h"
#include <QApplication>
#include <QFont>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QPixmap>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("LoggX");
    QCoreApplication::setApplicationName("LoggX Studio");
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setStyle(QStyleFactory::create("Fusion"));
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
