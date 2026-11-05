#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "UI/CFLobbyController.h"
#include "Network/CFMatchmakingSubsystem.h"
#include "Network/CFEntrySubsystem.h"
#include "UI/CFLobbySubsystem.h"
#include "UI/CFFrontendSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IHttpRequest.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace CFMatchingTest
{
const TCHAR* Ticket = TEXT("019aaa00-0000-7000-8000-000000000001");
const TCHAR* Match = TEXT("019aaa00-0000-7000-8000-000000000002");
FString Waiting(int32 Seconds = 170)
{
    return FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"WAITING\",\"expiresAt\":\"%s\"}"), Ticket, *(FDateTime::UtcNow() + FTimespan::FromSeconds(Seconds)).ToIso8601());
}
FString Assigned()
{
    return FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"ASSIGNED\",\"matchId\":\"%s\"}"), Ticket, Match);
}
FString MatchBody(const TCHAR* Status = TEXT("FORMING"), const TCHAR* Host = TEXT("test-player"))
{
    return FString::Printf(TEXT("{\"matchId\":\"%s\",\"status\":\"%s\",\"memberIds\":[\"test-player\",\"p2\",\"p3\",\"p4\"],\"hostPlayerId\":\"%s\",\"hostAttempt\":1,\"deadlineAt\":\"%s\",\"sessionId\":\"test-session\"}"), Match, Status, Host, *(FDateTime::UtcNow()+FTimespan::FromSeconds(55)).ToIso8601());
}
FString Cancelled() { return FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"CANCELLED\"}"), Ticket); }
const TCHAR* Empty = TEXT("{\"ticket\":null}");
FString Body(const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request)
{
    const auto& Bytes = Request->GetContent();
    FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    return FString(Converted.Length(), Converted.Get());
}
bool Access(FString& Base, FString& Token, FString& Player)
{
    Base = TEXT("https://automation.invalid"); Token = TEXT("automation-token"); Player = TEXT("test-player"); return true;
}
}

struct FCFMatchmakingTestDriver
{
    using ERequest = UCFMatchmakingSubsystem::ERequest;
    struct FCall { int32 Id; uint64 Serial; ERequest Kind; TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request; };
    UCFMatchmakingSubsystem* M = nullptr;
    UCFLobbySubsystem* UI = nullptr;
    TArray<FCall> Calls;
    void Setup(UCFMatchmakingSubsystem* Provider, UCFLobbySubsystem* Lobby)
    {
        M = Provider; UI = Lobby; M->Lobby = UI; M->BindLobby();
        M->TestAccess = CFMatchingTest::Access;
        M->TestSend = [this](auto Request) { Calls.Add({M->FlowId, M->Serial, M->InFlight, Request}); };
        M->bInLobby = true;
    }
    void Reset()
    {
        M->OnLobbyExited(); M->ClearTicket(); M->PlayerId.Reset(); M->NextAllowedAt = 0; M->Errors = 0;
        M->bInLobby = true; Calls.Reset();
    }
    void Pump()
    {
        M->GetGameInstance()->GetTimerManager().ClearTimer(M->PollTimer);
        M->DispatchPending();
    }
    void ReplyCall(const FCall& Call, int32 Status, const FString& Body, bool Connected = true, const FString& Retry = FString())
    {
        M->Receive(Call.Id, Call.Serial, Call.Kind, Connected, Status, Body, Retry, FString());
    }
    void Reply(int32 Status, const FString& Body, bool Connected = true, const FString& Retry = FString()) { ReplyCall(Calls.Last(), Status, Body, Connected, Retry); }
    void Waiting()
    {
        UI->RequestMatchmaking(); Reply(200, CFMatchingTest::Waiting());
    }
    void FreshCreate()
    {
        UI->RequestMatchmaking(); Reply(200, CFMatchingTest::Empty); Pump();
    }
    void Assign()
    {
        UI->RequestMatchmaking(); Reply(200, CFMatchingTest::Assigned()); Pump();
    }
    float Delay() { return M->GetGameInstance()->GetTimerManager().GetTimerRemaining(M->PollTimer); }
    void AdvanceRateLimit() { M->NextAllowedAt = 0; }
    float ParseRetry(const FString& Value, const FString& Date) { return M->RetryDelay(Value, Date); }
    bool Unresolved() { return M->bCreateUnresolved; }
    void ChangeAccount()
    {
        M->TestAccess = [](FString& Base, FString& Token, FString& Player)
        {
            CFMatchingTest::Access(Base, Token, Player); Player = TEXT("other-player"); return true;
        };
    }
    void Shutdown() { M->OnLobbyExited(); M->TestSend = nullptr; M->TestAccess = nullptr; }

