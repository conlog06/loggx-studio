# Umsetzung des LoggX-Konzepts

## 0.1 – implementierter technischer Prototyp

Native C++/Qt-Desktop-App, Import, gemeinsame Timeline, grundlegende Schnitte, Medienvorschau, eigene Basissynthese, Piano-Roll, Mixerparameter, Farbkorrektur, Projektformat, Autosave und Render-Queue bis 1080p. Die CI baut drei Betriebssysteme. Der ursprüngliche Performance-Meilenstein „flüssige 4K-Vorschau“ ist damit **noch nicht nachgewiesen**.

Die Oberfläche verwendet Qt Widgets statt QML. Die Audioausgabe nutzt vorläufig Qt Multimedia statt JUCE. Diese Abweichungen ermöglichen eine überschaubare, native erste Implementierung; die Schnittstellen zwischen Modell, Audio, Transport und Oberfläche sind getrennt. Synth/Bass/Drums sind die drei Basisinstrumente; der im MVP geplante Sampler fehlt noch.

## Nächster Meilenstein – belastbare Wiedergabe

1. Gemeinsame samplegenaue Transportuhr und Audio-Mixer, danach JUCE-Backend mit Geräteeinstellungen.
2. Asynchroner Videodecoder, Pre-Roll, Frame-Cache und Proxy-Workflow; 4K-Benchmark auf Windows und Apple Silicon mit dokumentierten Testmedien.
3. GPU-Compositing (Metal/DirectX/Vulkan), lineare 16-bit-Float-Farbpipeline und Übereinstimmung von Vorschau und Export.
4. Medien-Relinking mit Metadatenprüfung, mehrere Autosave-Versionen, Projektpakete und Mehrinstanzschutz.
5. Signierte Installer und auf echten Geräten geprüfte A/V-Synchronisation.

## MVP

Basis-Masken, Übergänge, Keyframes, LUFS/Normalisierung, Ducking, Sampler, Audioeffekte, MIDI-I/O, Pitch/Time-Stretch, LUTs, Proxy-Verwaltung und 4K. Erst danach eine kontrollierte Beta.

## 1.0 und später

8K/10-bit/ProRes, HDR, Tracking, lokale ONNX-Modelle, Aufnahme, Plugin-Hosting und Multicam. Das Konto-/Lizenzbackend wird als eigener Dienst mit serverseitiger Rollenprüfung, MFA, Audit-Log und Tests implementiert. Im Desktop gespeicherte Rollen sind keine Autorisierungsquelle.

Diese Liste beschreibt offene Arbeit und ist keine Behauptung, dass die Funktionen bereits in der App vorhanden sind.
