#include "Gameplay/Round3LogSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

ARound3LogSpawner::ARound3LogSpawner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    bReplicates = true;
    PreviewLog = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewLog"));
    SetRootComponent(PreviewLog);
    PreviewLog->SetMobility(EComponentMobility::Movable);
    PreviewLog->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewLog->SetHiddenInGame(true);
}

void ARound3LogSpawner::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(HasAuthority());
    if (HasAuthority()) SpawnLog();
}

void ARound3LogSpawner::SpawnLog()
{
    if (!PreviewLog->GetStaticMesh()) return;
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActiveLog = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), GetActorTransform(), Params);
    if (!ActiveLog) return;

    UStaticMeshComponent* Mesh = ActiveLog->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(PreviewLog->GetStaticMesh());
    for (int32 Index = 0; Index < PreviewLog->GetNumMaterials(); ++Index)
        Mesh->SetMaterial(Index, PreviewLog->GetMaterial(Index));
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Mesh->SetMassOverrideInKg(NAME_None, LogMass, true);
    Mesh->SetLinearDamping(0.05f);
    Mesh->SetAngularDamping(0.05f);
    ActiveLog->SetReplicates(true);
    ActiveLog->SetReplicateMovement(true);
    Mesh->SetIsReplicated(true);
    Mesh->SetSimulatePhysics(true);
    LogAge = 0.f;
}

void ARound3LogSpawner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    if (IsValid(ActiveLog))
    {
        LogAge += DeltaSeconds;
        // 경사 끝에 도착하거나 중간에 오래 끼면 제거해 다음 주기를 보장한다.
        if (ActiveLog->GetActorLocation().Z <= FinishHeight || LogAge >= MaximumLifetime)
        {
            ActiveLog->Destroy();
            ActiveLog = nullptr;
            RemainingDelay = RespawnDelay;
        }
    }
    else
    {
        RemainingDelay -= DeltaSeconds;
        if (RemainingDelay <= 0.f)
        {
            SpawnLog();
            RemainingDelay = RespawnDelay;
        }
    }
}

void ARound3LogSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && IsValid(ActiveLog)) ActiveLog->Destroy();
    Super::EndPlay(EndPlayReason);
}
