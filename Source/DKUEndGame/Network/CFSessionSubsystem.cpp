#include "Network/CFSessionSubsystem.h"
#include "Network/CFWaitingNetwork.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

namespace CFSession
{
const FName ProjectKey(TEXT("CF_PROJECT"));
const FName RoomKey(TEXT("CF_ROOM"));
const FString ProjectValue(TEXT("DKU_ChickenFight_Waiting_v1"));
const FName Arena(TEXT("/Game/Maps/Lvl_CF_Arena"));
const FName Menu(TEXT("/Game/Maps/Lvl_CF_MainMenu"));
FText Text(const TCHAR* Value) { return FText::FromString(Value); }

// Accept an IPv4 address or localhost plus an optional port, never travel URL options.
bool ParseAddress(FString Input, FString& URL)
{
    Input.TrimStartAndEndInline();
    FString Host=Input, PortText; int32 Port=7777;
    if (Input.Split(TEXT(":"),&Host,&PortText))
    {
        if (PortText.IsEmpty() || !PortText.IsNumeric() || PortText.Len()>5) return false;
        for (TCHAR C:PortText) if (C<TEXT('0') || C>TEXT('9')) return false;
        Port=FCString::Atoi(*PortText);
        if (Port<1 || Port>65535) return false;
    }
    if (Host.Equals(TEXT("localhost"),ESearchCase::IgnoreCase)) Host=TEXT("127.0.0.1");
    TArray<FString> Parts; Host.ParseIntoArray(Parts,TEXT("."),false);
    if (Parts.Num()!=4) return false;
    for (const FString& Part:Parts)
    {
        if (Part.IsEmpty() || Part.Len()>3) return false;
        for (TCHAR C:Part) if (C<TEXT('0') || C>TEXT('9')) return false;
        if (FCString::Atoi(*Part)>255) return false;
    }
    URL=FString::Printf(TEXT("%s:%d"),*Host,Port); return true;
}
}

void UCFSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StatusMessage=CFSession::Text(TEXT("닉네임을 입력하고 시작하세요."));
    NetworkHandle=GEngine->OnNetworkFailure().AddUObject(this,&ThisClass::OnNetworkFailure);
    TravelHandle=GEngine->OnTravelFailure().AddUObject(this,&ThisClass::OnTravelFailure);
    MapHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,&ThisClass::OnMapLoaded);
}

void UCFSessionSubsystem::Deinitialize()
{
    bDeinitializing=true;
    GetGameInstance()->GetTimerManager().ClearAllTimersForObject(this);
    ClearOperationDelegates();
    if (Sessions.IsValid())
    {
        if (State==ECFSessionState::Searching) Sessions->CancelFindSessions();
        if (Sessions->GetNamedSession(NAME_GameSession)) Sessions->DestroySession(NAME_GameSession);
    }
    if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
    Super::Deinitialize();
}

bool UCFSessionSubsystem::GetSessions()
{
    if (!Sessions.IsValid()) Sessions=Online::GetSessionInterface(GetWorld());
    return Sessions.IsValid();
}

bool UCFSessionSubsystem::IsBusy() const { return State!=ECFSessionState::Idle && State!=ECFSessionState::InRoom; }

FString UCFSessionSubsystem::CleanNickname(const FString& Nickname)
{
    FString Clean;
    for (TCHAR C:Nickname) if (!FChar::IsControl(C)) Clean.AppendChar(C);
    return Clean.TrimStartAndEnd().Left(24);
}

bool UCFSessionSubsystem::SetNickname(const FString& Nickname)
{
    if (IsBusy()) return false;
    const FString Clean=CleanNickname(Nickname);
    if (Clean.IsEmpty()) { Error(TEXT("닉네임을 입력해 주세요.")); return false; }
    LocalNickname=Clean; return true;
}

void UCFSessionSubsystem::SetState(ECFSessionState Next,const FText& Message)
{
    State=Next; StatusMessage=Message;
    UE_LOG(LogTemp,Display,TEXT("CF_SESSION state=%d %s"),int32(State),*Message.ToString());
    OnChanged.Broadcast();
}

void UCFSessionSubsystem::Error(const TCHAR* Message) { SetState(State,CFSession::Text(Message)); }

void UCFSessionSubsystem::ArmDeadline(float Seconds)
{
    GetGameInstance()->GetTimerManager().SetTimer(Deadline,this,&ThisClass::OnTimeout,Seconds,false);
}

