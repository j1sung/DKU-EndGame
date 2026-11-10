#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFCompletedMatchWidget.generated.h"

/** 월드/세션 종료와 독립적인 결과 사본. */
UCLASS()
class DKUENDGAME_API UCFCompletedMatchWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
private:
    UPROPERTY() TObjectPtr<class UButton> ConfirmButton;
    UPROPERTY() TObjectPtr<class UTextBlock> Message;
    UFUNCTION() void Confirm();
};
