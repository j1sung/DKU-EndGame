#include "Network/CFWaitingNetwork.h"
#include "Network/CFSessionSubsystem.h"
#include "Character/ChickenCharacter.h"
#include "UI/CFWaitingRoomWidget.h"
#include "UI/CFMatchStatusWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerStart.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

void ACFWaitingGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass,HostPlayerState);
    DOREPLIFETIME(ThisClass,RoomCapacity);
    DOREPLIFETIME(ThisClass,MinimumPlayers);
    DOREPLIFETIME(ThisClass,bRoomClosing);
    DOREPLIFETIME(ThisClass,Phase);
    DOREPLIFETIME(ThisClass,CurrentRound);
    DOREPLIFETIME(ThisClass,TotalRounds);
    DOREPLIFETIME(ThisClass,CountdownEndServerTime);
    DOREPLIFETIME(ThisClass,Participants);
    DOREPLIFETIME(ThisClass,AlivePlayers);
}

bool ACFWaitingGameState::CanStartRound() const
{
    int32 Connected=0;
    for (const APlayerState* PS:PlayerArray) if (IsValid(PS) && !PS->IsInactive() && !PS->IsOnlyASpectator()) ++Connected;
    return Phase==ECFMatchPhase::Waiting && !bRoomClosing && Connected>=MinimumPlayers && Connected<=RoomCapacity;
}

int32 ACFWaitingGameState::GetCountdownSeconds() const
{
    return Phase==ECFMatchPhase::Countdown ? FMath::Max(0,FMath::CeilToInt(CountdownEndServerTime-GetServerWorldTimeSeconds())) : 0;
}

ACFWaitingGameMode::ACFWaitingGameMode()
{
    GameStateClass=ACFWaitingGameState::StaticClass();
    static ConstructorHelpers::FClassFinder<AChickenCharacter> Combat(TEXT("/Game/Characters/BP_ChickenCharacter"));
    CombatPawnClass=Combat.Class;
}

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
    else if (GS && GS->Phase!=ECFMatchPhase::Waiting) ErrorMessage=TEXT("MATCH_IN_PROGRESS");
    else if (GetNumPlayers()>=UCFSessionSubsystem::RoomCapacity) ErrorMessage=TEXT("ROOM_FULL");
}

void ACFWaitingGameMode::PostLogin(APlayerController* NewPlayer)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (GetNumPlayers()>UCFSessionSubsystem::RoomCapacity)
    {
        if (GameSession) GameSession->KickPlayer(NewPlayer,FText::FromString(TEXT("ROOM_FULL")));
        return;
    }
    if (GS && (GS->Phase!=ECFMatchPhase::Waiting || GS->bRoomClosing))
    {
        if (GameSession) GameSession->KickPlayer(NewPlayer,FText::FromString(TEXT("MATCH_IN_PROGRESS")));
        return;
    }
    Super::PostLogin(NewPlayer);
    if (GS)
    {
        GS->RoomCapacity=UCFSessionSubsystem::RoomCapacity;
        if (NewPlayer->IsLocalController()) GS->HostPlayerState=NewPlayer->PlayerState;
        GS->ForceNetUpdate();
    }
}

