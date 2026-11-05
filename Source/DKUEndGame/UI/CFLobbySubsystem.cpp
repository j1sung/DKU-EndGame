#include "UI/CFLobbySubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"

namespace
{
FText LobbyText(const TCHAR* Value) { return FText::FromString(Value); }
constexpr const TCHAR* AudioSection = TEXT("ChickenFight.Audio");
float Volume(float Value) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : .9f; }
}

void UCFLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GConfig->GetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    GConfig->GetFloat(AudioSection, TEXT("EffectsVolume"), EffectsVolume, GGameUserSettingsIni);
    MusicVolume = Volume(MusicVolume); EffectsVolume = Volume(EffectsVolume);
}

void UCFLobbySubsystem::Deinitialize()
{
    ClearWatchdog(); SaveAudioSettings(); Super::Deinitialize();
}

void UCFLobbySubsystem::ResetForEntry()
{
    ClearWatchdog(); ActiveFlowId = 0; QueueDeadline = 0;
    bAssigned = bCancelRequested = bCancelSent = bReturnRequested = false;
    ConnectionMessage = FText::GetEmpty(); SetState(ECFQueueScreen::Idle);
}

void UCFLobbySubsystem::SetMusicVolume(float Value)
{
    MusicVolume = Volume(Value); OnAudioChanged.Broadcast();
}

void UCFLobbySubsystem::SetEffectsVolume(float Value)
{
    EffectsVolume = Volume(Value); OnAudioChanged.Broadcast();
}

