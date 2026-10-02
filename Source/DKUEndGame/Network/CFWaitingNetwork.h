#pragma once

#include "CoreMinimal.h"
#include "DKUEndGameGameMode.h"
#include "DKUEndGamePlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "CFWaitingNetwork.generated.h"

UENUM(BlueprintType)
enum class ECFMatchPhase : uint8 { Waiting, Countdown, Playing, RoundResult, FinalResult };

UCLASS()
class DKUENDGAME_API ACFWaitingGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintPure, Category="CF|Match") bool CanStartRound() const;
    UFUNCTION(BlueprintPure, Category="CF|Match") int32 GetCountdownSeconds() const;
    UPROPERTY(Replicated,BlueprintReadOnly) TObjectPtr<APlayerState> HostPlayerState;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 RoomCapacity = 4;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 MinimumPlayers = 2;
    UPROPERTY(Replicated,BlueprintReadOnly) bool bRoomClosing = false;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") ECFMatchPhase Phase = ECFMatchPhase::Waiting;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") int32 CurrentRound = 0;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") int32 TotalRounds = 4;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") double CountdownEndServerTime = 0;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") TArray<TObjectPtr<APlayerState>> Participants;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") TArray<TObjectPtr<APlayerState>> AlivePlayers;
};

UCLASS()
class DKUENDGAME_API ACFWaitingGameMode : public ADKUEndGameGameMode
{
    GENERATED_BODY()
public:
    ACFWaitingGameMode();
    virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    bool TryStartRound(class ACFWaitingPlayerController* Requester);
    void MarkPlayerEliminated(APlayerState* Player);
protected:
    UPROPERTY(EditDefaultsOnly,Category="CF|Match") TSubclassOf<class AChickenCharacter> CombatPawnClass;
    UPROPERTY(EditDefaultsOnly,Category="CF|Match",meta=(ClampMin="1",ClampMax="10")) float CountdownDuration = 3.f;
private:
    FTimerHandle CountdownTimer;
    void BeginRound();
    void CancelCountdown();
};

UCLASS()
class DKUENDGAME_API ACFWaitingPlayerController : public ADKUEndGamePlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    UFUNCTION(BlueprintCallable,Category="CF|Match") void RequestStartGame();
    UFUNCTION(Client,Reliable) void ClientRoomClosed();
    UFUNCTION(Client,Reliable) void ClientStartRejected(const FText& Reason);
    void RegisterWaitingWidget(class UCFWaitingRoomWidget* Widget);
protected:
    UFUNCTION(Server,Reliable) void ServerSetNickname(const FString& Nickname);
    UFUNCTION(Server,Reliable) void ServerRequestStartGame();
private:
    UPROPERTY() TObjectPtr<class UCFWaitingRoomWidget> WaitingWidget;
    UPROPERTY() TSubclassOf<class UCFWaitingRoomWidget> WaitingWidgetClass;
    UPROPERTY() TObjectPtr<class UCFMatchStatusWidget> MatchWidget;
    TWeakObjectPtr<APawn> LastInputPawn;
    ECFMatchPhase LastPhase = ECFMatchPhase::FinalResult;
    bool bMatchMenuOpen = false;
    FTimerHandle DisplayTimer;
    void UpdateMatchDisplay();
    void ToggleMatchMenu();
    void SetMatchInputMode();
};
