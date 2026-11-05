#requires -Version 5.1
[CmdletBinding()]
param([switch] $CheckOnly)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EOSLocal.psm1') -Force

try {
    $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $context = Get-EOSDevContext -ProjectRoot $projectRoot
    Assert-EOSLocalIgnored -ProjectRoot $projectRoot
    if (-not $CheckOnly) {
        Write-Host 'EOS game-client local configuration / EOS 로컬 개발 설정'
        Write-Host ('Client ID: ' + $context.ClientId)
        Write-Host 'Paste the Client Secret for this existing Peer2Peer client below.'
        Write-Host 'Client Secret을 아래 마스킹 입력에 붙여넣고 Enter를 누르세요. 채팅에는 보내지 마세요.'
        $credential = Read-Host 'Client Secret (hidden)' -AsSecureString
        try { Save-EOSLocalConfig -Context $context -ClientSecret $credential }
        finally { $credential.Dispose() }
    }
    Assert-EOSLocalReady -Context $context
    Write-Host 'READY: local EOS settings match the dev IDs and are Git-ignored.'
    Write-Host '설정 검사가 통과했습니다. 실제 Epic 로그인은 Unreal 실행 후 별도로 확인해야 합니다.'
} catch {
    # Never echo raw exceptions: a credential must not appear in an error message.
    Write-Host 'EOS 설정 준비/검사 실패. 최신 브랜치, Git 제외 경로, 입력값을 확인하세요.' -ForegroundColor Red
    exit 1
}
