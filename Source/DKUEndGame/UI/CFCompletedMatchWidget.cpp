#include "UI/CFCompletedMatchWidget.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

void UCFCompletedMatchWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.02f, .025f, .035f, 1));
    Background->SetHorizontalAlignment(HAlign_Center); Background->SetVerticalAlignment(VAlign_Center);
    Background->SetPadding(FMargin(40)); WidgetTree->RootWidget = Background;
    auto* Content = WidgetTree->ConstructWidget<UVerticalBox>(); Background->SetContent(Content);
    auto MakeText = [this](FName Name, const FText& Value, int32 Size)
    {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Label->SetText(Value); Label->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
        Label->SetColorAndOpacity(FSlateColor(FLinearColor(.98f, .94f, .85f)));
        return Label;
    };
    auto* Title = MakeText(TEXT("ResultTitle"), FText::FromString(TEXT("최종 결과")), 32);
    Content->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 24));

    // 라운드 결과처럼 헤더와 모든 행에 같은 열 너비를 사용한다.
    const float ColumnWidths[] = {72.f, 260.f, 120.f, 100.f};
    auto Cell = [this, &MakeText, &ColumnWidths](UHorizontalBox* Row, FName Name, const FText& Value, int32 Column, int32 FontSize)
    {
        auto* Box = WidgetTree->ConstructWidget<USizeBox>();
        Box->SetWidthOverride(ColumnWidths[Column]);
        auto* Label = MakeText(Name, Value, FontSize);
        Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Box->SetContent(Label);
        Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(6, 10));
        return Label;
    };
    auto* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Header);
    Cell(Header, TEXT("RankHeader"), FText::FromString(TEXT("순위")), 0, 17);
    Cell(Header, TEXT("PlayerHeader"), FText::FromString(TEXT("플레이어")), 1, 17);
    Cell(Header, TEXT("TotalHeader"), FText::FromString(TEXT("총점")), 2, 17);
    Cell(Header, TEXT("MVPHeader"), FText::FromString(TEXT("MVP")), 3, 17);

    const auto* Runtime = GetGameInstance()->GetSubsystem<UCFMatchRuntimeSubsystem>();
    const auto& Results = Runtime->Results();
    for (int32 Index = 0; Index < Results.Num(); ++Index)
    {
        const auto& Result = Results[Index];
        const bool bTied = Results.ContainsByPredicate([&Result](const FCFFinalStanding& Other)
        { return Other.PlayerId != Result.PlayerId && Other.Rank == Result.Rank; });
        const FString Rank = bTied ? FString::Printf(TEXT("공동 %d"), Result.Rank) : FString::FromInt(Result.Rank);
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
            FName(*FString::Printf(TEXT("FinalScoreRow%d"), Index)));
        Content->AddChildToVerticalBox(Row);
        Cell(Row, FName(*FString::Printf(TEXT("Rank%d"), Index)), FText::FromString(Rank), 0, 20);
        auto* Name = Cell(Row, FName(*FString::Printf(TEXT("Player%d"), Index)), FText::FromString(Result.PlayerName), 1, 20);
        Name->SetToolTipText(FText::FromString(Result.PlayerName));
        Cell(Row, FName(*FString::Printf(TEXT("Total%d"), Index)), FText::AsNumber(Result.TotalScore), 2, 20);
        Cell(Row, FName(*FString::Printf(TEXT("MVP%d"), Index)),
            FText::FromString(Result.Rank == 1 ? TEXT("MVP") : TEXT("—")), 3, 20);
    }

    Message = MakeText(TEXT("ResultMessage"), FText::FromString(TEXT("경기 종료를 확인하고 있습니다…")), 18);
    Message->SetAutoWrapText(true);
    auto* MessageBox = WidgetTree->ConstructWidget<USizeBox>();
    MessageBox->SetWidthOverride(600); MessageBox->SetContent(Message);
    Content->AddChildToVerticalBox(MessageBox)->SetPadding(FMargin(0, 24, 0, 0));
    ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
    auto* Label = MakeText(TEXT("ConfirmLabel"), FText::FromString(TEXT("확인 · 연습 로비로")), 22);
    ConfirmButton->SetContent(Label);
    auto* ConfirmSize = WidgetTree->ConstructWidget<USizeBox>();
    ConfirmSize->SetMinDesiredHeight(52); ConfirmSize->SetContent(ConfirmButton);
    Content->AddChildToVerticalBox(ConfirmSize)->SetPadding(FMargin(0, 20, 0, 0));
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
