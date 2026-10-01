#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CFWaitingAnimInstance.generated.h"

/** Presentation follows CharacterMovement velocity; animation never moves the actor. */
UCLASS(Blueprintable)
class DKUENDGAME_API UCFWaitingAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category="Waiting Locomotion")
    float GroundSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="Waiting Locomotion")
    float WalkPlayRate = 1.f;

    UPROPERTY(BlueprintReadOnly, Category="Waiting Locomotion")
    bool bIsWalking = false;

    /** Speed measured from AN_CF_WALK at unit mesh scale and 1x playback (cm/s). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Waiting Locomotion", meta=(ClampMin="1.0"))
    float AuthoredWalkSpeed = 65.75f;

    UFUNCTION(BlueprintPure, Category="Waiting Locomotion", meta=(BlueprintThreadSafe))
    bool ShouldWalk() const { return bIsWalking; }

    UFUNCTION(BlueprintPure, Category="Waiting Locomotion", meta=(BlueprintThreadSafe))
    bool ShouldStand() const { return !bIsWalking; }
};
