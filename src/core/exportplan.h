#pragma once
#include "project.h"
#include <QStringList>

namespace loggx {
struct ExportSettings {
    int width = 1920, height = 1080, fps = 30;
    bool audioOnly = false;
    QString encoder = QStringLiteral("libx264");
};
struct ExportPlan { QStringList arguments; QString graph; };
bool hasInstruments(const Project& project);
ExportPlan makeExportPlan(const Project&, const ExportSettings&, const QString& destination,
                          const QString& instrumentWav = {});
}
