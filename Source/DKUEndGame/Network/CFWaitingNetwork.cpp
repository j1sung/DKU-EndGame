#include "Network/CFWaitingNetwork.h"
#include "Network/CFSessionSubsystem.h"
#include "Character/ChickenCharacter.h"
#include "Character/CFWaitingCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
    DOREPLIFETIME(ThisClass,RoundWinner);
    DOREPLIFETIME(ThisClass,RoundWinnerName);
    DOREPLIFETIME(ThisClass,bRoundDraw);
    DOREPLIFETIME(ThisClass,CountdownEndServerTime);
    DOREPLIFETIME(ThisClass,ScoredRound);
    DOREPLIFETIME(ThisClass,Scoreboard);
    DOREPLIFETIME(ThisClass,NextRoundEndServerTime);
    DOREPLIFETIME(ThisClass,RoundProgressMessage);
    DOREPLIFETIME(ThisClass,bFinalResultPending);
    DOREPLIFETIME(ThisClass,FinalStandings);
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

int32 ACFWaitingGameState::GetResultSeconds() const
{
    return Phase==ECFMatchPhase::RoundResult && NextRoundEndServerTime>0 ?
        FMath::Max(0,FMath::CeilToInt(NextRoundEndServerTime-GetServerWorldTimeSeconds())) : 0;
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
    FString Error;
    if (!PrepareRound(1,Error)) return Reject(*Error);
    return true;
}

bool ACFWaitingGameMode::PrepareRound(int32 RoundNumber,FString& Error)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !GS || GS->bRoomClosing) return false;
    TArray<APlayerController*> Players;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if (auto* PC=Cast<ACFWaitingPlayerController>(It->Get()); PC && IsValid(PC->PlayerState) &&
            !PC->PlayerState->IsInactive() && !PC->PlayerState->IsOnlyASpectator()) Players.Add(PC);
    Players.Sort([](const APlayerController& A,const APlayerController& B) { return A.PlayerState->GetPlayerId()<B.PlayerState->GetPlayerId(); });
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld());It;++It) if (It->ActorHasTag(TEXT("ArenaSpawn"))) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A,const APlayerStart& B) { return A.GetName()<B.GetName(); });
    if (Players.Num()<GS->MinimumPlayers || Players.Num()>GS->RoomCapacity)
    { Error=TEXT("참가자가 2명 이상이어야 다음 라운드를 진행할 수 있습니다."); return false; }
    if (RoundNumber==1 && (Players.Num()!=GS->PlayerArray.Num() || Players.ContainsByPredicate([](const APlayerController* PC) { return !PC->GetPawn(); })))
    { Error=TEXT("참가자의 입장을 기다려 주세요."); return false; }
    if (!CombatPawnClass || Starts.Num()<GS->RoomCapacity)
    { Error=TEXT("경기용 캐릭터와 스폰 위치 4개를 확인해 주세요."); return false; }

    // Spectators have no pawn. Include unpossessed old combat pawns in cleanup as well.
    TArray<APawn*> OldPawns;
    for (auto* PC:Players) if (APawn* Pawn=PC->GetPawn()) OldPawns.AddUnique(Pawn);
    for (const auto& Pawn:RoundPawns) if (Pawn.IsValid()) OldPawns.AddUnique(Pawn.Get());
    TArray<bool> OldCollision;
    for (auto* Pawn:OldPawns) { OldCollision.Add(Pawn->GetActorEnableCollision()); Pawn->SetActorEnableCollision(false); }
    TArray<AChickenCharacter*> NewPawns;
    for (int32 Index=0;Index<Players.Num();++Index)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Pawn=GetWorld()->SpawnActor<AChickenCharacter>(CombatPawnClass,Starts[Index]->GetActorTransform(),Params);
        if (!Pawn)
        {
            for (auto* Created:NewPawns) Created->Destroy();
            for (int32 I=0;I<OldPawns.Num();++I) OldPawns[I]->SetActorEnableCollision(OldCollision[I]);
            Error=TEXT("경기 시작 위치가 막혀 있습니다. 스폰 위치를 확인해 주세요."); return false;
        }
        Pawn->SetRoundInputEnabled(false);
        NewPawns.Add(Pawn);
    }
    // Commit only after every spawn succeeded; failed preparation never awards or resets scores.
    ClearRemovalTimers();
    GetWorldTimerManager().ClearTimer(RoundResolutionTimer);
    GetWorldTimerManager().ClearTimer(NextRoundTimer);
    RoundPawns.Reset(); RoundPlayerIds.Reset(); Eliminations.Reset(); ForfeitedPlayers.Reset();
    if (RoundNumber==1)
    {
        GS->Scoreboard.Reset(); GS->ScoredRound=0; GS->FinalStandings.Reset();
        GS->RoundWinner=nullptr; GS->RoundWinnerName.Reset(); GS->bRoundDraw=false;
        for (const auto* PC:Players)
        {
            auto& Row=GS->Scoreboard.AddDefaulted_GetRef();
            Row.PlayerId=PC->PlayerState->GetPlayerId(); Row.PlayerName=PC->PlayerState->GetPlayerName();
        }
    }
    GS->Participants.Reset(); GS->AlivePlayers.Reset(); GS->CurrentRound=RoundNumber;
    GS->NextRoundEndServerTime=0; GS->RoundProgressMessage.Reset(); GS->bFinalResultPending=false;
    GS->Phase=ECFMatchPhase::Countdown;
    GS->CountdownEndServerTime=GS->GetServerWorldTimeSeconds()+FMath::Max(1.f,CountdownDuration);
    for (int32 I=0;I<Players.Num();++I)
    {
        auto* PC=Players[I];
        PC->UnPossess(); PC->Possess(NewPawns[I]);
        CastChecked<ACFWaitingPlayerController>(PC)->SetRoundEliminated(false);
        PC->SetControlRotation(Starts[I]->GetActorRotation()); PC->ClientSetRotation(Starts[I]->GetActorRotation(),true);
        GS->Participants.Add(PC->PlayerState); GS->AlivePlayers.Add(PC->PlayerState);
        RoundPlayerIds.Add(PC->PlayerState->GetPlayerId()); RoundPawns.Add(NewPawns[I]);
    }
    for (auto* Old:OldPawns) Old->Destroy();
    GS->ForceNetUpdate();
    GetWorldTimerManager().SetTimer(CountdownTimer,this,&ThisClass::BeginRound,FMath::Max(1.f,CountdownDuration),false);
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH countdown round=%d players=%d"),RoundNumber,Players.Num());
    return true;
}

