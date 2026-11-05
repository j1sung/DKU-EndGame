#include "Network/CFMatchmakingSubsystem.h"
#include "Network/CFEntrySubsystem.h"
#include "UI/CFLobbySubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

namespace CFMatchmaking
{
FText Text(const TCHAR* Value) { return FText::FromString(Value); }
bool String(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, FString& Out)
{
    const auto Value = Json ? Json->TryGetField(Key) : nullptr;
    return Value && Value->Type == EJson::String && Value->TryGetString(Out) && !Out.IsEmpty();
}
bool Id(const FString& Value)
{
    FGuid Guid;
    return FGuid::ParseExact(Value, EGuidFormats::DigitsWithHyphens, Guid) && Guid.IsValid();
}
FDateTime Now(const FString& ServerDate)
{
    FDateTime Result;
    return FDateTime::ParseHttpDate(ServerDate, Result) ? Result : FDateTime::UtcNow();
}
bool Terminal(const FString& Status)
{
    return Status == TEXT("FINISHED") || Status == TEXT("FAILED") || Status == TEXT("EXPIRED");
}
}

void UCFMatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UCFEntrySubsystem>();
    Collection.InitializeDependency<UCFLobbySubsystem>();
    Entry = GetGameInstance()->GetSubsystem<UCFEntrySubsystem>();
    Lobby = GetGameInstance()->GetSubsystem<UCFLobbySubsystem>();
    BindLobby();
}

void UCFMatchmakingSubsystem::BindLobby()
{
    Lobby->OnMatchmakingRequested.AddUniqueDynamic(this, &ThisClass::Start);
    Lobby->OnCancelRequested.AddUniqueDynamic(this, &ThisClass::Cancel);
    Lobby->OnStatusRecoveryRequested.AddUniqueDynamic(this, &ThisClass::Recover);
}

void UCFMatchmakingSubsystem::Deinitialize()
{
    OnLobbyExited();
    if (Lobby)
    {
        Lobby->OnMatchmakingRequested.RemoveDynamic(this, &ThisClass::Start);
        Lobby->OnCancelRequested.RemoveDynamic(this, &ThisClass::Cancel);
        Lobby->OnStatusRecoveryRequested.RemoveDynamic(this, &ThisClass::Recover);
    }
    Super::Deinitialize();
}

void UCFMatchmakingSubsystem::OnLobbyEntered()
{
    bInLobby = true;
    // 맵을 직접 Play한 연습 환경에서는 로그인 창을 자동으로 열지 않는다.
    if (Entry && Entry->HasBackendSession()) Lobby->RestoreOnEntry();
}

void UCFMatchmakingSubsystem::OnLobbyExited()
{
    RememberExitIntent();
    bInLobby = false;
    StopRequests(); FlowId = 0; bWantCreate = false;
    if (Lobby) Lobby->ResetForEntry();
    // 불명확한 쓰기 결과/멱등성 키는 같은 계정으로 재진입할 때 current로 복구한다.
}

bool UCFMatchmakingSubsystem::IsCurrent(int32 Id) const
{
    return bInLobby && Lobby && Id > 0 && FlowId == Id && Lobby->ActiveFlowId == Id;
}

void UCFMatchmakingSubsystem::StopRequests()
{
    ++Serial;
    if (HttpRequest)
    {
        HttpRequest->OnProcessRequestComplete().Unbind(); HttpRequest->CancelRequest(); HttpRequest.Reset();
    }
    InFlight = Pending = ERequest::None;
    GetGameInstance()->GetTimerManager().ClearTimer(PollTimer);
}

void UCFMatchmakingSubsystem::ClearTicket()
{
    TicketId.Reset(); MatchId.Reset(); CreateKey.Reset(); CancelKey.Reset(); CancelTicketId.Reset();
    bCreateUnresolved = false; Assignment = FCFMatchAssignment();
    bCancelAfterEntry = bReturnAfterEntry = false;
}

