#include "UI/CFLobbyWidget.h"
#include "UI/CFLobbySubsystem.h"
#include "UI/CFLobbyController.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

namespace { FText Text(const TCHAR* Value) { return FText::FromString(Value); } }

void UCFLobbyWidget::NativeConstruct()
{
    Super::NativeConstruct();
    Lobby = GetGameInstance()->GetSubsystem<UCFLobbySubsystem>();
    Lobby->OnStateChanged.AddDynamic(this, &ThisClass::Refresh);
    Lobby->OnAudioChanged.AddDynamic(this, &ThisClass::RefreshAudio);
    SettingsButton->OnClicked.AddDynamic(this, &ThisClass::ToggleSettings);
    MatchButton->OnClicked.AddDynamic(this, &ThisClass::StartMatch);
    CancelButton->OnClicked.AddDynamic(this, &ThisClass::CancelMatch);
    ReturnButton->OnClicked.AddDynamic(this, &ThisClass::ReturnToTitle);
    MusicSlider->OnValueChanged.AddDynamic(this, &ThisClass::MusicChanged);
    EffectsSlider->OnValueChanged.AddDynamic(this, &ThisClass::EffectsChanged);
    MusicSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::SaveAudio);
    EffectsSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::SaveAudio);
    MusicSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::SaveAudio);
    EffectsSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::SaveAudio);
    SetIsFocusable(true); bSettingsOpen = false;
    SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    RefreshAudio(); Refresh();
}

void UCFLobbyWidget::NativeDestruct()
{
    if (Lobby)
    {
        Lobby->OnStateChanged.RemoveDynamic(this, &ThisClass::Refresh);
        Lobby->OnAudioChanged.RemoveDynamic(this, &ThisClass::RefreshAudio);
    }
    SettingsButton->OnClicked.RemoveDynamic(this, &ThisClass::ToggleSettings);
    MatchButton->OnClicked.RemoveDynamic(this, &ThisClass::StartMatch);
    CancelButton->OnClicked.RemoveDynamic(this, &ThisClass::CancelMatch);
    ReturnButton->OnClicked.RemoveDynamic(this, &ThisClass::ReturnToTitle);
    MusicSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::MusicChanged);
    EffectsSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::EffectsChanged);
    MusicSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::SaveAudio);
    EffectsSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::SaveAudio);
    MusicSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::SaveAudio);
    EffectsSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::SaveAudio);
    Super::NativeDestruct();
}

void UCFLobbyWidget::ToggleSettings()
{
    bSettingsOpen = !bSettingsOpen;
    SettingsPanel->SetVisibility(bSettingsOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bSettingsOpen) SaveAudio();
}

void UCFLobbyWidget::StartMatch() { Lobby->RequestMatchmaking(); }
void UCFLobbyWidget::CancelMatch()
{
    if (Lobby->Screen == ECFQueueScreen::Recovering) Lobby->RecoverStatus();
    else Lobby->RequestCancel();
}
void UCFLobbyWidget::ReturnToTitle() { Lobby->RequestReturnToTitle(); }
void UCFLobbyWidget::MusicChanged(float Value) { Lobby->SetMusicVolume(Value); }
void UCFLobbyWidget::EffectsChanged(float Value) { Lobby->SetEffectsVolume(Value); }
void UCFLobbyWidget::SaveAudio() { if (Lobby) Lobby->SaveAudioSettings(); }
void UCFLobbyWidget::RefreshAudio()
{
    MusicSlider->SetValue(Lobby->MusicVolume); EffectsSlider->SetValue(Lobby->EffectsVolume);
    MusicSlider->SetToolTipText(FText::Format(Text(TEXT("배경음 {0}%")), FMath::RoundToInt(Lobby->MusicVolume * 100)));
    EffectsSlider->SetToolTipText(FText::Format(Text(TEXT("효과음 {0}%")), FMath::RoundToInt(Lobby->EffectsVolume * 100)));
}

void UCFLobbyWidget::Refresh()
{
    const ECFQueueScreen S = Lobby->Screen;
    const bool bIdle = S == ECFQueueScreen::Idle;
    QuickMatchPanel->SetVisibility(bIdle ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    SearchingPanel->SetVisibility(bIdle ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    const TCHAR* Heading = TEXT("매칭 중...");
    if (S == ECFQueueScreen::Starting) Heading = TEXT("매칭 요청 중...");
    else if (S == ECFQueueScreen::Cancelling) Heading = TEXT("취소 중...");
    else if (S == ECFQueueScreen::Connecting) Heading = TEXT("상대와 연결 중...");
    else if (S == ECFQueueScreen::Recovering) Heading = TEXT("연결 확인 중...");
    else if (S == ECFQueueScreen::AuthenticationRequired) Heading = TEXT("로그인이 필요합니다");
    StatusHeading->SetText(Text(Heading));
    CancelLabel->SetText(Text(S == ECFQueueScreen::Recovering ? TEXT("다시 확인") : TEXT("매칭 취소")));
    CancelButton->SetVisibility(S == ECFQueueScreen::Connecting || S == ECFQueueScreen::AuthenticationRequired ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    CancelButton->SetIsEnabled(S == ECFQueueScreen::Starting || S == ECFQueueScreen::Waiting || S == ECFQueueScreen::Recovering);
    ReturnButton->SetIsEnabled(Lobby->CanReturnToTitle());
    ReturnLabel->SetText(Text(S == ECFQueueScreen::AuthenticationRequired ? TEXT("로그인 화면으로") : Lobby->bReturnRequested ? TEXT("취소 후 돌아가기...") : TEXT("타이틀로 돌아가기")));
    ReturnButton->SetToolTipText(Text(S == ECFQueueScreen::AuthenticationRequired ? TEXT("기존 매칭의 취소를 확정하지 않고 다시 로그인합니다.") : S == ECFQueueScreen::Connecting ? TEXT("연결 중에는 돌아갈 수 없습니다.") : TEXT("매칭 중이면 취소 확인 후 타이틀로 돌아갑니다.")));
    ErrorLabel->SetText(Lobby->ErrorMessage);
    ErrorLabel->SetVisibility(Lobby->ErrorMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    LastRemainingSeconds = -2; RefreshTime();
}

void UCFLobbyWidget::RefreshTime()
{
    const int32 Remaining = Lobby->GetRemainingSeconds();
    if (LastRemainingSeconds == Remaining) return;
    LastRemainingSeconds = Remaining;
    FText Detail;
    if (Lobby->Screen == ECFQueueScreen::Waiting && Remaining >= 0)
        Detail = Remaining > 0 ? FText::FromString(FString::Printf(TEXT("남은 대기 시간 %d:%02d"), Remaining / 60, Remaining % 60)) : Text(TEXT("매칭 상태 확인 중..."));
    else if (Lobby->Screen == ECFQueueScreen::Connecting)
        Detail = Lobby->ConnectionMessage.IsEmpty() ? Text(TEXT("플레이어와 연결하고 있습니다.")) : Lobby->ConnectionMessage;
    StatusDetail->SetText(Detail);
}

void UCFLobbyWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (Lobby) RefreshTime();
}

FReply UCFLobbyWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape) { ToggleSettings(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::R)
    {
        if (auto* Controller = Cast<ACFLobbyController>(GetOwningPlayer()))
        {
            if (!Event.IsRepeat()) Controller->RequestLobbyRespawn();
            return FReply::Handled();
        }
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
