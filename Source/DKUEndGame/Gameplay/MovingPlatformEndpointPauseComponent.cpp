#include "Gameplay/MovingPlatformEndpointPauseComponent.h"

#include "Components/InterpToMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMovingPlatformPause, Log, All);

UMovingPlatformEndpointPauseComponent::UMovingPlatformEndpointPauseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMovingPlatformEndpointPauseComponent::BeginPlay()
{
    Super::BeginPlay();

    Movement = GetOwner()->FindComponentByClass<UInterpToMovementComponent>();
    if (!Movement || Movement->BehaviourType != EInterpToBehaviourType::PingPong || Movement->ControlPoints.Num() < 2)
    {
        return;
    }

    const auto WorldPoint = [this](const FInterpControlPoint& Point)
    {
        return Point.bPositionIsRelative
            ? GetOwner()->GetActorTransform().TransformPosition(Point.PositionControlPoint)
            : Point.PositionControlPoint;
    };
    bLastPointIsTop = WorldPoint(Movement->ControlPoints.Last()).Z >= WorldPoint(Movement->ControlPoints[0]).Z;
    Movement->OnInterpToReverse.AddUniqueDynamic(this, &ThisClass::HandleReverse);
}

void UMovingPlatformEndpointPauseComponent::HandleReverse(const FHitResult& Hit, float Time)
{
    UE_LOG(LogMovingPlatformPause, Verbose, TEXT("%s Time=%g Hit=%d Waiting=%d Top=%g Bottom=%g"),
        *GetOwner()->GetName(), Time, Hit.bBlockingHit, bWaitingAtEndpoint, TopWaitSeconds, BottomWaitSeconds);
    // 경로 중간의 충돌 반전은 정거장 도착으로 취급하지 않는다.
    if (!Movement || bWaitingAtEndpoint || Hit.bBlockingHit || (Time > KINDA_SMALL_NUMBER && Time < 1.f - KINDA_SMALL_NUMBER))
    {
        return;
    }

    const bool bAtTop = (Time >= .5f) == bLastPointIsTop;
    const float WaitSeconds = FMath::Max(0.f, bAtTop ? TopWaitSeconds : BottomWaitSeconds);
    if (WaitSeconds <= 0.f || !FMath::IsFinite(WaitSeconds))
    {
        return;
    }

    bWaitingAtEndpoint = true;
    // Deactivate는 StopMovementImmediately를 호출해 경로를 중단한다.
    // 활성 플래그만 내려 현재 틱의 잔여 이동을 차단하고, 반전된 시간·방향은 보존한다.
    Movement->SetActiveFlag(false);
    Movement->SetComponentTickEnabled(false);
    Movement->Velocity = FVector::ZeroVector;
    Movement->UpdateComponentVelocity();
    GetWorld()->GetTimerManager().SetTimer(ResumeTimer, this, &ThisClass::ResumeMovement, WaitSeconds, false);
}

void UMovingPlatformEndpointPauseComponent::ResumeMovement()
{
    if (IsValid(Movement) && bWaitingAtEndpoint)
    {
        Movement->SetActiveFlag(true);
        Movement->SetComponentTickEnabled(true);
    }
    bWaitingAtEndpoint = false;
}

void UMovingPlatformEndpointPauseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorld()->GetTimerManager().ClearTimer(ResumeTimer);
    if (IsValid(Movement))
    {
        Movement->OnInterpToReverse.RemoveDynamic(this, &ThisClass::HandleReverse);
    }
    Super::EndPlay(EndPlayReason);
}
