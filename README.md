# QR-Logger

Contest-Logger für Linux mit direkter Anbindung an FlexRadio-Transceiver über die SmartSDR-API.

*Contest logger for Linux with native FlexRadio (SmartSDR API) integration – SSB, CW, RTTY, SO2R/SO3R.*

> **Status:** frühes Entwicklungsstadium – noch nicht für den Contestbetrieb geeignet.

## Ziele

- **Radio:** FlexRadio FLEX-6000/8000/Aurora über die SmartSDR-API (TCP/UDP, VITA-49)
- **Betriebsarten:** SSB, CW, RTTY
  - CW- und RTTY-Ausgabe direkt über das Radio (SmartSDR-API)
  - SSB-Sprachspeicher mit einfach bedienbarem Audiorekorder
- **Bedienung:** Tastaturbelegung und Funktionen angelehnt an DXLog
- **Stationsbetrieb:** SO1R, SO2R/SO2V, SO3R/SO3V
- **Plattform:** Debian 12 / Ubuntu 24.04 und neuer

## Bauen

Abhängigkeiten (Debian 12 / Ubuntu 24.04):

```sh
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev qt6-multimedia-dev qt6-serialport-dev libqt6sql6-sqlite
```

Übersetzen:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
./build/qr-logger
```

## Werkzeuge

**`qrl-flexprobe`** – Verbindungstest ohne GUI: sucht Radios im Netz, verbindet, bindet sich an den GUI-Client (AetherSDR/Maestro) und zeigt GUI-Clients und Slices.

```sh
./build/qrl-flexprobe                      # erstes gefundenes Radio
./build/qrl-flexprobe --ip 192.168.1.50    # feste IP
./build/qrl-flexprobe --discover-only      # nur Radios auflisten
```

**`tools/fake_flex.py`** – einfacher Radio-Emulator für die Entwicklung ohne Radio (`python3 tools/fake_flex.py`).

## Technik

C++20, Qt 6 (ab 6.4.2), CMake.

- [Grundsatzentscheidungen](docs/entscheidungen.md)
- [Architektur](docs/architektur.md)
- [Tastenbelegung](docs/tastenbelegung.md)

## Danksagung

Das Projekt [AetherSDR](https://github.com/aethersdr/AetherSDR) (GPL-3.0) dient als Referenz für das SmartSDR-Protokoll.

## Lizenz

GNU General Public License v3.0 oder neuer – siehe [LICENSE](LICENSE).

QR-Logger ist ein unabhängiges Projekt und steht in keiner Verbindung zu FlexRadio Systems.
