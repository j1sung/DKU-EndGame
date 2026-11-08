#pragma once

#include "CoreMinimal.h"
#include "Network/CFWaitingNetwork.h"
#include "CFRoundGameMode.generated.h"

class APlayerStart;

/** 라운드 맵의 4인 스폰과 EOS 배정 명단/자동 시작 승인. */
UCLASS()
class DKUENDGAME_API ACFRoundGameMode : public ACFWaitingGameMode
{
    GENERATED_BODY()
public:
    ACFRoundGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void InitGameState() override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual bool TryStartRound(ACFWaitingPlayerController* Requester) override;
    bool StartAssignedRound(ACFWaitingPlayerController* Requester);
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void Logout(AController* Exiting) override;

    static int32 FindFreeSpawn(const TArray<FName>& SortedSpawns, const TSet<FName>& Reserved);

protected:
    virtual void OnRoundPlaying() override;

private:
    bool bRoundSetupReady = false;
    bool bAssignedMatch = false;
    bool bStartingAssignedRound = false;
    TSet<TWeakObjectPtr<AController>> AdmittedPlayers;
    TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStarts;
};
