#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "CFMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UCFSessionSubsystem;

/** Designer owns the layout; the subsystem owns asynchronous connection state. */
UCLASS(Abstract,Blueprintable)
class DKUENDGAME_API UCFMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> HostButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> JoinButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> QuitButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ConnectButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> BackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RefreshRoomsButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> NicknameInput;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> AddressInput;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> JoinStatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> JoinPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> MainContent;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UComboBoxString> RoomCombo;
private:
    UPROPERTY(Transient) TObjectPtr<UCFSessionSubsystem> Service;
    FString LastRoomList;
    UFUNCTION() void HandleHost();
    UFUNCTION() void HandleJoin();
    UFUNCTION() void HandleConnect();
    UFUNCTION() void HandleBack();
    UFUNCTION() void HandleRefresh();
    UFUNCTION() void HandleQuit();
    UFUNCTION() void RefreshState();
};