void ACFWaitingGameMode::StopRoundProgression(const FString& Reason)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!GS || GS->bRoomClosing || GS->Phase!=ECFMatchPhase::RoundResult) return;
    GS->RoundProgressMessage=Reason;
    // Later disconnects must not keep pushing the final-results deadline back.
    if (!GS->bFinalResultPending)
    {
        const float Duration=FMath::Max(3.f,RoundResultDuration);
        GS->bFinalResultPending=true;
        GS->NextRoundEndServerTime=GS->GetServerWorldTimeSeconds()+Duration;
        GetWorldTimerManager().SetTimer(NextRoundTimer,this,&ThisClass::ShowFinalResults,Duration,false);
    }
    GS->ForceNetUpdate();
}

void ACFWaitingGameMode::ShowFinalResults()
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !GS || GS->bRoomClosing || GS->Phase!=ECFMatchPhase::RoundResult || !GS->bFinalResultPending) return;
    GS->FinalStandings.Reset();
    for (const auto& Score:GS->Scoreboard)
    {
        auto& Row=GS->FinalStandings.AddDefaulted_GetRef();
        Row.PlayerId=Score.PlayerId; Row.PlayerName=Score.PlayerName;
        Row.TotalScore=Score.TotalScore; Row.bConnected=Score.bConnected;
    }
    GS->FinalStandings.Sort([](const FCFFinalStanding& A,const FCFFinalStanding& B)
    { return A.TotalScore!=B.TotalScore ? A.TotalScore>B.TotalScore : A.PlayerId<B.PlayerId; });
    for (int32 I=0;I<GS->FinalStandings.Num();++I)
    {
        auto& Row=GS->FinalStandings[I];
        Row.Rank=I>0 && Row.TotalScore==GS->FinalStandings[I-1].TotalScore ? GS->FinalStandings[I-1].Rank : I+1;
    }
    GS->NextRoundEndServerTime=0; GS->bFinalResultPending=false; GS->Phase=ECFMatchPhase::FinalResult;
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH final results rounds=%d players=%d"),GS->ScoredRound,GS->FinalStandings.Num());
}

