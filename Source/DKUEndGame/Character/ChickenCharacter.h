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
    virtual void MoveBlockedBy(const FHitResult& Impact) override;
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
    float GetNormalizedJumpCharge() const { return MaxJumpPower > MinJumpPower ? FMath::Clamp((CurrentJumpPower - MinJumpPower) / (MaxJumpPower - MinJumpPower), 0.f, 1.f) : 0.f; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // 착지 시 자동으로 다시 뛰도록 Landed 이벤트 오버라이드.
    virtual void Landed(const FHitResult& Hit) override;
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

private:
    FString DebugContext() const;
    void DebugInput(bool bForward, float Value);
    void DebugState();
    void DebugPostPush(const TCHAR* Observation);
    UFUNCTION()
    void DebugMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

    float DebugForwardInput = 0.f;
    float DebugRightInput = 0.f;
    uint8 DebugForwardBlock = 0;
    uint8 DebugRightBlock = 0;
    uint8 DebugPreviousState = 255;
    bool bDebugTiltAtLimit = false;
    uint64 DebugPushFrame = 0;
    int32 DebugPendingPushCount = 0;
    FVector DebugPendingImpulse = FVector::ZeroVector;
    FVector DebugPushVelocity = FVector::ZeroVector;

protected:

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
    float MaxBodyTilt = 60.0f / 35.0f;

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
    float MinJumpPower = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float MaxJumpPower = 700.0f;

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

    void ApplyCollisionResponse(const FVector& VelocityChange);
    TMap<TWeakObjectPtr<AChickenCharacter>, double> RecentCollisionTimes;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.01", Units="s"))
    float CollisionCooldown = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="1.0"))
    float MinCollisionSpeed = 30.f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0", ClampMax="1.0"))
    float CollisionPushScale = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0"))
    float MaxCollisionPushSpeed = 600.f;

    // Small upward velocity change proportional to the horizontal push.
    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0"))
    float CollisionLiftScale = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0"))
    float MaxCollisionLiftSpeed = 200.f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0"))
    float CollisionTiltScale = 0.004f;

    UPROPERTY(EditDefaultsOnly, Category="Chicken|Collision", meta=(ClampMin="0.0"))
    float MaxCollisionTiltImpulse = 1.2f;

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


protected :
    // 기울어지는 속도.
    // X: 전후, Y: 좌우. 기존 BodyTilt 좌표 규칙을 그대로 사용.
    FVector2D TiltVelocity = FVector2D::ZeroVector;

    // 입력으로 기울어지는 속도를 바꾸는 힘.
    UPROPERTY(EditAnywhere, Category = "Balance",
        meta = (ClampMin = "0.0"))
    float TiltInputAcceleration = 3.0f;

    // Assist only player input directed back toward the center, per axis.
    UPROPERTY(EditAnywhere, Category = "Balance", meta = (ClampMin = "1.0"))
    float CounterTiltInputMultiplier = 2.0f;

    // Extra braking only while counter-steering against outward tilt velocity.
    UPROPERTY(EditAnywhere, Category = "Balance", meta = (ClampMin = "0.0"))
    float CounterTiltBraking = 6.0f;

    // 기울어진 방향으로 더 넘어가게 하는 힘.
    // 실제 물리 중력이 아니라, 균형 게임용 중력 효과.
    UPROPERTY(EditAnywhere, Category = "Balance",
        meta = (ClampMin = "0.0"))
    float TiltGravityStrength = 2.0f;

    // 기울어지는 속도를 완화하는 저항.
    // 자세를 중심으로 복원하는 힘은 아님.
    UPROPERTY(EditAnywhere, Category = "Balance",
        meta = (ClampMin = "0.0"))
    float TiltDamping = 1.5f;
};