void UCFMatchmakingSubsystem::Start(int32 Id)
{
    if (!bInLobby || !Lobby || Id != Lobby->ActiveFlowId || FlowId != 0) return;
    FlowId = Id; bWantCreate = true; Errors = 0;
    // 기존 티켓 또는 이전 실행의 배정을 먼저 복구한다.
    Send(ERequest::Current);
}

void UCFMatchmakingSubsystem::Cancel(int32 Id)
{
    if (!IsCurrent(Id) || !MatchId.IsEmpty()) return;
    bWantCreate = false;
    if (InFlight != ERequest::None) return; // 요청의 응답 처리에서 취소 의도를 소비한다.
    Schedule(TicketId.IsEmpty() ? ERequest::Current : ERequest::Cancel);
}

void UCFMatchmakingSubsystem::Recover(int32 Id)
{
    if (!bInLobby || !Lobby || Id != Lobby->ActiveFlowId || (FlowId != 0 && FlowId != Id)) return;
    FlowId = Id;
    if (bReturnAfterEntry && !Lobby->bReturnRequested) Lobby->RequestReturnToTitle();
    else if (bCancelAfterEntry && !Lobby->bCancelRequested) Lobby->RequestCancel();
    if (InFlight == ERequest::None) Schedule(MatchId.IsEmpty() ? ERequest::Current : ERequest::Match);
}

void UCFMatchmakingSubsystem::Schedule(ERequest Kind, float Delay)
{
    if (!IsCurrent(FlowId) || InFlight != ERequest::None) return;
    Pending = Kind;
    const float Wait = FMath::Max3(.01f, Delay, float(NextAllowedAt - FPlatformTime::Seconds()));
    GetGameInstance()->GetTimerManager().SetTimer(PollTimer, this, &ThisClass::DispatchPending, Wait, false);
}

void UCFMatchmakingSubsystem::DispatchPending()
{
    const ERequest Kind = Pending; Pending = ERequest::None;
    if (Kind != ERequest::None) Send(Kind);
}

void UCFMatchmakingSubsystem::Send(ERequest Kind)
{
    if (!IsCurrent(FlowId) || InFlight != ERequest::None) return;
    if (NextAllowedAt > FPlatformTime::Seconds()) { Schedule(Kind); return; }
    FString Base, Token, Account;
    bool bAccess = false;
#if WITH_DEV_AUTOMATION_TESTS
    if (TestAccess) bAccess = TestAccess(Base, Token, Account);
    else
#endif
    bAccess = Entry && Entry->GetBackendAccess(Base, Token, Account);
    if (!bAccess) { RequireEntry(); return; }
    if (!PlayerId.IsEmpty() && PlayerId != Account)
    {
        // 다른 계정의 티켓/멱등성 키를 재사용하지 않는다.
        ClearTicket(); PlayerId = Account; bWantCreate = false;
        StopRequests(); FlowId = 0; Lobby->ResetForEntry(); Lobby->RestoreOnEntry();
        return;
    }
    PlayerId = Account;
    const int32 Id = FlowId;
    const uint64 RequestSerial = ++Serial;
    InFlight = Kind;
    FString Path = TEXT("/v1/matchmaking/tickets/current");
    FString Verb = TEXT("GET"), Body, Key;
    if (Kind == ERequest::Create)
    {
        if (CreateKey.IsEmpty()) CreateKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        bCreateUnresolved = true; bWantCreate = false;
        Path = TEXT("/v1/matchmaking/tickets"); Verb = TEXT("POST"); Key = CreateKey; Body = TEXT("{\"mode\":\"standard\"}");
    }
    else if (Kind == ERequest::Cancel)
    {
        if (TicketId.IsEmpty()) { InFlight = ERequest::None; Schedule(ERequest::Current); return; }
        if (CancelKey.IsEmpty() || CancelTicketId != TicketId)
        {
            CancelTicketId = TicketId; CancelKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        }
        Verb = TEXT("DELETE"); Key = CancelKey;
        Body = FString::Printf(TEXT("{\"ticketId\":\"%s\"}"), *CancelTicketId);
    }
    else if (Kind == ERequest::Match) Path = TEXT("/v1/matches/") + MatchId;
    HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Base + Path); HttpRequest->SetVerb(Verb);
    HttpRequest->SetTimeout(12.f);
    HttpRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
    if (!Key.IsEmpty())
    {
        HttpRequest->SetHeader(TEXT("Idempotency-Key"), Key);
        HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json")); HttpRequest->SetContentAsString(Body);
    }
    if (Kind == ERequest::Create) HttpRequest->SetHeader(TEXT("X-Client-Version"), TEXT("0.1.0"));
    HttpRequest->OnProcessRequestComplete().BindWeakLambda(this,
        [this, Id, RequestSerial, Kind](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
        {
            Receive(Id, RequestSerial, Kind, bConnected && Response.IsValid(), Response ? Response->GetResponseCode() : 0,
                Response ? Response->GetContentAsString() : FString(), Response ? Response->GetHeader(TEXT("Retry-After")) : FString(),
                Response ? Response->GetHeader(TEXT("Date")) : FString());
        });
#if WITH_DEV_AUTOMATION_TESTS
    if (TestSend) { TestSend(HttpRequest.ToSharedRef()); return; }
#endif
    if (!HttpRequest->ProcessRequest()) Receive(Id, RequestSerial, Kind, false, 0, FString(), FString(), FString());
}

