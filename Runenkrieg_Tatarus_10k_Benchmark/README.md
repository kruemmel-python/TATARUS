# TATARUS – 10.000-Runden-Mehrseed-Benchmark

Gerätenativer Gegenlauf zum konventionellen TensorFlow-Benchmark.
Der bestehende Ordner `Runenkrieg_Tatarus_LargeScale` bleibt dadurch
unverändert erhalten.

Das vorregistrierte Design steht in
[`EXPERIMENT_PROTOCOL.md`](EXPERIMENT_PROTOCOL.md).
Der aktuelle, noch nicht abgeschlossene Gerätestand steht in
[`RUN_STATUS.md`](RUN_STATUS.md).

Ein Smoke-Test für einen Seed:

```powershell
adb shell am instrument -w `
  -e class de.runenkrieg.game.ai.TatarusTenKBenchmarkInstrumentedTest `
  -e trainingSeed 20260730 `
  -e quick true `
  de.runenkrieg.game.tatarusbenchmark.test/androidx.test.runner.AndroidJUnitRunner
```

Der vollständige Lauf verwendet dieselben fünf Trainingsseeds wie der
TensorFlow-Versuch und lässt `quick` weg. Die Resultate entstehen auf dem
Gerät unter:

```text
/sdcard/Android/data/de.runenkrieg.game.tatarusbenchmark/files/
    tatarus_benchmark_full/seed_<SEED>/
```

Jeder Checkpoint enthält `metrics.json` und einen vollständigen
komprimierten TATARUS-Snapshot.

Lange Läufe können checkpointweise gestartet werden. Beispiel:

```powershell
adb shell am instrument -w `
  -e class de.runenkrieg.game.ai.TatarusTenKBenchmarkInstrumentedTest `
  -e trainingSeed 20260730 `
  -e maxCheckpoint 250 `
  de.runenkrieg.game.tatarusbenchmark.test/androidx.test.runner.AndroidJUnitRunner

adb shell am instrument -w `
  -e class de.runenkrieg.game.ai.TatarusTenKBenchmarkInstrumentedTest `
  -e trainingSeed 20260730 `
  -e maxCheckpoint 500 `
  -e resume true `
  de.runenkrieg.game.tatarusbenchmark.test/androidx.test.runner.AndroidJUnitRunner
```
