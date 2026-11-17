# QR-Logger – Architektur (Entwurf v0.1)

Stand: 2026-11-17 · Status: zur Abstimmung

## 1. Leitlinien

- **Tastatur zuerst** – Bedienung und Belegung wie DXLog ([tastenbelegung.md](tastenbelegung.md)).
- **Mehrkanal von Anfang an** – alles, was zu „einem Radio“ gehört, hängt an einem *Kanal*. SO1R ist der Sonderfall mit einem Kanal.
- **Radio-Abstraktion** – Flex ist das erste Backend, die Kernlogik kennt nur Schnittstellen.
- **Echtzeit getrennt von GUI** – Netzwerk und Audio/DSP laufen in eigenen Threads; die GUI blockiert nie das Senden.
- **Contest-Regeln als Daten** – Contest-Definitionen als Dateien, keine Neukompilierung je Contest.

## 2. Modulübersicht

```
src/
  app/        Hauptfenster, Fensterverwaltung, Einstellungen-Dialoge
  ui/         Eingabezeilen, Logansicht, Bandmap, Check-Fenster, Rate
  input/      KeyMap, Aktionen (DXLog-Belegung), ESM-Zustandsmaschine
  station/    Kanäle (SO1R…SO3R/V), Fokus, TX-Interlock, Audio-Routing, OTRSP
  radio/      IRadio-Schnittstelle + Backends
    flex/     Discovery, TCP-API, Statusmodell, VITA-49-Streams
  tx/         Nachrichten-Engine (Makros/Variablen), CW-, RTTY-, Voice-Sender
  audio/      Voice-Keyer-Rekorder/-Player, Resampling, Pegel
  dsp/        RTTY-Modulator (AFSK), CW-/RTTY-Decoder, Skimmer (Kanalisierung, Spots)
  contest/    Contest-Definitionen, Exchange-Parser, Dupe, Multis, Punkte
  log/        SQLite-Speicher, Cabrillo-/ADIF-Export
  data/       cty.dat, MASTER.SCP, Call-History
  net/        DX-Cluster (Telnet), später Multi-Op-Netz
```

## 3. Radio-Anbindung FlexRadio

