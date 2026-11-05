#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CFTitleMenuController.generated.h"

class UUserWidget;

UCLASS()
class DKUENDGAME_API ACFTitleMenuController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(Exec) void CFMenuLan();
    UFUNCTION(Exec) void CFMenuTitle();
    UPROPERTY(EditDefaultsOnly, Category="CF|Menu") TSoftObjectPtr<UWorld> LobbyMap;
protected:
    UPROPERTY(EditDefaultsOnly, Category="CF|Menu") TSubclassOf<UUserWidget> TitleWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="CF|Menu") TSubclassOf<UUserWidget> LanWidgetClass;
private:
    UPROPERTY(Transient) TObjectPtr<UUserWidget> ActiveMenu;
    void ShowMenu(TSubclassOf<UUserWidget> WidgetClass);
};