    static bool IsLobbyPIE(UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE || World->bIsTearingDown || !World->HasBegunPlay() || !World->GetGameInstance()) return false;
        const auto* Controller = Cast<ACFLobbyController>(World->GetGameInstance()->GetFirstLocalPlayerController());
        return Controller && Controller->GetWorld() == World;
    }

    static bool InstallPIE(UCFMatchmakingSubsystem* M)
    {
        if (!M || !M->Lobby || !M->bInLobby || !IsLobbyPIE(M->GetWorld())) return false;
        if (M->TestAccess && M->TestSend) return true; // 명령 재입력은 진행 중인 모의 대기/배정을 초기화하지 않는다.
        if (M->FlowId != 0 || M->InFlight != ERequest::None || M->bCreateUnresolved || !M->TicketId.IsEmpty() || !M->MatchId.IsEmpty()) return false;
        // 실제 요청 결과가 미확정일 때 모의 응답으로 덮어쓰지 않는다.
        struct FFixture { bool Ticket = false; bool Assigned = false; int32 Round = 0; int32 Polls = 0; int32 MatchPolls = 0; };
        auto State = MakeShared<FFixture>();
        M->OnLobbyExited(); M->ClearTicket(); M->NextAllowedAt = 0; M->PlayerId.Reset();
        M->TestAccess = CFMatchingTest::Access;
        const TWeakObjectPtr<UCFMatchmakingSubsystem> Weak(M);
        M->TestSend = [Weak, State](auto Request)
        {
            auto* Target = Weak.Get(); if (!Target) return;
            const int32 Id = Target->FlowId; const uint64 Serial = Target->Serial; const ERequest Kind = Target->InFlight;
            int32 Code = 200; FString Body;
            if (Kind == ERequest::Create)
            {
                State->Ticket = true; ++State->Round; State->Polls = 0; Code = 201; Body = CFMatchingTest::Waiting();
            }
            else if (Kind == ERequest::Cancel)
            {
                State->Ticket = false; Body = CFMatchingTest::Cancelled();
            }
            else if (Kind == ERequest::Match)
            {
                if (++State->MatchPolls >= 5) { Body = CFMatchingTest::MatchBody(TEXT("EXPIRED")); State->Ticket = State->Assigned = false; }
                else Body = CFMatchingTest::MatchBody();
            }
            else
            {
                if (State->Ticket && State->Round >= 2 && ++State->Polls >= 3) State->Assigned = true;
                Body = State->Assigned ? CFMatchingTest::Assigned() : State->Ticket ? CFMatchingTest::Waiting() : FString(CFMatchingTest::Empty);
            }
            FTimerHandle ResponseTimer;
            Target->GetGameInstance()->GetTimerManager().SetTimer(ResponseTimer, FTimerDelegate::CreateWeakLambda(Target,
                [Weak, Id, Serial, Kind, Code, Body]() { if (auto* Live = Weak.Get()) Live->Receive(Id, Serial, Kind, true, Code, Body, FString(), FString()); }), .2f, false);
        };
        M->bInLobby = true;
        return true;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchmakingStateTest, "ChickenFight.Matchmaking.StateMachine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchmakingStateTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    auto* UI = NewObject<UCFLobbySubsystem>(Instance.Get());
    auto* Provider = NewObject<UCFMatchmakingSubsystem>(Instance.Get());
    FCFMatchmakingTestDriver D; D.Setup(Provider, UI);
    using EScreen = ECFQueueScreen;
    using EReq = FCFMatchmakingTestDriver::ERequest;

    UI->RequestMatchmaking(); UI->RequestMatchmaking();
    TestEqual(TEXT("Duplicate click has one preflight GET"), D.Calls.Num(), 1);
    TestEqual(TEXT("Preflight path"), D.Calls.Last().Request->GetURL(), FString(TEXT("https://automation.invalid/v1/matchmaking/tickets/current")));
    UI->RequestCancel(); D.Reply(200, CFMatchingTest::Empty);
    TestEqual(TEXT("Early cancel before create does not POST"), D.Calls.Num(), 1);
    TestEqual(TEXT("Confirmed no ticket returns idle"), UI->Screen, EScreen::Idle);

    D.Reset(); D.FreshCreate();
    const auto Create = D.Calls.Last();
    const FString CreateKey = Create.Request->GetHeader(TEXT("Idempotency-Key"));
    TestEqual(TEXT("POST for standard mode"), CFMatchingTest::Body(Create.Request), FString(TEXT("{\"mode\":\"standard\"}")));
    TestEqual(TEXT("Client version sent"), Create.Request->GetHeader(TEXT("X-Client-Version")), FString(TEXT("0.1.0")));
    TestFalse(TEXT("Create UUID key exists"), CreateKey.IsEmpty());
    UI->RequestCancel(); D.Reply(201, CFMatchingTest::Waiting()); D.Pump();
    TestEqual(TEXT("Cached create response rechecked"), D.Calls.Last().Kind, EReq::Current);
    D.Reply(200, CFMatchingTest::Waiting()); D.Pump();
    TestEqual(TEXT("Pending cancel sends DELETE after ticket known"), D.Calls.Last().Request->GetVerb(), FString(TEXT("DELETE")));
    TestTrue(TEXT("Cancel body identifies ticket"), CFMatchingTest::Body(D.Calls.Last().Request).Contains(CFMatchingTest::Ticket));
    const auto Delete = D.Calls.Last();
    UI->RequestCancel(); TestEqual(TEXT("Cancel spam does not duplicate HTTP"), D.Calls.Last().Serial, Delete.Serial);
    D.Reply(200, CFMatchingTest::Cancelled()); D.Pump();
    TestEqual(TEXT("DELETE is confirmed by current"), UI->Screen, EScreen::Cancelling);
    D.Reply(200, CFMatchingTest::Empty);
    TestEqual(TEXT("Cancellation completes"), UI->Screen, EScreen::Idle);
    D.ReplyCall(Create, 201, CFMatchingTest::Waiting());
    TestEqual(TEXT("Late old POST cannot restore waiting"), UI->Screen, EScreen::Idle);

    D.Reset(); D.FreshCreate(); const FString LostKey = D.Calls.Last().Request->GetHeader(TEXT("Idempotency-Key"));
    D.Reply(0, FString(), false); UI->RequestCancel(); D.Pump();
    D.Reply(200, CFMatchingTest::Empty); D.Pump();
    TestEqual(TEXT("Unknown POST outcome reuses same key even after cancel"), D.Calls.Last().Request->GetHeader(TEXT("Idempotency-Key")), LostKey);
    TestTrue(TEXT("Still unresolved before replay response"), D.Unresolved());
    D.Reply(201, CFMatchingTest::Waiting()); D.Pump(); D.Reply(200, CFMatchingTest::Empty);
    TestEqual(TEXT("Stale cached create does not leave phantom wait"), UI->Screen, EScreen::Idle);

    D.Reset(); D.Waiting(); UI->RequestCancel(); D.Pump();
    const FString CancelKey = D.Calls.Last().Request->GetHeader(TEXT("Idempotency-Key"));
    D.Reply(0, FString(), false); D.Pump(); D.Reply(200, CFMatchingTest::Waiting()); D.Pump();
    TestEqual(TEXT("Unknown DELETE replay uses original key"), D.Calls.Last().Request->GetHeader(TEXT("Idempotency-Key")), CancelKey);
    D.Reply(409, TEXT("{\"error\":{\"code\":\"TICKET_ALREADY_ASSIGNED\"}}")); D.Pump(); D.Reply(200, CFMatchingTest::Assigned());
    TestEqual(TEXT("Assignment wins cancellation"), UI->Screen, EScreen::Connecting);
    TestFalse(TEXT("Assignment clears cancel intent"), UI->bCancelRequested);
    TestFalse(TEXT("Cannot leave through cancel after assignment"), UI->CanReturnToTitle());
    D.Pump(); D.Reply(200, CFMatchingTest::MatchBody());
    TestTrue(TEXT("Server host identity retained"), Provider->Assignment.bIsHost);
    TestEqual(TEXT("Exactly four assigned members exposed"), Provider->Assignment.MemberIds.Num(), 4);
    D.Pump(); D.Reply(200, CFMatchingTest::MatchBody(TEXT("SESSION_READY"), TEXT("p2")));
    TestFalse(TEXT("Guest role derives from host PUID"), Provider->Assignment.bIsHost);
    TestEqual(TEXT("Session id retained for future EOS adapter"), Provider->Assignment.SessionId, FString(TEXT("test-session")));
    D.Pump(); D.Reply(200, CFMatchingTest::MatchBody(TEXT("EXPIRED")));
    TestEqual(TEXT("Server match expiration permits manual retry"), UI->Screen, EScreen::Idle);
    TestFalse(TEXT("No automatic new match after expiration"), D.Unresolved());

    for (const TCHAR* Code : {TEXT("TICKET_MISMATCH"), TEXT("TICKET_NOT_FOUND")})
    {
        D.Reset(); D.Waiting(); UI->RequestCancel(); D.Pump();
        D.Reply(FString(Code) == TEXT("TICKET_NOT_FOUND") ? 404 : 409, FString::Printf(TEXT("{\"error\":{\"code\":\"%s\"}}"), Code)); D.Pump();
        TestEqual(TEXT("Conflict/no ticket triggers GET reconciliation"), D.Calls.Last().Kind, EReq::Current);
    }
    D.Reset(); D.Waiting(); D.Pump(); D.Reply(200, CFMatchingTest::Waiting(-10));
    TestEqual(TEXT("Local deadline zero is not cancellation"), UI->Screen, EScreen::Waiting);
    TestEqual(TEXT("Expired display clamps to zero"), UI->GetRemainingSeconds(), 0);
    D.Pump(); D.Reply(200, CFMatchingTest::Empty);
    TestEqual(TEXT("Server no-ticket ends expired wait"), UI->Screen, EScreen::Idle);

    D.Reset(); UI->RequestMatchmaking(); D.Reply(429, TEXT("{}"), true, TEXT("9"));
    TestTrue(TEXT("Retry-After outranks five-second backoff"), D.Delay() > 8.f);
    const int32 Before = D.Calls.Num(); UI->RecoverStatus(); D.Pump();
    TestEqual(TEXT("Manual recover respects rate limit"), D.Calls.Num(), Before);
    TestEqual(TEXT("HTTP-date Retry-After accepted"), D.ParseRetry(TEXT("Thu, 05 Nov 2026 00:00:09 GMT"), TEXT("Thu, 05 Nov 2026 00:00:00 GMT")), 9.f);
    D.AdvanceRateLimit(); D.Pump(); D.Reply(503, TEXT("private-server-detail"));
    TestFalse(TEXT("Raw error body never rendered"), UI->ErrorMessage.ToString().Contains(TEXT("private-server-detail")));
    TestEqual(TEXT("Network uncertainty retains active flow"), UI->Screen, EScreen::Recovering);
    TestTrue(TEXT("Recovery has active flow"), UI->ActiveFlowId > 0);

    for (const TCHAR* Bad : { TEXT("not json"), TEXT("{}"), TEXT("{\"ticketId\":42,\"status\":\"WAITING\"}"), TEXT("{\"ticket\":false}") })
    {
        D.Reset(); UI->RequestMatchmaking(); D.Reply(200, Bad);
        TestEqual(TEXT("Malformed reply cannot pretend idle"), UI->Screen, EScreen::Recovering);
    }
    D.Reset(); D.Assign(); D.Reply(200, CFMatchingTest::MatchBody(TEXT("FORMING"), TEXT("not-a-member")));
    TestEqual(TEXT("Non-member host assignment rejected"), UI->Screen, EScreen::Recovering);
    D.Reset(); D.FreshCreate(); D.Reply(426, TEXT("{\"error\":{\"code\":\"CLIENT_VERSION_UNSUPPORTED\"}}"));
    TestEqual(TEXT("Unsupported version gives idle error"), UI->Screen, EScreen::Idle);
    TestTrue(TEXT("Update message provided"), UI->ErrorMessage.ToString().Contains(TEXT("업데이트")));
    D.Reset(); D.FreshCreate(); D.Reply(409, TEXT("{\"error\":{\"code\":\"ALREADY_ACTIVE\"}}")); D.Pump();
    TestEqual(TEXT("Already active restores current"), D.Calls.Last().Kind, EReq::Current);
    D.Reset(); UI->RequestMatchmaking(); D.Reply(401, TEXT("{}"));
    TestEqual(TEXT("401 clears local flow for re-entry"), UI->ActiveFlowId, 0);
    D.Reset(); D.Waiting(); UI->RequestCancel(); D.Pump(); D.Reply(401, TEXT("{}"));
    UI->RestoreOnEntry(); D.Pump(); D.Reply(200, CFMatchingTest::Waiting()); D.Pump();
    TestEqual(TEXT("Cancel intent survives reauthentication"), D.Calls.Last().Kind, EReq::Cancel);
    D.Reset(); D.FreshCreate(); D.Reply(409, TEXT("{\"error\":{\"code\":\"NICKNAME_REQUIRED\"}}"));
    TestEqual(TEXT("Missing nickname routes back to entry"), UI->ActiveFlowId, 0);
    TestFalse(TEXT("Nickname rejection is not uncertain POST"), D.Unresolved());
    D.Reset(); D.Waiting(); UI->RequestCancel(); D.ChangeAccount(); D.Pump(); D.Pump();
    TestEqual(TEXT("Account change queries current instead of sending old cancel"), D.Calls.Last().Kind, EReq::Current);
    TestFalse(TEXT("Old cancel intent cannot cancel other account ticket"), UI->bCancelRequested);
    D.Shutdown();
    return true;
}

