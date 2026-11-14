#include "audio.h"
#include <QDataStream>
#include <QSaveFile>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace loggx {
namespace {
constexpr double tau = 2 * std::numbers::pi;
double voice(const QString& type, const Note& n, double t, double length) {
    if (t < 0 || t >= length) return 0;
    const double f = 440 * std::pow(2.0,(n.pitch-69)/12.0);
    const double attack = std::min(1.0,t/0.008);
    const double release = std::min(1.0,(length-t)/0.035);
    double sample;
    if (type == "drums") {
        if (n.pitch % 3 == 0) sample = std::sin(tau*(48*t+5*(1-std::exp(-25*t))))*std::exp(-9*t);
        else {
            const double raw = std::sin(std::floor(t*48000)*12.9898+n.pitch*78.233)*43758.5453;
            const double noise = 2*(raw-std::floor(raw))-1;
            sample = noise * std::exp(-(n.pitch%3 == 1 ? 17.0 : 45.0)*t);
        }
    } else if (type == "bass") {
        sample = (std::sin(tau*f*t)+0.25*std::sin(tau*2*f*t)) * std::exp(-1.2*t);
    } else {
        sample = (std::sin(tau*f*t)+0.22*std::sin(tau*2*f*t)+0.1*std::sin(tau*3*f*t))*std::exp(-0.7*t);
    }
    return sample*attack*release*n.velocity*0.22;
}
}
double clipEnvelope(const Clip& c, double local) {
    if (local < 0 || local >= c.duration) return 0;
    double v = c.gain;
    if (c.fadeIn > 0) v *= std::min(1.0,local/c.fadeIn);
    if (c.fadeOut > 0) v *= std::min(1.0,(c.duration-local)/c.fadeOut);
    return v;
}
Stereo instrumentSample(const Project& project, double time) {
    double left = 0, right = 0;
    for (const auto& track : project.tracks) {
        if (track.kind != TrackKind::Instrument || !project.audible(track)) continue;
        double value = 0;
        for (const auto& c : track.clips) {
            const double local = time-c.start;
            if (local < 0 || local >= c.duration) continue;
            double clip = 0;
            for (const auto& n : c.notes) clip += voice(track.instrument,n,
                local+c.sourceIn-n.beat*60/project.tempo,n.length*60/project.tempo);
            value += clip*clipEnvelope(c,local);
        }
        left += value*track.gain*std::sqrt((1-track.pan)/2);
        right += value*track.gain*std::sqrt((1+track.pan)/2);
    }
    return {float(std::clamp(left,-1.0,1.0)),float(std::clamp(right,-1.0,1.0))};
}
void writeInstrumentWav(const Project& p, const QString& path, const std::atomic_bool* cancelled,
        const std::function<void(double)>& progress) {
    p.validate();
    constexpr int rate = 48000;
    const auto frames = qint64(std::ceil(p.duration()*rate));
    if (frames < 1 || frames*4 > 0xffffffffLL-36) throw std::runtime_error("WAV-Dauer überschreitet das RIFF-Limit.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) throw std::runtime_error(file.errorString().toStdString());
    QDataStream out(&file); out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF",4); out << quint32(36+frames*4); out.writeRawData("WAVEfmt ",8);
    out << quint32(16) << quint16(1) << quint16(2) << quint32(rate) << quint32(rate*4) << quint16(4) << quint16(16);
    out.writeRawData("data",4); out << quint32(frames*4);
    QByteArray block; block.resize(4096*4);
    for (qint64 first = 0; first < frames; first += 4096) {
        if (cancelled && cancelled->load()) { file.cancelWriting(); throw std::runtime_error("Export abgebrochen."); }
        const int count = int(std::min<qint64>(4096,frames-first));
        for (int i = 0; i < count; ++i) {
            const auto s = instrumentSample(p,double(first+i)/rate);
            const qint16 values[2] = {qint16(std::round(s.left*32767)),qint16(std::round(s.right*32767))};
            for (int ch = 0; ch < 2; ++ch) {
                const quint16 v = quint16(values[ch]); block[i*4+ch*2] = char(v&255); block[i*4+ch*2+1] = char(v>>8);
            }
        }
        if (file.write(block.constData(),count*4) != count*4) throw std::runtime_error(file.errorString().toStdString());
        if (progress) progress(double(first+count)/double(frames));
    }
    if (!file.commit()) throw std::runtime_error(file.errorString().toStdString());
}
}
