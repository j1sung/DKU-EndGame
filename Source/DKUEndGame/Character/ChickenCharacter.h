#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CFKnockdownTypes.h"
#include "ChickenCharacter.generated.h"

UCLASS()
class DKUENDGAME_API AChickenCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChickenCharacter();

    // Direction is relative to this character, not the camera or incoming hit.
    UFUNCTION(BlueprintCallable, Category="Chicken|Knockdown")
    bool StartKnockdown(ECFKnockdownDirection Direction);

    UFUNCTION(BlueprintPure, Category="Chicken|Knockdown")
    ECFKnockdownDirection GetKnockdownDirection() const { return KnockdownDirection; }

    UFUNCTION(BlueprintPure, Category="Chicken|Knockdown")
    bool IsKnockdownPending() const { return bKnockdownPending; }

    bool IsChargingJump() const { return bIsCharging; }
    bool IsFallen() const { return bIsFallen; }
    bool IsRecoveringFromLanding() const { return bInLandingRecovery; }
    int32 GetTakeoffSerial() const { return TakeoffSerial; }
    float GetLandingRecoveryTime() const { return LandingRecoveryTime; }
    float GetNormalizedJumpCharge() const { return MaxJumpPower > 0.f ? CurrentJumpPower / MaxJumpPower : 0.f; }

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // 착지 시 자동으로 다시 뛰도록 Landed 이벤트 오버라이드.
    virtual void Landed(const FHitResult& Hit) override;

    // 기본 점프.
    UPROPERTY(EditAnywhere, Category = "Movement|Hopping")
    float BaseHopForce = 350.0f; // 기본으로 콩콩 뛰는 힘

    // 착지 시 재생할 몽타주 변수.
	UPROPERTY(EditAnywhere, Category = "Animation")
	class UAnimMontage* LandMontage;

    // 이동 및 기울기.
    void MoveForward(float Value);
    void MoveRight(float Value);

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    FVector2D CurrentTilt;

    // 균형.
    void FallOver(); // 넘어짐 처리 함수.

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    bool bIsFallen = false;

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    float TiltAccumulator = 0.0f; // 기울이고 있는 시간 누적.

    UPROPERTY(EditAnywhere, Category = "Balance")
    float MaxTiltTime = 2.0f; // 이 시간(초) 이상 한 방향으로 기울이면 넘어짐.

    // 차징 점프.
    void StartJumpCharge();
    void ExecuteJump();

    UPROPERTY(BlueprintReadOnly, Category = "Jump")
    float CurrentJumpPower = 0.f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float MaxJumpPower = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float ChargeRate = 1000.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Jump")
    bool bIsCharging = false;

    // Gameplay owns the rebound delay; animation only visualizes it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Hopping", meta=(ClampMin="0.01", Units="s"))
    float LandingRecoveryTime = 0.2f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    bool bInLandingRecovery = false;

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    int32 TakeoffSerial = 0;

    float LandingRecoveryRemaining = 0.f;

    UPROPERTY(EditDefaultsOnly, Category="Balance")
    bool bEnableBalanceFailure = true;

    UPROPERTY(EditDefaultsOnly, Category="Movement|Hopping")
    bool bEnableAutoHop = true;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Knockdown")
    ECFKnockdownDirection KnockdownDirection = ECFKnockdownDirection::Forward;

    UPROPERTY(BlueprintReadOnly, Category="Chicken|Knockdown")
    bool bKnockdownPending = false;

    void BeginKnockdown();

    // 카메라.
    UPROPERTY(EditAnywhere, Category = "Camera")
    float RotationInterpSpeed = 7.0f;

};