bool ACFWaitingGameMode::TryReturnToWaiting(ACFWaitingPlayerController* Requester)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !Requester || !GS) return false;
    auto Reject=[Requester](const TCHAR* Reason) { Requester->ClientReturnRejected(FText::FromString(Reason)); return false; };
    if (!Requester->IsLocalController() || Requester->PlayerState!=GS->HostPlayerState)
        return Reject(TEXT("호스트만 방 대기로 돌아갈 수 있습니다."));
    if (GS->Phase!=ECFMatchPhase::FinalResult || GS->bRoomClosing)
        return Reject(TEXT("최종 결과가 표시된 뒤 돌아갈 수 있습니다."));

    TArray<ACFWaitingPlayerController*> Players;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if (auto* PC=Cast<ACFWaitingPlayerController>(It->Get()); PC && IsValid(PC->PlayerState) && !PC->PlayerState->IsInactive()) Players.Add(PC);
    Players.Sort([](const ACFWaitingPlayerController& A,const ACFWaitingPlayerController& B) { return A.PlayerState->GetPlayerId()<B.PlayerState->GetPlayerId(); });
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld());It;++It) if (It->ActorHasTag(TEXT("ArenaSpawn"))) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A,const APlayerStart& B) { return A.GetName()<B.GetName(); });
    if (Players.IsEmpty() || Starts.Num()<Players.Num()) return Reject(TEXT("대기 캐릭터를 생성할 위치가 부족합니다."));
    for (auto* PC:Players)
    {
        UClass* WaitingClass=GetDefaultPawnClassForController(PC);
        if (!WaitingClass || !WaitingClass->IsChildOf(ACFWaitingCharacter::StaticClass()))
            return Reject(TEXT("게임모드의 대기용 캐릭터 설정을 확인해 주세요."));
    }
    TArray<APawn*> OldPawns;
    for (auto* PC:Players) if (APawn* Pawn=PC->GetPawn()) OldPawns.AddUnique(Pawn);
    for (const auto& Pawn:RoundPawns) if (Pawn.IsValid()) OldPawns.AddUnique(Pawn.Get());
    TArray<bool> OldCollision;
    for (auto* Pawn:OldPawns) { OldCollision.Add(Pawn->GetActorEnableCollision()); Pawn->SetActorEnableCollision(false); }
    TArray<APawn*> NewPawns;
    for (int32 I=0;I<Players.Num();++I)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Pawn=GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(Players[I]),Starts[I]->GetActorTransform(),Params);
        if (!Pawn)
        {
            for (auto* NewPawn:NewPawns) NewPawn->Destroy();
            for (int32 J=0;J<OldPawns.Num();++J) OldPawns[J]->SetActorEnableCollision(OldCollision[J]);
            return Reject(TEXT("대기 위치가 막혀 있습니다. 잠시 후 다시 시도해 주세요."));
        }
        NewPawns.Add(Pawn);
    }
    // Keep the session, connections, PlayerStates and host. Only the match is reset.
    GetWorldTimerManager().ClearTimer(NextRoundTimer);
    GetWorldTimerManager().ClearTimer(CountdownTimer);
    GetWorldTimerManager().ClearTimer(RoundResolutionTimer);
    ClearRemovalTimers();
    RoundPawns.Reset(); RoundPlayerIds.Reset(); Eliminations.Reset(); ForfeitedPlayers.Reset();
    GS->Participants.Reset(); GS->AlivePlayers.Reset(); GS->Scoreboard.Reset(); GS->FinalStandings.Reset();
    GS->CurrentRound=0; GS->ScoredRound=0; GS->CountdownEndServerTime=0; GS->NextRoundEndServerTime=0;
    GS->RoundWinner=nullptr; GS->RoundWinnerName.Reset(); GS->bRoundDraw=false;
    GS->bFinalResultPending=false; GS->RoundProgressMessage.Reset(); GS->Phase=ECFMatchPhase::Waiting;
    for (int32 I=0;I<Players.Num();++I)
    {
        auto* PC=Players[I]; PC->UnPossess(); PC->Possess(NewPawns[I]); PC->SetRoundEliminated(false);
        PC->SetControlRotation(Starts[I]->GetActorRotation()); PC->ClientSetRotation(Starts[I]->GetActorRotation(),true);
    }
    for (auto* Pawn:OldPawns) Pawn->Destroy();
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH returned to waiting players=%d"),Players.Num());
    return true;
}

