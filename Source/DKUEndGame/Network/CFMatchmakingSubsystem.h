#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CFMatchmakingSubsystem.generated.h"

class UCFEntrySubsystem;
class UCFLobbySubsystem;
class IHttpRequest;

/** 서버 배정 결과. EOS 세션 생성/참가 계층이 다음 단계에서 소비한다. */
USTRUCT(BlueprintType)
struct FCFMatchAssignment
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString MatchId;
    UPROPERTY(BlueprintReadOnly) FString Status;
    UPROPERTY(BlueprintReadOnly) TArray<FString> MemberIds;
    UPROPERTY(BlueprintReadOnly) FString HostPlayerId;
    UPROPERTY(BlueprintReadOnly) int32 HostAttempt = 0;
    UPROPERTY(BlueprintReadOnly) FString SessionId;
    UPROPERTY(BlueprintReadOnly) FString DeadlineAt;
    UPROPERTY(BlueprintReadOnly) bool bIsHost = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCFMatchmakingChanged);

/** 인증된 티켓 API와 로비 UI 연결. EOS 세션 및 경기 시작 보고는 별도 책임이다. */
UCLASS()
class DKUENDGAME_API UCFMatchmakingSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void OnLobbyEntered();
    void OnLobbyExited();
    UPROPERTY(BlueprintReadOnly, Category="CF|Matchmaking") FCFMatchAssignment Assignment;
    UPROPERTY(BlueprintAssignable, Category="CF|Matchmaking") FCFMatchmakingChanged OnAssignmentChanged;
    UPROPERTY(BlueprintAssignable, Category="CF|Matchmaking") FCFMatchmakingChanged OnEntryRequired;

private:
    enum class ERequest : uint8 { None, Current, Create, Cancel, Match };
    UPROPERTY(Transient) TObjectPtr<UCFLobbySubsystem> Lobby;
    UPROPERTY(Transient) TObjectPtr<UCFEntrySubsystem> Entry;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
    FTimerHandle PollTimer;
    ERequest InFlight = ERequest::None;
    ERequest Pending = ERequest::None;
    uint64 Serial = 0;
    int32 FlowId = 0;
    int32 Errors = 0;
    double NextAllowedAt = 0;
    bool bWantCreate = false;
    bool bCreateUnresolved = false;
    bool bInLobby = false;
    bool bCancelAfterEntry = false;
    bool bReturnAfterEntry = false;
    FString PlayerId, TicketId, MatchId, CreateKey, CancelKey, CancelTicketId;

    UFUNCTION() void Start(int32 Id);
    UFUNCTION() void Cancel(int32 Id);
    UFUNCTION() void Recover(int32 Id);
    void BindLobby();
    bool IsCurrent(int32 Id) const;
    void StopRequests();
    void ClearTicket();
    void Schedule(ERequest Kind, float Delay = .01f);
    void DispatchPending();
    void Send(ERequest Kind);
    void Receive(int32 Id, uint64 RequestSerial, ERequest Kind, bool bConnected, int32 Status,
        const FString& Body, const FString& RetryAfter, const FString& ServerDate);
    void Uncertain();
    void Finish(const FText& Message);
    void RequireEntry();
    void RememberExitIntent();
    static float RetryDelay(const FString& Value, const FString& ServerDate);

#if WITH_DEV_AUTOMATION_TESTS
    // C++ 테스트 드라이버에서만 설치. 수동 명령은 Editor PIE 전용이며 제품 실행/Blueprint에는 노출하지 않는다.
    TFunction<bool(FString&, FString&, FString&)> TestAccess;
    TFunction<void(TSharedRef<IHttpRequest, ESPMode::ThreadSafe>)> TestSend;
#endif
    friend struct FCFMatchmakingTestDriver;
};
