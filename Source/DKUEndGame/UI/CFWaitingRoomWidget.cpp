#include "UI/CFWaitingRoomWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Network/CFSessionSubsystem.h"
#include "Network/CFWaitingNetwork.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

void UCFWaitingPlayerRowWidget::SetPlayer(const FText& Name, bool bHost, bool bEmpty)
{
    NameText->SetText(Name);
    NameText->SetToolTipText(Name);
    RoleText->SetText(FText::FromString(bHost ? TEXT("호스트") : TEXT("참가자")));
    RoleBadge->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    RoleBadge->SetBrushColor(bHost ? FLinearColor(.34f,.12f,.07f,.95f) : FLinearColor(.13f,.14f,.15f,.95f));
    ConnectionDot->SetBrushColor(bEmpty ? FLinearColor(.23f,.25f,.25f,1) : FLinearColor(.36f,.66f,.25f,1));
    NameText->SetRenderOpacity(bEmpty ? .5f : 1.f);
}

void UCFWaitingRoomWidget::NativeConstruct()
{
    Super::NativeConstruct();
    StartButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleStart);
    LeaveButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleLeave);
    LastDisplayKey.Reset();
    if (bReadWorldRoster) RefreshFromWorld();
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>())
        Service->OnChanged.AddUniqueDynamic(this,&ThisClass::RefreshSessionState);
    RefreshSessionState();
    GetWorld()->GetTimerManager().SetTimer(RosterTimer,this,&ThisClass::RefreshLiveDisplay,.35f,true);
}

void UCFWaitingRoomWidget::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RosterTimer);
    StartButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleStart);
    LeaveButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleLeave);
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>())
        Service->OnChanged.RemoveDynamic(this,&ThisClass::RefreshSessionState);
    Super::NativeDestruct();
}

void UCFWaitingRoomWidget::RefreshLiveDisplay()
{
    // Network roster updates must not depend on whether Slate paints this window.
    if (bReadWorldRoster) RefreshFromWorld();
    RefreshSessionState();
}

void UCFWaitingRoomWidget::RefreshFromWorld()
{
    auto* PC=GetOwningPlayer();
    auto* GS=GetWorld() ? GetWorld()->GetGameState() : nullptr;
    TArray<FText> Names;
    int32 HostIndex=INDEX_NONE;
    const bool bLocalHost=PC && PC->HasAuthority();
    const auto* Room=Cast<ACFWaitingGameState>(GS);
    if (GS)
    {
        for (APlayerState* State : GS->PlayerArray)
        {
            if (!IsValid(State) || State->IsInactive()) continue;
            if ((Room && State==Room->HostPlayerState) || (!Room && bLocalHost && State==PC->PlayerState)) HostIndex=Names.Num();
            const FString Name=State->GetPlayerName();
            Names.Add(FText::FromString(Name.IsEmpty() ? FString::Printf(TEXT("PLAYER %02d"),Names.Num()+1) : Name));
        }
    }
    // Showing connected players does not mean a match-start backend is ready.
    ApplyDisplay(Names,HostIndex,Room ? Room->RoomCapacity : MaxPlayers,bLocalHost,false);
}

void UCFWaitingRoomWidget::SetRoomDisplay(const TArray<FText>& PlayerNames,int32 HostIndex,int32 Capacity,bool bLocalHost,bool bCanStart)
{
    bReadWorldRoster=false;
    ApplyDisplay(PlayerNames,HostIndex,FMath::Clamp(Capacity,2,16),bLocalHost,bCanStart);
}

void UCFWaitingRoomWidget::UseWorldRoster()
{
    bReadWorldRoster=true; LastDisplayKey.Reset(); RefreshFromWorld();
}

void UCFWaitingRoomWidget::ApplyDisplay(const TArray<FText>& Names,int32 HostIndex,int32 Capacity,bool bLocalHost,bool bCanStart)
{
    FString Key=FString::Printf(TEXT("%d/%d/%d/%d/%d"),Names.Num(),HostIndex,Capacity,bLocalHost,bCanStart);
    for (const auto& Name:Names) Key+=TEXT("|")+Name.ToString();
    if (Key==LastDisplayKey) return;
    LastDisplayKey=Key;
    DisplayedPlayerCount=Names.Num();
    ParticipantCountText->SetText(FText::Format(FText::FromString(TEXT("참가자 {0} / {1}")),FText::AsNumber(Names.Num()),FText::AsNumber(Capacity)));
    PlayerList->ClearChildren();
    if (PlayerRowClass)
    {
        for (int32 Index=0;Index<Names.Num();++Index)
        {
            auto* Entry=CreateWidget<UCFWaitingPlayerRowWidget>(GetOwningPlayer(),PlayerRowClass);
            PlayerList->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0,0,0,10));
            Entry->SetPlayer(Names[Index],Index==HostIndex,false);
        }
        if (Names.Num()<Capacity)
        {
            auto* Empty=CreateWidget<UCFWaitingPlayerRowWidget>(GetOwningPlayer(),PlayerRowClass);
            PlayerList->AddChildToVerticalBox(Empty);
            Empty->SetPlayer(FText::FromString(TEXT("참가 대기 중")),false,true);
        }
    }
    bStartPermitted=bLocalHost && bCanStart && Names.Num()>=2;
    StartButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    StartButton->SetIsEnabled(bStartPermitted);
    FString Message;
    if (!bLocalHost) Message=TEXT("호스트가 경기를 시작할 때까지 기다려 주세요.");
    else if (Names.Num()<2) Message=TEXT("다른 플레이어를 기다리고 있습니다.");
    else Message=bCanStart ? TEXT("플레이어가 모두 모였습니다.") : TEXT("경기 시작을 준비하고 있습니다.");
    StatusText->SetText(FText::FromString(Message));
    StartButton->SetToolTipText(FText::FromString(Message));
}

void UCFWaitingRoomWidget::HandleStart()
{
    if (bStartPermitted && StartButton->GetIsEnabled()) OnStartRequested.Broadcast();
}

void UCFWaitingRoomWidget::HandleLeave()
{
    if (OnLeaveRequested.IsBound()) { OnLeaveRequested.Broadcast(); return; }
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>()) { Service->LeaveRoom(); return; }
    if (auto* PC=GetOwningPlayer()) PC->ClientTravel(TEXT("/Game/Maps/Lvl_CF_MainMenu"),TRAVEL_Absolute);
}

void UCFWaitingRoomWidget::RefreshSessionState()
{
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>())
    {
        const bool bLeaving=Service->State==ECFSessionState::Leaving;
        LeaveButton->SetIsEnabled(!bLeaving);
        if (bLeaving) { StatusText->SetText(Service->StatusMessage); StartButton->SetIsEnabled(false); }
    }
}