void ACFWaitingGameMode::AdvanceRound()
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!GS || GS->Phase!=ECFMatchPhase::RoundResult || GS->bRoomClosing || GS->CurrentRound>=GS->TotalRounds) return;
    FString Error;
    if (!PrepareRound(GS->CurrentRound+1,Error)) StopRoundProgression(Error);
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
    GS->CountdownEndServerTime=0;
    if (GS->ScoredRound>0)
    {
        // A later countdown cancellation must not erase the already earned match scores.
        GS->Phase=ECFMatchPhase::RoundResult; GS->CurrentRound=GS->ScoredRound;
        for (const auto& Pawn:RoundPawns) if (Pawn.IsValid()) Pawn->FinishRound();
        StopRoundProgression(TEXT("참가자가 2명 미만이어서 다음 라운드 준비가 취소되었습니다."));
        return;
    }
    ClearRemovalTimers();
    RoundPawns.Reset(); RoundPlayerIds.Reset(); Eliminations.Reset(); ForfeitedPlayers.Reset();
    GS->Scoreboard.Reset(); GS->Phase=ECFMatchPhase::Waiting; GS->CurrentRound=0;
    GS->Participants.Reset(); GS->AlivePlayers.Reset();
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        auto* PC=It->Get(); if (!PC || !PC->PlayerState || PC->PlayerState->IsInactive()) continue;
        if (APawn* Old=PC->GetPawn()) { PC->UnPossess(); Old->Destroy(); }
        RestartPlayer(PC);
        if (auto* MatchPC=Cast<ACFWaitingPlayerController>(PC)) MatchPC->SetRoundEliminated(false);
    }
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH countdown cancelled"));
}

void ACFWaitingGameMode::Logout(AController* Exiting)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (GS && Exiting && Exiting->PlayerState)
    {
        const int32 Id=Exiting->PlayerState->GetPlayerId();
        if (auto* Row=GS->Scoreboard.FindByPredicate([Id](const FCFPlayerRoundScore& R) { return R.PlayerId==Id; })) Row->bConnected=false;
        if (auto* Row=GS->FinalStandings.FindByPredicate([Id](const FCFFinalStanding& R) { return R.PlayerId==Id; })) Row->bConnected=false;
        if (GS->Phase==ECFMatchPhase::Playing && RoundPlayerIds.Contains(Id)) ForfeitedPlayers.Add(Id);
        // Leaving before play begins is a non-participation, not an occupied finishing rank.
        if (GS->Phase==ECFMatchPhase::Countdown) RoundPlayerIds.Remove(Id);
        GS->Participants.Remove(Exiting->PlayerState); GS->AlivePlayers.Remove(Exiting->PlayerState);
    }
    Super::Logout(Exiting);
    if (GS)
    {
        if (GS->Phase==ECFMatchPhase::Countdown && GS->Participants.Num()<GS->MinimumPlayers)
            GetWorldTimerManager().SetTimerForNextTick(this,&ThisClass::CancelCountdown);
        if (GS->Phase==ECFMatchPhase::RoundResult && GS->CurrentRound<GS->TotalRounds && GS->Participants.Num()<GS->MinimumPlayers && !GS->bRoomClosing)
            StopRoundProgression(TEXT("참가자가 2명 미만이어서 다음 라운드를 진행할 수 없습니다."));
        GS->ForceNetUpdate(); QueueRoundResolution();
    }
}

