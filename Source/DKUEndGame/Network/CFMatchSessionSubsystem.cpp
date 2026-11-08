#include "Network/CFMatchSessionSubsystem.h"
#include "Network/CFEntrySubsystem.h"
#include "Network/CFMatchmakingSubsystem.h"
#include "Network/CFRoundGameMode.h"
#include "UI/CFLobbySubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DefaultValueHelper.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "eos_connect.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFMatchSession, Log, All);

namespace
{
const FName MatchKey(TEXT("CF_MATCH_ID"));
const FName VersionKey(TEXT("CF_CLIENT_VERSION"));
const FName RoundOne(TEXT("/Game/Map/L_Round1"));
bool IsRoundOne(const UWorld* World)
{
    return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == RoundOne.ToString();
}
float RetrySeconds(const FString& Header, const FString& Date)
{
    double Seconds = 0;
    if (!FDefaultValueHelper::ParseDouble(Header, Seconds))
    {
        FDateTime Until, Now;
        if (!FDateTime::ParseHttpDate(Header, Until)) return 0;
        if (!FDateTime::ParseHttpDate(Date, Now)) Now = FDateTime::UtcNow();
        Seconds = (Until - Now).GetTotalSeconds();
    }
    return FMath::IsFinite(Seconds) && Seconds > 0 ? static_cast<float>(FMath::Min(Seconds, 3600.0)) : 0;
}
}

void UCFMatchSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UCFMatchmakingSubsystem>();
    Entry = GetGameInstance()->GetSubsystem<UCFEntrySubsystem>();
    Tickets = GetGameInstance()->GetSubsystem<UCFMatchmakingSubsystem>();
    Lobby = GetGameInstance()->GetSubsystem<UCFLobbySubsystem>();
    AssignmentHandle = Tickets->OnMatchAssigned.AddUObject(this, &ThisClass::Assigned);
    AuthenticationHandle = Entry->OnAuthenticationRequired.AddUObject(this, &ThisClass::AuthenticationRequired);
    NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::NetworkFailure);
    TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::TravelFailure);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), .25f);
}

void UCFMatchSessionSubsystem::Deinitialize()
{
    bDeinitializing = true; ++Generation; CancelHttp(); ClearSessionDelegates();
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if (Tickets) Tickets->OnMatchAssigned.Remove(AssignmentHandle);
    if (Entry) Entry->OnAuthenticationRequired.Remove(AuthenticationHandle);
    GEngine->OnNetworkFailure().Remove(NetworkHandle);
    GEngine->OnTravelFailure().Remove(TravelHandle);
    if (Sessions && Sessions->GetNamedSession(NAME_GameSession)) Sessions->DestroySession(NAME_GameSession);
    Sessions.Reset(); Search.Reset();
    Super::Deinitialize();
}

FString UCFMatchSessionSubsystem::ProductUserId(const FUniqueNetId& Id)
{
    if (Id.GetType() != FName(TEXT("EOS"))) return FString();
    const EOS_ProductUserId User = static_cast<const IUniqueNetIdEOS&>(Id).GetProductUserId();
    char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
    int32 Size = UE_ARRAY_COUNT(Buffer);
    if (!EOS_ProductUserId_IsValid(User) || EOS_ProductUserId_ToString(User, Buffer, &Size) != EOS_EResult::EOS_Success) return FString();
    return FString(UTF8_TO_TCHAR(Buffer));
}

