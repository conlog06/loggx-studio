#pragma once
#include "project.h"
#include <atomic>
#include <functional>

namespace loggx {
struct Stereo { float left = 0, right = 0; };
double clipEnvelope(const Clip& c, double local);
Stereo instrumentSample(const Project& project, double time);
void writeInstrumentWav(const Project& p, const QString& path,
    const std::atomic_bool* cancelled = nullptr, const std::function<void(double)>& progress = {});
}
