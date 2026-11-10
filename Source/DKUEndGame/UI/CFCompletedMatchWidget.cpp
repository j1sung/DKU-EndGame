#include "UI/CFCompletedMatchWidget.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UCFCompletedMatchWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.02f, .025f, .035f, 1));
    Background->SetHorizontalAlignment(HAlign_Center); Background->SetVerticalAlignment(VAlign_Center);
    Background->SetPadding(FMargin(40)); WidgetTree->RootWidget = Background;
    auto* Content = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Content);
    auto AddText = [this, Content](const FString& Value, int32 Size)
    {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Value)); Label->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
        Content->AddChildToVerticalBox(Label); return Label;
    };
    AddText(TEXT("최종 결과"), 32);
    const auto* Runtime = GetGameInstance()->GetSubsystem<UCFMatchRuntimeSubsystem>();
    for (const auto& Row : Runtime->Results())
        AddText(FString::Printf(TEXT("%d위   %s   %d점%s"), Row.Rank, *Row.PlayerName, Row.TotalScore,
            Row.Rank == 1 ? TEXT("   MVP") : TEXT("")), 24);
    Message = AddText(TEXT("경기 종료를 확인하고 있습니다…"), 18);
    ConfirmButton = WidgetTree->ConstructWidget<UButton>();
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("확인 · 연습 로비로")));
    ConfirmButton->SetContent(Label); Content->AddChildToVerticalBox(ConfirmButton);
    ConfirmButton->SetIsEnabled(false); ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Confirm);
}
void UCFCompletedMatchWidget::NativeTick(const FGeometry& Geometry, float Delta)
{
    Super::NativeTick(Geometry, Delta);
    const auto* Runtime = GetGameInstance()->GetSubsystem<UCFMatchRuntimeSubsystem>();
    const bool bReady = Runtime && Runtime->CanConfirmResults();
    ConfirmButton->SetIsEnabled(bReady);
    Message->SetText(FText::FromString(bReady ? TEXT("확인을 누르면 연습 로비로 돌아갑니다.") : TEXT("경기 종료와 연결 정리를 확인하고 있습니다…")));
}
void UCFCompletedMatchWidget::Confirm()
{
    if (auto* Runtime = GetGameInstance()->GetSubsystem<UCFMatchRuntimeSubsystem>()) Runtime->ConfirmResults();
}