### 3.1 Verbindung
- **Discovery:** UDP 4992, VITA-49-Broadcasts der Radios (Liste im Verbinden-Dialog, manuelle IP als Alternative; SmartLink/WAN später).
- **Befehlskanal:** TCP 4992, Zeilenprotokoll
  - Radio → `V<version>`, `H<handle>`
  - Client → `C<seq>|<befehl>`; Antwort `R<seq>|<hex>|<text>` (0 = Erfolg)
  - Status `S<handle>|…`, Meldungen `M<nr>|…`
  - Quelle: [SmartSDR TCP/IP API](https://github.com/flexradio/smartsdr-api-docs/wiki/SmartSDR-TCPIP-API) (teilweise veraltet), aktuelle Details aus AetherSDR.
- **Rolle:** QR-Logger läuft als **Nicht-GUI-Client** neben einem GUI-Client (AetherSDR, SmartSDR, Maestro) und bindet sich per `client bind client_id=<GUI-Client-ID>` an diesen. Bei zwei GUI-Clients (MultiFlex) wählt der Nutzer, welchem er folgt.
- **Abonnements:** `sub slice all`, `sub tx all`, `sub radio all`, `sub interlock all`, … → internes Statusmodell (Slices, Frequenz, Mode, TX-Slice, Interlock-Zustand).

### 3.2 Senden je Betriebsart

| Mode | Weg | Befehle / Streams |
|---|---|---|
| CW | Radio-Keyer über **CWX** | `cwx send "<text>"`, `cwx wpm`, `cwx clear` (Abbruch), Fortschritt über `cwx sent=<idx>`; Leerzeichen als 0x7F |
| SSB | Audio aus dem PC über **DAX TX** | `stream create type=dax_tx`, `transmit set dax=1`, `xmit 1/0`; Audio 24 kHz Stereo float32 als VITA-49 per UDP |
| SSB (Alternative) | Radio-eigener **DVK** | `dvk rec_start/playback_start id=…` – nur wo das Radio DVK hat |
| RTTY | **AFSK** im Radio-Mode RTTY über DAX TX | eigener Baudot-Modulator (45,45 Bd / 170 Hz) → dax_tx-Stream |

Vor jedem Senden: TX-Slice des aktiven Kanals setzen (`slice set <n> tx=1`).

### 3.3 Empfang, Decoder und Skimmer

QR-Logger ersetzt SDC für Dekodierung und Skimming (CW **und** RTTY). Drei Ebenen, alle mit denselben Decoder-Kernen:

| Ebene | Quelle | Zweck |
|---|---|---|
| **Slice-Decoder** | `stream create type=dax_rx dax_channel=<n>` (24 kHz Audio) je Kanal | Volltext des eingestellten Signals im Decoder-Fenster |
| **Passband-Skimmer** | DAX-IQ um die Kanal-Frequenz (schmal, z. B. ±2 kHz) | alle Anrufer im Pileup gleichzeitig: Rufzeichen + WpM + Ablage → Anruferliste, Tempo-Anpassung |
| **Band-Skimmer** | `stream create type=dax_iq daxiq_channel=<n>` (bis 192 kHz je Panadapter) | ganzes Bandsegment → Spots in die Bandmap |

**Pipeline (Skimmer):** IQ → FFT-Spektrum → Signaldetektion (Peaks, Rauschboden) → je Signal Mischer + Dezimation → Decoder-Instanz (CW oder RTTY) → Rufzeichen-Extraktion → Spot.

- **Decoder-Kerne** hinter Schnittstellen `ICwDecoder` / `IRttyDecoder` (Text, WpM bzw. Baudrate/Shift, Pitch, Konfidenz).
  - **Anforderung:** Genauigkeit mindestens auf SDC-Niveau, Latenz < 1 s. ggmorse (AetherSDR) ist laut Praxiserfahrung zu ungenau.
  - **CW-Kandidaten:** (1) klassisch nach fldigi-Vorbild (GPL-3, Matched Filter, „Optimal ATC“ nach W7AY, adaptives Timing), (2) Bayes-Ansatz (wie CW Skimmer; libbmorse von AG1LE, Lizenz prüfen), (3) neuronal (DeepFist/DeepCW) nur bei passender Latenz/Lizenz, (4) ggmorse als Vergleich.
  - **RTTY:** Mark/Space-Filterbank mit ATC, automatische Shift-/Polaritätserkennung für den Skimmer. Referenz: `RttyDecoder` (AetherSDR, GPL-3).
- **Rufzeichen-Extraktion:** Muster + Plausibilität über cty.dat und MASTER.SCP; ein Spot erst nach mehrfacher übereinstimmender Dekodierung (wie bei Skimmern üblich), Konfidenzwert je Spot.
- **Bewertung:** Testkorpus aus realen Mitschnitten (DAX RX als WAV, DAX IQ als Rohdaten) mit SDC-Ausgabe bzw. Handkopie als Referenz; Zeichenfehlerrate, Rufzeichen-Trefferquote, Latenz und WpM-Genauigkeit automatisiert messen (`tests/decoder-bench`).
- **Last:** Decoder-Instanzen in einem Thread-Pool; maximale Anzahl gleichzeitiger Decoder konfigurierbar.
- **Zu klären:** DAX-IQ ist an einen Panadapter gebunden. Als gebundener Nicht-GUI-Client muss QR-Logger ggf. die Panadapter des GUI-Clients nutzen oder eigene anlegen dürfen – Verhalten der Firmware prüfen.
- **SSB/CW-Hören:** übernimmt der GUI-Client bzw. Kopfhörer am Radio; QR-Logger steuert nur Panorama/Mute je Slice (`audio_pan`, `audio_mute`).

## 4. Stationsmodell (SO1R … SO3R/V)

- **Kanal** = eine Eingabezeile + ein (Radio, Slice)-Paar + eigener Run/S&P-Zustand, eigene CQ-Frequenz, eigene Nachrichtenwarteschlange.
- **Fokus:** *Eingabefokus* (welche Zeile tippt) und *TX-Fokus* (welcher Kanal sendet) getrennt, wie DXLog.
- **Varianten**
  - **SO2V / SO3V:** ein Flex, 2–3 Slices. Hören über `audio_pan` (links/rechts/beide). Ein Sender → TX wechselt zwischen Slices.
  - **SO2R:** zwei Radios (zwei Flex oder ein Flex mit zwei SCUs, z. B. FLEX-6600/6700/8600 mit zwei Antenneneingängen). Software-Interlock: nie zwei Kanäle gleichzeitig im TX.
  - **SO3R:** drei Kanäle, Audio-Routing über SO2R/SO3R-Box.
- **Externe SO2R-Steuerung:** Protokoll **OTRSP** (seriell, von YCCC-Box, SO2R-mini u. a. verstanden) für TX-/RX-Umschaltung.
- **Interlock:** zentrale `TxArbiter`-Klasse; jede Sendeanforderung (CW, Voice, RTTY, Tune) geht hindurch. Hardware-Interlock des Radios wird respektiert.

## 5. Nachrichten und Makros

- F1–F12, Insert, `+` je Mode (CW/SSB/RTTY) und je Run/S&P, wie DXLog.
- Variablen im DXLog-Stil, z. B. `$MYCALL`, `$LOGGED`, `$CALL`, `$RST`, `$SERIAL`, `$EXCH`, `$CQFREQ`.
- **ESM** (Enter Sends Message) als Zustandsmaschine pro Kanal.
- CQ-Wiederholung (Ctrl-R) mit einstellbarer Pause.
- SSB: Nachricht = WAV-Datei je Taste und Contest-Profil.

### 5.1 CW-Geschwindigkeit (adaptiv)

- **Basis:** CW-Tempo aus der Konfiguration (je Kanal), änderbar mit Alt-V / Alt-F9 / Alt-F10.
- **CQ und eigenes Call (S&P)** immer im Basistempo.
- **Antworten an eine Gegenstation** (Exchange, Wiederholungen, TU) im **gemessenen Tempo der Gegenstation**, sobald ihr Ruf decodiert wurde:
  - Messwert = Geschwindigkeitsschätzung des CW-Decoders über die Dauer des Anrufs (geglättet, Ausreißer verworfen).
  - Begrenzung auf konfigurierbares Min/Max (Vorschlag: 16–40 WpM) und maximale Abweichung zur Basis.
  - Gültig nur, wenn der Decoder mit ausreichender Sicherheit eingerastet ist; sonst Basistempo.
- Nach dem Loggen bzw. Alt-W zurück auf das Basistempo.
- Gemessenes Tempo wird mit dem QSO gespeichert (Statistik, Call-History).
- Umsetzung: `cwx wpm <n>` vor dem jeweiligen `cwx send`; Anzeige des aktuell gesendeten Tempos in der Eingabezeile (Basis vs. angepasst).
- Ein-/ausschaltbar per Taste und Konfiguration.

## 6. Voice-Keyer-Rekorder

- Aufnahme vom PC-Mikrofon über QtMultimedia (`QAudioSource`, Qt 6.4).
- Bedienung: **Shift+F-Taste** = Aufnahme starten/stoppen (wie DXLog), F-Taste = abspielen/senden.
- Nachbearbeitung automatisch: Stille am Anfang/Ende abschneiden, Pegel normalisieren, Wiedergabe-Vorschau lokal.
- Speichern als WAV (48 kHz mono), beim Senden auf 24 kHz resampeln.

## 7. Contest-Engine und Log

- **Erste Contests:** TTC-SPCWC (wöchentlich, Di), CQ WW DX CW (28.–29.11.2026).
- **Stand:** Contests sind vorerst C++-Klassen hinter der Schnittstelle `Contest` (`src/contest/`). Später als Datei-Definition:
- **Contest-Definition** als Datei (Format: JSON, Vorschlag): Bänder, Modes, Exchange-Felder mit Validierung, Dupe-Regel, Multiplikatoren, Punkteformel. Spezialfälle (WAE-QTC u. ä.) als C++-Erweiterung.
- **Log:** SQLite, eine Datenbank je Contest-Log; WAL-Modus, jeder QSO sofort persistent.
- **Export:** Cabrillo 3.0, ADIF 3.x.
- **Hilfsdaten:** cty.dat (Präfixe/DXCC/Zonen), MASTER.SCP (Check Partial), Call-History-Dateien.

## 8. Threads

| Thread | Aufgabe |
|---|---|
| GUI | Widgets, Tastatur, Logansicht |
| Radio-I/O | TCP-API, Statusparser, Discovery |
| Stream-I/O | VITA-49 UDP senden/empfangen (DAX TX/RX/IQ) |
| DSP (Pool) | RTTY-Modulator, CW-/RTTY-Decoder, Skimmer, Resampling |
| Audio | Rekorder, lokale Vorschau |

Kommunikation ausschließlich über Qt-Signale (queued connections).

## 9. Meilensteine

| # | Inhalt | Ergebnis |
|---|---|---|
| M0 | Projektgerüst, CI | ✅ |
| M1 | Flex verbinden: Discovery, TCP-API, bind, Slice-/Frequenz-Anzeige | ✅ am FLEX-6700 (FW 4.2.20) bestätigt |
| M1b | Mitschnitt-Werkzeug DAX RX (WAV) / DAX IQ (roh) für den Decoder-Testkorpus | Testdaten aus Contesten |
| M2 | Log-Kern: Eingabezeile, SQLite, Dupe, Contests TTC-SPCWC und CQ WW CW, Cabrillo | TTC ✅ (Simulator), CQ WW folgt |
| M3 | CW über CWX, F-Tasten, ESM | ✅ (Simulator), Test am Radio: TTC 17.11. |
| M3b | CW-Slice-Decoder (Testkorpus, Benchmark, Auswahl), adaptives CW-Tempo | Tempo passt sich dem Anrufer an |
| M4 | Voice-Keyer + Rekorder (DAX TX) | SSB-Contest SO1R |
| M5 | RTTY: AFSK-Sender, Slice-Decoder | RTTY-Contest SO1R |
| M6 | SO2V, dann SO2R (OTRSP), dann SO3R/V | Mehrkanalbetrieb |
| M7 | Bandmap, DX-Cluster, SCP, Call-History, weitere Contests | Komfort |
| M8 | Passband-Skimmer CW/RTTY (DAX IQ) | Anruferliste im Pileup |
| M9 | Band-Skimmer CW/RTTY → Bandmap | SDC vollständig ersetzt |

## 10. Stationsumgebung (Stand 2026-11-17)

- **Radios:** ein bzw. zwei FLEX-6700 (je 2 SCUs, 8 Slices).
- **GUI-Clients:** AetherSDR oder Maestro – QR-Logger bindet sich an einen davon.
- **SDC** wird nicht mehr eingesetzt; QR-Logger übernimmt Decoder und Skimmer.

## 11. Offene Fragen

1. SO2R-Hardware vorhanden (Box/Protokoll), bzw. wie wird zwischen Radios/Slices umgeschaltet und gehört?
2. Voice-Keyer: PC-Audio über DAX TX als Standard, Radio-DVK als Option – einverstanden?
3. CW-Tempo-Anpassung nur nach unten oder in beide Richtungen?