void UCFMatchSessionSubsystem::Assigned(const FString& Id)
{
    if (bDeinitializing || IsActive() || !Tickets->ClaimAssignment(Id)) return;
    ++Generation; MatchId = Id; DisplayFlow = Lobby->ActiveFlowId;
    Match = FCFSessionMatch(); Connection = EConnection::None;
    bHost = bRegistered = bJoined = bPlaying = bStarted = bSessionStarted = bStartPending = false;
    bAborting = bCleaning = bLocalCleaned = bTerminal = false;
    bNeedSnapshot = true; bWriteDispatched = false; NextSearchAt = 0;
    PublishedSessionId.Reset(); PendingAction.Reset(); PendingKey.Reset(); PlayerId.Reset();
    NextRequestAt = 0; ConnectionDeadline = FPlatformTime::Seconds() + 65;
    IOnlineSubsystem* Default = Online::GetSubsystem(GetWorld());
    IOnlineSubsystem* EOS = Online::GetSubsystem(GetWorld(), TEXT("EOS"));
    Sessions = EOS ? EOS->GetSessionInterface() : nullptr;
    if (!Default || Default->GetSubsystemName() != FName(TEXT("EOS")) || !Sessions || Sessions->GetNamedSession(NAME_GameSession))
    {
        Sessions.Reset(); // 이 흐름이 만들지 않은 세션을 삭제하지 않는다.
        Abort(TEXT("EOS 설정 또는 기존 세션을 확인하세요. EOSLocal 설정은 -Upgrade로 갱신할 수 있습니다."));
    }
    Send(TEXT("get"));
}

void UCFMatchSessionSubsystem::CancelHttp()
{
    ++HttpSerial;
    if (Http) { Http->OnProcessRequestComplete().Unbind(); Http->CancelRequest(); Http.Reset(); }
}

void UCFMatchSessionSubsystem::QueueWrite(const FString& Action)
{
    if (PendingAction == Action) return;
    check(PendingAction.IsEmpty());
    PendingAction = Action;
    bWriteDispatched = false;
    PendingKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

void UCFMatchSessionSubsystem::Send(const FString& Action)
{
    if (bDeinitializing || !IsActive() || Http || FPlatformTime::Seconds() < NextRequestAt) return;
    FString Base, Token, CurrentPlayer;
    if (!Entry->GetMatchmakingContext(Base, Token, CurrentPlayer))
    {
        NextRequestAt = FPlatformTime::Seconds() + 3;
        Entry->InvalidateAuthentication(); return;
    }
    if (!PlayerId.IsEmpty() && PlayerId != CurrentPlayer)
    {
        // 다른 계정으로 이전 매치의 쓰기 요청을 보내지 않는다. 서버 만료는 기존 계정 재인증 후 확인한다.
        NextRequestAt = FPlatformTime::Seconds() + 3;
        Lobby->ReportUncertain(DisplayFlow, FText::FromString(TEXT("매칭에 사용한 계정으로 다시 로그인해 종료 상태를 확인하세요.")));
        return;
    }
    PlayerId = CurrentPlayer;
    const uint64 Epoch = Generation, Serial = ++HttpSerial;
    Http = FHttpModule::Get().CreateRequest();
    Http->SetURL(Base + TEXT("/v1/matches/") + MatchId + (Action == TEXT("get") ? FString() : TEXT("/") + Action));
    Http->SetVerb(Action == TEXT("get") ? TEXT("GET") : TEXT("POST"));
    Http->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    Http->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Http->SetTimeout(10.f);
    if (Action != TEXT("get"))
    {
        bWriteDispatched = true;
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("hostAttempt"), Match.Attempt);
        if (Action == TEXT("session")) Json->SetStringField(TEXT("sessionId"), PublishedSessionId);
        FString Body;
        FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Body));
        Http->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Http->SetHeader(TEXT("Idempotency-Key"), PendingKey);
        Http->SetContentAsString(Body);
    }
    Http->OnProcessRequestComplete().BindWeakLambda(this,
        [this, Epoch, Serial, Action](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
        {
            Receive(Serial, Epoch, Action, bConnected && Response ? Response->GetResponseCode() : 0,
                Response ? Response->GetContentAsString() : FString(),
                Response ? Response->GetHeader(TEXT("Retry-After")) : FString(),
                Response ? Response->GetHeader(TEXT("Date")) : FString());
        });
    const auto Request = Http;
    if (!Request->ProcessRequest()) Receive(Serial, Epoch, Action, 0, FString(), FString(), FString());
}

