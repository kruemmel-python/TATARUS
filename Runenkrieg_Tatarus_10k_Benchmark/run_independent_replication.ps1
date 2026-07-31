param(
    [string]$Adb = $(
        if ($env:ANDROID_HOME) {
            Join-Path $env:ANDROID_HOME "platform-tools\adb.exe"
        } else {
            "adb"
        }
    ),
    [string]$ExpectedSerial = "4c90bfcc",
    [string]$ExpectedModel = "RMX3853"
)

$ErrorActionPreference = "Stop"
$runner = "de.runenkrieg.game.tatarusbenchmark.test/androidx.test.runner.AndroidJUnitRunner"
$testClass = "de.runenkrieg.game.ai.TatarusWinnerReplicationInstrumentedTest"
$remoteRoot = "/sdcard/Android/data/de.runenkrieg.game.tatarusbenchmark/files"
$winner = Join-Path $PSScriptRoot "exports\tatarus_frozen_winner.json.gz"
$localResult = Join-Path $PSScriptRoot "results_full\independent_replication.json"

if (-not (Test-Path -LiteralPath $Adb)) {
    throw "adb not found: $Adb"
}
if (-not (Test-Path -LiteralPath $winner)) {
    throw "Frozen winner not found. Run analyze_results.py first: $winner"
}

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

& $Adb shell rm -rf "$remoteRoot/replication_input" "$remoteRoot/replication_results"
& $Adb shell mkdir -p "$remoteRoot/replication_input"
& $Adb push $winner "$remoteRoot/replication_input/tatarus_frozen_winner.json.gz"

$output = & $Adb shell am instrument -w `
    -e class $testClass `
    $runner 2>&1
$output | ForEach-Object { Write-Host $_ }
if (($output -join "`n") -notmatch "OK \(1 test\)") {
    throw "Independent replication failed."
}

& $Adb pull "$remoteRoot/replication_results/replication.json" $localResult
if (-not (Test-Path -LiteralPath $localResult)) {
    throw "Missing replication result: $localResult"
}
Write-Host "TATARUS_INDEPENDENT_REPLICATION_COMPLETE"
