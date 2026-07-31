param(
    [string]$Adb = $(
        if ($env:ANDROID_HOME) {
            Join-Path $env:ANDROID_HOME "platform-tools\adb.exe"
        } else {
            "adb"
        }
    ),
    [switch]$Resume,
    [string]$ExpectedSerial = "4c90bfcc",
    [string]$ExpectedModel = "RMX3853"
)

$ErrorActionPreference = "Stop"
$seeds = 20260730, 20260731, 20260732, 20260733, 20260734
$checkpoints = 250, 500, 1000, 2000, 5000, 10000
$runner = "de.runenkrieg.game.tatarusbenchmark.test/androidx.test.runner.AndroidJUnitRunner"
$testClass = "de.runenkrieg.game.ai.TatarusTenKBenchmarkInstrumentedTest"
$remoteRoot = "/sdcard/Android/data/de.runenkrieg.game.tatarusbenchmark/files/tatarus_benchmark_full"
$localRoot = Join-Path $PSScriptRoot "results_full"

if (-not (Test-Path -LiteralPath $Adb)) {
    throw "adb not found: $Adb"
}
New-Item -ItemType Directory -Path $localRoot -Force | Out-Null

$devices = & $Adb devices -l
$deviceLines = @($devices | Select-String -Pattern "\sdevice\s")
if ($deviceLines.Count -ne 1) {
    throw "Exactly one authorized Android device is required."
}
$deviceLine = $deviceLines[0].Line
if ($ExpectedSerial -and $deviceLine -notmatch "^$([regex]::Escape($ExpectedSerial))\s") {
    throw "Wrong device serial. Expected $ExpectedSerial, found: $deviceLine"
}
if ($ExpectedModel -and $deviceLine -notmatch "model:$([regex]::Escape($ExpectedModel))(\s|$)") {
    throw "Wrong device model. Expected $ExpectedModel, found: $deviceLine"
}

& $Adb shell svc power stayon usb | Out-Null
& $Adb shell input keyevent KEYCODE_WAKEUP | Out-Null

foreach ($checkpoint in $checkpoints) {
    foreach ($seed in $seeds) {
        $localSeed = Join-Path $localRoot "seed_$seed"
        $localMetrics = Join-Path $localSeed "round_$checkpoint\metrics.json"
        if ($Resume -and (Test-Path -LiteralPath $localMetrics)) {
            Write-Host "RESUME skip seed=$seed checkpoint=$checkpoint"
            continue
        }

        if ($Resume) {
            $localSeed = Join-Path $localRoot "seed_$seed"
            if (Test-Path -LiteralPath (Join-Path $localSeed "learning_curve.json")) {
                & $Adb shell mkdir -p "$remoteRoot/seed_$seed"
                & $Adb push "$localSeed/." "$remoteRoot/seed_$seed/"
            }
        }

        $arguments = @(
            "shell", "am", "instrument", "-w",
            "-e", "class", $testClass,
            "-e", "trainingSeed", "$seed",
            "-e", "maxCheckpoint", "$checkpoint"
        )
        if ($checkpoint -ne $checkpoints[0] -or $Resume) {
            $arguments += @("-e", "resume", "true")
        }
        $arguments += $runner

        Write-Host "START seed=$seed checkpoint=$checkpoint time=$([DateTime]::UtcNow.ToString('o'))"
        $output = & $Adb @arguments 2>&1
        $output | ForEach-Object { Write-Host $_ }
        if (($output -join "`n") -notmatch "OK \(1 test\)") {
            throw "Instrumentation failed for seed=$seed checkpoint=$checkpoint"
        }

        New-Item -ItemType Directory -Path $localSeed -Force | Out-Null
        & $Adb pull "$remoteRoot/seed_$seed/." $localSeed
        if (-not (Test-Path -LiteralPath $localMetrics)) {
            throw "Missing pulled metrics: $localMetrics"
        }
        $battery = & $Adb shell dumpsys battery
        $level = ($battery | Select-String "level:").Line.Trim()
        $temperature = ($battery | Select-String "temperature:").Line.Trim()
        Write-Host "DONE seed=$seed checkpoint=$checkpoint $level $temperature"
    }
}

Write-Host "TATARUS_FULL_BENCHMARK_COMPLETE"
