#include "Testing/CFKnockdownTestActors.h"
#include "Character/ChickenCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
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
    // 테스트용 R 리셋 입력은 유지하고 화면 안내 패널은 그리지 않는다.
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
