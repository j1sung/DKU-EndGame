#include "UI/CFMatchStatusWidget.h"
#include "Network/CFWaitingNetwork.h"
#include "Network/CFSessionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"

void UCFMatchStatusWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized(); SetIsFocusable(false);
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("MatchRoot"));
    WidgetTree->RootWidget=Canvas; Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto MakeText=[this](FName Name,int32 Size)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font);
        T->SetColorAndOpacity(FSlateColor(FLinearColor(.98f,.94f,.85f)));
        T->SetShadowOffset(FVector2D(1,1)); T->SetShadowColorAndOpacity(FLinearColor(0,0,0,.8f));
        T->SetJustification(ETextJustify::Center); T->SetVisibility(ESlateVisibility::HitTestInvisible);
        return T;
    };
    auto Text=[Canvas,&MakeText](FName Name,int32 Size,FVector2D Position,FAnchors Anchor,FVector2D Alignment)
    {
        auto* T=MakeText(Name,Size);
        auto* Slot=Canvas->AddChildToCanvas(T); Slot->SetAnchors(Anchor); Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position); Slot->SetAutoSize(true); return T;
    };
    RoundText=Text(TEXT("RoundText"),24,FVector2D(0,28),FAnchors(.5f,0),FVector2D(.5f,0));
    CountdownText=Text(TEXT("CountdownText"),64,FVector2D(0,-65),FAnchors(.5f,.5f),FVector2D(.5f,.5f));
    SpectatorText=Text(TEXT("SpectatorText"),20,FVector2D(0,-35),FAnchors(.5f,1),FVector2D(.5f,1));
    SpectatorText->SetText(FText::FromString(TEXT("탈락 · 관전 중")));
    auto* Hint=Text(TEXT("MenuHint"),16,FVector2D(-24,28),FAnchors(1,0),FVector2D(1,0));
    Hint->SetText(FText::FromString(TEXT("Esc : 메뉴")));

    ResultPanel=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ResultPanel"));
    ResultPanel->SetBrushColor(FLinearColor(.025f,.02f,.018f,.94f)); ResultPanel->SetPadding(FMargin(28));
    auto* PanelSlot=Canvas->AddChildToCanvas(ResultPanel); PanelSlot->SetAnchors(FAnchors(.5f,.5f));
    PanelSlot->SetAlignment(FVector2D(.5f,.5f)); PanelSlot->SetAutoSize(true);
    auto* Content=WidgetTree->ConstructWidget<UVerticalBox>(); ResultPanel->SetContent(Content);
    ResultText=MakeText(TEXT("ResultText"),30);
    Content->AddChildToVerticalBox(ResultText)->SetPadding(FMargin(0,0,0,24));
    auto Cell=[this,&MakeText](UHorizontalBox* Row,FName Name,const TCHAR* Label,float Width,int32 Size)
    {
        auto* Box=WidgetTree->ConstructWidget<USizeBox>(); Box->SetWidthOverride(Width);
        auto* T=MakeText(Name,Size); T->SetText(FText::FromString(Label));
        T->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); Box->SetContent(T);
        Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(6,10)); return T;
    };
    auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();
    Content->AddChildToVerticalBox(Header);
    Cell(Header,TEXT("RankHeader"),TEXT("순위"),72,17);
    Cell(Header,TEXT("PlayerHeader"),TEXT("플레이어"),260,17);
    PointsHeader=Cell(Header,TEXT("PointsHeader"),TEXT("이번 라운드"),120,17);
    TotalHeader=Cell(Header,TEXT("TotalHeader"),TEXT("누적"),100,17);
    for (int32 I=0;I<4;++I)
    {
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),FName(*FString::Printf(TEXT("ScoreRow%d"),I)));
        Content->AddChildToVerticalBox(Row); ScoreRows.Add(Row);
        RankTexts.Add(Cell(Row,FName(*FString::Printf(TEXT("Rank%d"),I)),TEXT(""),72,20));
        NameTexts.Add(Cell(Row,FName(*FString::Printf(TEXT("Player%d"),I)),TEXT(""),260,20));
        PointsTexts.Add(Cell(Row,FName(*FString::Printf(TEXT("Points%d"),I)),TEXT(""),120,20));
        TotalTexts.Add(Cell(Row,FName(*FString::Printf(TEXT("Total%d"),I)),TEXT(""),100,20));
    }
    RoundProgressText=MakeText(TEXT("RoundProgressText"),18);
    RoundProgressText->SetAutoWrapText(true);
    auto* ProgressBox=WidgetTree->ConstructWidget<USizeBox>(); ProgressBox->SetWidthOverride(600); ProgressBox->SetContent(RoundProgressText);
    Content->AddChildToVerticalBox(ProgressBox)->SetPadding(FMargin(0,24,0,0));

    ReturnButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ReturnButton"));
    ReturnButton->SetBackgroundColor(FLinearColor(.55f,.18f,.09f,1.f));
    auto* ReturnLabel=MakeText(TEXT("ReturnLabel"),22); ReturnLabel->SetText(FText::FromString(TEXT("방 대기로 돌아가기")));
    ReturnButton->SetContent(ReturnLabel);
    auto* ReturnSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("ReturnSize")); ReturnSize->SetMinDesiredHeight(52); ReturnSize->SetContent(ReturnButton);
    Content->AddChildToVerticalBox(ReturnSize)->SetPadding(FMargin(0,20,0,0));
    ReturnButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ReturnToWaiting);

    LeaveButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("LeaveButton"));
    auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(FText::FromString(TEXT("방 나가기"))); LeaveButton->SetContent(Label);
    auto* CanvasSlot=Canvas->AddChildToCanvas(LeaveButton); CanvasSlot->SetAnchors(FAnchors(1,0)); CanvasSlot->SetAlignment(FVector2D(1,0)); CanvasSlot->SetPosition(FVector2D(-24,64)); CanvasSlot->SetSize(FVector2D(140,44));
    LeaveButton->OnClicked.AddUniqueDynamic(this,&ThisClass::LeaveRoom);
}

