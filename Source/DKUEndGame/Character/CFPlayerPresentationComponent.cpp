#include "Character/CFPlayerPresentationComponent.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
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
    PrimaryComponentTick.bCanEverTick = true;
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

        // 상대 게이지는 깊이 테스트가 적용되는 월드 위젯으로 그려 벽 뒤로 비치지 않게 한다.
        OpponentJumpCharge = NewObject<UWidgetComponent>(Character, TEXT("OpponentJumpCharge"));
        OpponentJumpCharge->SetupAttachment(Character->GetRootComponent());
        OpponentJumpCharge->SetWidgetSpace(EWidgetSpace::World);
        OpponentJumpCharge->SetDrawSize(FVector2D(160, 160));
        OpponentJumpCharge->SetPivot(FVector2D(.5f, .5f));
        OpponentJumpCharge->SetRelativeScale3D(FVector(.65f));
        OpponentJumpCharge->SetBlendMode(EWidgetBlendMode::Masked);
        OpponentJumpCharge->SetCastShadow(false);
        OpponentJumpCharge->SetWidgetClass(UCFJumpChargeWidget::StaticClass());
        OpponentJumpCharge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        OpponentJumpCharge->SetVisibility(false);
        OpponentJumpCharge->RegisterComponent();
        OpponentJumpCharge->InitWidget();
        if (auto* Gauge = Cast<UCFJumpChargeWidget>(OpponentJumpCharge->GetUserWidgetObject()))
            Gauge->SetCharacter(Cast<AChickenCharacter>(Character));
        // 런타임 생성 직후에도 프록시가 실제 게이지 렌더 타깃을 참조하도록 초기화한다.
        OpponentJumpCharge->UpdateWidget();
        OpponentJumpCharge->UpdateRenderTarget(FIntPoint(160, 160));
        OpponentJumpCharge->SetMaterial(0, OpponentJumpCharge->GetMaterial(0));
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
    const bool bCanShowCharge = Chicken && Chicken->IsRoundInputEnabled() &&
        !Chicken->IsFallen() && !Chicken->IsKnockdownPending();
    if (JumpCharge)
        JumpCharge->SetVisibility(bCanShowCharge && Chicken->IsLocallyControlled());
    const auto* State = Pawn ? Pawn->GetPlayerState() : nullptr;
    if (OpponentJumpCharge)
    {
        bool bShowOpponent = false;
        auto* Viewer = GetWorld()->GetFirstPlayerController();
        if (bCanShowCharge && !Chicken->IsLocallyControlled() && State && Viewer && Viewer->IsLocalController())
        {
            FVector CameraLocation; FRotator CameraRotation;
            Viewer->GetPlayerViewPoint(CameraLocation, CameraRotation);
            const FVector Target = OpponentJumpCharge->GetComponentLocation();
            FVector2D ScreenPosition;
            int32 Width=0, Height=0;
            Viewer->GetViewportSize(Width, Height);
            bShowOpponent = FVector::DotProduct(Target-CameraLocation, CameraRotation.Vector()) > 0.f &&
                Viewer->ProjectWorldLocationToScreen(Target, ScreenPosition) &&
                ScreenPosition.X >= 0.f && ScreenPosition.Y >= 0.f &&
                ScreenPosition.X < Width && ScreenPosition.Y < Height;
            if (bShowOpponent)
                OpponentJumpCharge->SetWorldRotation((CameraLocation-Target).Rotation());
        }
        OpponentJumpCharge->SetVisibility(bShowOpponent);
    }
    // 게이지 방향만 매 프레임 갱신하고 이름 표시는 기존 10Hz를 유지한다.
    NameplateUpdateRemaining -= Delta;
    if (NameplateUpdateRemaining > 0.f) return;
    NameplateUpdateRemaining = .1f;
    if (!Nameplate) return;
    auto* Widget = Cast<UCFNameplateWidget>(Nameplate->GetUserWidgetObject());
    Nameplate->SetVisibility(State && !State->GetPlayerName().IsEmpty());
    if (Widget && State) Widget->SetName(State->GetPlayerName(), UCFMatchRuntimeSubsystem::PlayerColour(ColourSlot));
}
void UCFPlayerPresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UCFPlayerPresentationComponent, ColourSlot);
}
