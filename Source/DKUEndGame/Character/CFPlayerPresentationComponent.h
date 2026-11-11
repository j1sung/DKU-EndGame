#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "CFPlayerPresentationComponent.generated.h"

UCLASS()
class DKUENDGAME_API UCFNameplateWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetName(const FString& Name, FLinearColor Colour);
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> Label;
};

/** 색상 슬롯은 서버가 배정하고 이름은 복제 PlayerState에서 읽는다. */
UCLASS(ClassGroup=(CF))
class DKUENDGAME_API UCFPlayerPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCFPlayerPresentationComponent();
    void SetColourSlot(int32 Slot);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Colour) int32 ColourSlot = INDEX_NONE;
    UPROPERTY(Transient) TObjectPtr<class UWidgetComponent> Nameplate;
    UPROPERTY(Transient) TObjectPtr<class UWidgetComponent> JumpCharge;
    UPROPERTY(Transient) TObjectPtr<class UWidgetComponent> OpponentJumpCharge;
    UPROPERTY(Transient) TArray<TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    bool bTintWarningShown = false;
    float NameplateUpdateRemaining = 0.f;
    UFUNCTION() void OnRep_Colour();
};
