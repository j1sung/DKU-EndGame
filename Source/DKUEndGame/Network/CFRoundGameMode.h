#pragma once

#include "CoreMinimal.h"
#include "Network/CFWaitingNetwork.h"
#include "CFRoundGameMode.generated.h"

class APlayerStart;

/** 라운드 맵의 4인 경기 준비. EOS 승인/자동 시작과 맵 전환은 별도 통합 단계다. */
UCLASS()
class DKUENDGAME_API ACFRoundGameMode : public ACFWaitingGameMode
{
    GENERATED_BODY()
public:
    ACFRoundGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void InitGameState() override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
    virtual void Logout(AController* Exiting) override;

    static int32 FindFreeSpawn(const TArray<FName>& SortedSpawns, const TSet<FName>& Reserved);

private:
    bool bRoundSetupReady = false;
    TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APlayerStart>> AssignedStarts;
};