bool ACFWaitingGameMode::MarkPlayerEliminated(APlayerState* Player)
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !GS || GS->Phase!=ECFMatchPhase::Playing || GS->bRoomClosing || !Player)
        return false;
    // Removing from the roster is the authoritative, idempotent elimination gate.
    if (GS->AlivePlayers.Remove(Player)==0) return false;
    Eliminations.Add({Player->GetPlayerId(),GFrameCounter});
    if (auto* PC=Cast<ACFWaitingPlayerController>(Player->GetPlayerController())) PC->SetRoundEliminated(true);
    GS->ForceNetUpdate();
    QueueRoundResolution();
    return true;
}

void ACFWaitingGameMode::QueueRoundResolution()
{
    const auto* GS=GetGameState<ACFWaitingGameState>();
    if (!GS || GS->Phase!=ECFMatchPhase::Playing || GS->bRoomClosing || GS->AlivePlayers.Num()>1) return;
    if (!GetWorldTimerManager().IsTimerActive(RoundResolutionTimer))
        RoundResolutionTimer=GetWorldTimerManager().SetTimerForNextTick(this,&ThisClass::ResolveRound);
}

void ACFWaitingGameMode::ResolveRound()
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!GS || GS->Phase!=ECFMatchPhase::Playing || GS->bRoomClosing || GS->AlivePlayers.Num()>1) return;
    // Resolve after all elimination requests from this frame: zero survivors is a draw.
    GS->RoundWinner=GS->AlivePlayers.Num()==1 ? GS->AlivePlayers[0] : nullptr;
    GS->bRoundDraw=GS->RoundWinner==nullptr;
    GS->RoundWinnerName=GS->RoundWinner ? GS->RoundWinner->GetPlayerName() : FString();
    ScoreRound();
    GS->Phase=ECFMatchPhase::RoundResult;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        auto* PC=It->Get();
        if (PC && GS->AlivePlayers.Contains(PC->PlayerState))
            if (auto* Pawn=Cast<AChickenCharacter>(PC->GetPawn())) Pawn->FinishRound();
    }
    if (GS->CurrentRound>=GS->TotalRounds)
        StopRoundProgression(FString::Printf(TEXT("%d라운드가 모두 끝났습니다."),GS->TotalRounds));
    else if (GS->Participants.Num()<GS->MinimumPlayers)
        StopRoundProgression(TEXT("참가자가 2명 미만이어서 다음 라운드를 진행할 수 없습니다."));
    else
    {
        const float Duration=FMath::Max(3.f,RoundResultDuration);
        GS->NextRoundEndServerTime=GS->GetServerWorldTimeSeconds()+Duration;
        GS->RoundProgressMessage.Reset();
        GetWorldTimerManager().SetTimer(NextRoundTimer,this,&ThisClass::AdvanceRound,Duration,false);
    }
    // Eliminated pawns retain gravity until landing and finish their fall before removal.
    GS->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH round result winner=%s draw=%d"),*GS->RoundWinnerName,GS->bRoundDraw);
}

