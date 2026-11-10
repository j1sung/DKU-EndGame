#include "Gameplay/BumperCharacterLaunchComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UBumperCharacterLaunchComponent::UBumperCharacterLaunchComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UBumperCharacterLaunchComponent::BeginPlay()
{
    Super::BeginPlay();
    TInlineComponentArray<UStaticMeshComponent*> Meshes(GetOwner());
    for (UStaticMeshComponent* Mesh : Meshes)
    {
        if (Mesh->GetFName() == TEXT("Bumper"))
        {
            BumperMesh = Mesh;
            PreviousTop = Mesh->Bounds.Origin.Z + Mesh->Bounds.BoxExtent.Z;
            break;
        }
    }
    // 발사는 서버에서만 처리하고 기존 CharacterMovement 이동 복제를 사용한다.
    SetComponentTickEnabled(GetOwner()->HasAuthority() && IsValid(BumperMesh));
}

void UBumperCharacterLaunchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsValid(BumperMesh) || !GetOwner()->HasAuthority())
    {
        return;
    }

    const FBoxSphereBounds Bounds = BumperMesh->Bounds;
    const float Top = Bounds.Origin.Z + Bounds.BoxExtent.Z;
    const bool bRising = Top > PreviousTop + .01f;
    if (!bRising || !bWasRising)
    {
        LaunchedCharacters.Reset();
    }
    if (bRising && FMath::IsFinite(LaunchSpeed) && LaunchSpeed > 0.f)
    {
        // 급상승한 판의 이전/현재 높이를 모두 포함하여 한 프레임 사이에 밀린 발도 찾는다.
        const float Tolerance = FMath::Max(0.f, SurfaceTolerance);
        const float Low = PreviousTop - Tolerance;
        const float High = Top + Tolerance;
        const FVector Center(Bounds.Origin.X, Bounds.Origin.Y, (Low + High) * .5f);
        const FVector Extent(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, FMath::Max(1.f, (High - Low) * .5f));
        TArray<FOverlapResult> Overlaps;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(BumperCharacterLaunch), false, GetOwner());
        GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
            FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Extent), Params);
        for (const FOverlapResult& Overlap : Overlaps)
        {
            ACharacter* Character = Cast<ACharacter>(Overlap.GetActor());
            if (!Character || LaunchedCharacters.Contains(Character))
            {
                continue;
            }
            UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
            const float FeetZ = Character->GetCapsuleComponent()->GetComponentLocation().Z
                - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            // 아래쪽이나 판에서 멀리 떨어진 캐릭터, 이동이 정지된 래그돌은 제외한다.
            if (!Movement || Movement->MovementMode == MOVE_None || FeetZ < Low || FeetZ > High)
            {
                continue;
            }
            Character->LaunchCharacter(FVector(0.f, 0.f, LaunchSpeed), false, true);
            LaunchedCharacters.Add(Character);
        }
    }
    PreviousTop = Top;
    bWasRising = bRising;
}
