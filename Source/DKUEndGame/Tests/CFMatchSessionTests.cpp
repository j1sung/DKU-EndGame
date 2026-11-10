#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFMatchSessionContract.h"
#include "Network/CFMatchSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchSessionContractTest, "ChickenFight.EOS.MatchSessionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchSessionContractTest::RunTest(const FString& Parameters)
{
    const FString Local = TEXT("00000000000000000000000000000001");
    const FString Id = TEXT("019a0000-0000-7000-8000-000000000001");
    const FString Body = TEXT("{\"matchId\":\"019a0000-0000-7000-8000-000000000001\",\"status\":\"SESSION_READY\",\"hostAttempt\":1,\"hostPlayerId\":\"00000000000000000000000000000001\",\"memberIds\":[\"00000000000000000000000000000001\",\"00000000000000000000000000000002\",\"00000000000000000000000000000003\",\"00000000000000000000000000000004\"],\"sessionId\":\"session-1\",\"joinedCount\":2,\"deadlineAt\":\"2026-11-08T12:00:00.000Z\"}");
    FCFSessionMatch Match;
    TestTrue(TEXT("Valid member snapshot"), FCFSessionMatch::Parse(Body, Id, Local, Match));
    TestTrue(TEXT("Roster admission"), Match.Contains(Local));
    TestFalse(TEXT("Foreign admission"), Match.Contains(TEXT("00000000000000000000000000000005")));
    TestFalse(TEXT("Foreign account snapshot"), FCFSessionMatch::Parse(Body, Id, TEXT("other"), Match));
    TestFalse(TEXT("Wrong match"), FCFSessionMatch::Parse(Body, TEXT("other"), Local, Match));
    TestFalse(TEXT("Duplicate roster"), FCFSessionMatch::Parse(Body.Replace(TEXT("00000000000000000000000000000004"), *Local), Id, Local, Match));
    TestFalse(TEXT("Missing session in ready"), FCFSessionMatch::Parse(Body.Replace(TEXT(",\"sessionId\":\"session-1\""), TEXT("")), Id, Local, Match));
    TestFalse(TEXT("Unknown phase"), FCFSessionMatch::Parse(Body.Replace(TEXT("SESSION_READY"), TEXT("OTHER")), Id, Local, Match));
    TestFalse(TEXT("Fractional attempt"), FCFSessionMatch::Parse(Body.Replace(TEXT("\"hostAttempt\":1"), TEXT("\"hostAttempt\":1.5")), Id, Local, Match));
    TestFalse(TEXT("Impossible ready count"), FCFSessionMatch::Parse(Body.Replace(TEXT("SESSION_READY"), TEXT("READY_TO_START")), Id, Local, Match));
    TestFalse(TEXT("Control characters in session"), FCFSessionMatch::Parse(Body.Replace(TEXT("session-1"), TEXT("session\\n1")), Id, Local, Match));
    TestFalse(TEXT("Session exceeds backend limit"), FCFSessionMatch::Parse(Body.Replace(TEXT("session-1"), *FString::ChrN(129, 's')), Id, Local, Match));
    const FString Ready = Body.Replace(TEXT("SESSION_READY"), TEXT("READY_TO_START")).Replace(TEXT("\"joinedCount\":2"), TEXT("\"joinedCount\":4"));
    TestTrue(TEXT("Four joined ready"), FCFSessionMatch::Parse(Ready, Id, Local, Match));
    TestTrue(TEXT("In game snapshot"), FCFSessionMatch::Parse(Ready.Replace(TEXT("READY_TO_START"), TEXT("IN_GAME")), Id, Local, Match));
    TestTrue(TEXT("Terminal snapshot without active deadline"), FCFSessionMatch::Parse(Body.Replace(TEXT("SESSION_READY"), TEXT("EXPIRED")).Replace(TEXT(",\"deadlineAt\":\"2026-11-08T12:00:00.000Z\""), TEXT("")), Id, Local, Match));
    TestTrue(TEXT("Terminal detected"), Match.IsTerminal());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchSessionResponseTest, "ChickenFight.EOS.MatchSessionResponseGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchSessionResponseTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    auto* Provider = NewObject<UCFMatchSessionSubsystem>(Instance.Get());
    Provider->MatchId = TEXT("019a0000-0000-7000-8000-000000000001");
    Provider->Generation = 3; Provider->HttpSerial = 10; Provider->bNeedSnapshot = false;
    Provider->QueueWrite(TEXT("joined"));
    const FString Key = Provider->PendingKey;
    Provider->QueueWrite(TEXT("joined"));
    TestEqual(TEXT("Repeated pending action preserves key"), Provider->PendingKey, Key);
    Provider->Receive(9, 3, TEXT("joined"), 200, TEXT("{}"), TEXT(""), TEXT(""));
    Provider->Receive(10, 2, TEXT("joined"), 200, TEXT("{}"), TEXT(""), TEXT(""));
    TestFalse(TEXT("Stale callbacks cannot confirm joined"), Provider->bJoined);
    TestEqual(TEXT("Stale callbacks retain pending key"), Provider->PendingKey, Key);
    Provider->Receive(10, 3, TEXT("joined"), 200, TEXT("{}"), TEXT(""), TEXT(""));
    TestTrue(TEXT("Current acknowledgement confirms joined"), Provider->bJoined);
    TestTrue(TEXT("Acknowledgement requires a fresh GET before advancing"), Provider->bNeedSnapshot);
    TestTrue(TEXT("Acknowledged action retired"), Provider->PendingAction.IsEmpty());
    Provider->QueueWrite(TEXT("started"));
    Provider->bNeedSnapshot = false;
    Provider->Receive(Provider->HttpSerial, 3, TEXT("started"), 409, TEXT("{}"), TEXT(""), TEXT(""));
    TestFalse(TEXT("Conflict is not started success"), Provider->bStarted);
    TestTrue(TEXT("Conflict reconciles with GET"), Provider->bNeedSnapshot);
    Provider->bDeinitializing = true;
    const uint64 Serial = Provider->HttpSerial;
    Provider->Receive(Serial, 3, TEXT("started"), 200, TEXT("{}"), TEXT(""), TEXT(""));
    TestEqual(TEXT("Shutdown rejects completion"), Provider->HttpSerial, Serial);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchSessionTravelURLTest, "ChickenFight.EOS.MatchSessionTravelURL",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchSessionTravelURLTest::RunTest(const FString& Parameters)
{
    const FString AssignedMatchId = TEXT("019a0000-0000-7000-8000-000000000001");
    const FString Address = TEXT("[EOS:00000000000000000000000000000001]");
    const FString Travel = UCFMatchSessionSubsystem::BuildRoundTravelURL(Address, AssignedMatchId, 1);
    TestEqual(TEXT("EOS resolved address is preserved including brackets"), Travel,
        Address + TEXT("?CFMatch=") + AssignedMatchId + TEXT("?CFAttempt=1?CFProtocol=2"));
    const FURL Parsed(nullptr, *Travel, TRAVEL_Absolute);
    TestEqual(TEXT("ClientTravel receives a valid URL"), Parsed.Valid, 1);
    TestEqual(TEXT("EOS target stays in Host instead of Map"), Parsed.Host, FString(TEXT("EOS:00000000000000000000000000000001")));
    TestEqual(TEXT("Match option survives parsing"), FString(Parsed.GetOption(TEXT("CFMatch="), TEXT(""))), AssignedMatchId);
    TestEqual(TEXT("Attempt option survives parsing"), FString(Parsed.GetOption(TEXT("CFAttempt="), TEXT(""))), FString(TEXT("1")));
    return true;
}
#endif
