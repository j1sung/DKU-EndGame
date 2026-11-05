#requires -Version 5.1
Set-StrictMode -Version Latest

function Get-EOSDevContext {
    param([Parameter(Mandatory)][string] $ProjectRoot)
    $root = [IO.Path]::GetFullPath($ProjectRoot)
    $sam = [IO.File]::ReadAllText((Join-Path $root 'Backend-Infra\samconfig.toml'))
    $section = [regex]::Match($sam, '(?ms)^\[dev\.deploy\.parameters\]\s*(.*?)(?=^\[|\z)')
    $overrides = [regex]::Match($section.Groups[1].Value, 'parameter_overrides\s*=\s*"([^"]+)"')
    if (-not $overrides.Success) { throw 'The dev EOS parameters are missing from samconfig.toml.' }
    $values = @{}
    foreach ($name in @('ProductId', 'SandboxId', 'DeploymentId', 'ClientId')) {
        $idMatch = [regex]::Match($overrides.Groups[1].Value, '(?:^|\s)Eos' + $name + '=([A-Za-z0-9]+)(?=\s|$)')
        if (-not $idMatch.Success) { throw "The dev EOS $name is missing." }
        $values[$name] = $idMatch.Groups[1].Value
    }
    return [pscustomobject]@{
        ProjectRoot = $root
        ConfigPath = Join-Path $root 'Config\Custom\EOSLocal\DefaultEngine.ini'
        ProductId = $values.ProductId
        SandboxId = $values.SandboxId
        DeploymentId = $values.DeploymentId
        ClientId = $values.ClientId
    }
}

function Assert-EOSLocalIgnored {
    param([Parameter(Mandatory)][string] $ProjectRoot)
    # Tracked files are not treated as ignored by git check-ignore.
    & git -C $ProjectRoot check-ignore --quiet -- 'Config/Custom/EOSLocal/DefaultEngine.ini' 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw 'The local EOS credential path is not Git-ignored. Update the branch before entering credentials.'
    }
}

function New-EOSLocalConfig {
    param([Parameter(Mandatory)] $Context, [AllowEmptyString()][string] $ClientSecret)
    if ([string]::IsNullOrEmpty($ClientSecret) -or $ClientSecret.Length -gt 4096 -or
        $ClientSecret -cnotmatch '\A[A-Za-z0-9._~+/=-]+\z') {
        throw 'The Client Secret is empty or contains unsupported characters. Paste only the credential value.'
    }
    # File-storage encryption is unused in this login/profile test. Epic documents this placeholder.
    $encryptionKey = '1' * 64
    return @"
; Local development only. Do not commit or share this file.
[/Script/OnlineSubsystemEOS.EOSSettings]
bUseNamedPlatformConfig=false
DefaultArtifactName=ChickenGameDev
!Artifacts=ClearArray
+Artifacts=(ArtifactName="ChickenGameDev",ClientId="$($Context.ClientId)",ClientSecret="$ClientSecret",ProductId="$($Context.ProductId)",SandboxId="$($Context.SandboxId)",DeploymentId="$($Context.DeploymentId)",EncryptionKey="$encryptionKey")
"@
}

function Test-EOSLocalText {
    param([Parameter(Mandatory)] $Context, [AllowEmptyString()][string] $Text)
    if ([string]::IsNullOrEmpty($Text)) { return $false }
    $secretMatches = [regex]::Matches($Text, 'ClientSecret="([^"\r\n]*)"')
    if ($secretMatches.Count -ne 1) { return $false }
    try {
        $expected = New-EOSLocalConfig -Context $Context -ClientSecret $secretMatches[0].Groups[1].Value
        return $expected.Replace("`r`n", "`n").Trim() -ceq $Text.Replace("`r`n", "`n").Trim()
    } catch { return $false }
}

function Assert-EOSLocalReady {
    param([Parameter(Mandatory)] $Context)
    Assert-EOSLocalIgnored -ProjectRoot $Context.ProjectRoot
    if (-not [IO.File]::Exists($Context.ConfigPath)) {
        throw 'Local EOS settings are missing. Run Tools/Network/Set-EOSLocal.ps1 first.'
    }
    if (-not (Test-EOSLocalText -Context $Context -Text ([IO.File]::ReadAllText($Context.ConfigPath)))) {
        throw 'Local EOS settings do not match the current dev IDs or are incomplete. Run Set-EOSLocal.ps1 again.'
    }
}

function Save-EOSLocalConfig {
    param([Parameter(Mandatory)] $Context, [Parameter(Mandatory)][Security.SecureString] $ClientSecret)
    Assert-EOSLocalIgnored -ProjectRoot $Context.ProjectRoot
    $buffer = [IntPtr]::Zero
    $plain = $null
    $config = $null
    try {
        $buffer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($ClientSecret)
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($buffer)
        $config = New-EOSLocalConfig -Context $Context -ClientSecret $plain
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Context.ConfigPath)) | Out-Null
        [IO.File]::WriteAllText($Context.ConfigPath, $config, [Text.UTF8Encoding]::new($false))
    } finally {
        if ($buffer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($buffer) }
        $plain = $null
        $config = $null
    }
    Assert-EOSLocalReady -Context $Context
}

function Get-EOSLoginArguments {
    param([Parameter(Mandatory)][string] $ProjectRoot)
    $project = Join-Path ([IO.Path]::GetFullPath($ProjectRoot)) 'DKUEndGame.uproject'
    if (-not [IO.File]::Exists($project)) { throw 'DKUEndGame.uproject was not found.' }
    return @(('"' + $project + '"'), '-game', '-CustomConfig=EOSLocal', '-windowed', '-ResX=1280', '-ResY=720')
}

Export-ModuleMember -Function Get-EOSDevContext, Assert-EOSLocalIgnored, New-EOSLocalConfig, Test-EOSLocalText, Assert-EOSLocalReady, Save-EOSLocalConfig, Get-EOSLoginArguments
