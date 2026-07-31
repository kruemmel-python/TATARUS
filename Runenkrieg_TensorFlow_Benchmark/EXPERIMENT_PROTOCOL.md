# Vorregistriertes Mehrseed-Protokoll

Protokollkennung: `RUNENKRIEG-TF-MULTISEED-1`  
Festgelegt vor dem vollständigen Lauf am 31. Juli 2026.

## Forschungsfrage

Wie entwickeln sich MLP, GRU, DQN, PPO und Contextual Bandit unter
identischen Runenkrieg-Eingaben, Aktionsräumen, Belohnungen und
Umweltrunden bis 10.000 beobachtete Runden?

## Trainings- und Messplan

- Trainingsseeds: `20260730` bis `20260734`
- Lernkurvenpunkte: 250, 500, 1.000, 2.000, 5.000 und 10.000 Runden
- Holdout: 20 feste Partieseeds `30000` bis `30019`
- Regelwechsel-Holdout: 20 feste Partieseeds `40000` bis `40019`
- Anpassungsmessung nach dem Regelwechsel: 0, 250, 500 und 1.000 Runden
- finale Replikation des ausgewählten Modells: 50 unberührte Partieseeds
  `60000` bis `60049`
- Gewichtsänderungen sind während Holdout und Replikation deaktiviert.

## Vorab festgelegte Gewinnerregel

Die Architektur wird am Endcheckpoint über die fünf Trainingsseeds
aggregiert. Sortiert wird strikt nach:

1. höchster mittlerer Partie-Siegrate,
2. höchster mittlerer Token-Differenz,
3. niedrigster mittlerer Entscheidungszeit,
4. lexikografischer Agentenname als vollständig deterministischer letzter
   Gleichstandsbrecher.

Für den mobilen Export wird innerhalb der Gewinnerarchitektur der
Checkpoint mit der höchsten Holdout-Partie-Siegrate verwendet; danach
gelten Token-Differenz, Latenz und Seednummer als Gleichstandsbrecher. Erst
nach dieser Auswahl wird der eingefrorene Checkpoint auf den Seeds
`60000` bis `60049` geprüft.

## Statistik und Grenzen

Berichtet werden Mittelwerte und deterministisch gebootstrappte
95-%-Intervalle über die fünf unabhängigen Trainingsläufe. Ein Intervall
mit fünf Seeds ist breit und ersetzt keine externe Replikation.

Dieser Lauf vergleicht die konventionellen Architekturen untereinander.
Eine Aussage gegenüber TATARUS ist erst zulässig, wenn:

- der Kotlin- und Python-Spielkern per Cross-Language-Goldentest
  übereinstimmen,
- TATARUS dieselben vorregistrierten Umwelt- und Holdout-Seeds erhält,
- beide eingefrorenen Modelle auf demselben Android-Gerät vermessen
  werden.

Die Auswahl des Android-Gegners ist keine allgemeine
Überlegenheitsbehauptung, sondern die reproduzierbare Auswahl des besten
konventionellen Vergleichsmodells in diesem Versuchsraum.
