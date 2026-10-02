#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFMatchStatusWidget.generated.h"

/** Small initial match HUD. Waiting-room design remains in its existing Widget BP. */
UCLASS()
class DKUENDGAME_API UCFMatchStatusWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(const class ACFWaitingGameState* State,bool bMenuOpen);
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> RoundText;
    UPROPERTY() TObjectPtr<class UTextBlock> CountdownText;
    UPROPERTY() TObjectPtr<class UButton> LeaveButton;
    UFUNCTION() void LeaveRoom();
};
