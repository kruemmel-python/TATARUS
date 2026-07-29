# Experiment- und Ablationsplan

## Ziel

Geprüft wird, welche Gatewerte tatsächlich mit einem übertragenen Spike
multipliziert werden und ob der Kernel über event-konditionierte Kontrollen
hinaus eine dynamische Wirkung besitzt.

## Gemeinsames System

- 24 current-based LIF-Neuronen
- 80 % exzitatorisch, 20 % inhibitorisch
- spärliche, seed-deterministische Rekurrenz
- Dale-Prinzip
- Membranleck, Refraktärzeit, adaptive Schwelle
- identische lokale Paar-STDP in allen Hauptvarianten
- alternierende Assembly-Stimulation über 420 Schritte
- Seeds `11, 23, 38, 53, 71`

## Varianten

1. **Baseline:** `g=1`, Kernel deaktiviert.
2. **Originalkernel:** `g=clip((1+tanh(K(z)))/2,0.05,0.95)`.
3. **Globale Konstantkontrolle:** Mittelwert aller berechneten Gatewerte.
4. **Event-Konstantkontrolle:** Mittelwert nur der Gatewerte mit
   `previous_spike=1`.
5. **Verteilungskontrolle:** gleicher event-konditionierter Mittelwert und
   gleiche Varianz, aber deterministisch randomisiert.
6. **Zeitverschoben und state-shuffled:** prüfen zeitliche und neuronale
   Zuordnung.
7. **Algebraische Vereinfachung:** nicht verwendet, weil keine weitere
   Vereinfachung unter der geschützten Algebra semantisch äquivalent ist.

Die Konstantkontrolle trennt Kernelstruktur von bloßer Gesamtdämpfung. Alle
Varianten erhalten identische Topologie, Stimuli, Seeds und Lernparameter.

## Messgrößen

- mittlere Feuerrate und Gesamtspikezahl,
- aktive Neuronenfraktion,
- normalisierte Spannungsenergie,
- Fano-ähnlicher Index der Populationsspikezahl,
- mittlere paarweise Spike-Korrelation,
- binned coincidence rate,
- globale sowie event-konditionierte Gate-Mittel und -Varianz,
- Spannungs-RMSE gegenüber Baseline und Konstantkontrolle,
- Spikeentscheidungs-Differenz gegenüber Baseline,
- Endlichkeit sämtlicher Zustände.

## Akzeptanz

- alle Läufe endlich,
- alle Neuronen im Kernelmodus mindestens einmal aktiv,
- Gate-Varianz in jedem Kernel-Lauf größer null,
- eine dynamische Wirkung gilt nur bei wirksamer Gatevarianz über der
  numerischen Toleranz und Abweichung von der event-gematchten Konstante.

## Negative und adversariale Tests

- Stille Eingabe muss ohne spontane Spikes am Ruhepotential bleiben.
- Alternierende endliche Extremströme von `±10^6` dürfen keine NaN/Inf-Zustände
  erzeugen.
- Falsche Vektorlängen und nichtendliche Stimuli müssen explizit abgewiesen
  werden.
- Die Refraktärzeit darf durch Dauererregung nicht unterschritten werden.

Der Plan beweist keine Aufgabenüberlegenheit. Er prüft ausschließlich
Systemwirkung, numerische Sicherheit und die Gegenhypothese „nur
Mittelwertdämpfung“.
