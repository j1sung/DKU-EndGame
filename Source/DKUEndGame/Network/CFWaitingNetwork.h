#pragma once

#include "CoreMinimal.h"
#include "DKUEndGameGameMode.h"
#include "DKUEndGamePlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "CFWaitingNetwork.generated.h"

UENUM(BlueprintType)
enum class ECFMatchPhase : uint8 { Waiting, Countdown, Playing, RoundResult, FinalResult };

/** Value snapshots keep completed scores valid after a PlayerState disconnects. */
USTRUCT(BlueprintType)
struct FCFPlayerRoundScore
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FString PlayerName;
    UPROPERTY(BlueprintReadOnly) int32 Rank = 0;
    UPROPERTY(BlueprintReadOnly) int32 RoundScore = 0;
    UPROPERTY(BlueprintReadOnly) int32 TotalScore = 0;
    UPROPERTY(BlueprintReadOnly) bool bConnected = true;
    UPROPERTY(BlueprintReadOnly) bool bParticipated = false;
    UPROPERTY(BlueprintReadOnly) bool bForfeited = false;
};

/** Final standings are separate from the last round's finishing order. */
USTRUCT(BlueprintType)
struct FCFFinalStanding
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FString PlayerName;
    UPROPERTY(BlueprintReadOnly) int32 Rank = 0;
    UPROPERTY(BlueprintReadOnly) int32 TotalScore = 0;
    UPROPERTY(BlueprintReadOnly) bool bConnected = true;
};

UCLASS()
class DKUENDGAME_API ACFWaitingGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UFUNCTION(BlueprintPure, Category="CF|Match") bool CanStartRound() const;
    UFUNCTION(BlueprintPure, Category="CF|Match") int32 GetCountdownSeconds() const;
    UFUNCTION(BlueprintPure, Category="CF|Match") int32 GetResultSeconds() const;
    UPROPERTY(Replicated,BlueprintReadOnly) TObjectPtr<APlayerState> HostPlayerState;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 RoomCapacity = 4;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 MinimumPlayers = 2;
    UPROPERTY(Replicated,BlueprintReadOnly) bool bRoomClosing = false;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") ECFMatchPhase Phase = ECFMatchPhase::Waiting;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") int32 CurrentRound = 0;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") int32 TotalRounds = 4;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") TObjectPtr<APlayerState> RoundWinner;
    // Snapshot survives the winner leaving while the result is displayed.
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") FString RoundWinnerName;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") bool bRoundDraw = false;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") double CountdownEndServerTime = 0;
    // The latest completed round; preserved while the following round is prepared/played.
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") int32 ScoredRound = 0;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") TArray<FCFPlayerRoundScore> Scoreboard;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") double NextRoundEndServerTime = 0;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") FString RoundProgressMessage;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") bool bFinalResultPending = false;
    UPROPERTY(Replicated,BlueprintReadOnly,Category="CF|Match") TArray<FCFFinalStanding> FinalStandings;
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
    bool TryReturnToWaiting(class ACFWaitingPlayerController* Requester);
    bool MarkPlayerEliminated(APlayerState* Player);
    void ScheduleEliminatedPawnRemoval(class AChickenCharacter* Pawn);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
protected:
    UPROPERTY(EditDefaultsOnly,Category="CF|Match") TSubclassOf<class AChickenCharacter> CombatPawnClass;
    UPROPERTY(EditDefaultsOnly,Category="CF|Match",meta=(ClampMin="1",ClampMax="10")) float CountdownDuration = 3.f;
    // Current fall clips are 1.4 s; allow their transition to complete as well.
    UPROPERTY(EditDefaultsOnly,Category="CF|Match",meta=(ClampMin="1.4",Units="s")) float EliminationDisplayTime = 1.6f;
    UPROPERTY(EditDefaultsOnly,Category="CF|Match",meta=(ClampMin="3",Units="s")) float RoundResultDuration = 6.f;
private:
    struct FEliminationRecord { int32 PlayerId; uint64 Frame; };
    TArray<int32> RoundPlayerIds;
    TArray<FEliminationRecord> Eliminations;
    TSet<int32> ForfeitedPlayers;
    TArray<TWeakObjectPtr<class AChickenCharacter>> RoundPawns;
    FTimerHandle NextRoundTimer;
    FTimerHandle CountdownTimer;
    FTimerHandle RoundResolutionTimer;
    TMap<TWeakObjectPtr<class AChickenCharacter>,FTimerHandle> PendingRemovals;
    bool PrepareRound(int32 RoundNumber,FString& Error);
    void AdvanceRound();
    void ScoreRound();
    void ShowFinalResults();
    void ClearRemovalTimers();
    void StopRoundProgression(const FString& Reason);
    void QueueRoundResolution();
    void ResolveRound();
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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetRoundEliminated(bool bEliminated);
    void StartRoundSpectating();
    UFUNCTION(BlueprintPure,Category="CF|Match") bool IsRoundEliminated() const { return bRoundEliminated; }
    UFUNCTION(BlueprintPure,Category="CF|Match") bool IsRoundSpectating() const { return bRoundSpectating; }
    UFUNCTION(BlueprintCallable,Category="CF|Match") void RequestStartGame();
    UFUNCTION(BlueprintCallable,Category="CF|Match") void RequestReturnToWaiting();
    UFUNCTION(Client,Reliable) void ClientReturnRejected(const FText& Reason);
    UFUNCTION(Client,Reliable) void ClientRoomClosed();
    UFUNCTION(Client,Reliable) void ClientStartRejected(const FText& Reason);
    void RegisterWaitingWidget(class UCFWaitingRoomWidget* Widget);
protected:
    UFUNCTION(Server,Reliable) void ServerSetNickname(const FString& Nickname);
    UFUNCTION(Server,Reliable) void ServerRequestStartGame();
    UFUNCTION(Server,Reliable) void ServerRequestReturnToWaiting();
private:
    UPROPERTY() TObjectPtr<class UCFWaitingRoomWidget> WaitingWidget;
    UPROPERTY() TSubclassOf<class UCFWaitingRoomWidget> WaitingWidgetClass;
    UPROPERTY() TObjectPtr<class UCFMatchStatusWidget> MatchWidget;
    UPROPERTY(ReplicatedUsing=OnRep_RoundParticipation) bool bRoundEliminated = false;
    UPROPERTY(ReplicatedUsing=OnRep_RoundParticipation) bool bRoundSpectating = false;
    UPROPERTY() TObjectPtr<class ACameraActor> RoundSpectatorCamera;
    bool bOwnSpectatorCamera = false;
    UFUNCTION() void OnRep_RoundParticipation();
    void UpdateSpectatorCamera();
    TWeakObjectPtr<APawn> LastInputPawn;
    ECFMatchPhase LastPhase = ECFMatchPhase::FinalResult;
    bool bMatchMenuOpen = false;
    FTimerHandle DisplayTimer;
    void UpdateMatchDisplay();
    void ToggleMatchMenu();
    void SetMatchInputMode();
};
