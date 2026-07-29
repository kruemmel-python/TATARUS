param([Parameter(Mandatory=$true)][string]$ResultRoot)
$s18 = Get-Content -Raw -LiteralPath (Join-Path $ResultRoot 'replication_stage18\stage18_confirmation.json') | ConvertFrom-Json
$s19 = Get-Content -Raw -LiteralPath (Join-Path $ResultRoot 'replication_stage19\stage19_results.json') | ConvertFrom-Json
$s23 = Get-Content -Raw -LiteralPath (Join-Path $ResultRoot 'replication_stage20_23\stage20_23_results.json') | ConvertFrom-Json
if ($s18.status -ne 'confirmed' -or $s19.status -ne 'confirmed' -or $s23.stage20 -ne 'confirmed' -or $s23.stage21 -ne 'confirmed' -or $s23.maximum_executed_neurons -lt 16384) { throw 'Independent replication criteria not met' }
Write-Output 'INDEPENDENT REPLICATION RESULT: PASS'
