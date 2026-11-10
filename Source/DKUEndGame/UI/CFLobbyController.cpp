#include "UI/CFLobbyController.h"
#include "UI/CFLobbyWidget.h"
#include "UI/CFLobbySubsystem.h"
#include "UI/CFFrontendSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Character/ChickenCharacter.h"
#include "Engine/World.h"
#include "Network/CFWaitingNetwork.h"
#include "GameFramework/PlayerState.h"

void ACFLobbyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ApplyGameNickname();
    if (!bHasLobbySpawnTransform && Cast<AChickenCharacter>(InPawn))
    {
        LobbySpawnTransform = InPawn->GetActorTransform();
        bHasLobbySpawnTransform = true;
    }
}

void ACFLobbyController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    auto* Frontend = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>();
    Frontend->OnStateChanged.AddUniqueDynamic(this, &ThisClass::ApplyGameNickname);
    ApplyGameNickname();
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
    Lobby->RestoreOnEntry();
}

void ACFLobbyController::ApplyGameNickname()
{
    if (!IsLocalController() || !HasAuthority() || !PlayerState || !GetGameInstance()) return;
    const auto* Frontend = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>();
    if (!Frontend || !Frontend->HasAuthenticatedProfile()) return;
    PlayerState->SetPlayerName(Frontend->ProfileNickname);
    PlayerState->ForceNetUpdate();
}

void ACFLobbyController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::ToggleLobbySettings);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::RequestLobbyRespawn);
}

void ACFLobbyController::RequestLobbyRespawn()
{
    const auto* Chicken = Cast<AChickenCharacter>(GetPawn());
    if (Chicken && (Chicken->IsFallen() || Chicken->IsKnockdownPending())) ServerRespawnInLobby();
}

void ACFLobbyController::ServerRespawnInLobby_Implementation()
{
    auto* Chicken = Cast<AChickenCharacter>(GetPawn());
    // A lobby controller alone is insufficient: reject round maps and game modes too.
    if (!Chicken || !bHasLobbySpawnTransform ||
        (!Chicken->IsFallen() && !Chicken->IsKnockdownPending()) ||
        UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("L_Lobby_Playground") ||
        GetWorld()->GetAuthGameMode<ACFWaitingGameMode>()) return;

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    // Keep the original pawn if the spawn point is obstructed or spawning fails.
    const bool bHadCollision = Chicken->GetActorEnableCollision();
    Chicken->SetActorEnableCollision(false);
    auto* Revived = GetWorld()->SpawnActor<AChickenCharacter>(Chicken->GetClass(), LobbySpawnTransform, Params);
    if (!Revived)
    {
        Chicken->SetActorEnableCollision(bHadCollision);
        return;
    }
    Possess(Revived);
    Chicken->Destroy();
    ClientSetRotation(LobbySpawnTransform.Rotator(), true);
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

void ACFLobbyController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (const auto* Instance = GetGameInstance())
        if (auto* Frontend = Instance->GetSubsystem<UCFFrontendSubsystem>())
            Frontend->OnStateChanged.RemoveDynamic(this, &ThisClass::ApplyGameNickname);
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
