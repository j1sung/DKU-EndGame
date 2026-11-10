#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MovingPlatformEndpointPauseComponent.generated.h"

class UInterpToMovementComponent;

/** Adds endpoint dwell times without replacing the platform's existing movement component. */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class DKUENDGAME_API UMovingPlatformEndpointPauseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMovingPlatformEndpointPauseComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Platform|Endpoint Pause", meta=(ClampMin="0.0", Units="s"))
    float TopWaitSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Platform|Endpoint Pause", meta=(ClampMin="0.0", Units="s"))
    float BottomWaitSeconds = 0.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleReverse(const FHitResult& Hit, float Time);

    void ResumeMovement();

    UPROPERTY(Transient)
    TObjectPtr<UInterpToMovementComponent> Movement;

    FTimerHandle ResumeTimer;
    bool bWaitingAtEndpoint = false;
    bool bLastPointIsTop = true;
};