void UCFMatchSessionSubsystem::Receive(uint64 Serial, uint64 Epoch, const FString& Action, int32 Status,
    const FString& Body, const FString& RetryAfter, const FString& Date)
{
    if (bDeinitializing || !IsActive() || Generation != Epoch || HttpSerial != Serial) return;
    Http.Reset(); ++HttpSerial;
    NextRequestAt = FPlatformTime::Seconds() + FMath::Max(2.f, RetrySeconds(RetryAfter, Date));
    UE_LOG(LogCFMatchSession, Display, TEXT("Match API: Action=%s HTTP=%d"), *Action, Status);
    if (Status == 401) { Entry->InvalidateAuthentication(); return; }
    if (Status == 0 || Status == 429 || Status >= 500)
    {
        Lobby->ReportUncertain(DisplayFlow, FText::FromString(TEXT("연결 상태를 다시 확인하고 있습니다. 새 매칭은 아직 시작하지 않습니다.")));
        return; // 쓰기는 같은 키와 본문으로 재시도한다.
    }
    if (Action != TEXT("get"))
    {
        if (Status == 200)
        {
            if (Action == TEXT("session")) bRegistered = true;
            else if (Action == TEXT("joined")) bJoined = true;
            else if (Action == TEXT("started")) bStarted = true;
        }
        else if (Status != 409) Abort(TEXT("경기 상태 보고가 거절되었습니다. 종료 상태를 확인합니다."));
        PendingAction.Reset(); PendingKey.Reset(); bWriteDispatched = false;
        bNeedSnapshot = true;
        // 멱등 응답은 과거 단계일 수 있다. 항상 최신 GET으로 확정한다.
        Send(TEXT("get")); return;
    }
    if (Status != 200)
    {
        Abort(TEXT("경기 정보를 확인하지 못했습니다. 원래 계정과 연결 상태를 확인하세요.")); return;
    }
    FString Base, Token, CurrentPlayer;
    if (!Entry->GetMatchmakingContext(Base, Token, CurrentPlayer) || CurrentPlayer != PlayerId)
    { AuthenticationRequired(); return; }
    FCFSessionMatch Snapshot;
    if (!FCFSessionMatch::Parse(Body, MatchId, PlayerId, Snapshot))
    { Abort(TEXT("경기 응답이 올바르지 않습니다.")); return; }
    if (!Match.Id.IsEmpty() && (Snapshot.Host != Match.Host || Snapshot.Attempt != Match.Attempt || Snapshot.Members != Match.Members ||
        (!Match.SessionId.IsEmpty() && Snapshot.SessionId != Match.SessionId)))
    { Abort(TEXT("배정된 경기 정보가 변경되었습니다.")); return; }
    const bool bChanged = Snapshot.Status != Match.Status;
    Match = MoveTemp(Snapshot); bHost = PlayerId == Match.Host;
    bNeedSnapshot = false;
    if (bHost && !PublishedSessionId.IsEmpty() && !Match.SessionId.IsEmpty() && Match.SessionId != PublishedSessionId)
    { Abort(TEXT("등록된 세션이 현재 호스트 세션과 다릅니다.")); return; }
    if (bChanged)
    {
        FDateTime ServerNow;
        if (!FDateTime::ParseHttpDate(Date, ServerNow)) ServerNow = FDateTime::UtcNow();
        ConnectionDeadline = FPlatformTime::Seconds() + FMath::Clamp((Match.Deadline - ServerNow).GetTotalSeconds() + 5.0, 1.0, 65.0);
    }
    if (bChanged) UE_LOG(LogCFMatchSession, Display, TEXT("Match phase: %s Joined=%d Host=%d"), *Match.Status, Match.Joined, bHost);
    if (Match.IsTerminal())
    {
        bTerminal = true; bAborting = true;
        PendingAction.Reset(); PendingKey.Reset();
        BeginCleanup(); return;
    }
    if (!bAborting) Lobby->ReportAssigned(DisplayFlow, FText::FromString(Match.Status == TEXT("FORMING") ?
        TEXT("호스트가 경기장을 준비하고 있습니다.") : Match.Status == TEXT("SESSION_READY") ?
        TEXT("Round1에 접속하고 있습니다. 네 명의 준비를 기다립니다.") : TEXT("네 명의 접속을 확인했습니다. 경기를 시작합니다.")));
}

