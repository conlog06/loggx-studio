# Abhängigkeiten und Distribution

| Komponente | Verwendung | Distribution in diesem Repository |
| --- | --- | --- |
| Qt 6.8.3 | Native Oberfläche, Multimedia, Thread-Pool, JSON | Dynamisch gelinkt; CI-Deployment kopiert Runtime-Dateien |
| FFmpeg / FFprobe | Metadaten und Offline-Export | Im Windows-x64-Paket enthalten, auf macOS/Linux separat installiert |
| CMake / C++20 | Build | Quellcode-Build und CI |

Qt: https://www.qt.io/licensing/ und die Lizenztexte der konkret eingesetzten Distribution.
Die unveränderten LGPL-3.0- und GPL-3.0-Lizenztexte aus Qt 6.8.3 befinden sich in `docs/licenses/`. Diese Texte gehören zu den Abhängigkeiten und legen keine Lizenz für die LoggX-Quellen fest. Upstream-Quellen: https://download.qt.io/archive/qt/6.8/6.8.3/single/ und https://code.qt.io/qt/ .
FFmpeg: https://ffmpeg.org/legal.html und `ffmpeg -buildconf` für den lokalen Build.
`libx264` ist nur verfügbar, wenn der installierte FFmpeg-Build es enthält. Die Lizenzbedingungen eines externen Encoder-Builds sind nicht automatisch die der LoggX-Quellen.

Vor einer öffentlichen Produktdistribution müssen die passenden Lizenztexte und Hinweise für alle mitgelieferten Qt-/Codec-Bibliotheken, die Lizenzwahl für LoggX, Code-Signing und Apple-Notarisierung vervollständigt werden. Die CI-Artefakte dienen zunächst zur technischen Erprobung.

Die CI baut die Windows-Werkzeuge aus geprüften Quellen mit `scripts/build-ffmpeg-windows.sh`. Die zugehörigen Quellen, GPL-Lizenztexte und das Build-Skript liegen im ZIP unter `docs/ffmpeg/`.
