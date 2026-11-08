#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFLobbyWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class USlider;
class UCFLobbySubsystem;

/** WBP_CF_Lobby의 Designer 배치와 연습 로비 동작을 연결한다. */
UCLASS(Abstract, Blueprintable)
class DKUENDGAME_API UCFLobbyWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CF|Lobby") void ToggleSettings();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> SettingsPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> QuickMatchPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UCanvasPanel> SearchingPanel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> SettingsButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> MatchButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> CancelButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ReturnButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> MusicSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<USlider> EffectsSlider;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusHeading;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusDetail;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> CancelLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ReturnLabel;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ErrorLabel;
private:
    UPROPERTY(Transient) TObjectPtr<UCFLobbySubsystem> Lobby;
    bool bSettingsOpen = false;
    int32 LastRemainingSeconds = -2;
    UFUNCTION() void Refresh();
    UFUNCTION() void RefreshAudio();
    UFUNCTION() void StartMatch();
    UFUNCTION() void CancelMatch();
    UFUNCTION() void ReturnToTitle();
    UFUNCTION() void MusicChanged(float Value);
    UFUNCTION() void EffectsChanged(float Value);
    UFUNCTION() void SaveAudio();
    void RefreshTime();
};
