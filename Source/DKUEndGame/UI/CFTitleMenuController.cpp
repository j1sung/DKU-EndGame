#include "UI/CFTitleMenuController.h"
#include "UI/CFFrontendSubsystem.h"
#include "UI/CFTitleMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACFTitleMenuController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("CFLanMenu"))) { ShowMenu(LanWidgetClass); return; }
#endif
    ShowMenu(TitleWidgetClass);
}

void ACFTitleMenuController::ShowMenu(TSubclassOf<UUserWidget> WidgetClass)
{
    if (!IsLocalController() || !WidgetClass) return;
    if (ActiveMenu) ActiveMenu->RemoveFromParent();
    ActiveMenu = CreateWidget<UUserWidget>(this, WidgetClass);
    if (!ActiveMenu) return;
    ActiveMenu->AddToViewport();
    bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(ActiveMenu->TakeWidget()); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    if (auto* Title = Cast<UCFTitleMenuWidget>(ActiveMenu)) Title->FocusCurrentScreen();
}

void ACFTitleMenuController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ActiveMenu) { ActiveMenu->RemoveFromParent(); ActiveMenu = nullptr; }
    Super::EndPlay(Reason);
}

void ACFTitleMenuController::CFMenuLan()
{
#if !UE_BUILD_SHIPPING
    if (auto* Flow = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>()) Flow->ReturnToTitle();
    ShowMenu(LanWidgetClass);
#endif
}

void ACFTitleMenuController::CFMenuTitle()
{
#if !UE_BUILD_SHIPPING
    if (auto* Flow = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>()) Flow->ReturnToTitle();
    ShowMenu(TitleWidgetClass);
#endif
}