float UCFMatchmakingSubsystem::RetryDelay(const FString& Value, const FString& ServerDate)
{
    if (Value.IsEmpty()) return 0.f;
    double Seconds = 0;
    if (LexTryParseString(Seconds, *Value) && FMath::IsFinite(Seconds) && Seconds >= 0) return float(FMath::Min(Seconds, double(MAX_flt)));
    FDateTime Date;
    return FDateTime::ParseHttpDate(Value, Date) ? float(FMath::Max(0., (Date - CFMatchmaking::Now(ServerDate)).GetTotalSeconds())) : 0.f;
}

void UCFMatchmakingSubsystem::Uncertain()
{
    Errors = FMath::Min(Errors + 1, 3);
    Lobby->ReportUncertain(FlowId, CFMatchmaking::Text(TEXT("매칭 상태를 확인하지 못했습니다. 연결을 확인하고 다시 확인해 주세요.")));
    Schedule(MatchId.IsEmpty() ? ERequest::Current : ERequest::Match, Errors == 1 ? 2.f : Errors == 2 ? 4.f : 5.f);
}

void UCFMatchmakingSubsystem::Finish(const FText& Message)
{
    const int32 Id = FlowId;
    StopRequests(); ClearTicket(); FlowId = 0; Errors = 0; bWantCreate = false;
    Lobby->ReportIdle(Id, Message);
}

void UCFMatchmakingSubsystem::RequireEntry()
{
    RememberExitIntent();
    StopRequests(); FlowId = 0; bWantCreate = false;
    Lobby->ResetForEntry();
    // 인증 소실은 서버 취소 성공이 아니다. 재로그인 뒤 current를 복구한다.
    if (Entry) Entry->RequireBackendLogin();
    OnEntryRequired.Broadcast();
}

void UCFMatchmakingSubsystem::RememberExitIntent()
{
    if (Lobby && Lobby->ActiveFlowId > 0)
    {
        bCancelAfterEntry |= Lobby->bCancelRequested;
        bReturnAfterEntry |= Lobby->bReturnRequested;
    }
}

