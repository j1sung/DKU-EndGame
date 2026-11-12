#include "Network/CFMatchmakingSubsystem.h"
#include "Network/CFEntrySubsystem.h"
#include "UI/CFLobbySubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DefaultValueHelper.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFMatchmaking, Log, All);

void UCFMatchmakingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UCFEntrySubsystem>();
    Collection.InitializeDependency<UCFLobbySubsystem>();
    Entry = GetGameInstance()->GetSubsystem<UCFEntrySubsystem>();
    Lobby = GetGameInstance()->GetSubsystem<UCFLobbySubsystem>();
    Lobby->OnMatchmakingRequested.AddUniqueDynamic(this, &ThisClass::Start);
    Lobby->OnCancelRequested.AddUniqueDynamic(this, &ThisClass::Cancel);
    Lobby->OnStatusRecoveryRequested.AddUniqueDynamic(this, &ThisClass::Recover);
    AuthenticationHandle = Entry->OnAuthenticationRequired.AddUObject(this, &ThisClass::AuthenticationRequired);
}

void UCFMatchmakingSubsystem::StopIo()
{
    ++HttpSerial;
    if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(PollTimer);
    if (HttpRequest)
    {
        HttpRequest->OnProcessRequestComplete().Unbind();
        HttpRequest->CancelRequest(); HttpRequest.Reset();
    }
}

void UCFMatchmakingSubsystem::Deinitialize()
{
    bDeinitializing = true; StopIo();
    if (Lobby)
    {
        Lobby->OnMatchmakingRequested.RemoveDynamic(this, &ThisClass::Start);
        Lobby->OnCancelRequested.RemoveDynamic(this, &ThisClass::Cancel);
        Lobby->OnStatusRecoveryRequested.RemoveDynamic(this, &ThisClass::Recover);
    }
    if (Entry) Entry->OnAuthenticationRequired.Remove(AuthenticationHandle);
    OnMatchAssigned.Clear();
    Super::Deinitialize();
}

void UCFMatchmakingSubsystem::Start(int32 FlowId)
{
    if (bDeinitializing || FlowId <= 0 || Lobby->ActiveFlowId != FlowId || Flow.Id != 0) return;
    Flow.Begin(FlowId, FString(), true);
    Send(ECFMatchRequest::Create);
}

void UCFMatchmakingSubsystem::Cancel(int32 FlowId)
{
    if (bDeinitializing || Flow.Id != FlowId || Lobby->ActiveFlowId != FlowId) return;
    Flow.WantCancel();
    // 이미 진행 중인 요청은 서버에서 처리될 수 있다. 취소 의도를 보존하고 그 결과부터 확정한다.
    if (!HttpRequest) Send(Flow.RecoveryRequest());
}

void UCFMatchmakingSubsystem::Recover(int32 FlowId)
{
    if (bAssignmentClaimed) return;
    if (bDeinitializing || FlowId <= 0 || Lobby->ActiveFlowId != FlowId) return;
    if (Flow.Id == 0) Flow.Begin(FlowId, FString(), false);
    if (Flow.Id != FlowId || HttpRequest) return;
    if (Lobby->bCancelRequested) Flow.WantCancel();
    Send(Flow.RecoveryRequest());
}

bool UCFMatchmakingSubsystem::ClaimAssignment(const FString& MatchId)
{
    if (bAssignmentClaimed || MatchId.IsEmpty() || MatchId != Flow.MatchId) return false;
    bAssignmentClaimed = true; StopIo();
    return true;
}

void UCFMatchmakingSubsystem::CompleteAssignment(const FText& Message)
{
    if (!bAssignmentClaimed) return;
    StopIo(); bAssignmentClaimed = false;
    const int32 Id = Flow.Id;
    Flow = FCFMatchmakingFlow();
    Lobby->ReportIdle(Id, Message);
}

void UCFMatchmakingSubsystem::AuthenticationRequired()
{
    if (bDeinitializing || !Lobby) return;
    if (HttpRequest && HttpRequest->GetVerb() != TEXT("GET")) Flow.MarkWriteUncertain();
    StopIo();
    const int32 Id = Lobby->ActiveFlowId > 0 ? Lobby->ActiveFlowId : Lobby->BeginAccountRecovery();
    if (Flow.Id == 0) Flow.Begin(Id, FString(), false);
    Lobby->ReportAuthenticationRequired(Id);
}

bool UCFMatchmakingSubsystem::IsCurrentResponse(int32 FlowId, uint64 Serial) const
{
    return !bDeinitializing && Lobby && FlowId > 0 && Flow.Id == FlowId &&
        Lobby->ActiveFlowId == FlowId && HttpSerial == Serial;
}

void UCFMatchmakingSubsystem::Schedule(ECFMatchRequest Kind, float Delay)
{
    if (bDeinitializing || Kind == ECFMatchRequest::None || Flow.Id <= 0 || Lobby->ActiveFlowId != Flow.Id) return;
    const int32 Id = Flow.Id;
    GetGameInstance()->GetTimerManager().SetTimer(PollTimer, FTimerDelegate::CreateWeakLambda(this, [this, Kind, Id]()
    {
        if (Flow.Id == Id && Lobby->ActiveFlowId == Id) Send(Kind);
    }), FMath::Max(Delay, .01f), false);
}

