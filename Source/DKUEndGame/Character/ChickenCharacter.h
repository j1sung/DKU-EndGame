#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChickenCharacter.generated.h"

UCLASS()
class DKUENDGAME_API AChickenCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChickenCharacter();

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
    float CurrentJumpPower;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float MaxJumpPower = 1500.0f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float ChargeRate = 1000.0f;

    bool bIsCharging = false;

    // 카메라.
    UPROPERTY(EditAnywhere, Category = "Camera")
    float RotationInterpSpeed = 7.0f;

};
