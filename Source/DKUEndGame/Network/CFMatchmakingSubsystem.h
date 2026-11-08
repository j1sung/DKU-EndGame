#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Network/CFMatchmakingFlow.h"
#include "CFMatchmakingSubsystem.generated.h"

class UCFEntrySubsystem;
class UCFLobbySubsystem;
class IHttpRequest;

DECLARE_MULTICAST_DELEGATE_OneParam(FCFMatchAssigned, const FString&);

/** 실제 티켓 API 제공자. EOS 세션 생성·참가는 후속 계층이 담당한다. */
UCLASS()
class DKUENDGAME_API UCFMatchmakingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    const FString& GetAssignedMatchId() const { return Flow.MatchId; }
    FCFMatchAssigned OnMatchAssigned;

private:
    UPROPERTY(Transient) TObjectPtr<UCFEntrySubsystem> Entry;
    UPROPERTY(Transient) TObjectPtr<UCFLobbySubsystem> Lobby;
    FCFMatchmakingFlow Flow;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
    FTimerHandle PollTimer;
    FDelegateHandle AuthenticationHandle;
    uint64 HttpSerial = 0;
    bool bDeinitializing = false;
    double NextAllowedAt = 0;

    UFUNCTION() void Start(int32 FlowId);
    UFUNCTION() void Cancel(int32 FlowId);
    UFUNCTION() void Recover(int32 FlowId);
    void AuthenticationRequired();
    void StopIo();
    void Send(ECFMatchRequest Kind);
    void Schedule(ECFMatchRequest Kind, float Delay);
    void Receive(int32 FlowId, uint64 Serial, ECFMatchRequest Kind, int32 Status,
        const FString& Body, const FString& Date, const FString& RetryAfter);
    void Apply(const FCFMatchUpdate& Update);
    bool IsCurrentResponse(int32 FlowId, uint64 Serial) const;
    static float RetryDelay(const FString& Header, const FDateTime& Now);
    friend class FCFMatchmakingResponseGuardTest;
};
