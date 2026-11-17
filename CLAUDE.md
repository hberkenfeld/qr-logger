# Hinweise für KI-Assistenten

- Sprache der Kommunikation und Doku: Deutsch; Code-Bezeichner und Kommentare englisch.
- Mindestbasis Debian 12: Qt 6.4.2, GCC 12, CMake 3.25. Keine APIs neuer als Qt 6.4 ohne Versions-Fallback, kein `std::format`.
- Jede Quelldatei beginnt mit `// SPDX-License-Identifier: GPL-3.0-or-later`.
- Namespace `qrl`. `QT_NO_KEYWORDS` ist aktiv → `Q_SIGNALS`, `Q_SLOTS`, `Q_EMIT` verwenden.
- Code aus AetherSDR (GPL-3) nur mit Quellenangabe im Dateikopf übernehmen.
- Grundsatzentscheidungen: `docs/entscheidungen.md`.
