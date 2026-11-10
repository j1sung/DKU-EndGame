#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFJumpChargeWidget.generated.h"

class AChickenCharacter;
class UProgressBar;

/** 캐릭터 오른쪽에 표시하는 로컬 플레이어의 점프 충전량. */
UCLASS()
class DKUENDGAME_API UCFJumpChargeWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetCharacter(AChickenCharacter* InCharacter);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    TWeakObjectPtr<AChickenCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> ChargeBar;
};
