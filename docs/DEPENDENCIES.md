# Abhängigkeiten und Distribution

| Komponente | Verwendung | Distribution in diesem Repository |
| --- | --- | --- |
| Qt 6.8.3 | Native Oberfläche, Multimedia, Thread-Pool, JSON | Dynamisch gelinkt; CI-Deployment kopiert Runtime-Dateien |
| FFmpeg / FFprobe | Metadaten und Offline-Export | Separat vom Nutzer installiert, keine Binärdateien im Repository |
| CMake / C++20 | Build | Quellcode-Build und CI |

Qt: https://www.qt.io/licensing/ und die Lizenztexte der konkret eingesetzten Distribution.
FFmpeg: https://ffmpeg.org/legal.html und `ffmpeg -buildconf` für den lokalen Build.
`libx264` ist nur verfügbar, wenn der installierte FFmpeg-Build es enthält. Die Lizenzbedingungen eines externen Encoder-Builds sind nicht automatisch die der LoggX-Quellen.

Vor einer öffentlichen Produktdistribution müssen die passenden Lizenztexte und Hinweise für alle mitgelieferten Qt-/Codec-Bibliotheken, die Lizenzwahl für LoggX, Code-Signing und Apple-Notarisierung vervollständigt werden. Die CI-Artefakte dienen zunächst zur technischen Erprobung.
