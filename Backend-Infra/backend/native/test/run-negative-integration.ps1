[CmdletBinding()]
param(
    [switch]$OfflineOnly
)

$ErrorActionPreference = 'Stop'
$backendInfra = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..\..')).Path
$sdkRoot = Join-Path $backendInfra 'vendor\eos-sdk'
$buildImage = if ([string]::IsNullOrWhiteSpace($env:EOS_NATIVE_BUILD_IMAGE)) {
    'public.ecr.aws/amazonlinux/amazonlinux:2023'
} else {
    $env:EOS_NATIVE_BUILD_IMAGE
}
if (-not (Test-Path -LiteralPath (Join-Path $sdkRoot 'Include\eos_sdk.h')) -or
    -not (Test-Path -LiteralPath (Join-Path $sdkRoot 'lib\libEOSSDK-Linux-Shipping.so'))) {
    throw 'Local EOS SDK not found under Backend-Infra/vendor/eos-sdk.'
}

function Read-MaskedValue([string]$Prompt) {
    $secure = Read-Host -Prompt $Prompt -AsSecureString
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
    }
}

function Require-PlainEnvironment([string]$Name) {
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = Read-Host -Prompt $Name
        [Environment]::SetEnvironmentVariable($Name, $value)
    }
    return $value
}

function Require-MaskedEnvironment([string]$Name) {
    $value = [Environment]::GetEnvironmentVariable($Name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = Read-MaskedValue $Name
        [Environment]::SetEnvironmentVariable($Name, $value)
    }
    return $value
}

function Invoke-Verifier {
    param(
        [Parameter(Mandatory)] [string]$InputJson,
        [switch]$WithEnvironment,
        [string]$DeploymentOverride
    )

    $stderrPath = Join-Path ([IO.Path]::GetTempPath()) ("eos-verifier-{0}.stderr" -f [guid]::NewGuid())
    try {
        $arguments = @('exec', '-i')
        if ($WithEnvironment) {
            $arguments += @(
                '-e', 'EOS_PRODUCT_ID',
                '-e', 'EOS_SANDBOX_ID',
                '-e', 'EOS_DEPLOYMENT_ID',
                '-e', 'EOS_CLIENT_ID',
                '-e', 'EOS_CLIENT_SECRET'
            )
        }
        if (-not [string]::IsNullOrEmpty($DeploymentOverride)) {
            $arguments += @('-e', "EOS_DEPLOYMENT_ID=$DeploymentOverride")
        }
        $arguments += @($script:containerName, '/tmp/eos-build/eos-token-verifier')

        $stdout = $InputJson | & docker @arguments 2>$stderrPath
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Stdout = ($stdout -join "`n")
            Stderr = if (Test-Path -LiteralPath $stderrPath) {
                Get-Content -LiteralPath $stderrPath -Raw
            } else { '' }
        }
    }
    finally {
        Remove-Item -LiteralPath $stderrPath -Force -ErrorAction SilentlyContinue
    }
}

function Assert-ExitCode($Result, [int]$Expected, [string]$CaseName) {
    if ($Result.ExitCode -ne $Expected) {
        throw "$CaseName returned exit $($Result.ExitCode), expected $Expected."
    }
    $parsed = $Result.Stdout | ConvertFrom-Json
    if ($parsed.ok -ne $false) {
        throw "$CaseName unexpectedly returned success."
    }
}

function Flip-HexCharacter([string]$Value) {
    if ([string]::IsNullOrEmpty($Value)) { throw 'Cannot alter an empty identifier.' }
    $replacement = if ($Value[0] -eq '0') { '1' } else { '0' }
    return $replacement + $Value.Substring(1)
}

$script:containerName = "eos-negative-$([guid]::NewGuid().ToString('N').Substring(0, 10))"
try {
    docker run -d --rm --name $script:containerName --platform linux/amd64 `
        --mount "type=bind,source=$backendInfra,target=/workspace" `
        -w /workspace $buildImage sleep infinity | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Failed to start Amazon Linux build container.' }

    docker exec $script:containerName sh -lc `
        'dnf -y install gcc-c++ cmake make pkgconf-pkg-config json-c-devel >/dev/null && cmake -S backend/native -B /tmp/eos-build -DBUILD_TESTING=ON -DEOS_SDK_ROOT=/workspace/vendor/eos-sdk >/dev/null && cmake --build /tmp/eos-build --parallel 2 >/dev/null'
    if ($LASTEXITCODE -ne 0) { throw 'Native verifier build failed.' }

    $malformed = Invoke-Verifier -InputJson '{not-json'
    Assert-ExitCode $malformed 1 'malformed input'
    Write-Host 'PASS malformed input -> exit 1'

    if ($OfflineOnly) {
        Write-Host 'Offline-only verification complete; online negative cases were not requested.'
    }

    if (-not $OfflineOnly) {
        $productId = Require-PlainEnvironment 'EOS_PRODUCT_ID'
        $sandboxId = Require-PlainEnvironment 'EOS_SANDBOX_ID'
        $deploymentId = Require-PlainEnvironment 'EOS_DEPLOYMENT_ID'
        $clientId = Require-PlainEnvironment 'EOS_CLIENT_ID'
        $clientSecret = Require-MaskedEnvironment 'EOS_CLIENT_SECRET'
        $candidatePlayerId = Require-PlainEnvironment 'EOS_TEST_PUID'
        $validToken = Require-MaskedEnvironment 'EOS_TEST_CONNECT_ID_TOKEN'

        if ($candidatePlayerId -notmatch '^[0-9a-fA-F]{32}$') {
            throw 'EOS_TEST_PUID must be exactly 32 hexadecimal characters.'
        }

        $tamperedRequest = @{
            token = $validToken + 'x'
            candidatePlayerId = $candidatePlayerId
        } | ConvertTo-Json -Compress
        $tampered = Invoke-Verifier -InputJson $tamperedRequest -WithEnvironment
        Assert-ExitCode $tampered 2 'tampered token'
        Write-Host 'PASS tampered token -> exit 2'

        $wrongPuidRequest = @{
            token = $validToken
            candidatePlayerId = Flip-HexCharacter $candidatePlayerId
        } | ConvertTo-Json -Compress
        $wrongPuid = Invoke-Verifier -InputJson $wrongPuidRequest -WithEnvironment
        Assert-ExitCode $wrongPuid 2 'PUID mismatch'
        Write-Host 'PASS PUID mismatch -> exit 2'

        $validRequest = @{
            token = $validToken
            candidatePlayerId = $candidatePlayerId
        } | ConvertTo-Json -Compress
        $scopeMismatch = Invoke-Verifier -InputJson $validRequest -WithEnvironment `
            -DeploymentOverride (Flip-HexCharacter $deploymentId)
        Assert-ExitCode $scopeMismatch 2 'scope mismatch'
        Write-Host 'PASS scope mismatch -> exit 2'

        foreach ($result in @($tampered, $wrongPuid, $scopeMismatch)) {
            if ($result.Stdout.Contains($validToken) -or $result.Stdout.Contains($clientSecret)) {
                throw 'Verifier stdout leaked token or client secret.'
            }
        }
        Write-Host 'PASS stdout contains no token or client secret'
    }
}
finally {
    docker rm -f $script:containerName 2>$null | Out-Null
}