bool UCFMatchSessionSubsystem::Tick(float Delta)
{
    if (bDeinitializing) return false;
    if (!IsActive()) return true;
    const double Now = FPlatformTime::Seconds();
    if (bCleaning && Now >= CleanupDeadline) FinishCleanup();
    if (bTerminal && bLocalCleaned)
    {
        CancelHttp(); ClearSessionDelegates(); ++Generation;
        MatchId.Reset(); Match = FCFSessionMatch(); Sessions.Reset(); Search.Reset();
        Tickets->CompleteAssignment(FText::FromString(TEXT("경기가 종료되었습니다. 다시 빠른 대전을 시작할 수 있습니다.")));
        return true;
    }
    if (bTerminal) return true;
    if (!bAborting && Connection != EConnection::Connected && Now > ConnectionDeadline)
        Abort(TEXT("경기장 연결 시간이 초과되었습니다. 종료 상태를 확인합니다."));
    if (!bAborting) Advance();
    if (!Http && Now >= NextRequestAt)
    {
        if (bAborting && !bNeedSnapshot && Match.Attempt > 0 && PendingAction.IsEmpty() && !bTerminal) QueueWrite(TEXT("left"));
        Send(bNeedSnapshot || PendingAction.IsEmpty() ? TEXT("get") : PendingAction);
    }
    return true;
}

bool UCFMatchSessionSubsystem::LocalRoundReady() const
{
    const UWorld* World = GetWorld();
    if (!IsRoundOne(World) || !World->HasBegunPlay() || !World->GetNetDriver() ||
        !World->GetNetDriver()->GetClass()->GetName().Contains(TEXT("NetDriverEOS"))) return false;
    const auto* PC = Cast<ACFWaitingPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
    const auto* GS = World->GetGameState<ACFWaitingGameState>();
    return PC && PC->GetPawn() && PC->PlayerState && GS && GS->PlayerArray.Num() >= 1 &&
        PC->PlayerState->GetUniqueId().IsValid() && ProductUserId(*PC->PlayerState->GetUniqueId()) == PlayerId &&
        ((bHost && World->GetNetMode() == NM_ListenServer) || (!bHost && World->GetNetMode() == NM_Client));
}

bool UCFMatchSessionSubsystem::CanAdmit(const FUniqueNetIdRepl& UniqueId, const FString& Options) const
{
    return IsActive() && !IsStopping() && bHost && UniqueId.IsValid() && Match.Contains(ProductUserId(*UniqueId)) &&
        UGameplayStatics::ParseOption(Options, TEXT("CFMatch")) == MatchId &&
        UGameplayStatics::ParseOption(Options, TEXT("CFAttempt")) == FString::FromInt(Match.Attempt);
}

bool UCFMatchSessionSubsystem::CanAutoStart() const
{
    if (!IsActive() || IsStopping() || !bHost || !bSessionStarted || Match.Status != TEXT("READY_TO_START") || !bJoined || !LocalRoundReady()) return false;
    TSet<FString> Connected;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState || !PC->GetPawn() || !PC->HasClientLoadedCurrentWorld() || !PC->PlayerState->GetUniqueId().IsValid()) return false;
        const FString Player = ProductUserId(*PC->PlayerState->GetUniqueId());
        if (!Match.Contains(Player) || Connected.Contains(Player)) return false;
        Connected.Add(Player);
    }
    return Connected.Num() == 4;
}

