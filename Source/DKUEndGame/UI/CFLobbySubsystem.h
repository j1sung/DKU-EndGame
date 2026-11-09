#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CFLobbySubsystem.generated.h"

UENUM(BlueprintType)
enum class ECFQueueScreen : uint8 { Idle, Starting, Waiting, Cancelling, Connecting, Recovering, AuthenticationRequired };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCFLobbyChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCFLobbyRequest, int32, FlowId);

/** 연습 로비의 표시 상태. 인증·티켓·EOS 세션과 폴링은 통합 계층이 소유한다. */
UCLASS()
class DKUENDGAME_API UCFLobbySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") ECFQueueScreen Screen = ECFQueueScreen::Idle;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") FText ErrorMessage;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") FText ConnectionMessage;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") int32 ActiveFlowId = 0;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") bool bCancelRequested = false;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") bool bReturnRequested = false;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") float MusicVolume = .9f;
    UPROPERTY(BlueprintReadOnly, Category="CF|Lobby") float EffectsVolume = .9f;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby") FCFLobbyChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby") FCFLobbyChanged OnAudioChanged;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby") FCFLobbyChanged OnReturnToTitleAllowed;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby|Integration") FCFLobbyRequest OnMatchmakingRequested;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby|Integration") FCFLobbyRequest OnCancelRequested;
    UPROPERTY(BlueprintAssignable, Category="CF|Lobby|Integration") FCFLobbyRequest OnStatusRecoveryRequested;

    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void RequestMatchmaking();
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void RequestCancel();
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void RequestReturnToTitle();
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void RecoverStatus();
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void RestoreOnEntry();
    UFUNCTION(BlueprintPure, Category="CF|Lobby") int32 GetRemainingSeconds() const;
    UFUNCTION(BlueprintPure, Category="CF|Lobby") bool CanReturnToTitle() const;
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Audio") void SetMusicVolume(float Value);
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Audio") void SetEffectsVolume(float Value);
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Audio") void SaveAudioSettings();

    // FlowId는 시작부터 취소/종료 확인까지 동일하다. 통합 계층은 HTTP 응답 순서도 검증해야 한다.
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Integration") void ReportWaiting(int32 FlowId, int32 RemainingSeconds);
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Integration") void ReportAssigned(int32 FlowId, const FText& Stage);
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Integration") void ReportIdle(int32 FlowId, const FText& Message);
    UFUNCTION(BlueprintCallable, Category="CF|Lobby|Integration") void ReportUncertain(int32 FlowId, const FText& Message);
    void ReportAuthenticationRequired(int32 FlowId);
    int32 BeginAccountRecovery();

private:
    int32 FlowSerial = 0;
    bool bAssigned = false;
    bool bCancelSent = false;
    double QueueDeadline = 0;
    FTimerHandle Watchdog;
    void SetState(ECFQueueScreen Next, const FText& Error = FText::GetEmpty());
    void ArmWatchdog(float Seconds);
    void ClearWatchdog();
    void HandleTimeout();
    void SendCancel();
    bool Accept(int32 FlowId) const;
};