void UCFSessionSubsystem::ClearOperationDelegates()
{
    if (Sessions.IsValid())
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    }
    CreateHandle.Reset(); FindHandle.Reset(); JoinHandle.Reset(); DestroyHandle.Reset();
    GetGameInstance()->GetTimerManager().ClearTimer(Deadline);
}

void UCFSessionSubsystem::HostRoom(const FString& Nickname)
{
    if (State!=ECFSessionState::Idle || !SetNickname(Nickname)) return;
    if (!GetSessions()) { Error(TEXT("방 생성 서비스를 사용할 수 없습니다.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession)) { BeginReturn(CFSession::Text(TEXT("이전 연결을 정리했습니다. 다시 방을 만들어 주세요."))); return; }
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch=true; Settings.bIsDedicated=false;
    Settings.NumPublicConnections=RoomCapacity;
    Settings.bShouldAdvertise=true; Settings.bAllowJoinInProgress=true;
    Settings.bUsesPresence=false; Settings.bUseLobbiesIfAvailable=false;
    Settings.Set(CFSession::ProjectKey,CFSession::ProjectValue,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(CFSession::RoomKey,LocalNickname+TEXT("의 방"),EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SetState(ECFSessionState::Creating,CFSession::Text(TEXT("방을 만들고 있습니다…")));
    CreateHandle=Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this,&ThisClass::OnCreated));
    ArmDeadline(15.f);
    if (!Sessions->CreateSession(0,NAME_GameSession,Settings)) OnCreated(NAME_GameSession,false);
}

void UCFSessionSubsystem::OnCreated(FName Name,bool bSuccess)
{
    if (Name!=NAME_GameSession || State!=ECFSessionState::Creating) return;
    ClearOperationDelegates();
    if (!bSuccess) { BeginReturn(CFSession::Text(TEXT("방을 만들지 못했습니다. 다시 시도해 주세요."))); return; }
    SetState(ECFSessionState::Traveling,CFSession::Text(TEXT("경기장으로 이동하고 있습니다…")));
    ArmDeadline(30.f);
    UGameplayStatics::OpenLevel(GetGameInstance(),CFSession::Arena,true,TEXT("listen"));
}

void UCFSessionSubsystem::FindRooms()
{
    if (State!=ECFSessionState::Idle) return;
    if (!GetSessions()) { Error(TEXT("방 검색 서비스를 사용할 수 없습니다.")); return; }
    Rooms.Reset(); Results.Reset();
    Search=MakeShared<FOnlineSessionSearch>(); Search->bIsLanQuery=true;
    Search->MaxSearchResults=50; Search->TimeoutInSeconds=4.f;
    SetState(ECFSessionState::Searching,CFSession::Text(TEXT("같은 네트워크의 방을 찾고 있습니다…")));
    FindHandle=Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this,&ThisClass::OnFound));
    ArmDeadline(10.f);
    if (!Sessions->FindSessions(0,Search.ToSharedRef())) OnFound(false);
}

void UCFSessionSubsystem::OnFound(bool bSuccess)
{
    if (State!=ECFSessionState::Searching) return;
    ClearOperationDelegates(); Rooms.Reset(); Results.Reset();
    if (bSuccess && Search.IsValid()) for (const auto& Result:Search->SearchResults)
    {
        FString Marker; if (!Result.IsValid() || !Result.Session.SessionSettings.Get(CFSession::ProjectKey,Marker) || Marker!=CFSession::ProjectValue) continue;
        FCFRoomInfo Room;
        Result.Session.SessionSettings.Get(CFSession::RoomKey,Room.Name);
        if (Room.Name.IsEmpty()) Room.Name=TEXT("닭싸움 대기방");
        Room.Capacity=Result.Session.SessionSettings.NumPublicConnections;
        Room.Players=FMath::Clamp(Room.Capacity-Result.Session.NumOpenPublicConnections,0,Room.Capacity);
        Room.Ping=Result.PingInMs; Rooms.Add(Room); Results.Add(Result);
    }
    SetState(ECFSessionState::Idle,CFSession::Text(!bSuccess ? TEXT("검색에 실패했습니다. 다시 검색해 주세요.") : Rooms.IsEmpty() ? TEXT("찾은 방이 없습니다. 같은 네트워크인지 확인하거나 IP로 접속해 주세요.") : TEXT("참가할 방을 선택해 주세요.")));
}

void UCFSessionSubsystem::CancelSearch()
{
    if (State!=ECFSessionState::Searching) return;
    ClearOperationDelegates(); if (Sessions.IsValid()) Sessions->CancelFindSessions();
    SetState(ECFSessionState::Idle,CFSession::Text(TEXT("방 검색을 취소했습니다.")));
}

void UCFSessionSubsystem::JoinRoom(int32 Index,const FString& Nickname)
{
    if (State!=ECFSessionState::Idle || !SetNickname(Nickname)) return;
    if (!Results.IsValidIndex(Index)) { Error(TEXT("참가할 방을 선택하거나 직접 접속 IP를 입력해 주세요.")); return; }
    if (Rooms[Index].Players>=Rooms[Index].Capacity) { Error(TEXT("선택한 방이 가득 찼습니다.")); return; }
    if (!GetSessions()) { Error(TEXT("방 참가 서비스를 사용할 수 없습니다.")); return; }
    if (Sessions->GetNamedSession(NAME_GameSession)) { BeginReturn(CFSession::Text(TEXT("이전 연결을 정리했습니다. 다시 참가해 주세요."))); return; }
    SetState(ECFSessionState::Joining,CFSession::Text(TEXT("방에 참가하고 있습니다…")));
    JoinHandle=Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this,&ThisClass::OnJoined));
    ArmDeadline(20.f);
    if (!Sessions->JoinSession(0,NAME_GameSession,Results[Index])) OnJoined(NAME_GameSession,EOnJoinSessionCompleteResult::UnknownError);
}

