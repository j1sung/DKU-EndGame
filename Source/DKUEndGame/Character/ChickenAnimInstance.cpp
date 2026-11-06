#include "Character/ChickenAnimInstance.h"
#include "Character/ChickenCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void UChickenAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    CachedCharacter = Cast<AChickenCharacter>(TryGetPawnOwner());
    LastTakeoffSerial = CachedCharacter.IsValid() ? CachedCharacter->GetTakeoffSerial() : 0;
    AnimationState = ECFBasicAnimState::Idle;
    StateElapsed = 0.f;
}

void UChickenAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    auto* Character = Cast<AChickenCharacter>(TryGetPawnOwner());
    if (Character != CachedCharacter.Get())
    {
        CachedCharacter = Character;
        LastTakeoffSerial = Character ? Character->GetTakeoffSerial() : 0;
        AnimationState = ECFBasicAnimState::Idle;
        StateElapsed = 0.f;
    }
    if (!Character)
    {
        bInAir = bCharging = bFallen = false;
        Speed = NormalizedCharge = StateElapsed = 0.f;
        AnimationState = ECFBasicAnimState::Idle;
        return;
    }

    StateElapsed += FMath::Max(0.f, DeltaSeconds);
    bInAir = Character->GetCharacterMovement()->IsFalling();
    bCharging = Character->IsChargingJump();
    bFallen = Character->IsFallen();
    KnockdownIndex = static_cast<int32>(Character->GetKnockdownDirection());
    Speed = Character->GetVelocity().Size2D();
    NormalizedCharge = FMath::Clamp(Character->GetNormalizedJumpCharge(), 0.f, 1.f);
    const float EffectiveJumpDuration = FMath::Max(0.01f, FMath::Max(JumpStartDuration, MinimumJumpStartDuration));
    JumpStartPlayRate = 0.3f / EffectiveJumpDuration;
    LandPlayRate = 0.7f / FMath::Max(0.01f, Character->GetLandingRecoveryTime());

    const bool bNewTakeoff = Character->GetTakeoffSerial() != LastTakeoffSerial;
    LastTakeoffSerial = Character->GetTakeoffSerial();
    ECFBasicAnimState Next = ECFBasicAnimState::Idle;
    if (bNewTakeoff && !bFallen)
    {
        Next = ECFBasicAnimState::JumpStart;
    }
    else if (Character->IsRecoveringFromLanding() && !bFallen)
    {
        Next = ECFBasicAnimState::Land;
    }
    else if (!bFallen && AnimationState == ECFBasicAnimState::JumpStart && StateElapsed < EffectiveJumpDuration)
    {
        // LaunchCharacter may not have switched movement mode yet on the takeoff frame.
        Next = ECFBasicAnimState::JumpStart;
    }
    else if (bInAir)
    {
        // Charging in mid-air changes gameplay charge, but keeps an airborne pose.
        Next = ECFBasicAnimState::AirLoop;
    }
    else if (bCharging && !bFallen)
    {
        if (AnimationState == ECFBasicAnimState::ChargeLoop ||
            (AnimationState == ECFBasicAnimState::ChargeStart && StateElapsed >= ChargeStartDuration))
            Next = ECFBasicAnimState::ChargeLoop;
        else
            Next = ECFBasicAnimState::ChargeStart;
    }

    if (Next != AnimationState || bNewTakeoff)
    {
        AnimationState = Next;
        StateElapsed = 0.f;
    }
}
