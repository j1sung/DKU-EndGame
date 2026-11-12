#include "Gameplay/AmusementSeesawRig.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
    constexpr float PivotHeight = 850.f;
    constexpr float PusherX = 480.f;
    constexpr float RodBottom = 110.f;
    constexpr float DeckHalfThickness = 25.f;
    constexpr float DeckAxisOffset = 100.f;

    float Smooth(float Value)
    {
        const float T = FMath::Clamp(Value, 0.f, 1.f);
        return T * T * (3.f - 2.f * T);
    }
}

AAmusementSeesawRig::AAmusementSeesawRig()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RigRoot"));

    auto MeshPart = [this](FName Name, bool bCollision)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(RootComponent);
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionProfileName(bCollision ? TEXT("BlockAllDynamic") : TEXT("NoCollision"));
        Part->SetCollisionObjectType(ECC_WorldDynamic);
        Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Part->SetGenerateOverlapEvents(false);
        return Part;
    };
    Deck = MeshPart(TEXT("FightingDeck"), true);
    Deck->CanCharacterStepUpOn = ECB_Yes;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Rods.Add(MeshPart(*FString::Printf(TEXT("PusherRod%d"), Index), false));
        Heads.Add(MeshPart(*FString::Printf(TEXT("PusherHead%d"), Index), false));
        Bridges.Add(MeshPart(*FString::Printf(TEXT("BoardingBridge%d"), Index), true));
        Gates.Add(MeshPart(*FString::Printf(TEXT("BoardingGate%d"), Index), false));
    }
}

void AAmusementSeesawRig::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Deck->SetStaticMesh(DeckMesh);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        Rods[Index]->SetStaticMesh(RodMesh);
        Heads[Index]->SetStaticMesh(HeadMesh);
        Bridges[Index]->SetStaticMesh(BridgeMesh);
        Gates[Index]->SetStaticMesh(GateMesh);
    }
    ApplyPose(0.f, 0.f);
}

double AAmusementSeesawRig::ServerTime() const
{
    if (const AGameStateBase* State = GetWorld()->GetGameState())
    {
        return State->GetServerWorldTimeSeconds();
    }
    return GetWorld()->GetTimeSeconds();
}

void AAmusementSeesawRig::BeginPlay()
{
    Super::BeginPlay();
    BoardingClosureAlpha = 0.f;
    ApplyPose(0.f, 0.f);
    if (HasAuthority())
    {
        CycleStartServerTime = ServerTime() + FMath::Max(0.f, InitialDelay);
        ForceNetUpdate();
    }
}

void AAmusementSeesawRig::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (CycleStartServerTime < 0.) return;
    const double Elapsed = ServerTime() - CycleStartServerTime;
    // 출입부는 시작 때 한 번만 닫는다. 반복되는 푸셔 동작과 분리해 재개방하지 않는다.
    const float BoardingAge = static_cast<float>(Elapsed + FMath::Max(0.f, InitialDelay));
    BoardingClosureAlpha = FMath::Max(BoardingClosureAlpha,
        Smooth(BoardingAge / FMath::Max(.1f, BoardingTravelTime)));
    if (Elapsed < 0.)
    {
        ApplyPose(0.f, 0.f);
        return;
    }

    const float BridgeTime = FMath::Max(.1f, BoardingTravelTime);
    const float TravelTime = FMath::Max(.1f, TiltTravelTime);
    const float HoldTime = FMath::Max(0.f, TiltHoldTime);
    const float ActiveTime = 2.f * BridgeTime + 2.f * TravelTime + HoldTime;
    // 편집값을 줄여도 이전 복귀가 끝나기 전에 반대쪽이 시작하지 않게 한다.
    const double Interval = FMath::Max(PushInterval, ActiveTime + .1f);
    const int64 Cycle = static_cast<int64>(FMath::FloorToDouble(Elapsed / Interval));
    const float Phase = static_cast<float>(FMath::Fmod(Elapsed, Interval));
    const float Sign = (Cycle % 2 == 0) ? 1.f : -1.f;
    const float UpStart = BridgeTime;
    const float HoldStart = UpStart + TravelTime;
    const float DownStart = HoldStart + HoldTime;
    const float DownEnd = DownStart + TravelTime;
    float TiltFraction = 0.f;
    if (Phase >= UpStart && Phase < HoldStart) TiltFraction = Smooth((Phase - UpStart) / TravelTime);
    else if (Phase >= HoldStart && Phase < DownStart) TiltFraction = 1.f;
    else if (Phase >= DownStart && Phase < DownEnd) TiltFraction = 1.f - Smooth((Phase - DownStart) / TravelTime);

    float Retraction = 1.f;
    if (Phase < BridgeTime) Retraction = Smooth(Phase / BridgeTime);
    else if (Phase >= DownEnd) Retraction = 1.f - Smooth((Phase - DownEnd) / BridgeTime);
    ApplyPose(Sign * FMath::Clamp(MaximumTiltAngle, 0.f, 45.f) * TiltFraction, Retraction, Sign);
}

void AAmusementSeesawRig::ApplyPose(float Angle, float PusherExtension, float PushingSide)
{
    CurrentTiltAngle = Angle;
    bBoardingOpen = BoardingClosureAlpha < .001f;
    Deck->SetRelativeLocation(FVector(0.f, 0.f, PivotHeight)
        + FRotator(Angle, 0.f, 0.f).RotateVector(FVector(0.f, 0.f, DeckAxisOffset)));
    Deck->SetRelativeRotation(FRotator(Angle, 0.f, 0.f));
    const float Radians = FMath::DegreesToRadians(Angle);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const float Side = Index == 0 ? -1.f : 1.f;
        const bool bPushing = Side * PushingSide > 0.f;
        // 높은 쪽만 늘린다. 판을 기울이기 전에 머리를 먼저 접근시켜 순간 이동을 피한다.
        const float ContactZ = PivotHeight + Side * PusherX * FMath::Tan(Radians)
            + (DeckAxisOffset - DeckHalfThickness) / FMath::Cos(Radians);
        const float HeadZ = FMath::Lerp(RodBottom + 15.f, ContactZ - 15.f / FMath::Cos(Radians),
            bPushing ? PusherExtension : 0.f);
        const float RodLength = FMath::Max(1.f, HeadZ - 15.f - RodBottom);
        Rods[Index]->SetRelativeLocation(FVector(Side * PusherX, 0.f, RodBottom + RodLength * .5f));
        Rods[Index]->SetRelativeScale3D(FVector(1.f, 1.f, RodLength / 100.f));
        Heads[Index]->SetRelativeLocation(FVector(Side * PusherX, 0.f, HeadZ));
        Heads[Index]->SetRelativeRotation(FRotator(bPushing ? Angle : 0.f, 0.f, 0.f));
        Bridges[Index]->SetRelativeLocation(FVector(Side * (1000.f + 220.f * BoardingClosureAlpha), 0.f, 965.f));
        Gates[Index]->SetRelativeLocation(FVector(Side * 1120.f, 235.f * (1.f - BoardingClosureAlpha), 1040.f));
        Gates[Index]->SetCollisionEnabled(bBoardingOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        Gates[Index]->SetCollisionResponseToAllChannels(ECR_Block);
        Gates[Index]->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }
}

void AAmusementSeesawRig::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAmusementSeesawRig, CycleStartServerTime);
}
