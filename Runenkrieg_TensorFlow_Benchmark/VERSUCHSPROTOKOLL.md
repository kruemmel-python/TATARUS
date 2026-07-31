# Vergleichsprotokoll TATARUS gegen konventionelle KI

## Primäre Hypothese

\[
H_1:\quad
Q_{\mathrm{TATARUS}}(n) \ge
\max_m Q_m(n)
\]

für mindestens einen vorab festgelegten Messpunkt \(n\), wobei \(m\) MLP,
GRU, DQN, PPO und Contextual Bandit bezeichnet.

## Primärer Endpunkt

Holdout-Leistung pro beobachteter Umweltrunde. Berichtet werden
Rundensiegrate und Partie-Token-Differenz gemeinsam.

## Messpunkte

\[
n\in\{250,500,1000,2000,5000,10000\}.
\]

## Seeds

- Trainingsseed: 20260730; anschließend mindestens vier weitere Seeds
- Holdout: 30000–30019
- Regelwechsel: 40000–40019
- Geschichtsprobe: 51000
- externe Replikation: neuer Seedblock und zweites Gerät

## Pflichtbedingungen

1. Keine Architektur erhält fertig berechnete optimale Aktionen.
2. Jede Architektur erhält die gleichen 128 Kanäle.
3. GRU darf ausschließlich den eigenen Verlaufsspeicher zusätzlich nutzen.
4. DQN erhält keinen größeren Replay-Datensatz als selbst beobachtete Runden.
5. PPO sammelt nur eigene On-policy-Erfahrung.
6. Holdout verändert keine Gewichte.
7. Modellwahl erfolgt vor Öffnung der finalen Replikationsseeds.
8. Laufzeit- und Energiemetriken werden nur auf identischer Hardware
   verglichen; dafür werden TensorFlow-Modelle nach LiteRT exportiert.

## Zusatztests

- Entscheidungszeit und 95. Perzentil
- Parameter- und Persistenzgröße
- CPU-Zeit pro Trainingsrunde
- Save/Reload-Retention
- identischer aktueller Zustand bei verschiedener Vorgeschichte
- umgekehrte Elementhierarchie ohne Vorwarnung
- Wiederanpassung nach 250/500/1.000 Regelwechselrunden

## Aussagekriterium

Eine Sample-Effizienzüberlegenheit darf behauptet werden, wenn TATARUS einen
vorab definierten Qualitätswert mit signifikant weniger Umweltrunden
erreicht und der Effekt über Trainingsseeds und Replikation stabil bleibt.
Modellparameter, Rechenzeit und menschliche Spielrunden werden getrennt
ausgewiesen.
