#include "Character/ChickenCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Network/CFWaitingNetwork.h"
#include "Net/UnrealNetwork.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogChickenCollision, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogChickenTilt, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogChickenMovement, Log, All);

namespace
{
    TAutoConsoleVariable<int32> CVarChickenDebugEvents(
        TEXT("cf.Debug.Events"), 1, TEXT("Chicken event logs: 0 off, 1 on."));
    TAutoConsoleVariable<int32> CVarChickenDebugFrames(
        TEXT("cf.Debug.Frames"), 0, TEXT("Repeated logs: bit 1 collision, bit 2 tilt, bit 4 movement; 7 all."));
}

#define CF_EVENT(Category, Format, ...) \
    do { if (CVarChickenDebugEvents.GetValueOnGameThread() != 0) \
        { UE_LOG(Category, Log, TEXT("%s " Format), *DebugContext(), ##__VA_ARGS__); } } while (false)
#define CF_FRAME(Bit, Category, Format, ...) \
    do { if ((CVarChickenDebugFrames.GetValueOnGameThread() & Bit) != 0) \
        { UE_LOG(Category, Log, TEXT("%s " Format), *DebugContext(), ##__VA_ARGS__); } } while (false)

FString AChickenCharacter::DebugContext() const
{
    return FString::Printf(TEXT("Actor=%s World=%s Frame=%llu Time=%.3f Role=%d Local=%d"),
        *GetName(), *GetNameSafe(GetWorld()), static_cast<unsigned long long>(GFrameCounter),
        GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, static_cast<int32>(GetLocalRole()), IsLocallyControlled());
}

void AChickenCharacter::DebugInput(bool bForward, float Value)
{
    float& Previous = bForward ? DebugForwardInput : DebugRightInput;
    uint8& PreviousBlock = bForward ? DebugForwardBlock : DebugRightBlock;
    const uint8 Block = (!bRoundInputEnabled ? 1 : 0) | (bIsFallen ? 2 : 0)
        | (bKnockdownPending ? 4 : 0) | (bIsCharging ? 8 : 0);
    if (Previous != Value || (Value != 0.f && PreviousBlock != Block))
    {
        CF_EVENT(LogChickenMovement, "Input Axis=%s Raw=%.3f Released=%d BlockMask=%u Direction=%s Velocity=%s Fallen=%d",
            bForward ? TEXT("WS") : TEXT("AD"), Value, Value == 0.f, static_cast<uint32>(Block),
            *((bForward ? GetActorForwardVector() : GetActorRightVector()) * Value).ToString(),
            *GetVelocity().ToString(), bIsFallen);
        CF_EVENT(LogChickenTilt, "Input Axis=%s Raw=%.3f BalanceValue=%.3f Released=%d Body=%s TiltVelocity=%s TiltBlocked=%d",
            bForward ? TEXT("WS") : TEXT("AD"), Value, bForward ? Value : -Value,
            Value == 0.f, *BodyTilt.ToString(), *TiltVelocity.ToString(), (Block & 7) != 0);
    }
    Previous = Value;
    PreviousBlock = Block;
}

void AChickenCharacter::DebugState()
{
    const uint8 State = (bIsFallen ? 1 : 0) | (bKnockdownPending ? 2 : 0) | (bRoundInputEnabled ? 4 : 0);
    if (DebugPreviousState != State)
    {
        CF_EVENT(LogChickenTilt, "State PreviousMask=%u Mask=%u Fallen=%d Pending=%d Round=%d Direction=%d Velocity=%s",
            static_cast<uint32>(DebugPreviousState), static_cast<uint32>(State), bIsFallen,
            bKnockdownPending, bRoundInputEnabled, static_cast<int32>(KnockdownDirection), *GetVelocity().ToString());
        DebugPreviousState = State;
    }
}

void AChickenCharacter::DebugPostPush(const TCHAR* Observation)
{
    if (DebugPendingPushCount > 0 && GFrameCounter > DebugPushFrame)
    {
        CF_EVENT(LogChickenCollision, "PostPush FirstFrame=%llu Count=%d RequestedDeltaV=%s Before=%s Observed=%s Position=%s Fallen=%d Pending=%d Mode=%s Observation=%s",
            static_cast<unsigned long long>(DebugPushFrame), DebugPendingPushCount,
            *DebugPendingImpulse.ToString(), *DebugPushVelocity.ToString(), *GetVelocity().ToString(),
            *GetActorLocation().ToString(), bIsFallen, bKnockdownPending, *GetCharacterMovement()->GetMovementName(), Observation);
        DebugPendingPushCount = 0;
        DebugPendingImpulse = FVector::ZeroVector;
    }
}

void AChickenCharacter::DebugMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    DebugPostPush(TEXT("MovementUpdated"));
    CF_FRAME(4, LogChickenMovement, "MovementUpdate Dt=%.4f Input=%s Acceleration=%s OldVelocity=%s Velocity=%s Speed=%.3f DeltaLocation=%s Mode=%s Fallen=%d",
        DeltaSeconds, *GetLastMovementInputVector().ToString(), *GetCharacterMovement()->GetCurrentAcceleration().ToString(),
        *OldVelocity.ToString(), *GetVelocity().ToString(), GetVelocity().Size(),
        *(GetActorLocation() - OldLocation).ToString(), *GetCharacterMovement()->GetMovementName(), bIsFallen);
}

void AChickenCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
    CF_EVENT(LogChickenMovement, "Mode Previous=%s PreviousCustom=%u Current=%s Custom=%u Velocity=%s Fallen=%d",
        *UEnum::GetValueAsString(PrevMovementMode), static_cast<uint32>(PreviousCustomMode),
        *GetCharacterMovement()->GetMovementName(), static_cast<uint32>(GetCharacterMovement()->CustomMovementMode),
        *GetVelocity().ToString(), bIsFallen);
}

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
    GetCharacterMovement()->MaxWalkSpeed = 275.f;
}

void AChickenCharacter::BeginPlay()
{
    Super::BeginPlay();

	/* 컨트롤러 디버깅 로그 */
	//if (AController* CurrentController = GetController() )
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("%s %s"), *CurrentController->GetName(), *CurrentController->GetClass()->GetName());
	//}

    BaseMeshRotation = GetMesh()->GetRelativeRotation();
    OnCharacterMovementUpdated.AddDynamic(this, &AChickenCharacter::DebugMovementUpdated);

	// 닭의 Capsule 충돌 이벤트 등록.
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &AChickenCharacter::OnChickenHit);

	// 카메라 상하 회전 제한.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -25.f;
			PC->PlayerCameraManager->ViewPitchMax = 20.f;
		}
	}
}

void AChickenCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    DebugState();
    if (GetCharacterMovement()->MovementMode == MOVE_None)
        DebugPostPush(TEXT("MovementDisabled"));

    if (!bRoundInputEnabled) return;
    if (bIsFallen)
    {
        if (GetWorld()->GetGameState<ACFWaitingGameState>())
            GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
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
		CurrentJumpPower = FMath::Clamp(CurrentJumpPower + (ChargeRate * DeltaTime), MinJumpPower, FMath::Max(MinJumpPower, MaxJumpPower));
	}

	if (HasAuthority() && bInLandingRecovery)
	{
		LandingRecoveryRemaining -= DeltaTime;
		if (LandingRecoveryRemaining <= 0.f)
		{
			bInLandingRecovery = false;
			if (bEnableAutoHop && !bIsFallen && !bIsCharging && GetCharacterMovement()->IsMovingOnGround())
			{
				CF_EVENT(LogChickenMovement, "JumpLaunch Kind=AutoHop Z=%.3f Before=%s SerialNext=%d",
                    BaseHopForce, *GetVelocity().ToString(), TakeoffSerial + 1);
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

	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("JumpCharge", IE_Pressed, this, &AChickenCharacter::StartJumpCharge);
	PlayerInputComponent->BindAction("JumpCharge", IE_Released, this, &AChickenCharacter::ExecuteJump);
}

// 바닥에 닿을 때마다 자동으로 호출.
void AChickenCharacter::Landed(const FHitResult& Hit)
{
	CF_EVENT(LogChickenMovement, "Landed Floor=%s Impact=%s Velocity=%s Mode=%s Fallen=%d Pending=%d",
        *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString(), *GetVelocity().ToString(),
        *GetCharacterMovement()->GetMovementName(), bIsFallen, bKnockdownPending);
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
    DebugInput(true, Value);
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	const float Previous = CurrentTilt.X;
	CurrentTilt.X = FMath::Clamp(Value,-1.f,1.f);

    if (!HasAuthority() && Previous!=CurrentTilt.X) ServerSetTilt(CurrentTilt);

	if (bIsCharging) return;

	// 공중에서 해당 방향으로 이동 입력 적용.
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AChickenCharacter::MoveRight(float Value)
{
    DebugInput(false, Value);
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;
	//if (!GetCharacterMovement()->IsMovingOnGround()) return;

	const float Previous = CurrentTilt.Y;
	
	CurrentTilt.Y = FMath::Clamp(-Value,-1.f,1.f);
    if (!HasAuthority() && Previous!=CurrentTilt.Y) ServerSetTilt(CurrentTilt);

	if (bIsCharging) return;

	// 위랑 같다.
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AChickenCharacter::FallOver()
{
    // BodyTilt.Y uses the opposite sign to the character's local right axis.
    const ECFKnockdownDirection Direction = FMath::Abs(BodyTilt.X) >= FMath::Abs(BodyTilt.Y)
        ? (BodyTilt.X >= 0.f ? ECFKnockdownDirection::Forward : ECFKnockdownDirection::Back)
        : (BodyTilt.Y >= 0.f ? ECFKnockdownDirection::Left : ECFKnockdownDirection::Right);
    const bool bAccepted = StartKnockdown(Direction);
    CF_FRAME(2, LogChickenTilt, "BalanceFailure Accepted=%d Direction=%d Fallen=%d Pending=%d",
        bAccepted, static_cast<int32>(Direction), bIsFallen, bKnockdownPending);
}

bool AChickenCharacter::StartKnockdown(ECFKnockdownDirection Direction)
{
    if (!HasAuthority() || !bRoundInputEnabled || bIsFallen || bKnockdownPending || static_cast<uint8>(Direction) > 3) return false;
    if (auto* Mode=GetWorld()->GetAuthGameMode<ACFWaitingGameMode>())
        if (!Mode->MarkPlayerEliminated(GetPlayerState())) return false;
    KnockdownDirection = Direction;
    bIsCharging = false;
    bInLandingRecovery = false;
    LandingRecoveryRemaining = CurrentJumpPower = TiltAccumulator = 0.f;
    
	CurrentTilt = FVector2D::ZeroVector;
    BodyTilt = FVector2D::ZeroVector;
	TiltVelocity = FVector2D::ZeroVector;

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
    DebugState();
    ForceNetUpdate();
    return true;
}

void AChickenCharacter::BeginKnockdown()
{
    bKnockdownPending = false;
    bIsFallen = true;
    DebugState();
    // Full-mesh gameplay tilt must not rotate the authored directional fall pose.
    GetMesh()->SetRelativeRotation(BaseMeshRotation);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    if (auto* Mode=GetWorld()->GetAuthGameMode<ACFWaitingGameMode>())
    {
        // Keep the floor collision, but a fallen player must not obstruct survivors.
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
        Mode->ScheduleEliminatedPawnRemoval(this);
    }
    ForceNetUpdate();
}

void AChickenCharacter::StartJumpCharge()
{
    CF_EVENT(LogChickenMovement, "ChargePressed Before=%s Round=%d Fallen=%d Pending=%d",
        *GetVelocity().ToString(), bRoundInputEnabled, bIsFallen, bKnockdownPending);
	// 입력하자마자 로컬에서 잠금.
	bLocalChargeHeld = true;

	GetCharacterMovement()->StopMovementImmediately();
	ConsumeMovementInputVector();

    if (!HasAuthority()) { if (bRoundInputEnabled) ServerSetCharge(true); return; }
	if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;

	// 차징 시작하면 자동으로 뛰는 걸 멈춤.
	bIsCharging = true;
	CurrentJumpPower = MinJumpPower;
}

void AChickenCharacter::ExecuteJump()
{
    CF_EVENT(LogChickenMovement, "ChargeReleased Power=%.3f Charging=%d Round=%d Fallen=%d Pending=%d",
        CurrentJumpPower, bIsCharging, bRoundInputEnabled, bIsFallen, bKnockdownPending);
    if (!HasAuthority()) { if (bRoundInputEnabled) ServerSetCharge(false); return; }
	if (!bRoundInputEnabled || !bIsCharging || bIsFallen || bKnockdownPending) return;
	bIsCharging = false;

	//float TiltMagnitude = FMath::Clamp(CurrentTilt.Size(), 0.0f, 1.0f);
	float TiltMagnitude = FMath::Clamp(BodyTilt.Size(), 0.0f, 1.0f);
	FVector ForwardDir = GetActorForwardVector();
	FVector RightDir = GetActorRightVector();

	// 이동 입력이 없으면 위로만, 있으면 기울인 방향으로.
	FVector TiltWorldDirection = TiltMagnitude > 0.01f ? (ForwardDir * BodyTilt.X + (-RightDir) * BodyTilt.Y).GetSafeNormal() : FVector::ZeroVector;

	// 기울기에 따른 수직/수평 힘 비율.
	float VertForceRatio = FMath::Lerp(1.0f, 0.3f, TiltMagnitude);
	float HorizForceRatio = FMath::Lerp(0.0f, 0.95f, TiltMagnitude);

	FVector LaunchVelocity = (TiltWorldDirection * HorizForceRatio) + (FVector::UpVector * VertForceRatio);
	LaunchVelocity *= CurrentJumpPower;

	bInLandingRecovery = false;
	CF_EVENT(LogChickenMovement, "JumpLaunch Kind=Charged Power=%.3f Body=%s Launch=%s Before=%s SerialNext=%d",
        CurrentJumpPower, *BodyTilt.ToString(), *LaunchVelocity.ToString(), *GetVelocity().ToString(), TakeoffSerial + 1);
    LaunchCharacter(LaunchVelocity, true, true);
	++TakeoffSerial;

	CurrentJumpPower = 0.0f;
	TiltAccumulator = 0.0f;
}

void AChickenCharacter::UpdateBodyTilt(float DeltaTime)
{
	//// 입력이 있으면 해당 방향으로 몸의 기울기 누적.
	//if (CurrentTilt.SizeSquared() > KINDA_SMALL_NUMBER)
	//{
	//	BodyTilt += CurrentTilt * TiltSpeed * DeltaTime;
	//}
	//else
	//{
	//	// 입력이 없으면 천천히 중심으로 복귀.
	//	BodyTilt = FMath::Vector2DInterpTo(BodyTilt, FVector2D::ZeroVector, DeltaTime, TiltRecoverySpeed);
	//}

	//// 최대 기울기 제한.
	//BodyTilt.X = FMath::Clamp(BodyTilt.X, -MaxBodyTilt, MaxBodyTilt);
	//BodyTilt.Y = FMath::Clamp(BodyTilt.Y, -MaxBodyTilt, MaxBodyTilt);

	//// 어느 한 축이라도 한계를 넘으면 넘어짐 처리.
	//if (bEnableBalanceFailure && (FMath::Abs(BodyTilt.X) >= MaxBodyTilt || FMath::Abs(BodyTilt.Y) >= MaxBodyTilt))
	//{
	//	FallOver();
	//}
	if (DeltaTime <= 0.f || MaxBodyTilt <= 0.f)
	{
		return;
	}

	// 대각선 입력이 단일 방향보다 강해지지 않도록 제한.
	FVector2D BalanceInput = CurrentTilt;

	if (BalanceInput.SizeSquared() > 1.0f)
	{
		BalanceInput.Normalize();
	}

	// 프레임 시간이 길 때 계산이 튀지 않도록 나누어 계산.
	const float MaxStep = 1.0f / 120.0f;
	const int32 StepCount = FMath::Max(
		1,
		FMath::CeilToInt(DeltaTime / MaxStep)
	);

	const float StepTime = DeltaTime / StepCount;
    CF_FRAME(2, LogChickenTilt, "BalanceBegin Dt=%.4f Steps=%d Raw=%s Normalized=%s Body=%s TiltVelocity=%s BaseInputAcceleration=%s GravityAcceleration=%s BaseDampingFactor=%.6f RestoringForce=0 InputReleased=%d Limit=%.3f",
        DeltaTime, StepCount, *CurrentTilt.ToString(), *BalanceInput.ToString(), *BodyTilt.ToString(),
        *TiltVelocity.ToString(), *(BalanceInput * TiltInputAcceleration).ToString(),
        *(BodyTilt * TiltGravityStrength).ToString(), FMath::Exp(-TiltDamping * StepTime),
        CurrentTilt.IsNearlyZero(), MaxBodyTilt);

	for (int32 Step = 0; Step < StepCount; ++Step)
	{
		// 입력: 누른 방향으로 기울어지는 속도를 변경.
        FVector2D InputAcceleration = BalanceInput * TiltInputAcceleration;
        // Re-evaluate each substep so the assist ends as soon as an axis crosses center.
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            const bool bCounterSteering = BalanceInput[Axis] * BodyTilt[Axis] < 0.0;
            if (bCounterSteering)
            {
                InputAcceleration[Axis] *= CounterTiltInputMultiplier;
                // Brake outward momentum, but do not slow the return toward center.
                if (TiltVelocity[Axis] * BodyTilt[Axis] > 0.0)
                {
                    TiltVelocity[Axis] *= FMath::Exp(-CounterTiltBraking * StepTime);
                }
            }
        }

		// 중력 효과: 이미 기울어진 방향으로 더 넘어가게 함.
		// 기울기가 클수록 힘도 커짐.
		const FVector2D GravityAcceleration =
			BodyTilt * TiltGravityStrength;

		// 가속도 -> 속도.
		TiltVelocity +=
			(InputAcceleration + GravityAcceleration) * StepTime;

		// 속도에 저항 적용. 중심으로 당기지는 않음.
		TiltVelocity *= FMath::Exp(-TiltDamping * StepTime);

		// 속도 -> 기울기.
		BodyTilt += TiltVelocity * StepTime;

		// 기존 코드와 같은 축별 한계 판정.
		const bool bReachedLimit =
			FMath::Abs(BodyTilt.X) >= MaxBodyTilt ||
			FMath::Abs(BodyTilt.Y) >= MaxBodyTilt;

		BodyTilt.X = FMath::Clamp(
			BodyTilt.X, -MaxBodyTilt, MaxBodyTilt
		);

		BodyTilt.Y = FMath::Clamp(
			BodyTilt.Y, -MaxBodyTilt, MaxBodyTilt
		);

        if (bReachedLimit)
        {
            if (!bDebugTiltAtLimit)
            {
                CF_EVENT(LogChickenTilt, "Threshold Body=%s TiltVelocity=%s Limit=%.3f FailureEnabled=%d",
                    *BodyTilt.ToString(), *TiltVelocity.ToString(), MaxBodyTilt, bEnableBalanceFailure);
            }
            bDebugTiltAtLimit = true;
			TiltVelocity = FVector2D::ZeroVector;

			if (bEnableBalanceFailure)
			{
				FallOver();
			}

            return;
        }
        bDebugTiltAtLimit = false;
    }
    CF_FRAME(2, LogChickenTilt, "BalanceEnd Body=%s TiltVelocity=%s Fallen=%d InputReleased=%d",
        *BodyTilt.ToString(), *TiltVelocity.ToString(), bIsFallen, CurrentTilt.IsNearlyZero());
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
    DOREPLIFETIME(ThisClass,bRoundFinished);
}

void AChickenCharacter::SetRoundInputEnabled(bool bEnabled)
{
    if (!HasAuthority()) return;
    RecentCollisionTimes.Reset();
    bRoundFinished=false;
    bRoundInputEnabled=bEnabled;

    CurrentTilt=BodyTilt=FVector2D::ZeroVector;
	TiltVelocity = FVector2D::ZeroVector;

    bIsCharging=false; CurrentJumpPower=0;
    bInLandingRecovery=bEnabled;
    LandingRecoveryRemaining=FMath::Max(.01f,LandingRecoveryTime);
    ConsumeMovementInputVector();
    OnRep_RoundInputEnabled(); ForceNetUpdate();
}

void AChickenCharacter::FinishRound()
{
    if (!HasAuthority()) return;
    bRoundFinished=true;
    bRoundInputEnabled=false;
    CurrentTilt=BodyTilt=FVector2D::ZeroVector;
	TiltVelocity = FVector2D::ZeroVector;
    bIsCharging=bInLandingRecovery=false;
    CurrentJumpPower=0.f;
    ConsumeMovementInputVector();
    OnRep_RoundInputEnabled();
    ForceNetUpdate();
}

void AChickenCharacter::OnRep_RoundInputEnabled()
{
    if (!bRoundInputEnabled)
    {
        auto* Movement=GetCharacterMovement();
        if (bRoundFinished && Movement->IsFalling())
        {
            // The winner may still be airborne. Disable combat but allow gravity to land them.
            Movement->Velocity.X=Movement->Velocity.Y=0.f;
        }
        else
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }
        // Initial replication can arrive before BeginPlay captures the authored rotation.
        if (HasActorBegunPlay()) GetMesh()->SetRelativeRotation(BaseMeshRotation);
    }
    else if (!bIsFallen) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AChickenCharacter::ServerSetTilt_Implementation(FVector2D Tilt)
{
    if (!bRoundInputEnabled || bIsFallen || bKnockdownPending || Tilt.ContainsNaN()) return;
    if (Tilt != CurrentTilt)
    {
        CF_EVENT(LogChickenTilt, "ServerInput Received=%s Previous=%s Body=%s",
            *Tilt.ToString(), *CurrentTilt.ToString(), *BodyTilt.ToString());
    }
    CurrentTilt.X=FMath::Clamp(Tilt.X,-1.0,1.0);
    CurrentTilt.Y=FMath::Clamp(Tilt.Y,-1.0,1.0);
}

void AChickenCharacter::ServerSetCharge_Implementation(bool bPressed)
{
    if (!bRoundInputEnabled || bIsFallen || bKnockdownPending) return;
    if (bPressed) StartJumpCharge(); else ExecuteJump();
    ForceNetUpdate();
}

//void AChickenCharacter::OnChickenHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
//{
//	AChickenCharacter* DebugOther =
//		Cast<AChickenCharacter>(OtherActor);
//
//	if (DebugOther && DebugOther != this)
//	{
//		UE_LOG(LogTemp, Warning,
//			TEXT("[ChickenHit] %s -> %s | Server=%d | Capsule=%d | "
//				"Round=%d/%d | Fallen=%d/%d | Pending=%d/%d"),
//			*GetName(),
//			*DebugOther->GetName(),
//			HasAuthority() ? 1 : 0,
//			OtherComp == DebugOther->GetCapsuleComponent() ? 1 : 0,
//			bRoundInputEnabled ? 1 : 0,
//			DebugOther->bRoundInputEnabled ? 1 : 0,
//			bIsFallen ? 1 : 0,
//			DebugOther->bIsFallen ? 1 : 0,
//			bKnockdownPending ? 1 : 0,
//			DebugOther->bKnockdownPending ? 1 : 0
//		);
//	}
//    if (!HasAuthority() || !bRoundInputEnabled || bIsFallen || bKnockdownPending) return;
//    auto* Other = Cast<AChickenCharacter>(OtherActor);
//    if (!Other || Other == this || OtherComp != Other->GetCapsuleComponent() ||
//        !Other->bRoundInputEnabled || Other->bIsFallen || Other->bKnockdownPending) return;
//
//    // Resolve each pair once, even when both capsules report the same contact.
//    const double Now = GetWorld()->GetTimeSeconds();
//    for (auto It = RecentCollisionTimes.CreateIterator(); It; ++It)
//    {
//        if (!It.Key().IsValid() || Now - It.Value() >= CollisionCooldown) It.RemoveCurrent();
//    }
//    const TWeakObjectPtr<AChickenCharacter> OtherKey(Other);
//    if (RecentCollisionTimes.Contains(OtherKey)) return;
//
//    FVector Direction = (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
//    if (Direction.IsNearlyZero()) Direction = (-Hit.ImpactNormal).GetSafeNormal2D();
//    if (Direction.IsNearlyZero()) return;
//
//    // Tangential contacts and separating characters must not gain another impulse.
//    const float ClosingSpeed = FVector::DotProduct(GetVelocity() - Other->GetVelocity(), Direction);
//    if (ClosingSpeed < MinCollisionSpeed) return;
//    RecentCollisionTimes.Add(OtherKey, Now);
//    Other->RecentCollisionTimes.Add(TWeakObjectPtr<AChickenCharacter>(this), Now);
//
//    const float PushSpeed = FMath::Clamp(ClosingSpeed * CollisionPushScale, 0.f, MaxCollisionPushSpeed);
//    ApplyCollisionResponse(-Direction * PushSpeed);
//    Other->ApplyCollisionResponse(Direction * PushSpeed);
//}

void AChickenCharacter::MoveBlockedBy(const FHitResult& Impact)
{
    Super::MoveBlockedBy(Impact);
    if (!HasAuthority()) return;

    // HandleImpact calls this before slide/deflection changes movement velocity.
    // Deferred capsule hits still use the same pair cooldown, so they cannot double-push.
    if (const auto* Other = Cast<AChickenCharacter>(Impact.GetActor()))
    {
        CF_FRAME(1, LogChickenCollision, "MovementBlocked Other=%s RelativeVelocity=%s ImpactNormal=%s",
            *Other->GetName(), *(GetVelocity() - Other->GetVelocity()).ToString(), *Impact.ImpactNormal.ToString());
        OnChickenHit(GetCapsuleComponent(), Impact.GetActor(), Impact.GetComponent(), FVector::ZeroVector, Impact);
    }
}

void AChickenCharacter::OnChickenHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (const auto* DebugOther = Cast<AChickenCharacter>(OtherActor))
    {
        CF_FRAME(1, LogChickenCollision, "Hit Other=%s Component=%s Authority=%d RelativeVelocity=%s ImpactNormal=%s",
            *DebugOther->GetName(), *GetNameSafe(OtherComp), HasAuthority(),
            *(GetVelocity() - DebugOther->GetVelocity()).ToString(), *Hit.ImpactNormal.ToString());
    }
    if (!HasAuthority())
    {
        return;
    }

    AChickenCharacter* Other =
        Cast<AChickenCharacter>(OtherActor);

    if (!Other || Other == this)
    {
        return;
    }

    if (!bRoundInputEnabled || bIsFallen || bKnockdownPending ||
        !Other->bRoundInputEnabled ||
        Other->bIsFallen || Other->bKnockdownPending)
    {
        CF_FRAME(1, LogChickenCollision, "HitSkipped Reason=State Other=%s Round=%d/%d Fallen=%d/%d Pending=%d/%d",
            *Other->GetName(), bRoundInputEnabled, Other->bRoundInputEnabled, bIsFallen, Other->bIsFallen,
            bKnockdownPending, Other->bKnockdownPending);
        return;
    }

    if (OtherComp != Other->GetCapsuleComponent())
    {
        CF_FRAME(1, LogChickenCollision, "HitSkipped Reason=NotCapsule Other=%s Component=%s",
            *Other->GetName(), *GetNameSafe(OtherComp));
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();

    for (auto It = RecentCollisionTimes.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() ||
            Now - It.Value() >= CollisionCooldown)
        {
            It.RemoveCurrent();
        }
    }

    const TWeakObjectPtr<AChickenCharacter> OtherKey(Other);

    if (const double* LastTime = RecentCollisionTimes.Find(OtherKey))
    {
        CF_FRAME(1, LogChickenCollision, "HitSkipped Reason=Cooldown Other=%s Elapsed=%.3f Limit=%.3f",
            *Other->GetName(), Now - *LastTime, CollisionCooldown);
        return;
    }

    FVector Direction =
        (Other->GetActorLocation() - GetActorLocation())
        .GetSafeNormal2D();

    if (Direction.IsNearlyZero())
    {
        Direction = (-Hit.ImpactNormal).GetSafeNormal2D();
    }

    if (Direction.IsNearlyZero())
    {
        CF_FRAME(1, LogChickenCollision, "HitSkipped Reason=ZeroDirection Other=%s Normal=%s",
            *Other->GetName(), *Hit.ImpactNormal.ToString());
        return;
    }

    // Only approaching contacts transfer power.
    const float ClosingSpeed = FVector::DotProduct(GetVelocity() - Other->GetVelocity(), Direction);
    if (ClosingSpeed < MinCollisionSpeed)
    {
        CF_FRAME(1, LogChickenCollision, "HitSkipped Reason=ClosingSpeed Other=%s Speed=%.3f Min=%.3f",
            *Other->GetName(), ClosingSpeed, MinCollisionSpeed);
        return;
    }
    const float PushSpeed = FMath::Clamp(ClosingSpeed * CollisionPushScale, 0.f, MaxCollisionPushSpeed);

	RecentCollisionTimes.Add(OtherKey, Now);
	Other->RecentCollisionTimes.Add(
		TWeakObjectPtr<AChickenCharacter>(this),
		Now
	);

    CF_EVENT(LogChickenCollision, "PushDispatch Other=%s RelativeVelocity=%s ClosingSpeed=%.3f Direction=%s Mode=ClosingSpeed CalculatedDeltaV=%.3f SelfRequest=%s OtherRequest=%s Cooldown=Passed Limit=%.3f",
        *Other->GetName(), *(GetVelocity() - Other->GetVelocity()).ToString(),
        ClosingSpeed, *Direction.ToString(),
        PushSpeed, *(-Direction * PushSpeed).ToString(), *(Direction * PushSpeed).ToString(), CollisionCooldown);

	ApplyCollisionResponse(-Direction * PushSpeed);
	Other->ApplyCollisionResponse(Direction * PushSpeed);
}

void AChickenCharacter::ApplyCollisionResponse(
    const FVector& VelocityChange)
{
    if (!HasAuthority() ||
        !bRoundInputEnabled ||
        bIsFallen ||
        bKnockdownPending)
    {
        return;
    }

    const FVector HorizontalImpulse(
        VelocityChange.X,
        VelocityChange.Y,
        0.f
    );

    // 1. 캐릭터 위치를 밀어냄.
    const float LiftSpeed = FMath::Clamp(
        static_cast<float>(HorizontalImpulse.Size()) * CollisionLiftScale, 0.f, MaxCollisionLiftSpeed);
    const FVector CollisionImpulse = HorizontalImpulse + FVector::UpVector * LiftSpeed;
    GetCharacterMovement()->AddImpulse(CollisionImpulse, true);
    bInLandingRecovery = false;

    // 2. 월드 기준 충격 방향을 캐릭터 기준으로 변환.
    const FVector LocalImpulse =
        GetActorTransform().InverseTransformVectorNoScale(
            HorizontalImpulse
        );

    // 기존 BodyTilt 규칙: X는 전후, Y는 오른쪽의 반대 부호.
    const FVector2D BalanceImpulse(
        LocalImpulse.X,
        -LocalImpulse.Y
    );

    // 3. 충격 방향으로 몸이 기울어지는 속도를 추가.
    // 한 번의 충돌로 추가되는 양만 제한.
    const FVector2D TiltKick =
        (BalanceImpulse * CollisionTiltScale)
        .GetClampedToMaxSize(MaxCollisionTiltImpulse);

    TiltVelocity += TiltKick;
    CF_EVENT(LogChickenCollision, "PushApplied Requested=%s AppliedDeltaV=%s VelocityChangeMode=1 VelocityNow=%s TiltKick=%s TiltVelocity=%s Fallen=%d",
        *VelocityChange.ToString(), *CollisionImpulse.ToString(), *GetVelocity().ToString(),
        *TiltKick.ToString(), *TiltVelocity.ToString(), bIsFallen);
    if (DebugPendingPushCount == 0)
    {
        DebugPushFrame = GFrameCounter;
        DebugPushVelocity = GetVelocity();
    }
    ++DebugPendingPushCount;
    DebugPendingImpulse += CollisionImpulse;

    ForceNetUpdate();
}
#undef CF_EVENT
#undef CF_FRAME