void UCFLobbySubsystem::SaveAudioSettings()
{
    GConfig->SetFloat(AudioSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    GConfig->SetFloat(AudioSection, TEXT("EffectsVolume"), EffectsVolume, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void UCFLobbySubsystem::ClearWatchdog()
{
    if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(Watchdog);
}

void UCFLobbySubsystem::ArmWatchdog(float Seconds)
{
    ClearWatchdog();
    GetGameInstance()->GetTimerManager().SetTimer(Watchdog, this, &ThisClass::HandleTimeout, FMath::Max(Seconds, .1f), false);
}

void UCFLobbySubsystem::SetState(ECFQueueScreen Next, const FText& Error)
{
    Screen = Next; ErrorMessage = Error; OnStateChanged.Broadcast();
}

bool UCFLobbySubsystem::Accept(int32 FlowId) const
{
    return FlowId > 0 && ActiveFlowId == FlowId;
}

void UCFLobbySubsystem::RequestMatchmaking()
{
    if (Screen != ECFQueueScreen::Idle || ActiveFlowId != 0) return;
    if (!OnMatchmakingRequested.IsBound())
    {
        SetState(Screen, LobbyText(TEXT("현재 매칭을 시작할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    const int32 Id = ActiveFlowId = ++FlowSerial;
    bCancelRequested = bCancelSent = bReturnRequested = bAssigned = false;
    QueueDeadline = 0; ConnectionMessage = FText::GetEmpty(); ArmWatchdog(30.f);
    SetState(ECFQueueScreen::Starting);
    if (Accept(Id)) OnMatchmakingRequested.Broadcast(Id);
}

void UCFLobbySubsystem::ReportWaiting(int32 FlowId, int32 RemainingSeconds)
{
    if (!Accept(FlowId) || bAssigned) return;
    QueueDeadline = FPlatformTime::Seconds() + FMath::Clamp(RemainingSeconds, 0, 180);
    if (bCancelRequested)
    {
        // 시작 응답보다 먼저 취소를 눌렀다면 티켓 생성 확인 뒤 실제 취소를 요청한다.
        SetState(ECFQueueScreen::Cancelling); SendCancel(); return;
    }
    ArmWatchdog(FMath::Max(RemainingSeconds, 1)); SetState(ECFQueueScreen::Waiting);
}

void UCFLobbySubsystem::SendCancel()
{
    if (bCancelSent || bAssigned || ActiveFlowId <= 0) return;
    if (!OnCancelRequested.IsBound())
    {
        ReportUncertain(ActiveFlowId, LobbyText(TEXT("취소 여부를 확인하지 못했습니다. 현재 상태를 다시 확인해 주세요."))); return;
    }
    bCancelSent = true; ArmWatchdog(30.f); OnCancelRequested.Broadcast(ActiveFlowId);
}

void UCFLobbySubsystem::RequestCancel()
{
    if (ActiveFlowId <= 0 || bAssigned || Screen == ECFQueueScreen::Cancelling) return;
    bCancelRequested = true;
    if (Screen == ECFQueueScreen::Starting) { SetState(ECFQueueScreen::Cancelling); return; }
    if (Screen == ECFQueueScreen::Recovering) { RecoverStatus(); return; }
    SetState(ECFQueueScreen::Cancelling); SendCancel();
}

void UCFLobbySubsystem::ReportAssigned(int32 FlowId, const FText& Stage)
{
    if (!Accept(FlowId)) return;
    const bool bNewStage = !bAssigned || !ConnectionMessage.EqualTo(Stage);
    bAssigned = true; bCancelRequested = bCancelSent = bReturnRequested = false;
    QueueDeadline = 0; ConnectionMessage = Stage;
    if (bNewStage || Screen != ECFQueueScreen::Connecting) ArmWatchdog(60.f);
    SetState(ECFQueueScreen::Connecting);
}

void UCFLobbySubsystem::ReportIdle(int32 FlowId, const FText& Message)
{
    if (!Accept(FlowId)) return;
    // 취소·만료·실패·활성 티켓 없음이 서버에서 확정된 경우에만 호출한다.
    const bool bLeave = bReturnRequested;
    ClearWatchdog(); ActiveFlowId = 0; QueueDeadline = 0;
    bAssigned = bCancelRequested = bCancelSent = bReturnRequested = false;
    SetState(ECFQueueScreen::Idle, Message);
    if (bLeave) OnReturnToTitleAllowed.Broadcast();
}

void UCFLobbySubsystem::ReportUncertain(int32 FlowId, const FText& Message)
{
    if (!Accept(FlowId)) return;
    ClearWatchdog(); bCancelSent = false;
    SetState(ECFQueueScreen::Recovering, Message.IsEmpty() ? LobbyText(TEXT("연결 상태를 확인하고 있습니다. 다시 확인해 주세요.")) : Message);
}

void UCFLobbySubsystem::RecoverStatus()
{
    if (ActiveFlowId <= 0 || Screen != ECFQueueScreen::Recovering) return;
    if (!OnStatusRecoveryRequested.IsBound())
    {
        SetState(Screen, LobbyText(TEXT("현재 상태를 확인할 수 없습니다. 연결을 확인하고 다시 시도해 주세요."))); return;
    }
    // 같은 복구 요청을 중복 전송하지 않는다. 타임아웃 후에도 활성 매칭은 유지한다.
    if (GetGameInstance()->GetTimerManager().IsTimerActive(Watchdog)) return;
    ArmWatchdog(30.f); SetState(Screen, LobbyText(TEXT("매칭 상태를 확인 중입니다.")));
    OnStatusRecoveryRequested.Broadcast(ActiveFlowId);
}

void UCFLobbySubsystem::RestoreOnEntry()
{
    if (ActiveFlowId != 0 || !OnStatusRecoveryRequested.IsBound()) return;
    ActiveFlowId = ++FlowSerial;
    SetState(ECFQueueScreen::Recovering); RecoverStatus();
}

void UCFLobbySubsystem::HandleTimeout()
{
    if (ActiveFlowId <= 0) return;
    // 로컬 타임아웃을 서버의 취소 완료로 해석하지 않는다. 재조회 전 새 매칭은 차단한다.
    ReportUncertain(ActiveFlowId, LobbyText(TEXT("응답이 늦어지고 있습니다. 매칭 상태를 다시 확인해 주세요.")));
}

int32 UCFLobbySubsystem::GetRemainingSeconds() const
{
    return QueueDeadline > 0 ? FMath::Max(0, FMath::CeilToInt(QueueDeadline - FPlatformTime::Seconds())) : -1;
}

bool UCFLobbySubsystem::CanReturnToTitle() const
{
    return !bAssigned && !bReturnRequested;
}

void UCFLobbySubsystem::RequestReturnToTitle()
{
    if (!CanReturnToTitle()) return;
    if (Screen == ECFQueueScreen::Idle && ActiveFlowId == 0) { OnReturnToTitleAllowed.Broadcast(); return; }
    bReturnRequested = true; RequestCancel(); OnStateChanged.Broadcast();
}
