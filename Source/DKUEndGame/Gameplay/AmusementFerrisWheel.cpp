#include "Gameplay/AmusementFerrisWheel.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"

AAmusementFerrisWheel::AAmusementFerrisWheel()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WheelCenter"));
    auto Part = [this](FName Name)
    {
        auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionProfileName(TEXT("NoCollision"));
        Mesh->SetGenerateOverlapEvents(false);
        return Mesh;
    };
    Wheel = Part(TEXT("RotatingWheel"));
    for (int32 Index = 0; Index < 16; ++Index)
        Cabins.Add(Part(*FString::Printf(TEXT("UprightCabin%d"), Index)));
}

void AAmusementFerrisWheel::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Wheel->SetStaticMesh(WheelMesh);
    for (UStaticMeshComponent* Cabin : Cabins) Cabin->SetStaticMesh(CabinMesh);
    ApplyPose(0.f);
}

void AAmusementFerrisWheel::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const AGameStateBase* State = GetWorld()->GetGameState();
    const double Seconds = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    ApplyPose(static_cast<float>(FMath::Fmod(Seconds * RotationSpeed, 360.)));
}

void AAmusementFerrisWheel::ApplyPose(float Angle)
{
    Wheel->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
    for (int32 Index = 0; Index < Cabins.Num(); ++Index)
    {
        const float Radians = FMath::DegreesToRadians(Angle + Index * 360.f / Cabins.Num());
        // 탑승칸은 회전 링의 자식이 아니므로 위치만 공전하고 방향은 뒤집히지 않는다.
        Cabins[Index]->SetRelativeLocation(FVector(Radius * FMath::Cos(Radians), 0.f,
            Radius * FMath::Sin(Radians) - 300.f));
        Cabins[Index]->SetRelativeRotation(FRotator::ZeroRotator);
    }
}
