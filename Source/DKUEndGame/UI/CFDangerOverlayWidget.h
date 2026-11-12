#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFDangerOverlayWidget.generated.h"

/** 로컬 플레이어의 균형 위험도를 표시하는 전체 화면 비네트. */
UCLASS()
class DKUENDGAME_API UCFDangerOverlayWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetDangerIntensity(float Intensity);
    /** 에디터 생성 도구용. 기존 Designer 계층이 있으면 변경하지 않는다. */
    UFUNCTION(BlueprintCallable, Category="Editor Scripting|Danger HUD", meta=(DevelopmentOnly))
    static bool CreateDesignerContent(UObject* WidgetBlueprint, class UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable, Category="Editor Scripting|Danger HUD", meta=(DevelopmentOnly))
    static bool AddDesignerArrows(UObject* WidgetBlueprint, class UMaterialInterface* Material);

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Danger Arrow", meta=(ClampMin="0.0", DisplayName="Arrow Travel (Pixels)"))
    float ArrowTravelPixels = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Danger Arrow", meta=(ClampMin="0.1", Units="s"))
    float ArrowCycleSeconds = .7f;
    /** -1: 숨김, 0: 위, 1: 오른쪽, 2: 아래, 3: 왼쪽. */
    UFUNCTION(BlueprintPure, Category="Danger Arrow")
    int32 GetActiveDangerDirection() const { return ActiveDirection; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    /** WBP Designer의 이름을 유지해야 하는 전체 화면 이미지. */
    UPROPERTY(meta=(BindWidget)) TObjectPtr<class UImage> DangerVignette;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> DangerArrowUp;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> DangerArrowRight;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> DangerArrowDown;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> DangerArrowLeft;
private:
    void UpdateArrows(float DeltaTime);
    float DangerIntensity = 0.f;
    float ArrowAge = 0.f;
    int32 ActiveDirection = -1;
    FVector2D ArrowBaseTranslation[4];
};
