#include "Character/CFWaitingAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

void UCFWaitingAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const APawn* Pawn = TryGetPawnOwner();
    GroundSpeed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
    // Hysteresis prevents tiny braking/network corrections flickering between poses.
    bIsWalking = GroundSpeed > (bIsWalking ? 2.f : 5.f);
    const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
    const float UniformScale = Mesh ? FMath::Abs(Mesh->GetComponentScale().X) : 1.f;
    const float ReferenceSpeed = FMath::Max(1.f, AuthoredWalkSpeed * UniformScale);
    WalkPlayRate = bIsWalking ? FMath::Max(.1f, GroundSpeed / ReferenceSpeed) : 1.f;
}
