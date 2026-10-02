#include "UI/CFMatchStatusWidget.h"
#include "Network/CFWaitingNetwork.h"
#include "Network/CFSessionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"

void UCFMatchStatusWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); SetIsFocusable(false);
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("MatchRoot"));
    WidgetTree->RootWidget=Canvas; Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Text=[this,Canvas](FName Name,int32 Size,FVector2D Position,FAnchors Anchor,FVector2D Alignment)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
        T->SetColorAndOpacity(FSlateColor(FLinearColor(.98f,.94f,.85f)));
        T->SetShadowOffset(FVector2D(2,2)); T->SetShadowColorAndOpacity(FLinearColor(0,0,0,.85f));
        T->SetJustification(ETextJustify::Center); T->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* CanvasSlot=Canvas->AddChildToCanvas(T); CanvasSlot->SetAnchors(Anchor); CanvasSlot->SetAlignment(Alignment); CanvasSlot->SetPosition(Position); CanvasSlot->SetAutoSize(true);
        return T;
    };
    RoundText=Text(TEXT("RoundText"),24,FVector2D(0,28),FAnchors(.5f,0),FVector2D(.5f,0));
    CountdownText=Text(TEXT("CountdownText"),64,FVector2D(0,-65),FAnchors(.5f,.5f),FVector2D(.5f,.5f));
    auto* Hint=Text(TEXT("MenuHint"),16,FVector2D(-24,28),FAnchors(1,0),FVector2D(1,0));
    Hint->SetText(FText::FromString(TEXT("Esc : 메뉴")));
    LeaveButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("LeaveButton"));
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("방 나가기"))); LeaveButton->SetContent(Label);
    auto* CanvasSlot=Canvas->AddChildToCanvas(LeaveButton); CanvasSlot->SetAnchors(FAnchors(1,0)); CanvasSlot->SetAlignment(FVector2D(1,0)); CanvasSlot->SetPosition(FVector2D(-24,64)); CanvasSlot->SetSize(FVector2D(140,44));
    LeaveButton->OnClicked.AddUniqueDynamic(this,&ThisClass::LeaveRoom);
}

void UCFMatchStatusWidget::Refresh(const ACFWaitingGameState* State,bool bMenuOpen)
{
    RoundText->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d   ·   생존 %d / %d"),State->CurrentRound,State->TotalRounds,State->AlivePlayers.Num(),State->Participants.Num())));
    const bool bCountdown=State->Phase==ECFMatchPhase::Countdown;
    CountdownText->SetVisibility(bCountdown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    CountdownText->SetText(FText::FromString(FString::Printf(TEXT("경기 준비\n%d"),State->GetCountdownSeconds())));
    LeaveButton->SetVisibility(bMenuOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCFMatchStatusWidget::LeaveRoom()
{
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>()) Service->LeaveRoom();
}