void UCFMatchSessionSubsystem::Advance()
{
    if (Match.Id.IsEmpty() || bNeedSnapshot) return;
    if (Connection == EConnection::None)
    {
        if (bHost && Match.Status == TEXT("FORMING")) Create();
        else if (!bHost && Match.Status == TEXT("SESSION_READY") && FPlatformTime::Seconds() >= NextSearchAt) Find();
        else if (Match.Status == TEXT("IN_GAME") || Match.Status == TEXT("READY_TO_START") ||
            (bHost && Match.Status == TEXT("SESSION_READY"))) Abort(TEXT("이미 준비되거나 진행 중인 경기에는 재접속할 수 없습니다."));
    }
    if ((Connection == EConnection::HostTravel || Connection == EConnection::ClientTravel) && LocalRoundReady())
    {
        Connection = EConnection::Connected;
        UE_LOG(LogCFMatchSession, Display, TEXT("Round1 local connection ready: Host=%d"), bHost);
    }
    if (Connection != EConnection::Connected || !PendingAction.IsEmpty()) return;
    if (bHost && !bRegistered && Match.Status == TEXT("FORMING")) QueueWrite(TEXT("session"));
    else if (!bJoined && Match.Status == TEXT("SESSION_READY")) QueueWrite(TEXT("joined"));
    else if (bHost && Match.Status == TEXT("READY_TO_START") && !bSessionStarted && !bStartPending)
    {
        bStartPending = true;
        const uint64 Epoch = Generation;
        StartHandle = Sessions->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateWeakLambda(this,
            [this, Epoch](FName Name, bool bSuccess)
            {
                if (Epoch != Generation || IsStopping() || Name != NAME_GameSession) return;
                Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle); StartHandle.Reset(); bStartPending = false;
                if (!bSuccess) { Abort(TEXT("EOS 경기 시작에 실패했습니다.")); return; }
                bSessionStarted = true;
            }));
        if (!Sessions->StartSession(NAME_GameSession))
        {
            Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle); StartHandle.Reset();
            Abort(TEXT("EOS 경기 시작 요청에 실패했습니다."));
        }
    }
    else if (CanAutoStart())
    {
        auto* GM = GetWorld()->GetAuthGameMode<ACFRoundGameMode>();
        const auto* GS = GetWorld()->GetGameState<ACFWaitingGameState>();
        if (GM && GS && GS->Phase == ECFMatchPhase::Waiting &&
            !GM->StartAssignedRound(Cast<ACFWaitingPlayerController>(GetGameInstance()->GetFirstLocalPlayerController())))
            Abort(TEXT("네 명의 경기 캐릭터를 준비하지 못했습니다."));
    }
    if (bHost && bPlaying && !bStarted && PendingAction.IsEmpty()) QueueWrite(TEXT("started"));
}

void UCFMatchSessionSubsystem::Create()
{
    Connection = EConnection::Creating;
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = false; Settings.NumPublicConnections = 4;
    Settings.bShouldAdvertise = true; Settings.bAllowJoinInProgress = false;
    Settings.bUsesPresence = false; Settings.bAllowInvites = false;
    Settings.bUseLobbiesIfAvailable = false;
    Settings.Set(MatchKey, MatchId, EOnlineDataAdvertisementType::ViaOnlineService);
    Settings.Set(VersionKey, FString(TEXT("0.1.0")), EOnlineDataAdvertisementType::ViaOnlineService);
    const uint64 Epoch = Generation;
    CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateWeakLambda(this,
        [this, Epoch](FName Name, bool bSuccess)
        {
            if (Epoch != Generation || Name != NAME_GameSession) return;
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); CreateHandle.Reset();
            if (IsStopping()) { bCleaning = false; BeginCleanup(); return; }
            const FNamedOnlineSession* Session = Sessions->GetNamedSession(Name);
            if (!bSuccess || !Session || !Session->SessionInfo.IsValid()) { Abort(TEXT("EOS 세션 생성에 실패했습니다.")); return; }
            PublishedSessionId = Session->SessionInfo->GetSessionId().ToString();
            Connection = EConnection::HostTravel;
            const FString Options = FString::Printf(TEXT("listen?CFMatch=%s?CFAttempt=%d"), *MatchId, Match.Attempt);
            UGameplayStatics::OpenLevel(GetGameInstance(), RoundOne, true, Options);
        }));
    if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
    {
        Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); CreateHandle.Reset();
        Abort(TEXT("EOS 세션 생성 요청에 실패했습니다."));
    }
}

