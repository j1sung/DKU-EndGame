#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory)][string] $UnrealEditorPath)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EOSLocal.psm1') -Force

try {
    $projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
    $context = Get-EOSDevContext -ProjectRoot $projectRoot
    Assert-EOSLocalReady -Context $context
    if (-not (Test-Path -LiteralPath $UnrealEditorPath -PathType Leaf) -or
        [IO.Path]::GetFileName($UnrealEditorPath) -ine 'UnrealEditor.exe') {
        throw 'UnrealEditor.exe is required.'
    }
    $arguments = Get-EOSLoginArguments -ProjectRoot $projectRoot
    # This is the interactive game window the tester must see and control. No credential is an argument.
    Start-Process -FilePath $UnrealEditorPath -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Normal | Out-Null
    Write-Host 'Unreal login test launched with -CustomConfig=EOSLocal. Use 게임 시작 -> LOG IN NOW.'
} catch {
    Write-Host '실행 실패: EOS 로컬 설정 검사, UnrealEditor.exe 경로, 프로젝트 Editor 빌드 상태를 확인하세요.' -ForegroundColor Red
    exit 1
}
