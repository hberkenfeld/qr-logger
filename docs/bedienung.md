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

## Cluster / Skimmer

*Cluster → Einstellungen …*: Login-Rufzeichen, DX-Cluster (Host/Port) und Skimmer (Standard: RBN `telnet.reversebeacon.net:7000`), Spot-Lebensdauer. *Cluster → Verbinden* – die Verbindung wird beim nächsten Start automatisch wieder aufgebaut und bei Abbruch alle 15 s neu versucht.

Fenster **Spots** (links): Spots des aktuellen Bands (umschaltbar), sortiert nach Frequenz, je Rufzeichen und Band nur der neueste. Status **NEU** (grün, fett) / **neu Band** / **Dupe** (grau, durchgestrichen) nach den Regeln des Contests; „nur ungearbeitete“ blendet Dupes aus. Der Spot auf der aktuellen TX-Frequenz ist gelb markiert. Spots mit anderer Betriebsart als der Contest (laut Spot-Kommentar) werden ausgeblendet.

- Doppelklick oder Enter auf einen Spot: TX-Slice auf die Frequenz, Rufzeichen ins Call-Feld, Umschalten auf S&P.
- **Ctrl-↑ / Ctrl-↓** in der Eingabezeile: nächster ungearbeiteter Spot oberhalb / unterhalb.

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
| Alt-V | Fokus ins Tempo-Feld (rechts neben der Eingabezeile): Zahl tippen oder ↑/↓, Enter übernimmt und springt zurück ins Call-Feld, Esc verwirft |
| Alt-T oder Alt-K | CW-Freitext: jedes Zeichen wird sofort gesendet, TX erst ab dem ersten Zeichen · Enter schließt (Rest wird gesendet) · Esc schließt und bricht ab · PgUp/PgDn Tempo |
| Ctrl-M | ESM ein/aus |
| Ctrl-Tab | Run / S&P |
| Ctrl-R | CQ-Wiederholung ein/aus |
| Ctrl-↑ / Ctrl-↓ | nächster ungearbeiteter Spot oberhalb / unterhalb |
| Entf oder Rechtsklick → „QSO löschen“ (in der Logtabelle) | QSO löschen; Doppelklick auf eine Zelle = bearbeiten |

## Variablen in Nachrichten

`$MYCALL` `$CALL` `$LASTCALL` `$RST` (599 → 5NN) `$SERIAL` `$CQZONE` `$EXCHANGE`

## Nach dem Contest

*Datei → Cabrillo exportieren …* – TTC: Upload unter spcwc.pl/upload (Frist 24 h).
