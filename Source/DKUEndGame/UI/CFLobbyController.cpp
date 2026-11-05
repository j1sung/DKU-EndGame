#include "UI/CFLobbyController.h"
#include "UI/CFLobbyWidget.h"
#include "UI/CFLobbySubsystem.h"
#include "UI/CFFrontendSubsystem.h"
#include "Network/CFMatchmakingSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void ACFLobbyController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    Lobby = GetGameInstance()->GetSubsystem<UCFLobbySubsystem>();
    Lobby->OnAudioChanged.AddDynamic(this, &ThisClass::ApplyAudio);
    Lobby->OnReturnToTitleAllowed.AddDynamic(this, &ThisClass::ReturnToTitle);
    if (VolumeMix)
    {
        ApplyAudio(); UGameplayStatics::PushSoundMixModifier(this, VolumeMix); bMixPushed = true;
    }
    if (LobbyWidgetClass)
    {
        LobbyWidget = CreateWidget<UCFLobbyWidget>(this, LobbyWidgetClass);
        LobbyWidget->AddToViewport();
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(LobbyWidget->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode); bShowMouseCursor = true;
    }
    auto* Matching = GetGameInstance()->GetSubsystem<UCFMatchmakingSubsystem>();
    Matching->OnEntryRequired.AddDynamic(this, &ThisClass::OpenEntryScreen);
    Matching->OnLobbyEntered();
}

void ACFLobbyController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleLobbySettings);
}

void ACFLobbyController::ToggleLobbySettings()
{
    if (LobbyWidget) LobbyWidget->ToggleSettings();
}

void ACFLobbyController::ApplyAudio()
{
    if (!Lobby || !VolumeMix) return;
    if (MusicClass) UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, MusicClass, Lobby->MusicVolume, 1.f, .05f, true);
    if (EffectsClass) UGameplayStatics::SetSoundMixClassOverride(this, VolumeMix, EffectsClass, Lobby->EffectsVolume, 1.f, .05f, true);
}

void ACFLobbyController::ReturnToTitle()
{
    if (TitleMap.IsNull()) return;
    Lobby->SaveAudioSettings();
    GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>()->ReturnToTitle();
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, TitleMap);
}

void ACFLobbyController::OpenEntryScreen()
{
    if (!TitleMap.IsNull()) UGameplayStatics::OpenLevelBySoftObjectPtr(this, TitleMap);
}

void ACFLobbyController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsLocalController() && GetGameInstance())
    {
        if (auto* Matching = GetGameInstance()->GetSubsystem<UCFMatchmakingSubsystem>())
        {
            Matching->OnEntryRequired.RemoveDynamic(this, &ThisClass::OpenEntryScreen);
            Matching->OnLobbyExited();
        }
    }
    if (Lobby)
    {
        Lobby->OnAudioChanged.RemoveDynamic(this, &ThisClass::ApplyAudio);
        Lobby->OnReturnToTitleAllowed.RemoveDynamic(this, &ThisClass::ReturnToTitle);
        Lobby->SaveAudioSettings();
    }
    if (bMixPushed) UGameplayStatics::PopSoundMixModifier(this, VolumeMix);
    if (LobbyWidget) { LobbyWidget->RemoveFromParent(); LobbyWidget = nullptr; }
    Super::EndPlay(Reason);
}