void UCFSessionSubsystem::OnJoined(FName Name,EOnJoinSessionCompleteResult::Type Result)
{
    if (Name!=NAME_GameSession || State!=ECFSessionState::Joining) return;
    ClearOperationDelegates(); FString URL;
    if (Result==EOnJoinSessionCompleteResult::Success && Sessions->GetResolvedConnectString(Name,URL)) { TravelToHost(URL); return; }
    BeginReturn(CFSession::Text(Result==EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("방이 가득 찼습니다.") : TEXT("방에 참가하지 못했습니다. 방 목록을 새로고침해 주세요.")));
}

void UCFSessionSubsystem::JoinAddress(const FString& Address,const FString& Nickname)
{
    if (State!=ECFSessionState::Idle || !SetNickname(Nickname)) return;
    FString URL;
    if (!CFSession::ParseAddress(Address,URL)) { Error(TEXT("IP 주소를 확인해 주세요. 예: 192.168.0.10:7777")); return; }
    if (GetSessions() && Sessions->GetNamedSession(NAME_GameSession)) { BeginReturn(CFSession::Text(TEXT("이전 연결을 정리했습니다. 다시 참가해 주세요."))); return; }
    TravelToHost(URL);
}

void UCFSessionSubsystem::TravelToHost(const FString& URL)
{
    APlayerController* PC=GetGameInstance()->GetFirstLocalPlayerController();
    if (!PC) { BeginReturn(CFSession::Text(TEXT("플레이어를 찾지 못했습니다. 다시 시도해 주세요."))); return; }
    SetState(ECFSessionState::Traveling,CFSession::Text(TEXT("호스트에 연결하고 있습니다…")));
    ArmDeadline(25.f); PC->ClientTravel(URL,TRAVEL_Absolute);
}

void UCFSessionSubsystem::NotifyEnteredRoom()
{
    if (State==ECFSessionState::Leaving || bDeinitializing) return;
    ClearOperationDelegates(); SetState(ECFSessionState::InRoom,CFSession::Text(TEXT("경기 대기 중입니다.")));
}

void UCFSessionSubsystem::LeaveRoom() { BeginReturn(CFSession::Text(TEXT("방에서 나왔습니다."))); }
void UCFSessionSubsystem::HostClosedRoom() { BeginReturn(CFSession::Text(TEXT("호스트가 방을 종료했습니다."))); }

void UCFSessionSubsystem::BeginReturn(const FText& Reason)
{
    if (State==ECFSessionState::Leaving || bDeinitializing) return;
    const bool bHosting=GetWorld() && GetWorld()->GetNetMode()==NM_ListenServer;
    ClearOperationDelegates();
    if (Sessions.IsValid() && State==ECFSessionState::Searching) Sessions->CancelFindSessions();
    ReturnMessage=Reason; bReturningToMenu=true;
    SetState(ECFSessionState::Leaving,CFSession::Text(TEXT("연결을 정리하고 있습니다…")));
    if (bHosting)
    {
        if (auto* GS=GetWorld()->GetGameState<ACFWaitingGameState>()) { GS->bRoomClosing=true; GS->ForceNetUpdate(); }
        for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if (auto* PC=Cast<ACFWaitingPlayerController>(It->Get()); PC && !PC->IsLocalController()) PC->ClientRoomClosed();
    }
    // Let the reliable close notification flush before destroying the listen world.
    GetGameInstance()->GetTimerManager().SetTimer(CleanupDelay,this,&ThisClass::DestroyLocalSession,bHosting?.5f:.01f,false);
}

