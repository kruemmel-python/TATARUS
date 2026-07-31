# Vergleich TATARUS gegen konventionelle KI in Runenkrieg

Stand: 31. Juli 2026

## Versuchsrahmen

Beide Entwicklungszweige wurden mit fünf unabhängigen Trainingsseeds bis
10.000 beobachteten Umweltrunden geführt. Der Gewinner jedes Zweigs wurde
nach einer vorregistrierten Regel ausgewählt und anschließend genau einmal
auf 50 zuvor unberührten Replikationsseeds bewertet.

Die Zustands- und Aktionsräume sind gleich. Die Zufallszahlengeneratoren der
Python- und Kotlin-Umgebungen erzeugen jedoch keine bitidentischen
Episodenfolgen. Der Vergleich ist deshalb distributionsgleich geplant, aber
nicht streng spielweise gepaart.

## Ergebnisse

| Kennzahl | TATARUS LargeScale | konventioneller Gewinner |
|---|---:|---:|
| Gewinner | Seed 20260732 | Contextual Bandit, Seed 20260731 |
| Training | 10.000 Runden | 10.000 Runden |
| mittlere Siegrate über 5 Modelle bei 10k | 81 % [75–86 %] | 65 % [61–69 %] |
| Auswahl-Holdout des Gewinners | 90 % | nicht als Einzelwert maßgeblich |
| unabhängige Replikation | 35/50 = 70 % | 30/50 = 60 % |
| Replikations-Token-Differenz | +6,50 | +2,52 |
| Replikations-Rundensiegrate | 60,63 % | 53,26 % |
| exportierte Modellgröße | 1.564.970 Byte | 1.504 Byte |

## Explorative Inferenz der Replikation

Für 35/50 gegenüber 30/50 beträgt die beobachtete Differenz
**+10 Prozentpunkte zugunsten von TATARUS**.

- Wilson-95-%-Intervall TATARUS: 56,25–80,90 %,
- Wilson-95-%-Intervall konventioneller Gewinner: 46,18–72,39 %,
- Newcombe-95-%-Intervall der Differenz: −8,51 bis +27,60 Prozentpunkte,
- zweiseitiger exakter Fisher-Test: p = 0,4019.

Damit zeigt dieses Experiment einen replizierten numerischen Vorsprung von
TATARUS, bestätigt bei dieser Stichprobengröße aber noch keine statistische
Überlegenheit gegenüber dem konventionellen Gewinner.

## Was bereits gezeigt ist

TATARUS kann als vollständig neuronaler, persistenter Gegner ein komplexes
Kartenspiel lernen, nach 10.000 Umweltrunden eingefroren werden und auf
unberührten Seeds oberhalb der konventionellen Referenz sowie deutlich über
Zufallsniveau spielen. Während der Replikation blieben Modellzustand,
Eligibility, Readout und synaptische Parameter nachweislich unverändert.

## Grenzen

- Die unabhängige Replikation umfasst nur 50 Spiele je Gewinner.
- Die Episoden sind wegen Kotlin- und Python-RNG nicht bitidentisch gepaart.
- Die gemessenen Entscheidungszeiten stammen aus unterschiedlichen
  Laufzeitumgebungen und dürfen nicht direkt gegeneinander interpretiert
  werden.
- Die Modellgrößen repräsentieren unterschiedliche Architekturen und
  Zustandsbegriffe.
- Das Ergebnis belegt Leistung in Runenkrieg, keine allgemeine Intelligenz
  und keine biologische Gleichwertigkeit.

## Nächster Bestätigungslauf

Für eine belastbare Überlegenheitsbehauptung ist ein vorab erzeugter,
sprachunabhängiger Episodenstrom mit mindestens mehreren hundert
Replikationsspielen je Modell sinnvoll. Die primäre Hypothese muss vor dem
Lauf auf Partiensiegrate oder Token-Differenz festgelegt werden.
