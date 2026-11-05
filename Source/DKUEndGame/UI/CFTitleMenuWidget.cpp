#include "UI/CFTitleMenuWidget.h"
#include "UI/CFTitleMenuController.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"

void UCFTitleMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    StartButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Start);
    QuitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Quit);
    LoginButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Login);
    LoginBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Back);
    NicknameButton->OnClicked.AddUniqueDynamic(this, &ThisClass::SubmitNickname);
    NicknameBackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Back);
    NicknameInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::NicknameChanged);
    NicknameInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::NicknameCommitted);
    Flow = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>();
    Flow->OnStateChanged.AddUniqueDynamic(this, &ThisClass::RefreshState);
    bInitialFocusPending = true;
    RefreshState();
}

void UCFTitleMenuWidget::NativeDestruct()
{
    if (Flow) Flow->OnStateChanged.RemoveDynamic(this, &ThisClass::RefreshState);
    StartButton->OnClicked.RemoveDynamic(this, &ThisClass::Start);
    QuitButton->OnClicked.RemoveDynamic(this, &ThisClass::Quit);
    LoginButton->OnClicked.RemoveDynamic(this, &ThisClass::Login);
    LoginBackButton->OnClicked.RemoveDynamic(this, &ThisClass::Back);
    NicknameButton->OnClicked.RemoveDynamic(this, &ThisClass::SubmitNickname);
    NicknameBackButton->OnClicked.RemoveDynamic(this, &ThisClass::Back);
    NicknameInput->OnTextChanged.RemoveDynamic(this, &ThisClass::NicknameChanged);
    NicknameInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::NicknameCommitted);
    Super::NativeDestruct();
}

void UCFTitleMenuWidget::RefreshState()
{
    if (!Flow) return;
    const auto Screen = Flow->Screen;
    const bool bLogin = Screen == ECFFrontendScreen::Login || Screen == ECFFrontendScreen::LoggingIn;
    const bool bNickname = Screen == ECFFrontendScreen::Nickname || Screen == ECFFrontendScreen::SavingNickname;
    ScreenBackground->SetBrushColor(FLinearColor::FromSRGBColor(FColor::FromHex(bLogin || bNickname ? TEXT("18181C") : TEXT("FFF9F0"))));
    TitlePanel->SetVisibility(Screen == ECFFrontendScreen::Title ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    LoginPanel->SetVisibility(bLogin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    NicknamePanel->SetVisibility(bNickname ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ConnectingPanel->SetVisibility(Screen == ECFFrontendScreen::Connecting ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    LoginButton->SetVisibility(Screen == ECFFrontendScreen::LoggingIn ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    LoginProgress->SetVisibility(Screen == ECFFrontendScreen::LoggingIn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    LoginButton->SetIsEnabled(Screen == ECFFrontendScreen::Login);
    NicknameInput->SetIsEnabled(Screen == ECFFrontendScreen::Nickname);
    FString Normalized;
    NicknameButton->SetIsEnabled(Screen == ECFFrontendScreen::Nickname && UCFFrontendSubsystem::ValidateNickname(NicknameInput->GetText().ToString(), Normalized));
    NicknameButtonLabel->SetText(FText::FromString(Screen == ECFFrontendScreen::SavingNickname ? TEXT("등록 중...") : TEXT("설정")));
    for (auto* Text : {TitleError.Get(), LoginError.Get(), NicknameError.Get()})
    {
        Text->SetText(Flow->ErrorMessage);
        Text->SetVisibility(Flow->ErrorMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (PreviousScreen != Screen)
    {
        PreviousScreen = Screen;
        FocusCurrentScreen();
    }
}

void UCFTitleMenuWidget::FocusCurrentScreen()
{
    if (!Flow) return;
    if (Flow->Screen == ECFFrontendScreen::Title) StartButton->SetKeyboardFocus();
    else if (Flow->Screen == ECFFrontendScreen::Login) LoginButton->SetKeyboardFocus();
    else if (Flow->Screen == ECFFrontendScreen::Nickname) NicknameInput->SetKeyboardFocus();
    else if (Flow->Screen == ECFFrontendScreen::LoggingIn) LoginBackButton->SetKeyboardFocus();
    else if (Flow->Screen == ECFFrontendScreen::SavingNickname) NicknameBackButton->SetKeyboardFocus();
    else SetKeyboardFocus();
}

void UCFTitleMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    // PIE 창이 만들어지기 전 BeginPlay에서 지정한 포커스는 아직 적용되지 않을 수 있다.
    if (bInitialFocusPending)
    {
        bInitialFocusPending = false;
        FocusCurrentScreen();
    }
    if (Flow && Flow->Screen == ECFFrontendScreen::Connecting)
    {
        RingAngle = FMath::Fmod(RingAngle + DeltaTime * 180.f, 360.f);
        LoadingRing->SetRenderTransformAngle(RingAngle);
    }
}

FReply UCFTitleMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
#if !UE_BUILD_SHIPPING
    if (Event.GetKey() == EKeys::F9)
    {
        if (auto* PC = Cast<ACFTitleMenuController>(GetOwningPlayer())) PC->CFMenuLan();
        return FReply::Handled();
    }
#endif
    if (Event.GetKey() == EKeys::Escape && Flow && Flow->Screen != ECFFrontendScreen::Title && Flow->Screen != ECFFrontendScreen::Connecting)
    {
        Back(); return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UCFTitleMenuWidget::Start() { if (Flow) Flow->OpenEntry(); }
void UCFTitleMenuWidget::Login() { if (Flow) Flow->RequestLogin(); }
void UCFTitleMenuWidget::Back() { if (Flow) Flow->ReturnToTitle(); }
void UCFTitleMenuWidget::Quit()
{
    if (Flow && Flow->Screen == ECFFrontendScreen::Title)
        UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
void UCFTitleMenuWidget::SubmitNickname() { if (Flow) Flow->RequestNickname(NicknameInput->GetText().ToString()); }
void UCFTitleMenuWidget::NicknameChanged(const FText& Text) { if (Flow) Flow->ClearError(); RefreshState(); }
void UCFTitleMenuWidget::NicknameCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (Method == ETextCommit::OnEnter) SubmitNickname();
}
