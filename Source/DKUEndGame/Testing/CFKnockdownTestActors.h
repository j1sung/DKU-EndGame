#pragma once

#include "CoreMinimal.h"
#include "Character/CFKnockdownTypes.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "CFKnockdownTestActors.generated.h"

/** Test controls are additive. The original combat pawn and controller own movement. */
UCLASS()
class DKUENDGAME_API ACFKnockdownTestHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
protected:
    virtual void BeginPlay() override;
    void ResetTestCharacter();
};

UCLASS()
class DKUENDGAME_API ACFKnockdownTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACFKnockdownTestGameMode();
};

UCLASS()
class DKUENDGAME_API ACFKnockdownZone : public AActor
{
    GENERATED_BODY()
public:
    ACFKnockdownZone();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Knockdown Test")
    ECFKnockdownDirection Direction = ECFKnockdownDirection::Forward;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Knockdown Test")
    TObjectPtr<class UBoxComponent> Trigger;
private:
    UFUNCTION()
    void EnterZone(UPrimitiveComponent* Component, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool FromSweep, const FHitResult& Hit);
};
