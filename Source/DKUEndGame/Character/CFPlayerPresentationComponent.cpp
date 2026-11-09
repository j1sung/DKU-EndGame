#include "Character/CFPlayerPresentationComponent.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

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
    const auto* State = Pawn ? Pawn->GetPlayerState() : nullptr;
    if (!Nameplate) return;
    auto* Widget = Cast<UCFNameplateWidget>(Nameplate->GetUserWidgetObject());
    Nameplate->SetVisibility(State && !State->GetPlayerName().IsEmpty());
    if (Widget && State) Widget->SetName(State->GetPlayerName(), UCFMatchRuntimeSubsystem::PlayerColour(ColourSlot));
}
void UCFPlayerPresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(UCFPlayerPresentationComponent, ColourSlot);
}
