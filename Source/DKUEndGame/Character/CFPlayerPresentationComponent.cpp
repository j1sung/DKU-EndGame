#include "Character/CFPlayerPresentationComponent.h"
#include "UI/CFFrontendSubsystem.h"
#include "UI/CFLobbyController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Character/ChickenCharacter.h"
#include "UI/CFJumpChargeWidget.h"

void UCFNameplateWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Label = WidgetTree->ConstructWidget<UTextBlock>(); WidgetTree->RootWidget = Label;
    FSlateFontInfo Font = Label->GetFont(); Font.Size = 20; Label->SetFont(Font);
    Label->SetJustification(ETextJustify::Center); Label->SetShadowOffset(FVector2D(1.5, 1.5));
    Label->SetShadowColorAndOpacity(FLinearColor::Black);
}
void UCFNameplateWidget::SetName(const FString& Name, FLinearColor Colour)
{
    if (!Label) return;
    Label->SetText(FText::FromString(Name)); Label->SetColorAndOpacity(FSlateColor(Colour));
}
UCFPlayerPresentationComponent::UCFPlayerPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickInterval = .1f;
    SetIsReplicatedByDefault(true);
}
void UCFPlayerPresentationComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;
    if (Character->GetNetMode() != NM_DedicatedServer)
    {
        Nameplate = NewObject<UWidgetComponent>(Character);
        Nameplate->SetupAttachment(Character->GetRootComponent());
        Nameplate->SetRelativeLocation(FVector(0, 0, 150));
        Nameplate->SetWidgetSpace(EWidgetSpace::Screen); Nameplate->SetDrawSize(FVector2D(340, 40));
        Nameplate->SetWidgetClass(UCFNameplateWidget::StaticClass());
        Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision); Nameplate->RegisterComponent();
        Nameplate->SetVisibility(false);

        JumpCharge = NewObject<UWidgetComponent>(Character);
        JumpCharge->SetupAttachment(Character->GetRootComponent());
        JumpCharge->SetWidgetSpace(EWidgetSpace::Screen);
        JumpCharge->SetDrawSize(FVector2D(160, 160));
        JumpCharge->SetPivot(FVector2D(.5f, .5f));
        JumpCharge->SetWidgetClass(UCFJumpChargeWidget::StaticClass());
        JumpCharge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        JumpCharge->RegisterComponent();
        JumpCharge->InitWidget();
        if (auto* Gauge = Cast<UCFJumpChargeWidget>(JumpCharge->GetUserWidgetObject()))
            Gauge->SetCharacter(Cast<AChickenCharacter>(Character));
        JumpCharge->SetVisibility(false);
    }
    auto* Mesh = Character->GetMesh();
    if (Mesh && Character->GetNetMode() != NM_DedicatedServer)
        for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
        {
            UMaterialInterface* Material = Mesh->GetMaterial(I);
            if (!Material) continue;
            TArray<FMaterialParameterInfo> Parameters; TArray<FGuid> Ids;
            Material->GetAllVectorParameterInfo(Parameters, Ids);
            if (Parameters.ContainsByPredicate([](const FMaterialParameterInfo& Info) { return Info.Name == TEXT("CF_PlayerTint"); }))
                if (auto* Dynamic = Mesh->CreateDynamicMaterialInstance(I)) Materials.Add(Dynamic);
        }
    OnRep_Colour();
}
void UCFPlayerPresentationComponent::SetColourSlot(int32 Slot)
{
    if (!GetOwner()->HasAuthority() || Slot < 0 || Slot > 3) return;
    ColourSlot = Slot; OnRep_Colour(); GetOwner()->ForceNetUpdate();
}
void UCFPlayerPresentationComponent::OnRep_Colour()
{
    if (ColourSlot >= 0 && Materials.IsEmpty() && HasBegunPlay() && !bTintWarningShown && GetOwner()->GetNetMode() != NM_DedicatedServer)
    {
        bTintWarningShown = true;
        UE_LOG(LogTemp, Warning, TEXT("CF_PRESENTATION: CF_PlayerTint parameter missing; apply Tools/Editor/configure_player_tint.py in Editor."));
    }
    for (UMaterialInstanceDynamic* Material : Materials) if (Material) Material->SetVectorParameterValue(TEXT("CF_PlayerTint"), UCFMatchRuntimeSubsystem::PlayerColour(ColourSlot));
}
void UCFPlayerPresentationComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    auto* Pawn = Cast<APawn>(GetOwner());
    const auto* Chicken = Cast<AChickenCharacter>(Pawn);
    if (JumpCharge)
        JumpCharge->SetVisibility(Chicken && Chicken->IsLocallyControlled() && Chicken->IsRoundInputEnabled() &&
            !Chicken->IsFallen() && !Chicken->IsKnockdownPending());
    const auto* State = Pawn ? Pawn->GetPlayerState() : nullptr;
    if (!Nameplate) return;
    auto* Widget = Cast<UCFNameplateWidget>(Nameplate->GetUserWidgetObject());
    const auto* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const auto* Frontend = Instance ? Instance->GetSubsystem<UCFFrontendSubsystem>() : nullptr;
    const bool bLocal = Pawn && Pawn->IsLocallyControlled();
    const bool bUseProfile = bLocal && ((Frontend && Frontend->HasAuthenticatedProfile()) || Cast<ACFLobbyController>(Pawn->GetController()));
    const FString ProfileName = Frontend && Frontend->HasAuthenticatedProfile() ? Frontend->ProfileNickname : FString();
    const FString Name = SelectDisplayNickname(State ? State->GetPlayerName() : FString(), ProfileName, bUseProfile);
    Nameplate->SetVisibility(!Name.IsEmpty());
    if (Widget) Widget->SetName(Name, UCFMatchRuntimeSubsystem::PlayerColour(ColourSlot));
}
FString UCFPlayerPresentationComponent::SelectDisplayNickname(const FString& ReplicatedName, const FString& ProfileName, bool bUseProfile)
{
    // 로컬 게임 프로필을 선택했을 때 빈 값을 Epic 이름으로 대체하지 않는다.
    return bUseProfile ? ProfileName : ReplicatedName;
}
void UCFPlayerPresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UCFPlayerPresentationComponent, ColourSlot);
}
