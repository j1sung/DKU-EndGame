#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Network/CFWaitingNetwork.h"
#include "CFMatchRuntimeSubsystem.generated.h"

/** 서버의 경기 간 맵/색상/점수 상태와 클라이언트의 독립적인 최종 결과 보관. */
UCLASS()
class DKUENDGAME_API UCFMatchRuntimeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void BeginMatch(const TArray<FString>& Members);
    static TArray<int32> ShuffleFour(int32 Seed);
    static int32 MapNumber(const FString& Package);
    static FString MapPath(int32 Number);
    static FLinearColor PlayerColour(int32 Slot);
    FString CurrentMap() const;
    int32 RoundNumber = 1;
    int32 ColourFor(const FString& Player) const;
    bool TravelToNext(ACFWaitingGameState* State);
    void Restore(ACFWaitingGameState* State) const;
    void RestoreIdentity(APlayerState* State) const;
    bool IsTravelPending() const { return bTravelPending; }
    bool TravelTimedOut() const;
    void RoundStarted() { bTravelPending = false; }
    void FailTravel() { bTravelPending = false; }
    bool CacheResults(const TArray<FCFFinalStanding>& Rows);
    bool HasResults() const { return !FinalRows.IsEmpty() && !bResultConfirmed; }
    const TArray<FCFFinalStanding>& Results() const { return FinalRows; }
    bool CanConfirmResults() const;
    void ConfirmResults();
    void SuspendResultsForLogin();
private:
    TArray<int32> MapOrder;
    TMap<FString, int32> Colours, PlayerIds;
    TMap<FString, FString> PlayerNames;
    TArray<FCFPlayerRoundScore> Scores;
    TArray<FCFFinalStanding> FinalRows;
    int32 ScoredRound = 0;
    bool bTravelPending = false, bResultConfirmed = true;
    bool bResultsSuspended = false;
    double TravelDeadline = 0;
    UPROPERTY(Transient) TObjectPtr<class UCFCompletedMatchWidget> ResultWidget;
    FTSTicker::FDelegateHandle TickHandle;
    FDelegateHandle MapHandle;
    bool Tick(float Delta);
    void MapLoaded(UWorld* World);
};
