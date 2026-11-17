# QR-Logger – Bedienung (Stand 0.1, SO1R CW)

## Vorbereitung

1. AetherSDR (oder Maestro) starten, Slice auf CW, **Break-In (QSK/Semi) einschalten** – CWX tastet den Sender sonst evtl. nicht.
2. QR-Logger starten → *Radio → Verbinden …* → Radio wählen. QR-Logger bindet sich an den GUI-Client.
3. *Datei → Neues Log …* → Contest, Rufzeichen, Kategorie, Club, E-Mail.
4. Optional *Einstellungen → CW-Nachrichten …* prüfen.
5. *Einstellungen → CW: Mithörton im PC* (Standard an): QR-Logger erzeugt den Mithörton selbst auf der Soundkarte, Tonhöhe = CW-Pitch des Radios. Lautstärke unter *Mithörton-Lautstärke …*.
6. Falls das Radio nicht sendet: *Einstellungen → CW: PTT während Nachricht halten* probieren.

Geloggt wird immer auf der Frequenz des **TX-Slice** des gebundenen GUI-Clients.
Logs liegen unter `~/.local/share/qr-logger/QR-Logger/logs/*.qrl` (SQLite); das letzte Log wird beim Start wieder geöffnet.

## Tasten

| Taste | Funktion |
|---|---|
| Enter | ESM: leer → CQ (F1) · Call → Insert-Nachricht · Call + Exch → „+“-Nachricht und loggen |
| F1 … F8 | Nachricht senden (Run- bzw. S&P-Satz) |
| Shift-F1 … F8, Shift-Insert | Nachricht bearbeiten |
| Insert oder `;` | Call + Exchange senden, Cursor ins Exchange-Feld |
| `+` oder `'` | TU-Nachricht senden und loggen |
| Ctrl-`+` | still loggen (ohne Senden) |
| Esc | Senden abbrechen |
| Space | zwischen Call- und Exchange-Feld springen |
| Tab | nächstes Feld (inkl. RST) |
| Alt-W / F11 | Eingabe löschen |
| Ctrl-W | aktuelles Feld löschen |
| PgUp / PgDn, Alt-F10 / Alt-F9 | CW-Tempo ± 2 WpM |
| Alt-V | CW-Tempo eingeben |
| Ctrl-M | ESM ein/aus |
| Ctrl-Tab | Run / S&P |
| Ctrl-R | CQ-Wiederholung ein/aus |
| Entf (in der Logtabelle) | QSO löschen; Doppelklick auf eine Zelle = bearbeiten |

## Variablen in Nachrichten

`$MYCALL` `$CALL` `$LASTCALL` `$RST` (599 → 5NN) `$SERIAL` `$CQZONE` `$EXCHANGE`

## Nach dem Contest

*Datei → Cabrillo exportieren …* – TTC: Upload unter spcwc.pl/upload (Frist 24 h).
