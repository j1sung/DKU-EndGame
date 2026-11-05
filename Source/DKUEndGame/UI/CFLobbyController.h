#pragma once
#include "CoreMinimal.h"
#include "DKUEndGamePlayerController.h"
#include "CFLobbyController.generated.h"

class UCFLobbyWidget;
class UCFLobbySubsystem;
class USoundClass;
class USoundMix;

UCLASS(Blueprintable)
class DKUENDGAME_API ACFLobbyController : public ADKUEndGamePlayerController
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void ToggleLobbySettings();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    UPROPERTY(EditDefaultsOnly, Category="CF|Lobby") TSubclassOf<UCFLobbyWidget> LobbyWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="CF|Lobby") TSoftObjectPtr<UWorld> TitleMap;
    UPROPERTY(EditDefaultsOnly, Category="CF|Lobby|Audio") TObjectPtr<USoundClass> MusicClass;
    UPROPERTY(EditDefaultsOnly, Category="CF|Lobby|Audio") TObjectPtr<USoundClass> EffectsClass;
    UPROPERTY(EditDefaultsOnly, Category="CF|Lobby|Audio") TObjectPtr<USoundMix> VolumeMix;
private:
    UPROPERTY(Transient) TObjectPtr<UCFLobbyWidget> LobbyWidget;
    UPROPERTY(Transient) TObjectPtr<UCFLobbySubsystem> Lobby;
    bool bMixPushed = false;
    UFUNCTION() void ApplyAudio();
    UFUNCTION() void ReturnToTitle();
};
