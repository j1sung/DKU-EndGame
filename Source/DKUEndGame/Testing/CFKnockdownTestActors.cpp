#include "Testing/CFKnockdownTestActors.h"
#include "Character/ChickenCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

void ACFKnockdownTestHUD::BeginPlay()
{
    Super::BeginPlay();
    if (PlayerOwner && PlayerOwner->IsLocalController())
    {
        EnableInput(PlayerOwner);
        InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACFKnockdownTestHUD::ResetTestCharacter);
    }
}

void ACFKnockdownTestHUD::ResetTestCharacter()
{
    auto* Mode = GetWorld()->GetAuthGameMode<ACFKnockdownTestGameMode>();
    if (!Mode || !PlayerOwner) return;
    APawn* Old = PlayerOwner->GetPawn();
    PlayerOwner->UnPossess();
    if (Old) Old->Destroy();
    PlayerOwner->SetControlRotation(FRotator::ZeroRotator);
    Mode->RestartPlayer(PlayerOwner);
}

void ACFKnockdownTestHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    const float Scale = FMath::Clamp(Canvas->ClipX / 1000.f, .65f, 1.2f);
    DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.88f), 18*Scale, 18*Scale, 455*Scale, 117*Scale);
    DrawText(TEXT("DIRECTIONAL KNOCKDOWN TEST"), FLinearColor::White, 32*Scale, 28*Scale, nullptr, 1.35f*Scale);
    DrawText(TEXT("WASD Move / Tilt   R Reset"), FLinearColor::White, 32*Scale, 57*Scale, nullptr, Scale);
    DrawText(TEXT("Hold / release left mouse: charge jump"), FLinearColor::White, 32*Scale, 79*Scale, nullptr, Scale);
    auto* Chicken = PlayerOwner ? Cast<AChickenCharacter>(PlayerOwner->GetPawn()) : nullptr;
    if (!Chicken) return;
    const TCHAR* Names[] = {TEXT("FORWARD"), TEXT("BACK"), TEXT("LEFT"), TEXT("RIGHT")};
    FString State = TEXT("Keep your balance - enter a colored zone");
    if (Chicken->IsFallen() || Chicken->IsKnockdownPending())
        State = FString::Printf(TEXT("%s: %s"), Chicken->IsFallen() ? TEXT("DOWN") : TEXT("LANDING FIRST"),
            Names[static_cast<uint8>(Chicken->GetKnockdownDirection())]);
    DrawText(State, FLinearColor(1.f, .8f, .35f), 32*Scale, 103*Scale, nullptr, Scale);
}

ACFKnockdownTestGameMode::ACFKnockdownTestGameMode()
{
    static ConstructorHelpers::FClassFinder<APawn> Pawn(TEXT("/Game/Characters/BP_ChickenCharacter"));
    static ConstructorHelpers::FClassFinder<APlayerController> Controller(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
    if (Pawn.Succeeded()) DefaultPawnClass = Pawn.Class;
    if (Controller.Succeeded()) PlayerControllerClass = Controller.Class;
    HUDClass = ACFKnockdownTestHUD::StaticClass();
}

ACFKnockdownZone::ACFKnockdownZone()
{
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    SetRootComponent(Trigger);
    Trigger->SetBoxExtent(FVector(130.f, 130.f, 140.f));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ACFKnockdownZone::EnterZone);
}

void ACFKnockdownZone::EnterZone(UPrimitiveComponent* Component, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool FromSweep, const FHitResult& Hit)
{
    auto* Chicken = Cast<AChickenCharacter>(OtherActor);
    if (Chicken && OtherComponent == Chicken->GetCapsuleComponent()) Chicken->StartKnockdown(Direction);
}