void UCFMatchmakingSubsystem::Receive(int32 Id, uint64 RequestSerial, ERequest Kind, bool bConnected, int32 Status,
    const FString& Body, const FString& RetryAfter, const FString& ServerDate)
{
    if (!IsCurrent(Id) || Serial != RequestSerial || InFlight != Kind) return;
    HttpRequest.Reset(); InFlight = ERequest::None;
    NextAllowedAt = FMath::Max(NextAllowedAt, FPlatformTime::Seconds() + RetryDelay(RetryAfter, ServerDate));
    if (!bConnected) { Uncertain(); return; }
    if (Status == 401) { RequireEntry(); return; }
    TSharedPtr<FJsonObject> Json;
    const bool bJson = Body.Len() <= 65536 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) && Json.IsValid();
    FString Code;
    const TSharedPtr<FJsonObject>* Error = nullptr;
    if (bJson && Json->TryGetObjectField(TEXT("error"), Error) && Error && Error->IsValid()) CFMatchmaking::String(*Error, TEXT("code"), Code);
    if (Kind == ERequest::Create && Status == 409 && Code == TEXT("NICKNAME_REQUIRED"))
    {
        bCreateUnresolved = false; RequireEntry(); return; // 로그인 프로필 조회에서 닉네임 등록으로 이어진다.
    }
    if (Kind == ERequest::Create && Status == 426)
    {
        Finish(CFMatchmaking::Text(TEXT("게임을 최신 버전으로 업데이트한 뒤 다시 시도해 주세요."))); return;
    }
    if ((Kind == ERequest::Create && Status == 409 && Code == TEXT("ALREADY_ACTIVE")) ||
        (Kind == ERequest::Cancel && ((Status == 409 && (Code == TEXT("TICKET_ALREADY_ASSIGNED") || Code == TEXT("TICKET_MISMATCH"))) ||
        (Status == 404 && Code == TEXT("TICKET_NOT_FOUND")))))
    {
        if (Kind == ERequest::Create) bCreateUnresolved = false;
        Schedule(ERequest::Current); return;
    }
    if (!bJson || Status != (Kind == ERequest::Create ? 201 : 200)) { Uncertain(); return; }
    Errors = 0;
    FString Ticket, State;
    if (Kind == ERequest::Current)
    {
        const auto Empty = Json->TryGetField(TEXT("ticket"));
        if (Empty && Empty->IsNull() && !Json->HasField(TEXT("ticketId")))
        {
            // POST 응답 유실 후 GET null만으로 종료하면 늦게 완료된 티켓을 놓칠 수 있다.
            // 같은 키로 POST를 확인하고, 캐시된 POST 응답 뒤에도 반드시 current를 다시 조회한다.
            if (bCreateUnresolved) { Schedule(ERequest::Create); return; }
            if (!MatchId.IsEmpty()) { Schedule(ERequest::Match); return; }
            if (bWantCreate && !Lobby->bCancelRequested) { Schedule(ERequest::Create); return; }
            Finish(TicketId.IsEmpty() || Lobby->bCancelRequested ? FText::GetEmpty() : CFMatchmaking::Text(TEXT("매칭 대기가 종료되었습니다. 다시 시작할 수 있습니다."))); return;
        }
    }
    if (Kind == ERequest::Current || Kind == ERequest::Create || Kind == ERequest::Cancel)
    {
        if (!CFMatchmaking::String(Json, TEXT("ticketId"), Ticket) || !CFMatchmaking::Id(Ticket) || !CFMatchmaking::String(Json, TEXT("status"), State)) { Uncertain(); return; }
        if (Kind == ERequest::Cancel)
        {
            if (State != TEXT("CANCELLED") || Ticket != CancelTicketId) { Uncertain(); return; }
            Schedule(ERequest::Current); return;
        }
        if (State == TEXT("ASSIGNED") && Kind == ERequest::Current)
        {
            FString Assigned;
            if (!CFMatchmaking::String(Json, TEXT("matchId"), Assigned) || !CFMatchmaking::Id(Assigned) || (!MatchId.IsEmpty() && MatchId != Assigned)) { Uncertain(); return; }
            TicketId = Ticket; MatchId = Assigned; bWantCreate = bCreateUnresolved = false;
            bCancelAfterEntry = bReturnAfterEntry = false;
            Lobby->ReportAssigned(Id, CFMatchmaking::Text(TEXT("4명이 배정되었습니다. 경기 연결을 준비하고 있습니다.")));
            Schedule(ERequest::Match); return;
        }
        FString Expiry; FDateTime ExpiresAt;
        if (State != TEXT("WAITING") || !CFMatchmaking::String(Json, TEXT("expiresAt"), Expiry) || !FDateTime::ParseIso8601(*Expiry, ExpiresAt)) { Uncertain(); return; }
        if (!MatchId.IsEmpty()) { Schedule(ERequest::Match); return; }
        TicketId = Ticket; bWantCreate = false;
        if (Kind == ERequest::Create) { bCreateUnresolved = false; Schedule(ERequest::Current); return; }
        bCreateUnresolved = false;
        const int32 Seconds = FMath::Clamp(FMath::CeilToInt((ExpiresAt - CFMatchmaking::Now(ServerDate)).GetTotalSeconds()), 0, 180);
        Lobby->ReportWaiting(Id, Seconds);
        Schedule(Lobby->bCancelRequested ? ERequest::Cancel : ERequest::Current, Lobby->bCancelRequested ? .01f : 2.f);
        return;
    }
    FCFMatchAssignment Next;
    const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
    double Attempt = 0;
    const auto AttemptValue = Json->TryGetField(TEXT("hostAttempt"));
    if (!CFMatchmaking::String(Json, TEXT("matchId"), Next.MatchId) || Next.MatchId != MatchId ||
        !CFMatchmaking::String(Json, TEXT("status"), Next.Status) || !CFMatchmaking::String(Json, TEXT("hostPlayerId"), Next.HostPlayerId) ||
        !AttemptValue || AttemptValue->Type != EJson::Number || !AttemptValue->TryGetNumber(Attempt) || Attempt != 1 ||
        !Json->TryGetArrayField(TEXT("memberIds"), Members) || !Members || Members->Num() != 4) { Uncertain(); return; }
    for (const auto& Member : *Members)
    {
        FString Value;
        if (!Member || Member->Type != EJson::String || !Member->TryGetString(Value) || Value.IsEmpty() || Next.MemberIds.Contains(Value)) { Uncertain(); return; }
        Next.MemberIds.Add(Value);
    }
    if (!Next.MemberIds.Contains(PlayerId) || !Next.MemberIds.Contains(Next.HostPlayerId)) { Uncertain(); return; }
    if (Next.Status != TEXT("FORMING") && Next.Status != TEXT("SESSION_READY") && Next.Status != TEXT("READY_TO_START") &&
        Next.Status != TEXT("IN_GAME") && !CFMatchmaking::Terminal(Next.Status)) { Uncertain(); return; }
    Next.HostAttempt = 1; Next.bIsHost = Next.HostPlayerId == PlayerId;
    if (Next.Status != TEXT("FORMING") && !CFMatchmaking::Terminal(Next.Status) && !CFMatchmaking::String(Json, TEXT("sessionId"), Next.SessionId)) { Uncertain(); return; }
    FDateTime Deadline;
    if (!CFMatchmaking::Terminal(Next.Status) && (!CFMatchmaking::String(Json, TEXT("deadlineAt"), Next.DeadlineAt) || !FDateTime::ParseIso8601(*Next.DeadlineAt, Deadline))) { Uncertain(); return; }
    if (CFMatchmaking::Terminal(Next.Status))
    {
        Finish(CFMatchmaking::Text(Next.Status == TEXT("FINISHED") ? TEXT("경기가 종료되었습니다.") : TEXT("매치 준비가 종료되었습니다. 다시 매칭할 수 있습니다."))); return;
    }
    const bool bChanged = Assignment.MatchId != Next.MatchId || Assignment.Status != Next.Status || Assignment.SessionId != Next.SessionId;
    Assignment = MoveTemp(Next);
    Lobby->ReportAssigned(Id, CFMatchmaking::Text(Assignment.Status == TEXT("IN_GAME") ? TEXT("이미 진행 중인 경기입니다. 종료 상태를 확인하고 있습니다.") : TEXT("4명이 배정되었습니다. 경기 연결을 준비하고 있습니다.")));
    if (bChanged) OnAssignmentChanged.Broadcast();
    Schedule(ERequest::Match, 2.f);
}
