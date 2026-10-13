param([string]$PackageRoot='',[switch]$Editor,[string]$Prefix='combat-contact')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$pkg=Join-Path $root 'CampaignPreview\Windows\Frostwatch'
if($PackageRoot){$pkg=(Resolve-Path -LiteralPath $PackageRoot).Path}
$exe=Join-Path $pkg 'Binaries\Win64\Frostwatch.exe'
$args=@('-FrostwatchCapture','-CombatContactTest','-nullrhi','-unattended','-nosplash',('-abslog='+$root+'/Verification/'+$Prefix+'-tests.log'))
if($Editor){$pkg=$root;$exe='D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe';$args=@($root+'/Frostwatch.uproject','-game')+$args}
$saved=Join-Path $pkg 'Saved'
$protected=@(Get-ChildItem -LiteralPath $saved -Recurse -File -ErrorAction SilentlyContinue | Where-Object {$_.Extension -eq '.sav' -or $_.Name -eq 'GameUserSettings.ini'} | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$settingsBytes=@{}
foreach($file in $protected){if([IO.Path]::GetFileName($file.Path) -eq 'GameUserSettings.ini'){$settingsBytes[$file.Path]=[IO.File]::ReadAllBytes($file.Path)}}
$started=Get-Date
try{
$process=Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Hidden
if(!$process.WaitForExit(240000)){Stop-Process -Id $process.Id;throw 'Combat checks timed out'}
$process.Refresh()
}finally{
 # Unreal rewrites inherited defaults as differences on exit. Restore the exact
 # personal configuration after diagnostics; save-game changes remain failures.
 foreach($path in $settingsBytes.Keys){[IO.File]::WriteAllBytes($path,$settingsBytes[$path])}
}
foreach($file in $protected){if((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash){throw ('Test changed protected player data: '+$file.Path)}}
$report=Join-Path $pkg 'Verification/combat-contact-tests.json'
if(!(Test-Path -LiteralPath $report) -or (Get-Item -LiteralPath $report).LastWriteTime -lt $started){throw 'Combat report is missing or stale'}
$data=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if($report -ne (Join-Path $root ('Verification/'+$Prefix+'-tests.json'))){Copy-Item -LiteralPath $report -Destination (Join-Path $root ('Verification/'+$Prefix+'-tests.json')) -Force}
if($process.ExitCode -ne 0 -or !$data.all_passed){throw ('Combat checks failed: '+(($data.checks | Where-Object {!$_.passed}).name -join '; '))}
[pscustomobject]@{Passed=$data.all_passed;Checks=$data.checks.Count;ExitCode=$process.ExitCode;ProtectedFiles=$protected.Count} | ConvertTo-Json
