#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CFWaitingRoomWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCFWaitingRoomRequest);

/** Designer-owned row; all roster values are supplied at runtime. */
UCLASS(Abstract, Blueprintable)
class DKUENDGAME_API UCFWaitingPlayerRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetPlayer(const FText& Name, bool bHost, bool bEmpty);
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> RoleText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> RoleBadge;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UBorder> ConnectionDot;
};

/** Waiting roster presentation. Session lifecycle is handled by CFSessionSubsystem. */
UCLASS(Abstract, Blueprintable)
class DKUENDGAME_API UCFWaitingRoomWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Waiting Room")
    void SetRoomDisplay(const TArray<FText>& PlayerNames, int32 HostIndex, int32 Capacity, bool bLocalHost, bool bCanStart);

    UFUNCTION(BlueprintCallable, Category="Waiting Room")
    void UseWorldRoster();

    UPROPERTY(BlueprintAssignable, Category="Waiting Room")
    FCFWaitingRoomRequest OnStartRequested;

    UPROPERTY(BlueprintAssignable, Category="Waiting Room")
    FCFWaitingRoomRequest OnLeaveRequested;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Waiting Room", meta=(ClampMin="2", ClampMax="16"))
    int32 MaxPlayers = 2;

    UPROPERTY(EditDefaultsOnly, Category="Waiting Room")
    TSubclassOf<UCFWaitingPlayerRowWidget> PlayerRowClass;

    UPROPERTY(BlueprintReadOnly, Category="Waiting Room")
    int32 DisplayedPlayerCount = 0;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ParticipantCountText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UVerticalBox> PlayerList;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> StartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> LeaveButton;

private:
    UFUNCTION() void HandleStart();
    UFUNCTION() void HandleLeave();
    UFUNCTION() void RefreshSessionState();
    void RefreshLiveDisplay();
    void RefreshFromWorld();
    void ApplyDisplay(const TArray<FText>& Names, int32 HostIndex, int32 Capacity, bool bLocalHost, bool bCanStart);
    bool bReadWorldRoster = true;
    bool bStartPermitted = false;
    FTimerHandle RosterTimer;
    FString LastDisplayKey;
};
