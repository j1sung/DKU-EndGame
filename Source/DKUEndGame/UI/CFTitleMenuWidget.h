#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/CFFrontendSubsystem.h"
#include "Components/EditableTextBox.h"
#include "CFTitleMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class UImage;
class UBorder;

/** 배치는 WBP_CF_TitleMenu의 Designer, 동작은 이 클래스와 FrontendSubsystem이 담당한다. */
UCLASS(Abstract, Blueprintable)
class DKUENDGAME_API UCFTitleMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void FocusCurrentScreen();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> ScreenBackground;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> TitlePanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> LoginPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> NicknamePanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> ConnectingPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> StartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> QuitButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LoginButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LoginBackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NicknameButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> NicknameBackButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> NicknameInput;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> TitleError;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> LoginError;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> NicknameError;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> LoginProgress;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> NicknameButtonLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> LoadingRing;
private:
    UPROPERTY(Transient) TObjectPtr<UCFFrontendSubsystem> Flow;
    ECFFrontendScreen PreviousScreen = ECFFrontendScreen::Connecting;
    float RingAngle = 0;
    bool bInitialFocusPending = true;
    UFUNCTION() void RefreshState();
    UFUNCTION() void Start();
    UFUNCTION() void Quit();
    UFUNCTION() void Login();
    UFUNCTION() void Back();
    UFUNCTION() void SubmitNickname();
    UFUNCTION() void NicknameChanged(const FText& Text);
    UFUNCTION() void NicknameCommitted(const FText& Text, ETextCommit::Type Method);
};