void UCFSessionSubsystem::DestroyLocalSession()
{
    if (GetSessions() && Sessions->GetNamedSession(NAME_GameSession))
    {
        DestroyHandle=Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this,&ThisClass::OnDestroyed));
        ArmDeadline(5.f);
        if (!Sessions->DestroySession(NAME_GameSession)) OnDestroyed(NAME_GameSession,false);
    }
    else FinishReturn();
}

void UCFSessionSubsystem::OnDestroyed(FName Name,bool bSuccess)
{
    if (Name!=NAME_GameSession || State!=ECFSessionState::Leaving) return;
    FinishReturn();
}

void UCFSessionSubsystem::FinishReturn()
{
    ClearOperationDelegates();
    if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession)) Sessions->RemoveNamedSession(NAME_GameSession);
    Rooms.Reset(); Results.Reset(); Search.Reset();
    // A local map load alone does not cancel an outstanding connection attempt.
    if (FWorldContext* Context=GetGameInstance()->GetWorldContext(); Context && Context->PendingNetGame)
        GEngine->CancelPending(Context->World());
    UGameplayStatics::OpenLevel(GetGameInstance(),CFSession::Menu,true);
}

void UCFSessionSubsystem::OnMapLoaded(UWorld* World)
{
    if (!World || World->GetGameInstance()!=GetGameInstance()) return;
    if (bReturningToMenu && World->GetMapName().Contains(TEXT("Lvl_CF_MainMenu")))
    {
        bReturningToMenu=false; SetState(ECFSessionState::Idle,ReturnMessage);
    }
}

bool UCFSessionSubsystem::OwnsFailure(UWorld* World,UNetDriver* Driver) const
{
    if (World) return World->GetGameInstance()==GetGameInstance();
    const FWorldContext* Context=GetGameInstance()->GetWorldContext();
    return Driver && Context && Context->PendingNetGame && Context->PendingNetGame->NetDriver==Driver;
}

void UCFSessionSubsystem::OnNetworkFailure(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Detail)
{
    if (!OwnsFailure(World,Driver) || State==ECFSessionState::Leaving || bDeinitializing) return;
    if (World && World->GetNetMode()==NM_ListenServer &&
        (Type==ENetworkFailure::ConnectionLost || Type==ENetworkFailure::ConnectionTimeout || Type==ENetworkFailure::NetGuidMismatch || Type==ENetworkFailure::NetChecksumMismatch)) return;
    if (State==ECFSessionState::Idle || State==ECFSessionState::Searching) return;
    UE_LOG(LogTemp,Warning,TEXT("CF_SESSION network failure %d: %s"),int32(Type),*Detail);
    BeginReturn(CFSession::Text(Detail.Contains(TEXT("ROOM_FULL")) || Detail.Contains(TEXT("Server full")) ? TEXT("방이 가득 찼습니다.") : State==ECFSessionState::InRoom ? TEXT("호스트와의 연결이 끊겼습니다. 메인 메뉴로 돌아왔습니다.") : TEXT("호스트에 연결하지 못했습니다. 주소와 네트워크 상태를 확인해 주세요.")));
}

void UCFSessionSubsystem::OnTravelFailure(UWorld* World,ETravelFailure::Type Type,const FString& Detail)
{
    if (!OwnsFailure(World,nullptr) || State==ECFSessionState::Leaving || bDeinitializing) return;
    UE_LOG(LogTemp,Warning,TEXT("CF_SESSION travel failure %d: %s"),int32(Type),*Detail);
    BeginReturn(CFSession::Text(TEXT("경기장으로 이동하지 못했습니다. 다시 시도해 주세요.")));
}

void UCFSessionSubsystem::OnTimeout()
{
    if (State==ECFSessionState::Searching) { CancelSearch(); Error(TEXT("방 검색 시간이 초과되었습니다. 다시 검색해 주세요.")); }
    else if (State==ECFSessionState::Leaving) FinishReturn();
    else BeginReturn(CFSession::Text(TEXT("연결 시간이 초과되었습니다. 다시 시도해 주세요.")));
}
