[CmdletBinding()]
param(
    [string]$Image = 'chickengame-backend-auth:smoke'
)

$ErrorActionPreference = 'Stop'
$backendInfra = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$dockerfile = Join-Path $backendInfra 'backend\Dockerfile'

docker build --platform linux/amd64 --file $dockerfile --tag $Image $backendInfra
if ($LASTEXITCODE -ne 0) { throw 'Lambda image build failed.' }

docker run --rm --platform linux/amd64 --entrypoint /bin/sh $Image -c `
    'test -f /var/task/dist/handler.js && test -x /opt/bin/eos-token-verifier && test -f /opt/lib/libEOSSDK-Linux-Shipping.so'
if ($LASTEXITCODE -ne 0) { throw 'Required runtime artifact is missing.' }

$dependencyOutput = docker run --rm --platform linux/amd64 --entrypoint /usr/bin/ldd `
    $Image /opt/bin/eos-token-verifier
if ($LASTEXITCODE -ne 0 -or ($dependencyOutput -join "`n") -match 'not found') {
    throw 'Native verifier has an unresolved dynamic dependency.'
}

$stdoutPath = Join-Path ([IO.Path]::GetTempPath()) ("eos-container-{0}.stdout" -f [guid]::NewGuid())
$stderrPath = Join-Path ([IO.Path]::GetTempPath()) ("eos-container-{0}.stderr" -f [guid]::NewGuid())
try {
    '{not-json' | docker run --rm -i --platform linux/amd64 `
        --entrypoint /opt/bin/eos-token-verifier $Image 1>$stdoutPath 2>$stderrPath
    $exitCode = $LASTEXITCODE
    $stdoutLines = @(Get-Content -LiteralPath $stdoutPath)
    if ($exitCode -ne 1) { throw "Malformed input returned exit $exitCode instead of 1." }
    if ($stdoutLines.Count -ne 1 -or
        $stdoutLines[0] -ne '{"ok":false,"error":"INVALID_INPUT"}') {
        throw 'Verifier stdout is not exactly one safe protocol JSON object.'
    }

    $fakeRequest = '{"token":"a.b.c","candidatePlayerId":"0123456789abcdef0123456789abcdef"}'
    $fakeRequest | docker run --rm -i --platform linux/amd64 `
        --entrypoint /opt/bin/eos-token-verifier `
        --env EOS_PRODUCT_ID=00000000000000000000000000000000 `
        --env EOS_SANDBOX_ID=00000000000000000000000000000000 `
        --env EOS_DEPLOYMENT_ID=00000000000000000000000000000000 `
        --env EOS_CLIENT_ID=00000000000000000000000000000000 `
        --env EOS_CLIENT_SECRET=container-smoke-placeholder `
        $Image 1>$stdoutPath 2>$stderrPath
    $exitCode = $LASTEXITCODE
    $stdoutLines = @(Get-Content -LiteralPath $stdoutPath)
    if ($exitCode -ne 2) { throw "Invalid token returned exit $exitCode instead of 2." }
    if ($stdoutLines.Count -ne 1 -or
        $stdoutLines[0] -ne '{"ok":false,"error":"TOKEN_INVALID"}') {
        throw 'Packaged EOS SDK did not return the safe invalid-token protocol result.'
    }
}
finally {
    Remove-Item -LiteralPath $stdoutPath,$stderrPath -Force -ErrorAction SilentlyContinue
}

$inspection = docker image inspect $Image | ConvertFrom-Json
$environment = @($inspection[0].Config.Env)
if ($environment -match '^EOS_CLIENT_SECRET=') {
    throw 'Image environment contains an EOS Client Secret setting.'
}
$history = docker history --no-trunc --format '{{.CreatedBy}}' $Image
if (($history -join "`n") -match 'EOS_CLIENT_SECRET=') {
    throw 'Image history contains an EOS Client Secret setting.'
}

Write-Host "PASS Lambda image smoke checks: $Image"