void ACFWaitingGameMode::ScoreRound()
{
    auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !GS || GS->ScoredRound>=GS->CurrentRound) return;
    TMap<int32,int32> Ranks;
    int32 Remaining=RoundPlayerIds.Num()-ForfeitedPlayers.Num();
    // Same server frame = shared rank, e.g. 1,2,2,4. Forfeits receive no rank or points.
    for (int32 I=0;I<Eliminations.Num();)
    {
        int32 End=I+1;
        while (End<Eliminations.Num() && Eliminations[End].Frame==Eliminations[I].Frame) ++End;
        int32 GroupSize=0;
        for (int32 J=I;J<End;++J) if (!ForfeitedPlayers.Contains(Eliminations[J].PlayerId)) ++GroupSize;
        const int32 Rank=Remaining-GroupSize+1;
        for (int32 J=I;J<End;++J)
            if (!ForfeitedPlayers.Contains(Eliminations[J].PlayerId)) Ranks.Add(Eliminations[J].PlayerId,Rank);
        Remaining-=GroupSize; I=End;
    }
    for (const APlayerState* Player:GS->AlivePlayers) if (Player) Ranks.Add(Player->GetPlayerId(),1);
    const int32 Points[]={5,3,1,0};
    for (auto& Row:GS->Scoreboard)
    {
        Row.bParticipated=RoundPlayerIds.Contains(Row.PlayerId);
        Row.bForfeited=ForfeitedPlayers.Contains(Row.PlayerId);
        Row.Rank=Ranks.FindRef(Row.PlayerId); Row.RoundScore=0;
        for (const APlayerState* Player:GS->PlayerArray)
            if (Player && Player->GetPlayerId()==Row.PlayerId) { Row.PlayerName=Player->GetPlayerName(); break; }
        if (Row.bParticipated && !Row.bForfeited && Row.Rank>=1 && Row.Rank<=UE_ARRAY_COUNT(Points)) Row.RoundScore=Points[Row.Rank-1];
        Row.TotalScore+=Row.RoundScore;
    }
    GS->Scoreboard.Sort([](const FCFPlayerRoundScore& A,const FCFPlayerRoundScore& B)
    {
        const int32 ARank=A.Rank>0 ? A.Rank : MAX_int32, BRank=B.Rank>0 ? B.Rank : MAX_int32;
        return ARank!=BRank ? ARank<BRank : A.PlayerId<B.PlayerId;
    });
    GS->ScoredRound=GS->CurrentRound;
    UE_LOG(LogTemp,Display,TEXT("CF_MATCH scores committed round=%d rows=%d"),GS->ScoredRound,GS->Scoreboard.Num());
}

void ACFWaitingGameMode::ClearRemovalTimers()
{
    for (auto& Entry:PendingRemovals) GetWorldTimerManager().ClearTimer(Entry.Value);
    PendingRemovals.Reset();
}

void ACFWaitingGameMode::ScheduleEliminatedPawnRemoval(AChickenCharacter* Pawn)
{
    const auto* GS=GetGameState<ACFWaitingGameState>();
    if (!HasAuthority() || !IsValid(Pawn) || !Pawn->IsFallen() || !GS ||
        !GS->Participants.Contains(Pawn->GetPlayerState()) || GS->AlivePlayers.Contains(Pawn->GetPlayerState())) return;
    TWeakObjectPtr<AChickenCharacter> WeakPawn=Pawn;
    if (PendingRemovals.Contains(WeakPawn)) return;
    FTimerHandle RemovalTimer;
    GetWorldTimerManager().SetTimer(RemovalTimer,FTimerDelegate::CreateWeakLambda(this,[this,WeakPawn]()
    {
        PendingRemovals.Remove(WeakPawn);
        auto* FallenPawn=WeakPawn.Get();
        if (!FallenPawn) return;
        if (auto* PC=Cast<ACFWaitingPlayerController>(FallenPawn->GetController()))
        {
            PC->StartRoundSpectating();
            PC->UnPossess();
        }
        FallenPawn->Destroy();
    }),FMath::Max(1.4f,EliminationDisplayTime),false);
    PendingRemovals.Add(WeakPawn,RemovalTimer);
}

void ACFWaitingGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    PendingRemovals.Reset();
    Super::EndPlay(Reason);
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
    if (bOwnSpectatorCamera && IsValid(RoundSpectatorCamera)) RoundSpectatorCamera->Destroy();
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

void ACFWaitingPlayerController::RequestReturnToWaiting() { if (IsLocalController()) ServerRequestReturnToWaiting(); }
void ACFWaitingPlayerController::ServerRequestReturnToWaiting_Implementation()
{
    if (auto* GM=GetWorld()->GetAuthGameMode<ACFWaitingGameMode>()) GM->TryReturnToWaiting(this);
}
void ACFWaitingPlayerController::ClientReturnRejected_Implementation(const FText& Reason)
{
    if (MatchWidget) MatchWidget->ShowReturnError(Reason);
}