void UCFMatchSessionSubsystem::Find()
{
    Connection = EConnection::Searching;
    NextSearchAt = FPlatformTime::Seconds() + 2;
    Search = MakeShared<FOnlineSessionSearch>();
    Search->bIsLanQuery = false; Search->MaxSearchResults = 10;
    Search->QuerySettings.Set(MatchKey, MatchId, EOnlineComparisonOp::Equals);
    const uint64 Epoch = Generation;
    FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateWeakLambda(this,
        [this, Epoch](bool bSuccess)
        {
            if (Epoch != Generation) return;
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); FindHandle.Reset();
            if (IsStopping()) { FinishCleanup(); return; }
            for (const auto& Result : Search->SearchResults)
            {
                FString ResultMatch, Version;
                if (!bSuccess || !Result.IsValid() || !Result.Session.OwningUserId.IsValid() ||
                    ProductUserId(*Result.Session.OwningUserId) != Match.Host ||
                    Result.GetSessionIdStr() != Match.SessionId ||
                    !Result.Session.SessionSettings.Get(MatchKey, ResultMatch) || ResultMatch != MatchId ||
                    !Result.Session.SessionSettings.Get(VersionKey, Version) || Version != TEXT("0.1.0")) continue;
                Connection = EConnection::Joining;
                JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateWeakLambda(this,
                    [this, Epoch](FName Name, EOnJoinSessionCompleteResult::Type Outcome)
                    {
                        if (Epoch != Generation || Name != NAME_GameSession) return;
                        Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); JoinHandle.Reset();
                        if (IsStopping()) { bCleaning = false; BeginCleanup(); return; }
                        FString Address;
                        if (Outcome != EOnJoinSessionCompleteResult::Success || !Sessions->GetResolvedConnectString(Name, Address))
                        { Abort(TEXT("EOS 세션에 참가하지 못했습니다.")); return; }
                        APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
                        if (!PC) { Abort(TEXT("로컬 플레이어가 없습니다.")); return; }
                        Connection = EConnection::ClientTravel;
                        PC->ClientTravel(BuildRoundTravelURL(Address, MatchId, Match.Attempt), TRAVEL_Absolute);
                    }));
                if (!Sessions->JoinSession(0, NAME_GameSession, Result))
                {
                    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); JoinHandle.Reset();
                    Abort(TEXT("EOS 참가 요청에 실패했습니다."));
                }
                return;
            }
            // 광고 전파가 늦으면 동일 세션을 다음 조회 주기에 다시 검색한다.
            Connection = EConnection::None;
        }));
    if (!Sessions->FindSessions(0, Search.ToSharedRef()))
    {
        Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); FindHandle.Reset();
        Abort(TEXT("EOS 세션 검색 요청에 실패했습니다."));
    }
}

void UCFMatchSessionSubsystem::NotifyPlaying()
{
    if (IsActive() && bHost && !IsStopping())
    {
        bPlaying = true;
        UE_LOG(LogCFMatchSession, Display, TEXT("Round1 Playing confirmed; reporting started."));
    }
}

FString UCFMatchSessionSubsystem::BuildRoundTravelURL(const FString& Address, const FString& AssignedMatchId, int32 Attempt)
{
    // FURL 재직렬화는 기본 포트의 [EOS:PUID] 대괄호를 없애 다음 파싱에서 맵으로 오인하게 한다.
    // OSS가 반환한 접속 주소를 그대로 유지하고 서버 승인에 필요한 옵션만 덧붙인다.
    return Address + FString::Printf(TEXT("?CFMatch=%s?CFAttempt=%d"), *AssignedMatchId, Attempt);
}

void UCFMatchSessionSubsystem::NotifyPlayerLeft() { if (IsActive() && !IsStopping()) Abort(TEXT("참가자가 나가 경기를 종료합니다.")); }
void UCFMatchSessionSubsystem::RequestLeave() { if (IsActive()) Abort(TEXT("경기 연결을 종료합니다.")); }

void UCFMatchSessionSubsystem::ClearSessionDelegates()
{
    if (!Sessions) return;
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); CreateHandle.Reset();
    Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); FindHandle.Reset();
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); JoinHandle.Reset();
    Sessions->ClearOnStartSessionCompleteDelegate_Handle(StartHandle); StartHandle.Reset();
    Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle); DestroyHandle.Reset();
}

