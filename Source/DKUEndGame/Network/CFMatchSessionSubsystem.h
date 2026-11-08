#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineReplStructs.h"
#include "Network/CFMatchSessionContract.h"
#include "CFMatchSessionSubsystem.generated.h"

class UCFEntrySubsystem;
class UCFMatchmakingSubsystem;
class UCFLobbySubsystem;
class IHttpRequest;
class ACFWaitingPlayerController;

/** 티켓 배정 이후 실제 EOS 세션과 매치 API의 단일 소유자. */
UCLASS()
class DKUENDGAME_API UCFMatchSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool IsActive() const { return !MatchId.IsEmpty(); }
    bool IsStopping() const { return bAborting || bCleaning; }
    bool CanAdmit(const FUniqueNetIdRepl& UniqueId, const FString& Options) const;
    bool CanAutoStart() const;
    void NotifyPlaying();
    void NotifyPlayerLeft();
    void RequestLeave();
    static FString ProductUserId(const FUniqueNetId& Id);

private:
    enum class EConnection : uint8 { None, Creating, HostTravel, Searching, Joining, ClientTravel, Connected };
    UPROPERTY(Transient) TObjectPtr<UCFEntrySubsystem> Entry;
    UPROPERTY(Transient) TObjectPtr<UCFMatchmakingSubsystem> Tickets;
    UPROPERTY(Transient) TObjectPtr<UCFLobbySubsystem> Lobby;
    FString MatchId, PlayerId, PublishedSessionId, PendingAction, PendingKey;
    FCFSessionMatch Match;
    EConnection Connection = EConnection::None;
    IOnlineSessionPtr Sessions;
    TSharedPtr<FOnlineSessionSearch> Search;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Http;
    FDelegateHandle AssignmentHandle, AuthenticationHandle, NetworkHandle, TravelHandle;
    FDelegateHandle CreateHandle, FindHandle, JoinHandle, StartHandle, DestroyHandle;
    FTSTicker::FDelegateHandle TickHandle;
    uint64 Generation = 0, HttpSerial = 0;
    double NextRequestAt = 0, NextSearchAt = 0, ConnectionDeadline = 0, CleanupDeadline = 0;
    int32 DisplayFlow = 0;
    bool bHost = false, bRegistered = false, bJoined = false, bPlaying = false, bStarted = false;
    bool bSessionStarted = false, bStartPending = false, bAborting = false, bCleaning = false;
    bool bLocalCleaned = false, bTerminal = false, bDeinitializing = false;
    bool bNeedSnapshot = true;
    bool bWriteDispatched = false;

    void Assigned(const FString& Id);
    bool Tick(float Delta);
    void Send(const FString& Action);
    void Receive(uint64 Serial, uint64 Epoch, const FString& Action, int32 Status, const FString& Body,
        const FString& RetryAfter, const FString& Date);
    void QueueWrite(const FString& Action);
    void CancelHttp();
    void Advance();
    bool LocalRoundReady() const;
    void Create();
    void Find();
    void ClearSessionDelegates();
    void Abort(const TCHAR* Reason);
    void BeginCleanup();
    void FinishCleanup();
    void AuthenticationRequired();
    void NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Detail);
    void TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Detail);
    bool OwnsWorld(UWorld* World, UNetDriver* Driver) const;
    friend class FCFMatchSessionResponseTest;
};
