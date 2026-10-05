#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Character/CFKnockdownTypes.h"
#include "ChickenCharacter.generated.h"

class UPrimitiveComponent;

UCLASS()
class DKUENDGAME_API AChickenCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AChickenCharacter();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetRoundInputEnabled(bool bEnabled);
    void FinishRound();
    UFUNCTION(BlueprintPure, Category="Chicken|Match")
    bool IsRoundInputEnabled() const { return bRoundInputEnabled; }

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
    virtual void BeginPlay() override;
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

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    FVector2D CurrentTilt;

    // 균형.
	void UpdateBodyTilt(float DeltaTime);
    void FallOver(); // 넘어짐 처리 함수.

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Balance")
	FVector2D BodyTilt = FVector2D::ZeroVector; // 실제 몸체 기울기. 입력과 물리적 반응을 기반으로 계산됨.

    // 기울기 변화 속도.
	UPROPERTY(EditAnywhere, Category = "Balance")
    float TiltSpeed = 0.8f;

    //넘어지는 최대 기울기.
	UPROPERTY(EditAnywhere, Category = "Balance")
    float MaxBodyTilt = 1.0f;

    // 입력이 없을 때 중심으로 돌아오는 속도.
	UPROPERTY(EditAnywhere, Category = "Balance")
    float TiltRecoverySpeed = 1.5f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Balance")
    bool bIsFallen = false;

    UPROPERTY(BlueprintReadOnly, Category = "Balance")
    float TiltAccumulator = 0.0f; // 기울이고 있는 시간 누적.

    UPROPERTY(EditAnywhere, Category = "Balance")
    float MaxTiltTime = 2.0f; // 이 시간(초) 이상 한 방향으로 기울이면 넘어짐.

    // 차징 점프.
    void StartJumpCharge();
    void ExecuteJump();

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Jump")
    float CurrentJumpPower = 0.f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float MaxJumpPower = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float ChargeRate = 700.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Jump")
    bool bIsCharging = false;

    // Gameplay owns the rebound delay; animation only visualizes it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Hopping", meta=(ClampMin="0.01", Units="s"))
    float LandingRecoveryTime = 0.2f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
    bool bInLandingRecovery = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
    int32 TakeoffSerial = 0;

    float LandingRecoveryRemaining = 0.f;

    UPROPERTY(EditDefaultsOnly, Category="Balance")
    bool bEnableBalanceFailure = true;

    UPROPERTY(EditDefaultsOnly, Category="Movement|Hopping")
    bool bEnableAutoHop = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Chicken|Knockdown")
    ECFKnockdownDirection KnockdownDirection = ECFKnockdownDirection::Forward;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Chicken|Knockdown")
    bool bKnockdownPending = false;

	bool bLocalChargeHeld = false;

    void BeginKnockdown();
    UPROPERTY(ReplicatedUsing=OnRep_RoundInputEnabled, BlueprintReadOnly, Category="Chicken|Match")
    bool bRoundInputEnabled = true;
    UPROPERTY(ReplicatedUsing=OnRep_RoundInputEnabled)
    bool bRoundFinished = false;
    UFUNCTION() void OnRep_RoundInputEnabled();
    UFUNCTION(Server, Reliable) void ServerSetTilt(FVector2D Tilt);
    UFUNCTION(Server, Reliable) void ServerSetCharge(bool bPressed);

    // Capture the Blueprint mesh orientation before gameplay tilt starts.
    FRotator BaseMeshRotation = FRotator::ZeroRotator;

    // 카메라.
    UPROPERTY(EditAnywhere, Category = "Camera")
    float RotationInterpSpeed = 7.0f;

    // 닭끼리 충돌했을 때 호출.
    UFUNCTION()
    void OnChickenHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    // 충돌한 상대에게 전달할 넘어짐 방향 계산.
    ECFKnockdownDirection GetKnockdownDirectionFor(const AActor* OtherActor) const;
};