void UCFMatchSessionSubsystem::Abort(const TCHAR* Reason)
{
    if (!IsActive() || bAborting) return;
    bAborting = true;
    // 아직 보내지 않은 참가/시작 보고를 실패 정리 중 새로 전송하지 않는다.
    if (!bWriteDispatched) { PendingAction.Reset(); PendingKey.Reset(); }
    UE_LOG(LogCFMatchSession, Warning, TEXT("Connection stopped: %s"), Reason);
    Lobby->ReportUncertain(DisplayFlow, FText::FromString(Reason));
    BeginCleanup();
}

void UCFMatchSessionSubsystem::BeginCleanup()
{
    if (bCleaning || bLocalCleaned) return;
    // 생성/참가가 늦게 완료되면 세션이 남지 않도록 완료 콜백을 기다린다. 상한 뒤에는 재시도를 차단한다.
    bCleaning = true; CleanupDeadline = FPlatformTime::Seconds() + 10;
    if (CreateHandle.IsValid() || JoinHandle.IsValid() || FindHandle.IsValid()) return;
    if (!Sessions || !Sessions->GetNamedSession(NAME_GameSession)) { FinishCleanup(); return; }
    const uint64 Epoch = Generation;
    DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateWeakLambda(this,
        [this, Epoch](FName Name, bool bSuccess)
        {
            if (Epoch == Generation && Name == NAME_GameSession) FinishCleanup();
        }));
    if (!Sessions->DestroySession(NAME_GameSession)) CleanupDeadline = FPlatformTime::Seconds() + 1;
}

void UCFMatchSessionSubsystem::FinishCleanup()
{
    if (bLocalCleaned) return;
    // 진행 중인 EOS 콜백이 끝나기 전에는 완전 정리로 표시하지 않는다.
    if (CreateHandle.IsValid() || JoinHandle.IsValid() || FindHandle.IsValid())
    { CleanupDeadline = FPlatformTime::Seconds() + 1; return; }
    if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
    {
        if (DestroyHandle.IsValid()) { Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle); DestroyHandle.Reset(); }
        bCleaning = false; BeginCleanup(); return;
    }
    ClearSessionDelegates();
    bLocalCleaned = true; bCleaning = false;
    if (FWorldContext* Context = GetGameInstance()->GetWorldContext(); Context && Context->PendingNetGame)
        GEngine->CancelPending(Context->World());
    if (IsRoundOne(GetWorld()) || Connection == EConnection::HostTravel || Connection == EConnection::ClientTravel || Connection == EConnection::Connected)
        UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("/Game/Map/L_Lobby_Playground")), true);
}

void UCFMatchSessionSubsystem::AuthenticationRequired()
{
    if (!IsActive()) return;
    CancelHttp(); Abort(TEXT("인증이 만료되었습니다. 같은 계정으로 로그인한 뒤 종료 상태를 확인하세요."));
    Lobby->ReportAuthenticationRequired(DisplayFlow);
}

bool UCFMatchSessionSubsystem::OwnsWorld(UWorld* World, UNetDriver* Driver) const
{
    if (World) return World->GetGameInstance() == GetGameInstance();
    const FWorldContext* Context = GetGameInstance()->GetWorldContext();
    return Driver && Context && Context->PendingNetGame && Context->PendingNetGame->NetDriver == Driver;
}

void UCFMatchSessionSubsystem::NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Detail)
{
    // 리슨 서버의 개별 접속 종료는 승인된 Controller의 Logout에서만 경기 실패로 처리한다.
    if (World && World->GetNetMode() == NM_ListenServer &&
        (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout ||
         Type == ENetworkFailure::NetGuidMismatch || Type == ENetworkFailure::NetChecksumMismatch)) return;
    if (IsActive() && OwnsWorld(World, Driver)) Abort(TEXT("경기 네트워크 연결이 끊겼습니다."));
}

void UCFMatchSessionSubsystem::TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Detail)
{
    if (IsActive() && OwnsWorld(World, nullptr)) Abort(TEXT("Round1 경기장 이동에 실패했습니다."));
}
