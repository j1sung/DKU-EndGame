#include "Network/CFWaitingNetwork.h"
#include "Network/CFSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

void ACFWaitingGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass,HostPlayerState);
    DOREPLIFETIME(ThisClass,RoomCapacity);
    DOREPLIFETIME(ThisClass,bRoomClosing);
}

ACFWaitingGameMode::ACFWaitingGameMode() { GameStateClass=ACFWaitingGameState::StaticClass(); }

void ACFWaitingGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
    Super::InitGame(MapName,Options,ErrorMessage);
    if (GameSession) GameSession->MaxPlayers=UCFSessionSubsystem::RoomCapacity;
}

void ACFWaitingGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage)
{
    Super::PreLogin(Options,Address,UniqueId,ErrorMessage);
    const auto* GS=GetGameState<ACFWaitingGameState>();
    if (GS && GS->bRoomClosing) ErrorMessage=TEXT("ROOM_CLOSING");
    else if (GetNumPlayers()>=UCFSessionSubsystem::RoomCapacity) ErrorMessage=TEXT("ROOM_FULL");
}

void ACFWaitingGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (auto* GS=GetGameState<ACFWaitingGameState>())
    {
        GS->RoomCapacity=UCFSessionSubsystem::RoomCapacity;
        if (NewPlayer->IsLocalController()) GS->HostPlayerState=NewPlayer->PlayerState;
        GS->ForceNetUpdate();
    }
}

void ACFWaitingPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController()) if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>())
    {
        if (!Service->LocalNickname.IsEmpty()) ServerSetNickname(Service->LocalNickname);
        Service->NotifyEnteredRoom();
    }
}

void ACFWaitingPlayerController::ServerSetNickname_Implementation(const FString& Nickname)
{
    const FString Clean=UCFSessionSubsystem::CleanNickname(Nickname);
    if (PlayerState && !Clean.IsEmpty()) { PlayerState->SetPlayerName(Clean); PlayerState->ForceNetUpdate(); }
}

void ACFWaitingPlayerController::ClientRoomClosed_Implementation()
{
    if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>()) Service->HostClosedRoom();
}
