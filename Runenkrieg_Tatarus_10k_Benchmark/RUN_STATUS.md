# Laufstatus des TATARUS-10k-Benchmarks

Stand: 31. Juli 2026

Der vorregistrierte Mehrseed-Lauf ist implementiert, getestet und
checkpointweise fortsetzbar. Er ist noch nicht abgeschlossen.

## Bestätigte Checkpoints

| Checkpoint | bestätigte Seeds |
|---:|---:|
| 250 | 5/5 |
| 500 | 5/5 |
| 1.000 | 5/5 |
| 2.000 | 1/5 |
| 5.000 | 0/5 |
| 10.000 | 0/5 |

Damit sind 16 von 30 geplanten Lernkurvenpunkten vollständig geschrieben,
vom Gerät gezogen und lokal gesichert.

## Sicherer Abbruch

Der Lauf begann auf:

- Modell: `RMX3853`
- ADB-Seriennummer: `4c90bfcc`

Während `Seed 20260731 / Checkpoint 2.000` wurde dieses Smartphone durch
ein anderes Gerät ersetzt:

- Modell: `RMX3472`
- ADB-Seriennummer: `1dd6e851`

Der Orchestrator stoppte daraufhin. Die Ergebnisse werden nicht über
verschiedene Geräte hinweg als eine gemeinsame Latenz- oder Thermalmessung
fortgeschrieben.

## Fortsetzung

Nach Wiederanschluss des registrierten `RMX3853` kann
`run_full_benchmark.ps1 -Resume` den letzten bestätigten Snapshot jedes
Seeds zurückspielen und ohne doppelt gezählte Trainingsrunden fortfahren.

Bis zum vollständigen 10.000er-Lauf, der vorregistrierten Auswahl und der
unabhängigen Replikation darf kein TATARUS-Snapshot als endgültiger
mobiler Gewinner bezeichnet werden.
