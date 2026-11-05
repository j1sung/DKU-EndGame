#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '..\EOSLocal.psm1') -Force

function Assert-True([bool] $Value, [string] $Message) {
    if (-not $Value) { throw $Message }
}
function Assert-Rejected([scriptblock] $Action, [string] $Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-True $rejected $Message
}

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$context = Get-EOSDevContext -ProjectRoot $projectRoot
$fakeSecret = 'TEST_ONLY_NOT_A_REAL_SECRET_123'
$config = New-EOSLocalConfig -Context $context -ClientSecret $fakeSecret
Assert-True (Test-EOSLocalText -Context $context -Text $config) 'Valid artifact configuration rejected'
Assert-True ($config.Contains('DefaultArtifactName=ChickenGameDev')) 'Default artifact missing'
Assert-True ($config.Contains('bUseNamedPlatformConfig=false')) 'Named platform must not override local artifact'
Assert-True (-not (Test-EOSLocalText -Context $context -Text ($config.Replace($context.ClientId, 'xyzWrongClient')))) 'Wrong client accepted'
Assert-True (-not (Test-EOSLocalText -Context $context -Text ($config.Replace($fakeSecret, '')))) 'Empty secret accepted'
Assert-True (-not (Test-EOSLocalText -Context $context -Text ($config.Replace('ChickenGameDev', 'OtherArtifact')))) 'Wrong artifact accepted'
foreach ($invalid in @('', ('line' + "`n" + 'break'), ("trailing`n"), ("trailing`r`n"), 'quote"injection', 'slash\injection')) {
    Assert-Rejected { New-EOSLocalConfig -Context $context -ClientSecret $invalid } 'Unsafe secret accepted'
}

$arguments = Get-EOSLoginArguments -ProjectRoot $projectRoot
Assert-True ($arguments -contains '-CustomConfig=EOSLocal') 'Custom config argument missing'
Assert-True ($arguments -contains '-game') 'Game entry argument missing'
Assert-True (-not (($arguments -join ' ').Contains($fakeSecret))) 'Credentials in launch arguments'

# A non-Git folder must fail closed before any credentials can be written.
Assert-Rejected { Assert-EOSLocalIgnored -ProjectRoot $env:TEMP } 'Non-Git destination accepted'
Assert-EOSLocalIgnored -ProjectRoot $projectRoot

# Exercise the actual credential write and Git checks in a disposable repo, never the real local settings.
$fixture = Join-Path $projectRoot ('.tmp\eos-config-test-' + [Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path (Join-Path $fixture 'Backend-Infra') -Force | Out-Null
    Copy-Item (Join-Path $projectRoot 'Backend-Infra\samconfig.toml') (Join-Path $fixture 'Backend-Infra\samconfig.toml')
    Copy-Item (Join-Path $projectRoot 'DKUEndGame.uproject') (Join-Path $fixture 'DKUEndGame.uproject')
    [IO.File]::WriteAllText((Join-Path $fixture '.gitignore'), "/Config/Custom/EOSLocal/`n")
    & git -C $fixture init --quiet
    if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize test repository' }
    $testContext = Get-EOSDevContext -ProjectRoot $fixture
    $secure = ConvertTo-SecureString $fakeSecret -AsPlainText -Force
    try { $captured = @(Save-EOSLocalConfig -Context $testContext -ClientSecret $secure *>&1) }
    finally { $secure.Dispose() }
    Assert-True ($captured.Count -eq 0) 'Credential save unexpectedly writes output'
    Assert-EOSLocalReady -Context $testContext
    Assert-True ([IO.File]::ReadAllText($testContext.ConfigPath).Contains($fakeSecret)) 'Expected local credential not written'
    $untracked = @(& git -C $fixture ls-files --others --exclude-standard)
    Assert-True (-not ($untracked -match 'EOSLocal')) 'Credential file visible to Git'
    & git -C $fixture add --force -- 'Config/Custom/EOSLocal/DefaultEngine.ini'
    if ($LASTEXITCODE -ne 0) { throw 'Cannot prepare tracked-file guard test' }
    Assert-Rejected { Assert-EOSLocalReady -Context $testContext } 'Tracked credentials must be rejected'
} finally {
    if (Test-Path -LiteralPath $fixture) {
        $resolved = (Resolve-Path -LiteralPath $fixture).Path
        $allowed = [IO.Path]::GetFullPath((Join-Path $projectRoot '.tmp')) + [IO.Path]::DirectorySeparatorChar
        if (-not $resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid test cleanup path' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
Write-Host 'PASS: config validation, client alignment, injection rejection, credential write without output, Git exclusion/tracked guard, launch arguments'
