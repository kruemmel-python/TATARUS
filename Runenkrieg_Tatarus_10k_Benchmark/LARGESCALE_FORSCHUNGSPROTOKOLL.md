# Vorregistriertes LargeScale-Vergleichsprotokoll

## Ziel

Prüfung, ob die zusätzliche neuronale Kapazität zu besserer
Runenkrieg-Leistung führt und ob TATARUS seine nichtneuronalen Baselines
unter kontrollierten Bedingungen übertrifft.

## Eingefrorene Kandidaten

1. LargeScale `Reines TATARUS`
2. LargeScale `Hybrid`
3. deterministische Regelbaseline
4. Zufallsbaseline
5. LargeScale ohne Eligibility
6. LargeScale ohne Generated Operator
7. LargeScale ohne Assemblies
8. separat gebaute 72-Neuronen-Referenz aus `../Runenkrieg_Tatarus`

Der kleine Referenzstand wird nicht nachträglich an die LargeScale-Seeds
angepasst.

## Entwicklungs- und Testtrennung

- Entwicklungsseeds: 10000–10049
- Auswahlseed für das endgültige Modell: 20000
- unberührte Testseeds: 30000–30049
- optionale Replikationsseeds auf zweitem Gerät: 40000–40049

Nur Entwicklungsseeds dürfen Parameterentscheidungen beeinflussen. Nach
Einfrieren des Modells werden die Testseeds genau einmal ausgewertet.

## Training

- mindestens fünf unabhängige Trainingsläufe,
- identische Anzahl aufgelöster Trainingsrunden,
- identische Karten-/Wetterseeds je Vergleich,
- Auswahl des eingefrorenen Kandidaten ausschließlich nach
  Entwicklungsdaten,
- keine Gewichtsänderung während Holdout und Replikation.

Die App verwendet vollständige Partien. Ein Trainingswert von 250 bedeutet
250 tatsächlich aufgelöste Runden, nicht 250 voneinander isolierte
Kartenpaare.

## Primäre Endpunkte

1. Partie-Siegrate,
2. mittlere Token-Differenz am Partieende.

Sekundär:

- Rundensiegrate,
- Spikes pro Partie,
- synaptische Übertragungen pro Partie,
- Energiekosten pro Partie,
- mittlere Entscheidungszeit,
- Assembly-Entropie und -Separation.

## Vorab festgelegte Erfolgsregel

Eine Überlegenheitsbehauptung gegenüber der Regelbaseline ist nur zulässig,
wenn auf unberührten Seeds:

1. die LargeScale-Partie-Siegrate höher ist,
2. die mittlere Token-Differenz höher ist,
3. das 95-%-Konfidenzintervall des gepaarten Vorteils für mindestens einen
   primären Endpunkt vollständig über null liegt,
4. der zweite primäre Endpunkt nicht signifikant schlechter ist,
5. das Ergebnis auf den Replikationsseeds dieselbe Richtung besitzt.

Ein bloßer Trainingsgewinn oder ein Fünf-Seed-Schnelllauf genügt nicht.

## Skalierungshypothese

Für den Vergleich mit der 72-Neuronen-Referenz gilt zusätzlich:

\[
\Delta Q =
Q_{\text{1024/32768/128}}-
Q_{\text{72/432/32}}.
\]

Ein positiver Wert zeigt zunächst nur eine Kapazitätsassoziation. Weil sich
auch Eingaberaum, Readout und Trainingsregime ändern, darf er nicht allein
der Neuronenzahl zugeschrieben werden. Dafür wären weitere kontrollierte
Zwischengrößen oder faktorielle Ablationen erforderlich.

## Mechanismusnachweis

Der vollständige Kern muss gegen `ohne Eligibility`, `ohne Operator` und
`ohne Assemblies` antreten. Eine Mechanismusbehauptung erfordert einen
reproduzierbaren Leistungsverlust bei Entfernung genau dieses Mechanismus,
nicht nur veränderte Spike- oder Energiewerte.

## Smartphone-Budget

Vor dem Holdout werden pro Gerät dokumentiert:

- Hersteller und Modell,
- Android-Version,
- CPU/SoC,
- freie Arbeitsspeichermenge,
- App-Version und Commit,
- mittlere und 95. Perzentil-Entscheidungszeit,
- Abbruch, Thermal Throttling oder Speicherfehler.

LargeScale gilt nur dann als praktisch spielbar, wenn Entscheidungen auf dem
Zielgerät ohne UI-Watchdog, Speicherfehler oder Zustandsverlust abgeschlossen
werden.

## Berichtssprache

Zulässig:

> „Im vorregistrierten Runenkrieg-Holdout erzielte TATARUS LargeScale unter
> den getesteten Seeds einen statistisch gestützten Vorteil.“

Nicht zulässig:

> „Mehr Neuronen machen TATARUS allgemein intelligenter.“

Der Nachweis bleibt auf Version, Aufgabe, Trainingsbudget, Geräte und Seeds
des Experiments beschränkt.