#if WITH_EDITOR
namespace CFMatchingTest
{
void EnableCurrentPIE(UWorld* ContextWorld)
{
    if (!GEngine) return;
    UWorld* Target = FCFMatchmakingTestDriver::IsLobbyPIE(ContextWorld) ? ContextWorld : nullptr;
    if (!Target)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* Candidate = Context.World();
            if (!FCFMatchmakingTestDriver::IsLobbyPIE(Candidate)) continue;
            if (Target)
            {
                UE_LOG(LogTemp, Warning, TEXT("CF_MATCHING_MOCK_NOT_ENABLED: Multiple lobby PIE worlds. Run from the intended play window."));
                return;
            }
            Target = Candidate;
        }
    }
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("CF_MATCHING_MOCK_NOT_ENABLED: Start L_Lobby_Playground in Selected Viewport or New Editor Window (PIE), then run CF.Matchmaking.Mock."));
        return;
    }
    if (!FCFMatchmakingTestDriver::InstallPIE(Target->GetGameInstance()->GetSubsystem<UCFMatchmakingSubsystem>()))
    {
        UE_LOG(LogTemp, Warning, TEXT("CF_MATCHING_MOCK_NOT_ENABLED: A real matchmaking flow is active or unresolved. It was not replaced."));
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("CF_MATCHING_MOCK_ENABLED: %s (Editor test only; no AWS/EOS requests)"), *Target->GetName());
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green, TEXT("Mock matchmaking ON (Editor test only)"));
}

