#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "CFSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECFSessionState : uint8 { Idle, Creating, Searching, Joining, Traveling, InRoom, Leaving };

USTRUCT(BlueprintType)
struct FCFRoomInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 Players = 0;
    UPROPERTY(BlueprintReadOnly) int32 Capacity = 2;
    UPROPERTY(BlueprintReadOnly) int32 Ping = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCFSessionChanged);

/** Local connection/session lifecycle. Replicated room state lives in CFWaitingGameState. */
UCLASS()
class DKUENDGAME_API UCFSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 RoomCapacity = 2;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="CF|Session") void HostRoom(const FString& Nickname);
    UFUNCTION(BlueprintCallable, Category="CF|Session") void FindRooms();
    UFUNCTION(BlueprintCallable, Category="CF|Session") void JoinRoom(int32 Index, const FString& Nickname);
    UFUNCTION(BlueprintCallable, Category="CF|Session") void JoinAddress(const FString& Address, const FString& Nickname);
    UFUNCTION(BlueprintCallable, Category="CF|Session") void CancelSearch();
    UFUNCTION(BlueprintCallable, Category="CF|Session") void LeaveRoom();
    UFUNCTION(BlueprintPure, Category="CF|Session") bool IsBusy() const;
    UFUNCTION(BlueprintCallable, Category="CF|Session") bool SetNickname(const FString& Nickname);
    UFUNCTION(BlueprintPure, Category="CF|Session") static FString CleanNickname(const FString& Nickname);
    void NotifyEnteredRoom();
    void HostClosedRoom();

    UPROPERTY(BlueprintAssignable) FCFSessionChanged OnChanged;
    UPROPERTY(BlueprintReadOnly) ECFSessionState State = ECFSessionState::Idle;
    UPROPERTY(BlueprintReadOnly) FText StatusMessage;
    UPROPERTY(BlueprintReadOnly) FString LocalNickname;
    UPROPERTY(BlueprintReadOnly) TArray<FCFRoomInfo> Rooms;

private:
    IOnlineSessionPtr Sessions;
    TSharedPtr<FOnlineSessionSearch> Search;
    TArray<FOnlineSessionSearchResult> Results;
    FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle;
    FDelegateHandle NetworkHandle, TravelHandle, MapHandle;
    FTimerHandle Deadline, CleanupDelay;
    FText ReturnMessage;
    bool bReturningToMenu = false;
    bool bDeinitializing = false;

    bool GetSessions();
    void SetState(ECFSessionState Next, const FText& Message);
    void Error(const TCHAR* Message);
    void ArmDeadline(float Seconds);
    void OnTimeout();
    void ClearOperationDelegates();
    void OnCreated(FName Name, bool bSuccess);
    void OnFound(bool bSuccess);
    void OnJoined(FName Name, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroyed(FName Name, bool bSuccess);
    void TravelToHost(const FString& URL);
    void BeginReturn(const FText& Reason);
    void DestroyLocalSession();
    void FinishReturn();
    void OnMapLoaded(UWorld* World);
    void OnNetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Detail);
    void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Detail);
    bool OwnsFailure(UWorld* World, UNetDriver* Driver) const;
};
