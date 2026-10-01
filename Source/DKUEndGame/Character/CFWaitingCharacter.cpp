#include "Character/CFWaitingCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ACFWaitingCharacter::ACFWaitingCharacter()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = 110.f;
    Movement->MinAnalogWalkSpeed = 0.f;
    Movement->MaxAcceleration = 600.f;
    Movement->BrakingDecelerationWalking = 800.f;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
}

bool ACFWaitingCharacter::CanJumpInternal_Implementation() const
{
    return false;
}
