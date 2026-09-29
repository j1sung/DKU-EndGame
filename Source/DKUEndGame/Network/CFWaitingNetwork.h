#pragma once

#include "CoreMinimal.h"
#include "DKUEndGameGameMode.h"
#include "DKUEndGamePlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "CFWaitingNetwork.generated.h"

UCLASS()
class DKUENDGAME_API ACFWaitingGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated,BlueprintReadOnly) TObjectPtr<APlayerState> HostPlayerState;
    UPROPERTY(Replicated,BlueprintReadOnly) int32 RoomCapacity = 2;
    UPROPERTY(Replicated,BlueprintReadOnly) bool bRoomClosing = false;
};

UCLASS()
class DKUENDGAME_API ACFWaitingGameMode : public ADKUEndGameGameMode
{
    GENERATED_BODY()
public:
    ACFWaitingGameMode();
    virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
};

UCLASS()
class DKUENDGAME_API ACFWaitingPlayerController : public ADKUEndGamePlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    UFUNCTION(Client,Reliable) void ClientRoomClosed();
protected:
    UFUNCTION(Server,Reliable) void ServerSetNickname(const FString& Nickname);
};
