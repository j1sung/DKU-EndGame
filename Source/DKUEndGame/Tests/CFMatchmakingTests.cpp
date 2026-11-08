#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFMatchmakingFlow.h"
#include "Network/CFMatchmakingSubsystem.h"
#include "UI/CFLobbySubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchmakingFlowTest, "ChickenFight.Matchmaking.TicketFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchmakingFlowTest::RunTest(const FString& Parameters)
{
    const FDateTime Now(2026, 11, 6, 0, 0, 0);
    const FString Ticket = TEXT("019a4800-0000-7000-8000-000000000001");
    const FString Match = TEXT("019a4800-0000-7000-8000-000000000002");
    const FString Waiting = FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"WAITING\",\"expiresAt\":\"2026-11-06T00:03:00.000Z\"}"), *Ticket);
    const FString Assigned = FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"ASSIGNED\",\"matchId\":\"%s\"}"), *Ticket, *Match);
    const FString Cancelled = FString::Printf(TEXT("{\"ticketId\":\"%s\",\"status\":\"CANCELLED\"}"), *Ticket);
    FCFMatchmakingFlow Flow;
    Flow.Begin(1, TEXT("player-a"), true);
    const FString CreateKey = Flow.KeyFor(ECFMatchRequest::Create);
    TestFalse(TEXT("Create has UUID key"), CreateKey.IsEmpty());
    TestEqual(TEXT("Single supported mode"), Flow.BodyFor(ECFMatchRequest::Create), FString(TEXT("{\"mode\":\"standard\"}")));
    Flow.WantCancel();
    auto Update = Flow.Receive(ECFMatchRequest::Create, 0, TEXT(""), Now);
    TestTrue(TEXT("Lost create retries create"), Update.Next == ECFMatchRequest::Create);
    TestEqual(TEXT("Lost create preserves key"), Flow.KeyFor(Update.Next), CreateKey);
    TestTrue(TEXT("Recovery resolves uncertain create before GET"), Flow.RecoveryRequest() == ECFMatchRequest::Create);
    Update = Flow.Receive(ECFMatchRequest::Create, 201, Waiting, Now);
    TestTrue(TEXT("Replayed create must be reconciled with current"), Update.Next == ECFMatchRequest::Current);
    TestTrue(TEXT("Create replay does not show stale waiting"), Update.State == ECFMatchUpdate::None);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Waiting, Now);
    TestTrue(TEXT("Early cancel sends DELETE after ticket known"), Update.Next == ECFMatchRequest::Cancel);
    const FString CancelKey = Flow.KeyFor(ECFMatchRequest::Cancel);
    const FString CancelBody = Flow.BodyFor(ECFMatchRequest::Cancel);
    Update = Flow.Receive(ECFMatchRequest::Cancel, 0, TEXT(""), Now);
    TestEqual(TEXT("Lost delete preserves key"), Flow.KeyFor(Update.Next), CancelKey);
    TestEqual(TEXT("Lost delete preserves exact body"), Flow.BodyFor(Update.Next), CancelBody);
    Update = Flow.Receive(ECFMatchRequest::Cancel, 200, Cancelled, Now);
    TestTrue(TEXT("Cancel replay also rechecks current"), Update.Next == ECFMatchRequest::Current);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, TEXT("{\"ticket\":null}"), Now);
    TestTrue(TEXT("Server confirms no active ticket"), Update.State == ECFMatchUpdate::Idle);

    Flow.Begin(2, TEXT("player-a"), false);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Waiting, Now);
    TestEqual(TEXT("Waiting shows 180 seconds"), Update.RemainingSeconds, 180);
    TestTrue(TEXT("Waiting polls every two seconds"), Update.Next == ECFMatchRequest::Current && Update.Delay == 2.f);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Waiting, Now + FTimespan::FromSeconds(190));
    TestEqual(TEXT("Past local deadline clamps to zero"), Update.RemainingSeconds, 0);
    TestTrue(TEXT("Local expiry does not fabricate idle"), Update.State == ECFMatchUpdate::Waiting);
    Flow.WantCancel();
    Update = Flow.Receive(ECFMatchRequest::Cancel, 409, TEXT("{\"error\":{\"code\":\"TICKET_ALREADY_ASSIGNED\"}}"), Now);
    TestTrue(TEXT("Cancel assignment conflict queries current"), Update.Next == ECFMatchRequest::Current);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Assigned, Now);
    TestTrue(TEXT("Assigned blocks cancellation"), Flow.IsAssigned());
    TestTrue(TEXT("Assignment is handed off once"), Update.bNewAssignment);
    TestEqual(TEXT("Match id preserved"), Flow.MatchId, Match);
    Flow.WantCancel();
    TestTrue(TEXT("Recovery after assigned never deletes"), Flow.RecoveryRequest() == ECFMatchRequest::Current);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Assigned, Now);
    TestFalse(TEXT("Repeated poll does not duplicate handoff"), Update.bNewAssignment);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, Waiting, Now);
    TestTrue(TEXT("Assigned cannot regress to waiting"), Update.State == ECFMatchUpdate::Uncertain);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, TEXT("{\"ticket\":null}"), Now);
    TestTrue(TEXT("Server terminal cleanup releases assigned flow"), Update.State == ECFMatchUpdate::Idle);

    Flow.Begin(3, TEXT("player-a"), true);
    const FString AuthKey = Flow.KeyFor(ECFMatchRequest::Create);
    Update = Flow.Receive(ECFMatchRequest::Create, 401, TEXT("private-error"), Now);
    TestTrue(TEXT("401 requires authentication"), Update.State == ECFMatchUpdate::AuthenticationRequired);
    TestEqual(TEXT("Reauthentication preserves uncertain write identity"), Flow.KeyFor(ECFMatchRequest::Create), AuthKey);
    Flow.Begin(4, TEXT("player-b"), false);
    TestTrue(TEXT("Other account does not inherit old create"), Flow.RecoveryRequest() == ECFMatchRequest::Current);
    TestTrue(TEXT("Other account has no old ticket"), Flow.TicketId.IsEmpty());

    for (const TCHAR* Bad : { TEXT("not-json"), TEXT("{}"), TEXT("{\"ticket\":false}"),
        TEXT("{\"ticketId\":\"../../bad\",\"status\":\"ASSIGNED\",\"matchId\":\"bad\"}"),
        TEXT("{\"ticketId\":\"019a4800-0000-7000-8000-000000000001\",\"status\":\"WAITING\",\"expiresAt\":\"bad\"}") })
    {
        Flow.Begin(5, TEXT("player-a"), false);
        Update = Flow.Receive(ECFMatchRequest::Current, 200, Bad, Now);
        TestTrue(TEXT("Malformed success cannot unlock new matching"), Update.State == ECFMatchUpdate::Uncertain);
    }
    Flow.Begin(6, TEXT("player-a"), true);
    Update = Flow.Receive(ECFMatchRequest::Create, 409, TEXT("{\"error\":{\"code\":\"ALREADY_ACTIVE\"}}"), Now);
    TestTrue(TEXT("Already active reconciles instead of creating again"), Update.Next == ECFMatchRequest::Current);
    Flow.Begin(7, TEXT("player-a"), true);
    Update = Flow.Receive(ECFMatchRequest::Create, 426, TEXT("{\"error\":{\"code\":\"CLIENT_VERSION_UNSUPPORTED\"}}"), Now);
    TestTrue(TEXT("Unsupported client stops start"), Update.State == ECFMatchUpdate::Idle);
    Flow.Begin(7, TEXT("player-a"), true);
    Update = Flow.Receive(ECFMatchRequest::Create, 409, TEXT("{\"error\":{\"code\":\"NICKNAME_REQUIRED\"}}"), Now);
    TestTrue(TEXT("Missing nickname refreshes cached authenticated profile"), Update.State == ECFMatchUpdate::AuthenticationRequired);
    Flow.Begin(8, TEXT("player-a"), false);
    Update = Flow.Receive(ECFMatchRequest::Current, 429, TEXT("raw-secret"), Now, 17.f);
    TestTrue(TEXT("Retry-After takes precedence"), Update.Delay >= 17.f);
    TestFalse(TEXT("Raw API errors are not displayed"), Update.Message.Contains(TEXT("raw-secret")));
    Flow.Receive(ECFMatchRequest::Current, 503, TEXT(""), Now);
    Update = Flow.Receive(ECFMatchRequest::Current, 503, TEXT(""), Now);
    TestTrue(TEXT("Repeated network failures require manual recovery"), Update.Next == ECFMatchRequest::None && Update.State == ECFMatchUpdate::Uncertain);
    Flow.Begin(9, TEXT("player-a"), true);
    Flow.Receive(ECFMatchRequest::Create, 0, TEXT(""), Now);
    Update = Flow.Receive(ECFMatchRequest::Create, 426, TEXT("{\"error\":{\"code\":\"CLIENT_VERSION_UNSUPPORTED\"}}"), Now);
    TestTrue(TEXT("A later rejection cannot erase uncertain earlier write"), Update.State == ECFMatchUpdate::Uncertain);
    Flow.Begin(10, TEXT("player-a"), false);
    Update = Flow.Receive(ECFMatchRequest::Current, 200, TEXT("{\"ticket\":null,\"status\":\"ASSIGNED\"}"), Now);
    TestTrue(TEXT("Contradictory empty result is not authoritative idle"), Update.State == ECFMatchUpdate::Uncertain);
    Flow.Begin(11, TEXT("player-a"), true);
    Flow.MarkWriteUncertain();
    Update = Flow.Receive(ECFMatchRequest::Create, 426, TEXT("{\"error\":{\"code\":\"CLIENT_VERSION_UNSUPPORTED\"}}"), Now);
    TestTrue(TEXT("Interrupted write cannot be cleared by a later rejection"), Update.State == ECFMatchUpdate::Uncertain);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchmakingResponseGuardTest, "ChickenFight.Matchmaking.ResponseGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchmakingResponseGuardTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    auto* Lobby = NewObject<UCFLobbySubsystem>(Instance.Get());
    auto* Provider = NewObject<UCFMatchmakingSubsystem>(Instance.Get());
    Provider->Lobby = Lobby;
    Provider->Flow.Begin(8, TEXT("player-a"), false);
    Provider->HttpSerial = 20; Lobby->ActiveFlowId = 8;
    TestTrue(TEXT("Current response accepted"), Provider->IsCurrentResponse(8, 20));
    TestFalse(TEXT("Old HTTP completion rejected"), Provider->IsCurrentResponse(8, 19));
    TestFalse(TEXT("Old flow rejected"), Provider->IsCurrentResponse(7, 20));
    Lobby->ActiveFlowId = 9;
    TestFalse(TEXT("UI retired flow rejected"), Provider->IsCurrentResponse(8, 20));
    Lobby->ActiveFlowId = 8; Provider->bDeinitializing = true;
    TestFalse(TEXT("Shutdown rejects completion"), Provider->IsCurrentResponse(8, 20));
    const FDateTime Now(2026, 11, 6, 0, 0, 0);
    TestEqual(TEXT("Numeric Retry-After supported"), Provider->RetryDelay(TEXT("17"), Now), 17.f);
    TestEqual(TEXT("HTTP-date Retry-After uses server clock"), Provider->RetryDelay(TEXT("Fri, 06 Nov 2026 00:00:17 GMT"), Now), 17.f);
    TestEqual(TEXT("Invalid Retry-After ignored"), Provider->RetryDelay(TEXT("bad"), Now), 0.f);
    TestEqual(TEXT("Negative Retry-After ignored"), Provider->RetryDelay(TEXT("-1"), Now), 0.f);
    return true;
}
#endif
