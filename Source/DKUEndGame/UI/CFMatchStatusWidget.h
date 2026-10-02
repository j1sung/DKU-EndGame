#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFMatchStatusWidget.generated.h"

/** Match HUD and the replicated per-round scoreboard. */
UCLASS()
class DKUENDGAME_API UCFMatchStatusWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(const class ACFWaitingGameState* State,bool bMenuOpen,bool bEliminated);
    void ShowReturnError(const FText& Reason) { ReturnError=Reason; }
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY() TObjectPtr<class UTextBlock> RoundText;
    UPROPERTY() TObjectPtr<class UTextBlock> CountdownText;
    UPROPERTY() TObjectPtr<class UTextBlock> ResultText;
    UPROPERTY() TObjectPtr<class UTextBlock> RoundProgressText;
    UPROPERTY() TObjectPtr<class UTextBlock> SpectatorText;
    UPROPERTY() TObjectPtr<class UBorder> ResultPanel;
    UPROPERTY() TArray<TObjectPtr<class UHorizontalBox>> ScoreRows;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> RankTexts;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> NameTexts;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> PointsTexts;
    UPROPERTY() TArray<TObjectPtr<class UTextBlock>> TotalTexts;
    UPROPERTY() TObjectPtr<class UButton> LeaveButton;
    UPROPERTY() TObjectPtr<class UButton> ReturnButton;
    UPROPERTY() TObjectPtr<class UTextBlock> PointsHeader;
    UPROPERTY() TObjectPtr<class UTextBlock> TotalHeader;
    FText ReturnError;
    UFUNCTION() void ReturnToWaiting();
    UFUNCTION() void LeaveRoom();
};
