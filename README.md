# LoggX Studio

Native Video- und Musiksoftware für Windows und macOS. C++20, Qt 6 und FFmpeg.
Video, Audio und Instrumente teilen sich eine Timeline und eine `.loggx`-Projektdatei.

**Stand: 0.1.0 – technischer Prototyp, keine fertige 1.0/DAW.** Die Roadmap des Konzepts ist in [docs/ROADMAP.md](docs/ROADMAP.md) nachvollziehbar abgegrenzt.

## Was funktioniert

- Fünf Arbeitsbereiche: Medien, Schnitt, Farbe, Musik, Export; native dunkle Oberfläche.
- Lokaler Video-/Audioimport, Drag & Drop, Medienpool und Suche; Metadaten über FFprobe, Import im Hintergrund.
- Beliebig viele Video-, Audio- und Instrumentenspuren auf derselben Sekunden-Zeitachse.
- Clips verschieben, an den Rändern trimmen, am Abspielkopf teilen, duplizieren, löschen, Ripple-Löschen über alle Spuren.
- Frame- oder Beat-Snap, Tempo, Zoom, Quell-In, Clip-Pegel und Audio-Fades.
- Gemeinsame Wiedergabe mit Video-Vorschau und überlagerten Audiospuren.
- Synth, Bass und synthetische Drums; editierbare Piano-Roll mit 1/16-Raster, Notenlänge und Velocity für neue Noten.
- Mixer mit Lautstärke, Mute, Solo und Panorama; Panorama für Instrumente live und für alle Spuren beim Export.
- Helligkeit, Kontrast, Sättigung; annähernde Vorschau und FFmpeg-Farbkorrektur im Export.
- Validierte JSON-Projekte mit relativen Medienpfaden, atomarem Speichern, Undo/Redo, History und Wiederherstellungskopie alle 60 Sekunden.
- Hintergrund-Render-Queue mit Projekt-Snapshot, Fortschritt, Abbruch und geschützter Zieldatei.
- H.264/AAC bis 1080p (auch Hochformat) oder WAV 24-bit/48 kHz inklusive Instrumenten; Encoder-Auswahl für Software/NVENC/QSV/AMF/VideoToolbox.
- CMake, Tests und CI für Windows, macOS und Linux; die CI stellt portable Build-Artefakte bereit.

## Schnellstart

1. In GitHub unter **Actions → Build and test LoggX Studio** einen erfolgreichen Lauf öffnen und das passende `LoggX-Studio-*`-Artefakt herunterladen.
2. ZIP vollständig entpacken. Windows: `bin/LoggXStudio.exe`; macOS: `LoggXStudio.app`.
3. FFmpeg inklusive FFprobe separat installieren: [offizielle Downloadseite](https://ffmpeg.org/download.html). Unter **Datei → Einstellungen** beide ausführbaren Programme auswählen. Alternativ deren Ordner in den `PATH` aufnehmen.
4. **Importieren**, danach ein Medium im Pool doppelt anklicken. Clips landen an der Abspielposition.
5. Im Musik-Arbeitsbereich **Neues Pattern** wählen. Im Piano-Roll links klicken, um Noten zu setzen; rechts klicken zum Löschen. Ziehen verschiebt, der rechte Notenrand ändert die Länge.
6. `.loggx`-Projekt speichern und im Export-Arbeitsbereich eine neue Ausgabedatei wählen.

Die Desktop-Artefakte sind noch nicht signiert oder notarisiert. Ein Installationsprogramm und Release-Signaturen sind noch nicht eingerichtet. FFmpeg wird nicht mitgeliefert. Nur die Encoder, die der installierte FFmpeg-Build und die Hardware unterstützen, funktionieren.

## Selbst bauen

Voraussetzungen: C++20-Compiler (Visual Studio 2022, Xcode/Clang oder GCC), CMake ≥ 3.24, Qt **6.8.3** mit Core, Gui, Widgets, Multimedia, Concurrent und Test. `CMAKE_PREFIX_PATH` muss auf die Qt-Installation zeigen.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/platform
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix /absolute/path/to/dist
```

Windows mit Visual Studio: beispielsweise `-DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64`.
Die Tests nutzen kein Audiogerät. Der zusätzliche Integrationstest rendert echtes Video/Audio, sofern FFmpeg im PATH liegt (auf dem Linux-CI-Runner verpflichtend installiert).

## Tastatur

| Aktion | Windows / Linux |
| --- | --- |
| Projekt neu / öffnen / speichern | Strg+N / Strg+O / Strg+S |
| Import | Strg+I |
| Wiedergabe / Pause | Leertaste |
| Clip am Abspielkopf teilen | Strg+B |
| Clip duplizieren | Strg+D |
| Löschen / Ripple-Löschen | Entf / Umschalt+Entf |
| Undo / Redo | Strg+Z / Strg+Umschalt+Z |

Auf macOS verwenden die Standardaktionen die üblichen Command-Tasten.

## Grenzen dieses Prototyps

- Qt Multimedia übernimmt die Vorschau. Das ist noch keine framegenaue, GPU-basierte NLE-Engine und keine 4K/8K-Performance-Zusage. Videoframes werden für die Vorschau auf maximal 1280 Pixel Breite reduziert. Mehrere Decoder werden ungefähr auf die gemeinsame Uhr synchronisiert.
- Medien-Panorama und Pegel über 100 % sind nur im Export verfügbar. Instrumente werden live in einem separaten 48-kHz-Stereostream berechnet. Noch keine gemeinsame samplegenaue Audio-Hardware-Uhr oder ASIO/CoreAudio-DAW-Engine.
- Bildfarbe der CPU-Vorschau und FFmpeg `eq` können abweichen; kein farbverwalteter Referenzmonitor, keine Scopes, LUTs oder HDR.
- Musiknotenzeiten bleiben in Beats gespeichert; Clip-Start und Clip-Dauer bleiben bei Tempoänderungen in Sekunden. Inhalte außerhalb des Clips werden abgeschnitten.
- Quellmedien bleiben externe Dateien. Ein Projekt enthält keine Kopien. Neuverknüpfen erwartet dasselbe Originalmedium; die bisherige Dauer/Stream-Struktur bleibt erhalten.
- Keine Video-Überblendungen, Keyframes, Masken, Proxy-Generierung, MIDI-Dateien/Keyboard-Eingabe, Audioaufnahme, Sampler, Time-Stretch, VST/AU/CLAP oder KI.
- Kein Login, Lizenzsystem oder Admin-Backend. Lokal gibt es keinen unsicheren „Admin-Schalter“, der serverseitige Rechte vortäuscht.
- Eine Instanzsperre schützt die gemeinsame Autosave-Datei. Große Projekte und viele gleichzeitig laufende Instrumentenstimmen sind noch nicht optimiert.

## Aufbau

`src/core` enthält Projektmodell, Validierung, Schnittbefehle, Synthese und Exportplan. `src/engine` verbindet Medienprobe, Wiedergabe und Render-Queue mit Qt/FFmpeg. `src/ui` enthält die native Oberfläche, Timeline und Piano-Roll. Tests prüfen Datenintegrität, Schnittsemantik und echten Export.

Qt wird dynamisch gelinkt. Hinweise zu Abhängigkeiten stehen in [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md). Für dieses Repository wurde noch keine neue Projektlizenz festgelegt.
