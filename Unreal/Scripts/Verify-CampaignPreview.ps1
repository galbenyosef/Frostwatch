param([string]$Prefix='polish',[string]$PackageRoot='',[switch]$Editor)
$ErrorActionPreference='Stop'
$fwRoot=Split-Path $PSScriptRoot
$fwPackage=Join-Path $fwRoot 'CampaignPreview\Windows\Frostwatch'
if($PackageRoot){$fwPackage=(Resolve-Path -LiteralPath $PackageRoot).Path}
$fwExe=Join-Path $fwPackage 'Binaries\Win64\Frostwatch.exe'
$fwLeadingArgs=@()
if($Editor){$fwPackage=$fwRoot;$fwExe='D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe';$fwLeadingArgs=@($fwRoot+'/Frostwatch.uproject','-game')}
if(!(Test-Path -LiteralPath $fwExe)){throw 'Campaign preview executable is missing'}
$fwResults=@()
foreach($fwTest in @(@{Flag='CampaignTest';Report='campaign-tests.json'},@{Flag='FrostwatchTest';Report='runtime-tests.json'})){
    $fwStarted=Get-Date
    $fwLog=Join-Path $fwRoot ('Verification\'+$Prefix+'-packaged-'+$fwTest.Flag+'.log')
    $protected=@(Get-ChildItem -LiteralPath (Join-Path $fwPackage 'Saved') -Recurse -File -ErrorAction SilentlyContinue|Where-Object {$_.Extension -eq '.sav' -or $_.Name -eq 'GameUserSettings.ini'}|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    $settingsBytes=@{};foreach($f in $protected){if([IO.Path]::GetFileName($f.Path) -eq 'GameUserSettings.ini'){$settingsBytes[$f.Path]=[IO.File]::ReadAllBytes($f.Path)}}
    try {
        $fwProcess=Start-Process -FilePath $fwExe -ArgumentList ($fwLeadingArgs+@('-'+$fwTest.Flag,'-nullrhi','-unattended','-nosplash',('-abslog="'+$fwLog+'"'))) -PassThru -WindowStyle Hidden
        if(!$fwProcess.WaitForExit(240000)){Stop-Process -Id $fwProcess.Id;throw 'Campaign diagnostic timed out'}
        $fwProcess.Refresh()
    } finally {foreach($p in $settingsBytes.Keys){[IO.File]::WriteAllBytes($p,$settingsBytes[$p])}}
    foreach($f in $protected){if((Get-FileHash -LiteralPath $f.Path).Hash -ne $f.Hash){throw ('Diagnostic changed player data: '+$f.Path)}}
    if($fwProcess.ExitCode -ne 0){throw ('Packaged test exited with '+$fwProcess.ExitCode+': '+$fwTest.Flag)}
    $fwReportPath=Join-Path $fwPackage ('Verification\'+$fwTest.Report)
    if(!(Test-Path -LiteralPath $fwReportPath)){throw ('Missing report: '+$fwReportPath)}
    if((Get-Item -LiteralPath $fwReportPath).LastWriteTime -lt $fwStarted){throw 'Test report was not refreshed'}
    $fwReport=Get-Content -LiteralPath $fwReportPath -Raw | ConvertFrom-Json
    if(!$fwReport.all_passed){throw ('Failed checks: '+(($fwReport.checks | Where-Object {!$_.passed}).name -join '; '))}
    Copy-Item -LiteralPath $fwReportPath -Destination (Join-Path $fwRoot ('Verification\'+$Prefix+'-packaged-'+$fwTest.Report)) -Force
    if($fwTest.Flag -eq 'CampaignTest'){$fwCampaignStarted=$fwStarted}
    $fwResults+=@{test=$fwTest.Flag;checks=$fwReport.checks.Count;all_passed=$true;exit_code=$fwProcess.ExitCode}
}
$fwTraversal=Join-Path $fwPackage 'Verification\world-traversal.json'
if(Test-Path -LiteralPath $fwTraversal){
    if((Get-Item -LiteralPath $fwTraversal).LastWriteTime -lt $fwCampaignStarted){throw 'Traversal report was not refreshed'}
    $fwWorld=Get-Content -LiteralPath $fwTraversal -Raw | ConvertFrom-Json
    if($fwWorld.problems.Count -gt 0){throw 'Packaged traversal checks reported problems'}
    Copy-Item -LiteralPath $fwTraversal -Destination (Join-Path $fwRoot ('Verification\'+$Prefix+'-packaged-world-traversal.json')) -Force
}
$fwResults | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $fwRoot ('Verification\'+$Prefix+'-package-validation.json'))
$fwResults | Format-Table