void ACFWaitingPlayerController::SetMatchInputMode()
{
    const bool bWaiting=LastPhase==ECFMatchPhase::Waiting;
    bShowMouseCursor=bWaiting || bMatchMenuOpen || LastPhase==ECFMatchPhase::FinalResult;
    if (bShowMouseCursor) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); }
    else SetInputMode(FInputModeGameOnly());
    ResetIgnoreMoveInput(); ResetIgnoreLookInput();
    if (bRoundEliminated || (LastPhase!=ECFMatchPhase::Waiting && LastPhase!=ECFMatchPhase::Playing))
    { SetIgnoreMoveInput(true); SetIgnoreLookInput(true); }
}

void ACFWaitingPlayerController::ToggleMatchMenu()
{
    if (LastPhase==ECFMatchPhase::Waiting) return;
    bMatchMenuOpen=!bMatchMenuOpen; SetMatchInputMode();
}

void ACFWaitingPlayerController::UpdateMatchDisplay()
{
    const auto* GS=GetWorld()->GetGameState<ACFWaitingGameState>(); if (!GS) return;
    UpdateSpectatorCamera();
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
        MatchWidget->Refresh(GS,bMatchMenuOpen,bRoundEliminated);
    }
}

void ACFWaitingPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ThisClass,bRoundEliminated,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ThisClass,bRoundSpectating,COND_OwnerOnly);
}

void ACFWaitingPlayerController::SetRoundEliminated(bool bEliminated)
{
    if (!HasAuthority()) return;
    bRoundEliminated=bEliminated;
    if (!bEliminated) bRoundSpectating=false;
    OnRep_RoundParticipation();
    ForceNetUpdate();
}

void ACFWaitingPlayerController::StartRoundSpectating()
{
    if (!HasAuthority() || !bRoundEliminated) return;
    bRoundSpectating=true;
    OnRep_RoundParticipation();
    ForceNetUpdate();
}

void ACFWaitingPlayerController::OnRep_RoundParticipation()
{
    if (!IsLocalController()) return;
    SetMatchInputMode();
    UpdateSpectatorCamera();
}

void ACFWaitingPlayerController::UpdateSpectatorCamera()
{
    if (!IsLocalController()) return;
    if (!bRoundSpectating)
    {
        if (IsValid(RoundSpectatorCamera))
        {
            if (GetPawn()) SetViewTarget(GetPawn());
            if (bOwnSpectatorCamera) RoundSpectatorCamera->Destroy();
            RoundSpectatorCamera=nullptr;
        }
        bAutoManageActiveCameraTarget=true;
        return;
    }
    bAutoManageActiveCameraTarget=false;
    if (!IsValid(RoundSpectatorCamera))
    {
        // A level designer can supply an explicit camera later without code changes.
        for (TActorIterator<ACameraActor> It(GetWorld());It;++It)
            if (It->ActorHasTag(TEXT("ArenaSpectatorCamera"))) { RoundSpectatorCamera=*It; break; }
        if (!RoundSpectatorCamera)
        {
            FVector Center=FVector::ZeroVector;
            int32 Count=0;
            for (TActorIterator<APlayerStart> It(GetWorld());It;++It)
                if (It->ActorHasTag(TEXT("ArenaSpawn"))) { Center+=It->GetActorLocation(); ++Count; }
            if (Count>0) Center/=Count;
            const FVector Position=Center+FVector(-1400,-1400,1800);
            FActorSpawnParameters Params;
            Params.ObjectFlags|=RF_Transient;
            Params.Owner=this;
            RoundSpectatorCamera=GetWorld()->SpawnActor<ACameraActor>(Position,(Center-Position).Rotation(),Params);
            bOwnSpectatorCamera=RoundSpectatorCamera!=nullptr;
            if (RoundSpectatorCamera) RoundSpectatorCamera->GetCameraComponent()->SetFieldOfView(65.f);
        }
    }
    if (RoundSpectatorCamera && GetViewTarget()!=RoundSpectatorCamera) SetViewTarget(RoundSpectatorCamera);
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
