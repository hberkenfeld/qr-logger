# QR-Logger – Grundsatzentscheidungen

Stand: 2026-11-17

| Thema | Entscheidung | Begründung |
|---|---|---|
| Sprache | C++20 | Echtzeit (CW/RTTY/PTT/Voice-Keyer), kein Garbage Collector |
| Framework | Qt 6 (Core, Gui, Widgets, Network, Multimedia, SerialPort, Sql) | SmartSDR TCP/UDP, Audio, GUI, SQLite aus einer Hand |
| Build | CMake ≥ 3.25, Ninja | Debian 12 liefert CMake 3.25 |
| Mindest-Plattform | Debian 12 / Ubuntu 24.04 | – |
| Qt-Mindestversion | 6.4.2 | Distro-Paket auf beiden Zielsystemen; neuere Qt-Features nur mit Fallback |
| Compiler | GCC 12 (Debian 12) / GCC 13 (Ubuntu 24.04) | C++20; **kein `std::format`** (erst ab GCC 13) → `QString::arg` |
| Lizenz | GPL-3.0-or-later | Frei; kompatibel mit AetherSDR (GPL-3) |
| SmartSDR-Referenz | [AetherSDR](https://github.com/aethersdr/AetherSDR) | Übernahme von Code mit Quellenangabe zulässig |
| Erstes Radio | FlexRadio über SmartSDR-API | – |
| Betriebsarten | SSB (mit Voice-Keyer/Rekorder), CW, RTTY | – |
| Bedienung | Tastaturbelegung/Funktionen angelehnt an DXLog | – |
| Stationsbetrieb | SO1R, SO2R/SO2V, SO3R/SO3V von Anfang an eingeplant | – |