bool ACFWaitingGameMode::TryStartRound(ACFWaitingPlayerController* Requester)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !Requester || !GS) return false;
    auto Reject=[Requester](const TCHAR* Text) { Requester->ClientStartRejected(FText::FromString(Text)); return false; };
    if (!Requester->IsLocalController() || Requester->PlayerState!=GS->HostPlayerState)
        return Reject(TEXT("호스트만 경기를 시작할 수 있습니다."));
    if (!GS->CanStartRound()) return Reject(TEXT("대기 중인 플레이어가 2명 이상일 때 시작할 수 있습니다."));
    TArray<APlayerController*> Players;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if (auto* PC=It->Get(); PC && PC->PlayerState && PC->GetPawn() && !PC->PlayerState->IsOnlyASpectator()) Players.Add(PC);
    Players.Sort([](const APlayerController& A,const APlayerController& B) { return A.PlayerState->GetPlayerId()<B.PlayerState->GetPlayerId(); });
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld());It;++It) if (It->ActorHasTag(TEXT("ArenaSpawn"))) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A,const APlayerStart& B) { return A.GetName()<B.GetName(); });
    if (Players.Num()<GS->MinimumPlayers || Players.Num()!=GS->PlayerArray.Num()) return Reject(TEXT("참가자의 입장을 기다려 주세요."));
    if (!CombatPawnClass || Starts.Num()<GS->RoomCapacity) return Reject(TEXT("경기용 캐릭터와 스폰 위치 4개를 확인해 주세요."));

    // Prepare every new pawn before replacing anyone's waiting pawn.
    TArray<AChickenCharacter*> NewPawns;
    TArray<bool> OldCollision;
    for (auto* PC:Players) { OldCollision.Add(PC->GetPawn()->GetActorEnableCollision()); PC->GetPawn()->SetActorEnableCollision(false); }
    for (int32 Index=0;Index<Players.Num();++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Pawn=GetWorld()->SpawnActor<AChickenCharacter>(CombatPawnClass,Starts[Index]->GetActorTransform(),Params);
        if (!Pawn)
        {
            for (auto* Created:NewPawns) Created->Destroy();
            for (int32 I=0;I<Players.Num();++I) Players[I]->GetPawn()->SetActorEnableCollision(OldCollision[I]);
            return Reject(TEXT("경기 시작 위치가 막혀 있습니다. 다시 시도해 주세요."));
        }
        Pawn->SetRoundInputEnabled(false);
        NewPawns.Add(Pawn);
    }
    GS->Participants.Reset(); GS->AlivePlayers.Reset(); GS->CurrentRound=1;
    GS->Phase=ECFMatchPhase::Countdown;
    GS->CountdownEndServerTime=GS->GetServerWorldTimeSeconds()+FMath::Max(1.f,CountdownDuration);
    for (int32 I=0;I<Players.Num();++I)
    {
        auto* PC=Players[I]; APawn* Old=PC->GetPawn();
        PC->UnPossess(); PC->Possess(NewPawns[I]);
        PC->SetControlRotation(Starts[I]->GetActorRotation()); PC->ClientSetRotation(Starts[I]->GetActorRotation(),true);
        Old->Destroy(); GS->Participants.Add(PC->PlayerState); GS->AlivePlayers.Add(PC->PlayerState);
    }
    GS->ForceNetUpdate();
    GetWorldTimerManager().SetTimer(CountdownTimer,this,&ThisClass::BeginRound,FMath::Max(1.f,CountdownDuration),false);
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH countdown players=%d"),Players.Num());
    return true;
}

void ACFWaitingGameMode::BeginRound()
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!GS || GS->Phase!=ECFMatchPhase::Countdown || GS->bRoomClosing) return;
    if (GS->Participants.Num()<GS->MinimumPlayers) { CancelCountdown(); return; }
    GS->Phase=ECFMatchPhase::Playing; GS->CountdownEndServerTime=0;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if (auto* Pawn=Cast<AChickenCharacter>(It->Get()->GetPawn())) Pawn->SetRoundInputEnabled(true);
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH playing round=%d players=%d"),GS->CurrentRound,GS->Participants.Num());
}

void ACFWaitingGameMode::CancelCountdown()
{
    GetWorldTimerManager().ClearTimer(CountdownTimer);
    auto* GS=GetGameState<ACFWaitingGameState>(); if (!GS || GS->Phase!=ECFMatchPhase::Countdown || GS->bRoomClosing) return;
    GS->Phase=ECFMatchPhase::Waiting; GS->CurrentRound=0; GS->CountdownEndServerTime=0;
    GS->Participants.Reset(); GS->AlivePlayers.Reset();
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        auto* PC=It->Get(); if (!PC || !PC->PlayerState || PC->PlayerState->IsInactive()) continue;
        if (APawn* Old=PC->GetPawn()) { PC->UnPossess(); Old->Destroy(); }
        RestartPlayer(PC);
    }
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH countdown cancelled"));
}

void ACFWaitingGameMode::Logout(AController* Exiting)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (GS && Exiting) { GS->Participants.Remove(Exiting->PlayerState); GS->AlivePlayers.Remove(Exiting->PlayerState); }
    Super::Logout(Exiting);
    if (GS)
    {
        if (GS->Phase==ECFMatchPhase::Countdown && GS->Participants.Num()<GS->MinimumPlayers)
            GetWorldTimerManager().SetTimerForNextTick(this,&ThisClass::CancelCountdown);
        GS->ForceNetUpdate();
    }
}

void ACFWaitingGameMode::MarkPlayerEliminated(APlayerState* Player)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (HasAuthority() && GS && GS->Phase==ECFMatchPhase::Playing && Player)
    { GS->AlivePlayers.Remove(Player); GS->ForceNetUpdate(); }
}

void ACFWaitingPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController())
    {
        if (auto* Service=GetGameInstance()->GetSubsystem<UCFSessionSubsystem>())
        {
            if (!Service->LocalNickname.IsEmpty()) ServerSetNickname(Service->LocalNickname);
            Service->NotifyEnteredRoom();
        }
        GetWorldTimerManager().SetTimer(DisplayTimer,this,&ThisClass::UpdateMatchDisplay,.1f,true);
    }
}

void ACFWaitingPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(DisplayTimer);
    if (WaitingWidget) WaitingWidget->RemoveFromParent();
    if (MatchWidget) MatchWidget->RemoveFromParent();
    Super::EndPlay(Reason);
}

void ACFWaitingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ThisClass::ToggleMatchMenu);
}

void ACFWaitingPlayerController::RegisterWaitingWidget(UCFWaitingRoomWidget* Widget)
{
    if (WaitingWidget && WaitingWidget!=Widget) WaitingWidget->RemoveFromParent();
    WaitingWidget=Widget; WaitingWidgetClass=Widget->GetClass();
}

void ACFWaitingPlayerController::RequestStartGame() { if (IsLocalController()) ServerRequestStartGame(); }
void ACFWaitingPlayerController::ServerRequestStartGame_Implementation()
{
    if (auto* GM=GetWorld()->GetAuthGameMode<ACFWaitingGameMode>()) GM->TryStartRound(this);
}
void ACFWaitingPlayerController::ClientStartRejected_Implementation(const FText& Reason)
{ if (WaitingWidget) WaitingWidget->ShowStartError(Reason); }

void ACFWaitingPlayerController::SetMatchInputMode()
{
    const bool bWaiting=LastPhase==ECFMatchPhase::Waiting;
    bShowMouseCursor=bWaiting || bMatchMenuOpen;
    if (bShowMouseCursor) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); }
    else SetInputMode(FInputModeGameOnly());
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    if (LastPhase==ECFMatchPhase::Countdown) { SetIgnoreMoveInput(true); SetIgnoreLookInput(true); }
}

void ACFWaitingPlayerController::ToggleMatchMenu()
{
    if (LastPhase==ECFMatchPhase::Waiting) return;
    bMatchMenuOpen=!bMatchMenuOpen; SetMatchInputMode();
}

void ACFWaitingPlayerController::UpdateMatchDisplay()
{
    const auto* GS=GetWorld()->GetGameState<ACFWaitingGameState>(); if (!GS) return;
    if (LastPhase!=GS->Phase || LastInputPawn.Get()!=GetPawn())
    {
        LastPhase=GS->Phase; LastInputPawn=GetPawn(); bMatchMenuOpen=false;
        if (auto* Subsystem=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            for (const TCHAR* Path:{TEXT("/Game/Input/IMC_CF_Waiting.IMC_CF_Waiting"),TEXT("/Game/Input/IMC_Default.IMC_Default"),TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")})
                if (auto* Context=LoadObject<UInputMappingContext>(nullptr,Path)) Subsystem->RemoveMappingContext(Context);
            const TCHAR* Path=GS->Phase==ECFMatchPhase::Waiting ? TEXT("/Game/Input/IMC_CF_Waiting.IMC_CF_Waiting") : TEXT("/Game/Input/IMC_Default.IMC_Default");
            if (auto* Context=LoadObject<UInputMappingContext>(nullptr,Path)) Subsystem->AddMappingContext(Context,0);
            if (GS->Phase!=ECFMatchPhase::Waiting)
                if (auto* Look=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"))) Subsystem->AddMappingContext(Look,0);
        }
        SetMatchInputMode();
    }
    if (GS->Phase==ECFMatchPhase::Waiting)
    {
        if (MatchWidget) { MatchWidget->RemoveFromParent(); MatchWidget=nullptr; }
        if (!WaitingWidget && WaitingWidgetClass)
        { auto* Widget=CreateWidget<UCFWaitingRoomWidget>(this,WaitingWidgetClass); Widget->AddToPlayerScreen(); }
    }
    else
    {
        if (WaitingWidget) { WaitingWidget->RemoveFromParent(); WaitingWidget=nullptr; }
        if (!MatchWidget) { MatchWidget=CreateWidget<UCFMatchStatusWidget>(this,UCFMatchStatusWidget::StaticClass()); MatchWidget->AddToPlayerScreen(10); }
        MatchWidget->Refresh(GS,bMatchMenuOpen);
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