// 자동화 실행과 달리 현재 PIE를 종료하지 않는 수동 확인 명령. Editor + 개발 테스트 빌드에만 존재한다.
FAutoConsoleCommandWithWorld MockCommand(TEXT("CF.Matchmaking.Mock"),
    TEXT("Enable mock matchmaking in the current idle lobby PIE. Stop Play to remove the fixture. Editor test only."),
    FConsoleCommandWithWorldDelegate::CreateStatic(&EnableCurrentPIE), ECVF_Cheat);
}

// 외부 PIE 검증에서만 호출하는 fixture. 실제 HTTP/EOS를 대체한 테스트라는 사실을 이름에 명시한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchmakingPIEFixture, "ChickenFight.Matchmaking.InstallPIEFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCFMatchmakingPIEFixture::RunTest(const FString& Parameters)
{
    // Editor automation은 테스트 시작 전 PIE를 닫는다. 외부 검증 스크립트가 다음에 여는 PIE에 한 번만 설치한다.
    const double Until = FPlatformTime::Seconds() + 30;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Until](float)
    {
        if (!GEngine) return false;
        if (FPlatformTime::Seconds() >= Until)
        {
            UE_LOG(LogTemp, Warning, TEXT("CF_MATCHING_FIXTURE_EXPIRED: Start lobby PIE, then run CF.Matchmaking.Mock without a time limit."));
            return false;
        }
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (UWorld* World = Context.World(); FCFMatchmakingTestDriver::IsLobbyPIE(World))
                if (FCFMatchmakingTestDriver::InstallPIE(World->GetGameInstance()->GetSubsystem<UCFMatchmakingSubsystem>()))
                {
                    UE_LOG(LogTemp, Display, TEXT("CF_MATCHING_FIXTURE_APPLIED: %s"), *World->GetName());
                    return false;
                }
        return true;
    }), .1f);
    UE_LOG(LogTemp, Display, TEXT("CF_MATCHING_FIXTURE_ARMED"));
    return true;
}
#endif
#endif
