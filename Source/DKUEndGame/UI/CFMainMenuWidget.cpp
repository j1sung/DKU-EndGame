#include "UI/CFMainMenuWidget.h"
#include "Network/CFSessionSubsystem.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

void UCFMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>();
    HostButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleHost);
    JoinButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleJoin);
    ConnectButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleConnect);
    BackButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleBack);
    RefreshRoomsButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleRefresh);
    QuitButton->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleQuit);
    JoinPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (Service)
    {
        Service->OnChanged.AddUniqueDynamic(this,&ThisClass::RefreshState);
        NicknameInput->SetText(FText::FromString(Service->LocalNickname));
    }
    LastRoomList=TEXT("uninitialized"); RefreshState();
}

void UCFMainMenuWidget::NativeDestruct()
{
    if (Service) Service->OnChanged.RemoveDynamic(this,&ThisClass::RefreshState);
    HostButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleHost);
    JoinButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleJoin);
    ConnectButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleConnect);
    BackButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleBack);
    RefreshRoomsButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleRefresh);
    QuitButton->OnClicked.RemoveDynamic(this,&ThisClass::HandleQuit);
    Super::NativeDestruct();
}

void UCFMainMenuWidget::RefreshState()
{
    if (!Service) return;
    const bool Busy=Service->IsBusy();
    MainContent->SetIsEnabled(!Busy && JoinPanel->GetVisibility()==ESlateVisibility::Collapsed);
    HostButton->SetIsEnabled(!Busy); JoinButton->SetIsEnabled(!Busy); QuitButton->SetIsEnabled(!Busy);
    ConnectButton->SetIsEnabled(!Busy); RefreshRoomsButton->SetIsEnabled(!Busy);
    BackButton->SetIsEnabled(!Busy || Service->State==ECFSessionState::Searching);
    NicknameInput->SetIsEnabled(!Busy); AddressInput->SetIsEnabled(!Busy);
    StatusText->SetText(Service->StatusMessage); JoinStatusText->SetText(Service->StatusMessage);
    FString Key;
    for (const auto& Room:Service->Rooms) Key+=FString::Printf(TEXT("%s|%d|%d;"),*Room.Name,Room.Players,Room.Capacity);
    if (Key!=LastRoomList)
    {
        LastRoomList=Key; RoomCombo->ClearOptions();
        for (const auto& Room:Service->Rooms)
            RoomCombo->AddOption(FString::Printf(TEXT("%s  (%d / %d)"),*Room.Name,Room.Players,Room.Capacity));
        if (Service->Rooms.IsEmpty()) RoomCombo->AddOption(TEXT("검색된 방 없음"));
        RoomCombo->SetSelectedIndex(0);
    }
    RoomCombo->SetIsEnabled(!Busy && !Service->Rooms.IsEmpty());
}

void UCFMainMenuWidget::HandleHost()
{
    if (Service) Service->HostRoom(NicknameInput->GetText().ToString());
}

void UCFMainMenuWidget::HandleJoin()
{
    if (!Service || !Service->SetNickname(NicknameInput->GetText().ToString())) return;
    JoinPanel->SetVisibility(ESlateVisibility::Visible);
    Service->FindRooms(); BackButton->SetKeyboardFocus();
}

void UCFMainMenuWidget::HandleConnect()
{
    if (!Service || Service->IsBusy()) return;
    const FString Address=AddressInput->GetText().ToString().TrimStartAndEnd();
    const FString Nickname=NicknameInput->GetText().ToString();
    if (Address.IsEmpty()) Service->JoinRoom(RoomCombo->GetSelectedIndex(),Nickname);
    else Service->JoinAddress(Address,Nickname);
}

void UCFMainMenuWidget::HandleBack()
{
    if (!Service) return;
    if (Service->State==ECFSessionState::Searching) Service->CancelSearch();
    if (Service->IsBusy()) return;
    JoinPanel->SetVisibility(ESlateVisibility::Collapsed); RefreshState(); JoinButton->SetKeyboardFocus();
}

void UCFMainMenuWidget::HandleRefresh() { if (Service) Service->FindRooms(); }
void UCFMainMenuWidget::HandleQuit()
{
    if (!Service || !Service->IsBusy()) UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}