void UCFMatchmakingSubsystem::Send(ECFMatchRequest Kind)
{
    if (bDeinitializing || bAssignmentClaimed || Kind == ECFMatchRequest::None || Flow.Id <= 0 || Flow.Id != Lobby->ActiveFlowId || HttpRequest) return;
    const double RemainingDelay = NextAllowedAt - FPlatformTime::Seconds();
    if (RemainingDelay > .01) { Schedule(Kind, static_cast<float>(RemainingDelay)); return; }
    GetGameInstance()->GetTimerManager().ClearTimer(PollTimer);
    FString Base, Token, Player;
    if (!Entry->GetMatchmakingContext(Base, Token, Player)) { Entry->InvalidateAuthentication(); return; }
    if (!Flow.PlayerId.IsEmpty() && Flow.PlayerId != Player)
    {
        // 다른 계정의 토큰으로 이전 계정의 취소·생성 키를 보내지 않는다.
        StopIo();
        const int32 Id = Lobby->BeginAccountRecovery();
        Flow.Begin(Id, Player, false); Kind = ECFMatchRequest::Current;
    }
    Flow.PlayerId = Player;
    const int32 Id = Flow.Id;
    const uint64 Serial = ++HttpSerial;
    HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Base + (Kind == ECFMatchRequest::Create ? TEXT("/v1/matchmaking/tickets") : TEXT("/v1/matchmaking/tickets/current")));
    HttpRequest->SetVerb(Kind == ECFMatchRequest::Create ? TEXT("POST") : Kind == ECFMatchRequest::Cancel ? TEXT("DELETE") : TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
    HttpRequest->SetTimeout(12.f);
    if (Kind == ECFMatchRequest::Create) HttpRequest->SetHeader(TEXT("X-Client-Version"), TEXT("0.1.0"));
    if (Kind != ECFMatchRequest::Current)
    {
        HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        HttpRequest->SetHeader(TEXT("Idempotency-Key"), Flow.KeyFor(Kind));
        HttpRequest->SetContentAsString(Flow.BodyFor(Kind));
    }
    HttpRequest->OnProcessRequestComplete().BindWeakLambda(this,
        [this, Id, Serial, Kind](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
        {
            Receive(Id, Serial, Kind, bConnected && Response ? Response->GetResponseCode() : 0,
                Response ? Response->GetContentAsString() : FString(),
                Response ? Response->GetHeader(TEXT("Date")) : FString(),
                Response ? Response->GetHeader(TEXT("Retry-After")) : FString());
        });
    // ProcessRequest의 동기 실패에서도 같은 응답 경로와 순서 검증을 적용한다.
    const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request = HttpRequest;
    if (!Request->ProcessRequest()) Receive(Id, Serial, Kind, 0, FString(), FString(), FString());
}

float UCFMatchmakingSubsystem::RetryDelay(const FString& Header, const FDateTime& Now)
{
    double Seconds = 0;
    if (!FDefaultValueHelper::ParseDouble(Header, Seconds))
    {
        FDateTime Until;
        if (!FDateTime::ParseHttpDate(Header, Until)) return 0.f;
        Seconds = (Until - Now).GetTotalSeconds();
    }
    return FMath::IsFinite(Seconds) && Seconds > 0 && Seconds < MAX_flt ? static_cast<float>(Seconds) : 0.f;
}

void UCFMatchmakingSubsystem::Receive(int32 FlowId, uint64 Serial, ECFMatchRequest Kind, int32 Status,
    const FString& Body, const FString& Date, const FString& RetryAfter)
{
    if (!IsCurrentResponse(FlowId, Serial)) return;
    HttpRequest.Reset();
    ++HttpSerial; // 중복 완료 콜백 역시 무시한다.
    FString Base, Token, Player;
    if (!Entry->GetMatchmakingContext(Base, Token, Player))
    {
        if (Kind != ECFMatchRequest::Current) Flow.MarkWriteUncertain();
        Entry->InvalidateAuthentication(); return;
    }
    if (Flow.PlayerId != Player) { Send(ECFMatchRequest::Current); return; }
    FDateTime ServerNow;
    if (!FDateTime::ParseHttpDate(Date, ServerNow)) ServerNow = FDateTime::UtcNow();
    const float Delay = RetryDelay(RetryAfter, ServerNow);
    NextAllowedAt = FPlatformTime::Seconds() + Delay;
    // 원문 응답, PUID, 티켓/매치 ID, 인증 헤더는 로그에 남기지 않는다.
    UE_LOG(LogCFMatchmaking, Display, TEXT("Ticket response: Flow=%d Operation=%d HTTP=%d"), FlowId, static_cast<int32>(Kind), Status);
    Apply(Flow.Receive(Kind, Status, Body, ServerNow, Delay));
}

void UCFMatchmakingSubsystem::Apply(const FCFMatchUpdate& Update)
{
    const int32 Id = Flow.Id;
    if (Update.State == ECFMatchUpdate::AuthenticationRequired) { Entry->InvalidateAuthentication(); return; }
    if (Update.State == ECFMatchUpdate::Idle)
    {
        StopIo(); Flow = FCFMatchmakingFlow();
        Lobby->ReportIdle(Id, FText::FromString(Update.Message)); return;
    }
    // ReportWaiting가 즉시 취소 이벤트를 발생시켜도 현재 조회 타이머를 덮어쓸 수 있게 먼저 예약한다.
    if (Update.Next != ECFMatchRequest::None) Schedule(Update.Next, Update.Delay);
    if (Update.State == ECFMatchUpdate::Waiting) Lobby->ReportWaiting(Id, Update.RemainingSeconds);
    else if (Update.State == ECFMatchUpdate::Assigned)
    {
        Lobby->ReportAssigned(Id, FText::FromString(TEXT("매칭이 완료되었습니다.\n연결을 기다리고 있습니다.")));
        if (Update.bNewAssignment) OnMatchAssigned.Broadcast(Flow.MatchId);
    }
    else if (Update.State == ECFMatchUpdate::Uncertain)
        Lobby->ReportUncertain(Id, FText::FromString(Update.Message));
}
