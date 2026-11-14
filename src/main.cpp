#include "ui/window.h"
#include <QApplication>
#include <QFont>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("LoggX");
    QCoreApplication::setApplicationName("LoggX Studio");
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI",10));
    loggx::Window window; window.show();
    if (app.arguments().size() > 1) window.openPath(app.arguments().at(1));
    return app.exec();
}
