#include "Character/ChickenCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Network/CFWaitingNetwork.h"
#include "Net/UnrealNetwork.h"

AChickenCharacter::AChickenCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

	// 보간.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 뛰는 동안 공중에서 이동이 가능하도록 AirCountrol 수치 설정.
	GetCharacterMovement()->AirControl = 0.8f;
}

void AChickenCharacter::BeginPlay()
{
    Super::BeginPlay();
    BaseMeshRotation = GetMesh()->GetRelativeRotation();
}

void AChickenCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (!bRoundInputEnabled) return;
    if (bIsFallen)
    {
        GetMesh()->SetRelativeRotation(BaseMeshRotation);
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        return;
    }
    if (bKnockdownPending) return;

	// 카메라 방향으로 회전.
	FRotator ControlRot = GetControlRotation();
	FRotator TargetRot = FRotator(0.f, ControlRot.Yaw, 0.f);
	FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, RotationInterpSpeed);
	if (HasAuthority() || IsLocallyControlled()) SetActorRotation(NewRot);

	// 균형 시스템 처리.
	if (HasAuthority() && !bIsFallen)
	{
		UpdateBodyTilt(DeltaTime);
	}
	// Balance failure may have started a grounded or queued knockdown this frame.
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	// 차징 점프 게이지 채우기.
	if (HasAuthority() && bIsCharging && !bIsFallen)
	{
		CurrentJumpPower = FMath::Clamp(CurrentJumpPower + (ChargeRate * DeltaTime), 0.0f, MaxJumpPower);
	}

	if (HasAuthority() && bInLandingRecovery)
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

	// Mesh 회전도 같이 연결.
	float Pitch = -BodyTilt.Y * 35.0f;
	float Roll = BodyTilt.X * 35.0f;

	FRotator TargetMeshRotation(Pitch, -90.f, Roll);

	FRotator CurrentMeshRotation = GetMesh()->GetRelativeRotation();

	FRotator NewMeshRotation = FMath::RInterpTo(CurrentMeshRotation, TargetMeshRotation, DeltaTime, 8.0f);
	GetMesh()->SetRelativeRotation(NewMeshRotation);
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
    if (!HasAuthority() || !bRoundInputEnabled) return;
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
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	const float Previous = CurrentTilt.X;
	CurrentTilt.X = FMath::Clamp(Value,-1.f,1.f);
    if (!HasAuthority() && Previous!=CurrentTilt.X) ServerSetTilt(CurrentTilt);

	// 공중에서 해당 방향으로 이동 입력 적용.
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AChickenCharacter::MoveRight(float Value)
{
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	const float Previous = CurrentTilt.Y;
	CurrentTilt.Y = FMath::Clamp(-Value,-1.f,1.f);
    if (!HasAuthority() && Previous!=CurrentTilt.Y) ServerSetTilt(CurrentTilt);

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
    if (!HasAuthority() || !bRoundInputEnabled || bIsFallen || bKnockdownPending || static_cast<uint8>(Direction) > 3) return false;
    KnockdownDirection = Direction;
    if (auto* Mode=GetWorld()->GetAuthGameMode<ACFWaitingGameMode>()) Mode->MarkPlayerEliminated(GetPlayerState());
    bIsCharging = false;
    bInLandingRecovery = false;
    LandingRecoveryRemaining = CurrentJumpPower = TiltAccumulator = 0.f;
    CurrentTilt = FVector2D::ZeroVector;
    BodyTilt = FVector2D::ZeroVector;
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
    // Full-mesh gameplay tilt must not rotate the authored directional fall pose.
    GetMesh()->SetRelativeRotation(BaseMeshRotation);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

void AChickenCharacter::StartJumpCharge()
{
    if (!HasAuthority()) { if (bRoundInputEnabled) ServerSetCharge(true); return; }
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	// 차징 시작하면 자동으로 뛰는 걸 멈춤.
	bIsCharging = true;
	CurrentJumpPower = 0.f;
}

void AChickenCharacter::ExecuteJump()
{
    if (!HasAuthority()) { if (bRoundInputEnabled) ServerSetCharge(false); return; }
	if (!bRoundInputEnabled || !bIsCharging || bIsFallen || bKnockdownPending) return;
	bIsCharging = false;

	//float TiltMagnitude = FMath::Clamp(CurrentTilt.Size(), 0.0f, 1.0f);
	float TiltMagnitude = FMath::Clamp(BodyTilt.Size(), 0.0f, 1.0f);
	FVector ForwardDir = GetActorForwardVector();
	FVector RightDir = GetActorRightVector();

	// 이동 입력이 없으면 위로만, 있으면 기울인 방향으로.
	//FVector TiltWorldDirection = TiltMagnitude > 0.01f ? (ForwardDir * CurrentTilt.X + RightDir * CurrentTilt.Y).GetSafeNormal() : FVector::ZeroVector;
	FVector TiltWorldDirection = TiltMagnitude > 0.01f ? (ForwardDir * BodyTilt.X + RightDir * BodyTilt.Y).GetSafeNormal() : FVector::ZeroVector;

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

void AChickenCharacter::UpdateBodyTilt(float DeltaTime)
{
	// 입력이 있으면 해당 방향으로 몸의 기울기 누적.
	if (CurrentTilt.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		BodyTilt += CurrentTilt * TiltSpeed * DeltaTime;
	}
	else
	{
		// 입력이 없으면 천천히 중심으로 복귀.
		BodyTilt = FMath::Vector2DInterpTo(BodyTilt, FVector2D::ZeroVector, DeltaTime, TiltRecoverySpeed);
	}

	// 최대 기울기 제한.
	BodyTilt.X = FMath::Clamp(BodyTilt.X, -MaxBodyTilt, MaxBodyTilt);
	BodyTilt.Y = FMath::Clamp(BodyTilt.Y, -MaxBodyTilt, MaxBodyTilt);

	// 어느 한 축이라도 한계를 넘으면 넘어짐 처리.
	if (bEnableBalanceFailure && (FMath::Abs(BodyTilt.X) >= MaxBodyTilt || FMath::Abs(BodyTilt.Y) >= MaxBodyTilt))
	{
		FallOver();
	}
}


void AChickenCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass,CurrentTilt);
    DOREPLIFETIME(ThisClass,BodyTilt);
    DOREPLIFETIME(ThisClass,bIsFallen);
    DOREPLIFETIME(ThisClass,CurrentJumpPower);
    DOREPLIFETIME(ThisClass,bIsCharging);
    DOREPLIFETIME(ThisClass,bInLandingRecovery);
    DOREPLIFETIME(ThisClass,TakeoffSerial);
    DOREPLIFETIME(ThisClass,KnockdownDirection);
    DOREPLIFETIME(ThisClass,bKnockdownPending);
    DOREPLIFETIME(ThisClass,bRoundInputEnabled);
}