void UCFMatchStatusWidget::Refresh(const ACFWaitingGameState* State,bool bMenuOpen,bool bEliminated)
{
    RoundText->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d   ·   생존 %d / %d"),State->CurrentRound,State->TotalRounds,State->AlivePlayers.Num(),State->Participants.Num())));
    const bool bCountdown=State->Phase==ECFMatchPhase::Countdown;
    CountdownText->SetVisibility(bCountdown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    CountdownText->SetText(FText::FromString(FString::Printf(TEXT("경기 준비\n%d"),State->GetCountdownSeconds())));
    const bool bFinal=State->Phase==ECFMatchPhase::FinalResult;
    const bool bResult=State->Phase==ECFMatchPhase::RoundResult || bFinal;
    auto* PC=Cast<ACFWaitingPlayerController>(GetOwningPlayer());
    const bool bHost=PC && PC->HasAuthority() && PC->PlayerState==State->HostPlayerState;
    ResultPanel->SetVisibility(bResult ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bFinal) ReturnError=FText::GetEmpty();
    const FString Result=bFinal ? TEXT("최종 결과") : State->bRoundDraw ? TEXT("라운드 종료 · 무승부") :
        State->RoundWinnerName.IsEmpty() ? TEXT("라운드 종료") : FString::Printf(TEXT("라운드 종료\n승자: %s"),*State->RoundWinnerName);
    ResultText->SetText(FText::FromString(Result));
    PointsHeader->SetText(FText::FromString(bFinal ? TEXT("MVP") : TEXT("이번 라운드")));
    TotalHeader->SetText(FText::FromString(bFinal ? TEXT("총점") : TEXT("누적")));
    const bool bScoresReady=State->ScoredRound==State->CurrentRound;
    int32 MVPCount=0;
    for (int32 I=0;I<ScoreRows.Num();++I)
    {
        const bool bShow=bFinal ? State->FinalStandings.IsValidIndex(I) : bScoresReady && State->Scoreboard.IsValidIndex(I);
        ScoreRows[I]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow) continue;
        FString Rank,Name,Points; int32 Total=0; bool bConnected=true,bMVP=false;
        if (bFinal)
        {
            const auto& Row=State->FinalStandings[I];
            const bool bTied=State->FinalStandings.ContainsByPredicate([&Row](const FCFFinalStanding& R) { return R.PlayerId!=Row.PlayerId && R.Rank==Row.Rank; });
            Rank=bTied ? FString::Printf(TEXT("공동 %d"),Row.Rank) : FString::FromInt(Row.Rank);
            Name=Row.PlayerName; Total=Row.TotalScore; bConnected=Row.bConnected; bMVP=Row.Rank==1;
            Points=bMVP ? TEXT("MVP") : TEXT("—"); if (bMVP) ++MVPCount;
        }
        else
        {
            const auto& Row=State->Scoreboard[I];
            const bool bTied=Row.Rank>0 && State->Scoreboard.ContainsByPredicate([&Row](const FCFPlayerRoundScore& R) { return R.PlayerId!=Row.PlayerId && R.Rank==Row.Rank; });
            Rank=Row.bForfeited ? TEXT("기권") : Row.Rank>0 ? (bTied ? FString::Printf(TEXT("공동 %d"),Row.Rank) : FString::FromInt(Row.Rank)) : TEXT("—");
            Name=Row.PlayerName; Total=Row.TotalScore; bConnected=Row.bConnected;
            Points=FString::Printf(TEXT("+%d"),Row.RoundScore);
        }
        RankTexts[I]->SetText(FText::FromString(Rank));
        NameTexts[I]->SetText(FText::FromString(Name+(bConnected ? TEXT("") : TEXT(" (퇴장)"))));
        PointsTexts[I]->SetText(FText::FromString(Points)); TotalTexts[I]->SetText(FText::AsNumber(Total));
        ScoreRows[I]->SetRenderOpacity(bConnected ? 1.f : .6f);
        const FSlateColor Color(bMVP ? FLinearColor(1.f,.72f,.25f) : FLinearColor(.98f,.94f,.85f));
        for (auto* Text:{RankTexts[I].Get(),NameTexts[I].Get(),PointsTexts[I].Get(),TotalTexts[I].Get()}) Text->SetColorAndOpacity(Color);
    }
    FString Progress=State->RoundProgressMessage;
    if (bFinal)
    {
        Progress=FString::Printf(TEXT("%d라운드 완료"),State->ScoredRound);
        if (MVPCount>1) Progress+=FString::Printf(TEXT(" · 공동 MVP %d명"),MVPCount);
        if (State->ScoredRound<State->TotalRounds) Progress+=TEXT(" · 경기 조기 종료");
        Progress+=bHost ? TEXT("\n방 대기로 돌아가 새 경기를 시작할 수 있습니다.") : TEXT("\n호스트가 방 대기로 돌아가기를 기다리고 있습니다.");
        if (!ReturnError.IsEmpty()) Progress=ReturnError.ToString();
    }
    else if (State->NextRoundEndServerTime>0)
    {
        const FString Next=State->bFinalResultPending ? FString::Printf(TEXT("%d초 후 최종 결과"),State->GetResultSeconds()) :
            FString::Printf(TEXT("%d초 후 %d라운드 시작"),State->GetResultSeconds(),State->CurrentRound+1);
        Progress=Progress.IsEmpty() ? Next : Progress+TEXT("\n")+Next;
    }
    RoundProgressText->SetText(FText::FromString(Progress));
    WidgetTree->FindWidget(TEXT("ReturnSize"))->SetVisibility(bFinal && bHost ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    ReturnButton->SetVisibility(bFinal && bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ReturnButton->SetIsEnabled(bFinal && bHost && !State->bRoomClosing);
    SpectatorText->SetVisibility(bEliminated && !bFinal ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    LeaveButton->SetVisibility(bMenuOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UCFMatchStatusWidget::LeaveRoom()
{
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>()) Service->LeaveRoom();
}

void UCFMatchStatusWidget::ReturnToWaiting()
{
    ReturnError=FText::GetEmpty();
    if (auto* PC=Cast<ACFWaitingPlayerController>(GetOwningPlayer())) PC->RequestReturnToWaiting();
}
