#include "Gameplay/TemporaryFloatingPlatform.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr float DropSeconds = .6f;
    constexpr float RespawnSeconds = 6.f;
}

ATemporaryFloatingPlatform::ATemporaryFloatingPlatform()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("FloatingHome"));
    Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
    Platform->SetupAttachment(RootComponent);
    Platform->SetMobility(EComponentMobility::Movable);
    Platform->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Platform->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Platform->SetGenerateOverlapEvents(false);
    Platform->CanCharacterStepUpOn = ECB_Yes;
}

void ATemporaryFloatingPlatform::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Platform->SetStaticMesh(PlatformMesh);
}

double ATemporaryFloatingPlatform::ServerTime() const
{
    if (const AGameStateBase* State = GetWorld()->GetGameState())
        return State->GetServerWorldTimeSeconds();
    return GetWorld()->GetTimeSeconds();
}

void ATemporaryFloatingPlatform::BeginPlay()
{
    Super::BeginPlay();
    const TArray<FName> Slots = Platform->GetMaterialSlotNames();
    for (int32 Index = 0; Index < Platform->GetNumMaterials(); ++Index)
    {
        // 상면만 동적 재질로 복제해 공용 재질은 바꾸지 않는다.
        if (Slots.IsValidIndex(Index) && Slots[Index] == TEXT("FloatingSurface"))
            SurfaceMaterials.Add(Platform->CreateDynamicMaterialInstance(Index));
    }
    UpdateVisuals();
}

void ATemporaryFloatingPlatform::SetPhase(uint8 Phase)
{
    check(HasAuthority());
    Cycle.Phase = Phase;
    Cycle.StartedAt = ServerTime();
    if (Phase == 1) Cycle.HoldSeconds = FMath::Max(.1f, StandDuration);
    ForceNetUpdate();
    UpdateVisuals();
}

bool ATemporaryFloatingPlatform::SpawnSpaceClear() const
{
    for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
    {
        const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
        const FVector Local = GetActorTransform().InverseTransformPosition(It->GetActorLocation());
        const FVector Scale = GetActorScale3D().GetAbs().ComponentMax(FVector(.01));
        const float Radius = Capsule->GetScaledCapsuleRadius();
        if (FMath::Abs(Local.X) < 55.f + Radius / Scale.X &&
            FMath::Abs(Local.Y) < 95.f + Radius / Scale.Y &&
            FMath::Abs(Local.Z) < 35.f + Capsule->GetScaledCapsuleHalfHeight() / Scale.Z)
            return false;
    }
    return true;
}

void ATemporaryFloatingPlatform::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority())
    {
        const double Age = ServerTime() - Cycle.StartedAt;
        if (Cycle.Phase == 0)
        {
            // 아래나 옆에 닿은 접촉은 무시하고 실제 이동 베이스로 착지를 판정한다.
            for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
                if (It->GetMovementBaseObject() == Platform.Get()) { SetPhase(1); break; }
        }
        else if (Cycle.Phase == 1 && Age >= Cycle.HoldSeconds) SetPhase(2);
        else if (Cycle.Phase == 2 && Age >= DropSeconds) SetPhase(3);
        else if (Cycle.Phase == 3 && Age >= RespawnSeconds && SpawnSpaceClear()) SetPhase(0);
    }
    UpdateVisuals();
}

void ATemporaryFloatingPlatform::OnRep_Cycle()
{
    UpdateVisuals();
}

void ATemporaryFloatingPlatform::UpdateVisuals()
{
    const float Age = FMath::Max(0.f, static_cast<float>(ServerTime() - Cycle.StartedAt));
    const bool bSolid = Cycle.Phase < 2;
    Platform->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Platform->SetHiddenInGame(Cycle.Phase == 3);
    Platform->SetRelativeLocation(FVector::ZeroVector);
    Platform->SetRelativeRotation(FRotator::ZeroRotator);
    Platform->SetRelativeScale3D(FVector::OneVector);
    float Warning = 0.f;
    if (Cycle.Phase == 1)
    {
        const float Window = FMath::Min(1.f, Cycle.HoldSeconds);
        Warning = FMath::Clamp((Age - Cycle.HoldSeconds + Window) / Window, 0.f, 1.f);
        Platform->SetRelativeRotation(FRotator(Warning * FMath::Sin(Age * 32.f), 0.f,
            Warning * FMath::Sin(Age * 27.f) * 1.2f));
    }
    else if (Cycle.Phase == 2)
    {
        const float T = FMath::Clamp(Age / DropSeconds, 0.f, 1.f);
        Platform->SetRelativeLocation(FVector(0.f, 0.f, -180.f * T * T));
        Platform->SetRelativeScale3D(FVector(1.f - .8f * T));
        Warning = 1.f;
    }
    const FLinearColor Color = FMath::Lerp(FLinearColor(.75f,.72f,.55f), FLinearColor(.9f,.09f,.025f), Warning);
    for (UMaterialInstanceDynamic* Material : SurfaceMaterials)
        if (Material) Material->SetVectorParameterValue(TEXT("PlatformTint"), Color);
}

void ATemporaryFloatingPlatform::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATemporaryFloatingPlatform, Cycle);
}