void AChickenCharacter::SetRoundInputEnabled(bool bEnabled)
{
    if (!HasAuthority()) return;
    bRoundInputEnabled=bEnabled;
    CurrentTilt=BodyTilt=FVector2D::ZeroVector;
    bIsCharging=false; CurrentJumpPower=0;
    bInLandingRecovery=bEnabled;
    LandingRecoveryRemaining=FMath::Max(.01f,LandingRecoveryTime);
    ConsumeMovementInputVector();
    OnRep_RoundInputEnabled(); ForceNetUpdate();
}

void AChickenCharacter::OnRep_RoundInputEnabled()
{
    if (!bRoundInputEnabled)
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCharacterMovement()->DisableMovement();
        // Initial replication can arrive before BeginPlay captures the authored rotation.
        if (HasActorBegunPlay()) GetMesh()->SetRelativeRotation(BaseMeshRotation);
    }
    else if (!bIsFallen) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AChickenCharacter::ServerSetTilt_Implementation(FVector2D Tilt)
{
    if (!bRoundInputEnabled || bIsFallen || bKnockdownPending || Tilt.ContainsNaN()) return;
    CurrentTilt.X=FMath::Clamp(Tilt.X,-1.0,1.0);
    CurrentTilt.Y=FMath::Clamp(Tilt.Y,-1.0,1.0);
}

void AChickenCharacter::ServerSetCharge_Implementation(bool bPressed)
{
    if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;
    if (bPressed) StartJumpCharge(); else ExecuteJump();
    ForceNetUpdate();
}
