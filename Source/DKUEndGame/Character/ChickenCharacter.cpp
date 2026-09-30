#include "Character/ChickenCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

AChickenCharacter::AChickenCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 보간.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 뛰는 동안 공중에서 이동이 가능하도록 AirCountrol 수치 설정.
	GetCharacterMovement()->AirControl = 0.8f;
}

void AChickenCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (bIsFallen)
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        return;
    }
    if (bKnockdownPending) return;

	// 카메라 방향으로 회전.
	FRotator ControlRot = GetControlRotation();
	FRotator TargetRot = FRotator(0.f, ControlRot.Yaw, 0.f);
	FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, RotationInterpSpeed);
	SetActorRotation(NewRot);

	// 균형 시스템 처리.
	if (bEnableBalanceFailure && !bIsFallen)
	{
		// 입력이 있는 상태인지 확인.
		if (CurrentTilt.SizeSquared() > 0.01f)
		{
			// 기울이고 있으면 시간 누적.
			TiltAccumulator += DeltaTime;

			// 한계치에 도달하면 넘어짐.
			if (TiltAccumulator >= MaxTiltTime)
			{
				FallOver();
			}
		}
		else
		{
			// 입력을 놓으면 기울기가 회복됨.
			TiltAccumulator = FMath::Max(0.0f, TiltAccumulator - (DeltaTime * 2.0f));
		}
	}

	// 차징 점프 게이지 채우기.
	if (bIsCharging && !bIsFallen)
	{
		CurrentJumpPower = FMath::Clamp(CurrentJumpPower + (ChargeRate * DeltaTime), 0.0f, MaxJumpPower);
	}

	if (bInLandingRecovery)
	{
		LandingRecoveryRemaining -= DeltaTime;
		if (LandingRecoveryRemaining <= 0.f)
		{
			bInLandingRecovery = false;
			if (bEnableAutoHop && !bIsFallen && !bIsCharging && GetCharacterMovement()->IsMovingOnGround())
			{
				LaunchCharacter(FVector(0.f, 0.f, BaseHopForce), false, true);
				++TakeoffSerial;
			}
		}
	}
}

void AChickenCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AChickenCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AChickenCharacter::MoveRight);

	PlayerInputComponent->BindAction("JumpCharge", IE_Pressed, this, &AChickenCharacter::StartJumpCharge);
	PlayerInputComponent->BindAction("JumpCharge", IE_Released, this, &AChickenCharacter::ExecuteJump);
}

// 바닥에 닿을 때마다 자동으로 호출.
void AChickenCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
    if (bKnockdownPending)
    {
        BeginKnockdown();
        return;
    }
	// ABP_Chicken plays Land in its state machine. Do not also play AM_Land.
	bInLandingRecovery = !bIsFallen;
	LandingRecoveryRemaining = FMath::Max(0.01f, LandingRecoveryTime);
}
// WASD 입력.
// 카메라 방향 기준 상체 기울기.
void AChickenCharacter::MoveForward(float Value)
{
	if (bIsFallen || bKnockdownPending) return;

	CurrentTilt.X = Value;

	// 공중에서 해당 방향으로 이동 입력 적용.
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AChickenCharacter::MoveRight(float Value)
{
	if (bIsFallen || bKnockdownPending) return;

	CurrentTilt.Y = Value;

	// 위랑 같다.
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AChickenCharacter::FallOver()
{
    // TODO: derive a direction from balance/hit data in the combat system.
    StartKnockdown(ECFKnockdownDirection::Forward);
}

bool AChickenCharacter::StartKnockdown(ECFKnockdownDirection Direction)
{
    if (bIsFallen || bKnockdownPending || static_cast<uint8>(Direction) > 3) return false;
    KnockdownDirection = Direction;
    bIsCharging = false;
    bInLandingRecovery = false;
    LandingRecoveryRemaining = CurrentJumpPower = TiltAccumulator = 0.f;
    CurrentTilt = FVector2D::ZeroVector;
    ConsumeMovementInputVector();
    if (GetCharacterMovement()->IsFalling())
    {
        // Keep gravity and vertical speed. Play the grounded clip on landing.
        bKnockdownPending = true;
        GetCharacterMovement()->Velocity.X = 0.f;
        GetCharacterMovement()->Velocity.Y = 0.f;
    }
    else
    {
        BeginKnockdown();
    }
    return true;
}

void AChickenCharacter::BeginKnockdown()
{
    bKnockdownPending = false;
    bIsFallen = true;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

void AChickenCharacter::StartJumpCharge()
{
	if (bIsFallen || bKnockdownPending) return;

	// 차징 시작하면 자동으로 뛰는 걸 멈춤.
	bIsCharging = true;
	CurrentJumpPower = 0.f;
}

void AChickenCharacter::ExecuteJump()
{
	if (!bIsCharging || bIsFallen || bKnockdownPending) return;
	bIsCharging = false;

	float TiltMagnitude = FMath::Clamp(CurrentTilt.Size(), 0.0f, 1.0f);
	FVector ForwardDir = GetActorForwardVector();
	FVector RightDir = GetActorRightVector();

	// 이동 입력이 없으면 위로만, 있으면 기울인 방향으로.
	FVector TiltWorldDirection = TiltMagnitude > 0.01f ? (ForwardDir * CurrentTilt.X + RightDir * CurrentTilt.Y).GetSafeNormal() : FVector::ZeroVector;

	// 기울기에 따른 수직/수평 힘 비율.
	float VertForceRatio = FMath::Lerp(1.0f, 0.3f, TiltMagnitude);
	float HorizForceRatio = FMath::Lerp(0.0f, 0.95f, TiltMagnitude);

	FVector LaunchVelocity = (TiltWorldDirection * HorizForceRatio) + (FVector::UpVector * VertForceRatio);
	LaunchVelocity *= CurrentJumpPower;

	bInLandingRecovery = false;
	LaunchCharacter(LaunchVelocity, true, true);
	++TakeoffSerial;

	CurrentJumpPower = 0.0f;
	TiltAccumulator = 0.0f;
}


