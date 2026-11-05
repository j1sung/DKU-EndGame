#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFEntrySubsystem.h"
#include "UI/CFFrontendSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFEntryResponseTest, "ChickenFight.Entry.ProfileResponses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFEntryResponseTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    auto* UI = NewObject<UCFFrontendSubsystem>(Instance.Get());
    auto* Entry = NewObject<UCFEntrySubsystem>(Instance.Get());
    Entry->Frontend = UI;
    using EOp = UCFEntrySubsystem::EOperation;
    auto Prepare = [&](EOp Op, int32 Id = 10)
    {
        Entry->ClearOperation();
        UI->InvalidateProfile();
        UI->Screen = Op == EOp::Nickname ? ECFFrontendScreen::SavingNickname : ECFFrontendScreen::LoggingIn;
        UI->ActiveRequestId = Id;
        Entry->RequestId = Id; Entry->Operation = Op; Entry->HttpSerial = 20;
        Entry->ExpectedPlayerId = TEXT("test-player"); Entry->PendingNickname = TEXT("새닉네임");
    };
    auto Reply = [&](bool Post, int32 Status, const TCHAR* Body, bool Connected = true)
    { Entry->ReceiveProfile(10, 20, Post, Connected, Status, Body); };

    Prepare(EOp::Login);
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\"}"));
    TestEqual(TEXT("First profile requires nickname"), UI->Screen, ECFFrontendScreen::Nickname);
    TestTrue(TEXT("No fabricated nickname"), UI->ProfileNickname.IsEmpty());

    Prepare(EOp::Login);
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"등록된 이름\"}"));
    TestEqual(TEXT("Registered server name retained"), UI->ProfileNickname, FString(TEXT("등록된 이름")));
    TestTrue(TEXT("Registered profile skips nickname screen"), UI->Screen != ECFFrontendScreen::Nickname);

    Prepare(EOp::Login);
    Reply(false, 200, TEXT("{\"playerId\":\"different-player\",\"nickname\":\"잘못된이름\"}"));
    TestEqual(TEXT("Other account response rejected"), UI->Screen, ECFFrontendScreen::Login);
    TestTrue(TEXT("Other account name never cached"), UI->ProfileNickname.IsEmpty());

    for (const TCHAR* Body : { TEXT("not-json"), TEXT("{}"), TEXT("{\"playerId\":\"test-player\",\"nickname\":42}"), TEXT("{\"playerId\":\"test-player\",\"nickname\":\"  bad  \"}") })
    {
        Prepare(EOp::Login); Reply(false, 200, Body);
        TestEqual(TEXT("Malformed profile permits login retry"), UI->Screen, ECFFrontendScreen::Login);
        TestFalse(TEXT("Malformed profile gives safe error"), UI->ErrorMessage.IsEmpty());
    }
    Prepare(EOp::Login); Reply(false, 503, TEXT("raw-secret-must-not-be-displayed"));
    TestFalse(TEXT("Raw server body is not shown"), UI->ErrorMessage.ToString().Contains(TEXT("raw-secret")));
    TestEqual(TEXT("Server error retry screen"), UI->Screen, ECFFrontendScreen::Login);

    Prepare(EOp::Login); Reply(false, 0, TEXT(""), false);
    TestEqual(TEXT("Transport failure permits retry"), UI->Screen, ECFFrontendScreen::Login);

    Prepare(EOp::Nickname); Reply(true, 401, TEXT("{}"));
    TestEqual(TEXT("Expired token returns to login"), UI->Screen, ECFFrontendScreen::Login);
    TestTrue(TEXT("Expired token requires fresh EOS authentication"), Entry->bNeedsReauthentication);

    Prepare(EOp::Nickname);
    Reply(true, 409, TEXT("{\"error\":{\"code\":\"NICKNAME_TAKEN\",\"message\":\"private server detail\"}}"));
    TestEqual(TEXT("Taken name stays editable"), UI->Screen, ECFFrontendScreen::Nickname);
    TestTrue(TEXT("Taken name has specific message"), UI->ErrorMessage.ToString().Contains(TEXT("이미 사용 중")));

    Prepare(EOp::Nickname);
    Reply(true, 201, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"확정된 이름\"}"));
    TestEqual(TEXT("Registration uses confirmed name"), UI->ProfileNickname, FString(TEXT("확정된 이름")));

    Prepare(EOp::Nickname);
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"이전등록성공\"}"));
    TestEqual(TEXT("Retry recovers committed registration via GET"), UI->ProfileNickname, FString(TEXT("이전등록성공")));

    Prepare(EOp::Nickname); Entry->bResolvingRegistrationConflict = true;
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\"}"));
    TestEqual(TEXT("Inconsistent conflict response does not loop POST"), UI->Screen, ECFFrontendScreen::Nickname);

    Prepare(EOp::Nickname); Reply(true, 201, TEXT("{\"playerId\":\"test-player\"}"));
    TestEqual(TEXT("POST without nickname cannot enter lobby"), UI->Screen, ECFFrontendScreen::Nickname);

    Prepare(EOp::Login, 11);
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"늦은응답\"}"));
    TestEqual(TEXT("Old request cannot replace new login"), UI->ActiveRequestId, 11);
    TestTrue(TEXT("Old response name ignored"), UI->ProfileNickname.IsEmpty());

    Prepare(EOp::Login); Entry->HttpSerial = 21;
    Reply(false, 200, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"늦은응답\"}"));
    TestEqual(TEXT("Old HTTP attempt ignored within same request"), UI->Screen, ECFFrontendScreen::LoggingIn);

    Prepare(EOp::Nickname);
    Entry->RegistrationKey = TEXT("reusable-key");
    Entry->Cancel(10); UI->ReturnToTitle();
    Reply(true, 201, TEXT("{\"playerId\":\"test-player\",\"nickname\":\"취소뒤응답\"}"));
    TestEqual(TEXT("Cancelled registration cannot reopen UI"), UI->Screen, ECFFrontendScreen::Title);
    TestEqual(TEXT("Uncertain write retains idempotency key"), Entry->RegistrationKey, FString(TEXT("reusable-key")));

    TestTrue(TEXT("HTTPS profile endpoint accepted"), Entry->ValidApiBase(TEXT("https://example.com/dev")));
    for (const TCHAR* URL : { TEXT("http://example.com"), TEXT("https:///missing"), TEXT("https://user@example.com"), TEXT("https://example.com?x=1"), TEXT("https://example.com/#x"), TEXT("https://exam ple.com") })
        TestFalse(TEXT("Invalid token destination rejected"), Entry->ValidApiBase(URL));
    Entry->ClearOperation();
    UI->Deinitialize();
    return true;
}
#endif
