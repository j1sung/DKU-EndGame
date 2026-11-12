#include "Gameplay/Round3LogSpawner.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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
    // 활성 인스턴스에서만 접촉 직전 속도를 매 프레임 보관한다.
    if (bEnableRollingPlayerPush) SetActorTickInterval(0.f);
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
    if (bEnableRollingPlayerPush)
    {
        Mesh->SetNotifyRigidBodyCollision(true);
        Mesh->OnComponentHit.AddDynamic(this, &ARound3LogSpawner::OnLogHit);
    }
    ActiveLog->SetReplicates(true);
    ActiveLog->SetReplicateMovement(true);
    Mesh->SetIsReplicated(true);
    Mesh->SetSimulatePhysics(true);
    PreviousLogVelocity = FVector::ZeroVector;
    LastPlayerPushTimes.Empty();
    LogAge = 0.f;
}

void ARound3LogSpawner::OnLogHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    auto* Character = Cast<ACharacter>(OtherActor);
    if (!HasAuthority() || !bEnableRollingPlayerPush || !IsValid(ActiveLog) || HitComponent != ActiveLog->GetStaticMeshComponent() ||
        !Character || !Character->GetCharacterMovement() ||
        Character->GetCharacterMovement()->MovementMode == MOVE_None) return;

    const FVector Direction = (Character->GetActorLocation()-ActiveLog->GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) return;
    const FVector CurrentVelocity = HitComponent->GetPhysicsLinearVelocity();
    const float IncomingSpeed = FMath::Max(FVector::DotProduct(CurrentVelocity, Direction),
        FVector::DotProduct(PreviousLogVelocity, Direction));
    // 정지한 물체를 걷기로 미는 접촉에는 추가 밀침이나 운동량 보정을 적용하지 않는다.
    if (IncomingSpeed < MinimumRollingPushSpeed || PlayerPushMultiplier <= 0.f) return;

    const float Now = GetWorld()->GetTimeSeconds();
    const TWeakObjectPtr<ACharacter> Key(Character);
    if (const float* LastTime = LastPlayerPushTimes.Find(Key); LastTime && Now-*LastTime < .35f) return;
    LastPlayerPushTimes.Add(Key, Now);

    const float TargetSpeed = FMath::Min(IncomingSpeed*PlayerPushMultiplier, MaximumPlayerPushSpeed);
    const float DeltaSpeed = FMath::Max(0.f, TargetSpeed-FVector::DotProduct(Character->GetVelocity(), Direction));
    if (DeltaSpeed > 0.f)
    {
        // 옆 방향 이동은 보존하고 바닥 마찰로 즉시 멈추지 않을 정도만 살짝 띄운다.
        const FVector Launch = Direction*DeltaSpeed + FVector(0.f, 0.f, FMath::Max(100.f, Character->GetVelocity().Z));
        Character->LaunchCharacter(Launch, false, true);
    }
    const float LostSpeed = IncomingSpeed*RollingMomentumRetention-FVector::DotProduct(CurrentVelocity, Direction);
    if (LostSpeed > 0.f)
        HitComponent->AddImpulse(Direction*LostSpeed, NAME_None, true);
}

void ARound3LogSpawner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    if (IsValid(ActiveLog))
    {
        PreviousLogVelocity = ActiveLog->GetStaticMeshComponent()->GetPhysicsLinearVelocity();
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
