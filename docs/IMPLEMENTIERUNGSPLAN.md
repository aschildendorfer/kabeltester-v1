# Implementierungsplan fuer kabeltester-v0

Stand: 2026-05-05

## Ziel
Dieser Plan beschreibt die schrittweise Implementierung der Features aus der Projekt-README fuer den SDI Cable Tester auf RP2350.

## Schritt 0: Board- und Multicore-Basistest
Ziel: Hardware und Dual-Core Architektur stabil verifizieren, bevor die Messlogik implementiert wird.

Umsetzung:
1. Core0: Onboard LED auf GPIO7 blinken lassen.
2. Core1: NeoPixel auf GPIO21 blinken lassen (WS2812 Beispiel aus dem blink-Projekt uebernehmen).
3. Serielle Logs je Core ausgeben (Heartbeat + Zaehler), um Parallelbetrieb sichtbar zu machen.
4. Keine blockierenden Ausgaben in der Core1-Loop zulassen.

Akzeptanzkriterien:
1. Beide LEDs blinken unabhaengig ueber mindestens 5 Minuten.
2. Core0 Logging beeinflusst Core1 NeoPixel-Blinken nicht sichtbar.
3. Build und Flash funktionieren mit den VS Code Tasks Compile Project und Run Project.

## Schritt 1: Codebasis aufraeumen und Architektur-Skelett
Ziel: Von Demo-Code zu klarer Produktstruktur wechseln.

Umsetzung:
1. Unnoetige I2C, DMA, UART Demo-Teile entfernen.
2. Modulstruktur definieren:
   - Core0: Management, USB, UI, Ausgabe.
   - Core1: High-Speed Engine, PIO, Sampling.
3. CMake auf benoetigte Libraries reduzieren und spaeter erweitern.

Akzeptanzkriterien:
1. Projekt kompiliert sauber ohne Demo-Altlasten.
2. Start-Logs zeigen klare Rollen von Core0 und Core1.

## Schritt 2: Trigger/HST auf GPIO2
Ziel: Reproduzierbaren Einspeisepuls erzeugen.

Umsetzung:
1. GPIO2 als HST Trigger-Ausgang konfigurieren.
2. Pulsparameter (Breite, Abstand, Wiederholrate) als Konstanten kapseln.
3. Triggersequenz mit Zeitstempel und Sequenznummer protokollieren.

Akzeptanzkriterien:
1. Trigger erscheinen reproduzierbar (Scope/Logic Analyzer empfohlen).
2. Keine Aussetzer bei gleichzeitigem USB-Logging.

## Schritt 3: PIO Zeitmessung fuer Echo auf GPIO3
Ziel: Taktgenaue Laufzeitmessung unabhaengig von CPU-Jitter.

Umsetzung:
1. Eigene PIO State Machine fuer Echo-Erfassung anlegen.
2. Start/Stop Ereignisse in Zyklen zaehlen.
3. Rohdaten sicher an Core1-Auswertung uebergeben.

Akzeptanzkriterien:
1. Messwerte bei fixer Last sind stabil.
2. Aufloesung entspricht den Zielannahmen aus der README.

## Schritt 4: Distanzberechnung und Defekterkennung
Ziel: Rohzeit in kabelrelevante Metriken umrechnen.

Umsetzung:
1. Distanz aus Laufzeit berechnen mit konfigurierbarem v_p (Startwert 0.66).
2. Klassifikation einfuehren: offen, kurzschluss, auffaellig.
3. Glattung mit Median/Mittelwert ueber N Messungen.

Akzeptanzkriterien:
1. Plausible Laengen bei Referenzkabeln.
2. Wiederholbare Defektklassifikation.

## Schritt 5: Threshold Sweep ueber GPIO4 PWM
Ziel: Statistische Signalanalyse fuer Daempfung und Jitter.

Umsetzung:
1. GPIO4 als PWM Referenzspannung (DAC-Ersatz ueber RC) nutzen.
2. Sweep von 0V bis 2.5V in Stufen durchfuehren.
3. Pro Stufe High-Dichte statistisch erfassen.
4. Auswertung: Amplitudenindikator und Flankenbreite als Jitter-Indikator.

Akzeptanzkriterien:
1. Sweep laeuft vollstaendig und reproduzierbar.
2. Messkurven unterscheiden Kabelzustaende nachvollziehbar.

## Schritt 6: Adaptive Kalibrierung
Ziel: Empfindlichkeit automatisch an Rauschbedingungen anpassen.

Umsetzung:
1. Kalibrierfenster vor Messstart durchfuehren.
2. Rauschboden abschaetzen und Schwellwerte dynamisch setzen.
3. Kalibrierstatus und Parameter im UI anzeigen.

Akzeptanzkriterien:
1. Weniger Fehltrigger im Vergleich zu statischer Schwelle.
2. Stabilere Ergebnisse bei Umgebungsveraenderungen.

## Schritt 7: Echtzeit ASCII Visualisierung
Ziel: Reflexionsverlauf und Qualitaet direkt im Terminal sichtbar machen.

Umsetzung:
1. Core0 rendert ASCII Plot (Distanz gegen Reflexion/Qualitaet).
2. Zusammenfassung pro Messung anzeigen: Laenge, Status, Qualitaetsindex.
3. Betriebsarten anbieten: Single Shot und Continuous.

Akzeptanzkriterien:
1. Darstellung bleibt lesbar bei Live-Messung.
2. Ergebnisse sind ohne Rohdatenanalyse interpretierbar.

## Schritt 8: Dual-Core Datenpipeline robust machen
Ziel: Zeitkritische Messung und UI strikt entkoppeln.

Umsetzung:
1. Ringbuffer oder Queue zwischen Core1 und Core0 verwenden.
2. Backpressure Strategie definieren (drop oldest oder skip frame).
3. Telemetrie fuer dropped samples und queue depth ausgeben.

Akzeptanzkriterien:
1. Keine messseitigen Blockaden durch UI.
2. Definiertes Verhalten bei Lastspitzen.

## Schritt 9: Validierung und Dokumentation
Ziel: Reproduzierbarer, nachvollziehbarer Entwicklungsstand.

Umsetzung:
1. Testmatrix: kurzes Kabel, langes Kabel, offen, kurzschluss.
2. README um Build, Flash, Testablauf und Kalibrierung ergaenzen.
3. Grenzen und bekannte Unsicherheiten dokumentieren.

Akzeptanzkriterien:
1. Alle Testfaelle liefern erwartete Klassifikation.
2. Neuer Entwickler kann den Ablauf Ende-zu-Ende nachbauen.

## Milestones
1. M1: Schritt 0 abgeschlossen (Multicore Blink stabil).
2. M2: Schritt 3 abgeschlossen (PIO Laufzeitmessung verfuegbar).
3. M3: Schritt 6 abgeschlossen (adaptive Kalibrierung aktiv).
4. M4: Schritt 9 abgeschlossen (validiertes MVP).
