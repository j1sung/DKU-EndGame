#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ChickenAnimInstance.generated.h"

UENUM(BlueprintType)
enum class ECFBasicAnimState : uint8
{
    Idle, ChargeStart, ChargeLoop, JumpStart, AirLoop, Land
};

/** Reads character state on the game thread. Never applies movement or gameplay effects. */
UCLASS(Transient, Blueprintable)
class DKUENDGAME_API UChickenAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure, Category="Chicken|Animation", meta=(BlueprintThreadSafe))
    bool ShouldPlayState(ECFBasicAnimState State) const { return AnimationState == State; }

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    ECFBasicAnimState AnimationState = ECFBasicAnimState::Idle;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    float StateElapsed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    bool bInAir = false;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    bool bCharging = false;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    bool bFallen = false;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    float NormalizedCharge = 0.f;

    // These match AN_CF_Charge_Start (0.4s), AN_CF_Jump_Start (0.3s), AN_CF_Land (0.7s).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chicken|Animation", meta=(ClampMin="0.01"))
    float ChargeStartDuration = 0.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chicken|Animation", meta=(ClampMin="0.01"))
    float JumpStartDuration = 0.15f;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    float JumpStartPlayRate = 2.f;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Animation")
    float LandPlayRate = 3.5f;

private:
    TWeakObjectPtr<class AChickenCharacter> CachedCharacter;
    int32 LastTakeoffSerial = 0;
};
