# Entwicklungsprotokoll

## Version 1 – post-cue filtered state

Hash: `018228D7DC492D7E`

```text
Gesamtaccuracy       0.524740
mittlere Verzögerung 0.562500
lange Verzögerung    0.486979
Holdout freigegeben  nein
```

Die vier exponentiellen Spiketraces und die Soma-/Dendritzustände nach dem
zweiten Hinweis bewahren den ersten Hinweis nicht ausreichend. Die 16
Bestätigungs-Seeds wurden nicht ausgeführt.

## Version 2 – cue-gebundene Eligibility-Memory

Nur anhand der Entwicklungsdaten wird ergänzt:

- Netzwerkzustand während des ersten Hinweises,
- exponentielles Abklingen dieses Zustands mit `50`, `100`, `200 ms` bis
  zum zweiten Hinweis,
- Zustand des zweiten Hinweises,
- begrenzte Produkte zwischen erstem Eligibility-Zustand und zweitem
  Hinweiszustand.

Die Merkmale stammen aus Spikeaktivität, Somamembran und Dendritspannung.
Zielklasse oder rohe Eingabebits werden nicht übergeben. Die
Bestätigungs-Seeds bleiben bis zum Erreichen der festgelegten
Entwicklungsgrenze gesperrt.

Ergebnis Version 2:

```text
Hash                 EECE7A502A958561
Gesamtaccuracy       0.906250
mittlere Verzögerung 0.881944
lange Verzögerung    0.930556
Holdout freigegeben  ja
```

Die Readoutkonfiguration ist damit eingefroren. Nach diesem Punkt sind nur
noch Ausführung, Statistik und Berichtserzeugung für die bereits
festgelegten Bestätigungs-Seeds zulässig.
