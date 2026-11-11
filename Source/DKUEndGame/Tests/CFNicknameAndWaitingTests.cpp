#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Character/CFPlayerPresentationComponent.h"
#include "Network/CFWaitingNetwork.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFNicknameAndWaitingTest, "ChickenFight.MatchRounds.NicknameAndWaiting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCFNicknameAndWaitingTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Local profile replaces Epic display name"),
        UCFPlayerPresentationComponent::SelectDisplayNickname(TEXT("EpicAccount"), TEXT("샤피한"), true), FString(TEXT("샤피한")));
    TestEqual(TEXT("Profile lookup never silently falls back to Epic"),
        UCFPlayerPresentationComponent::SelectDisplayNickname(TEXT("EpicAccount"), FString(), true), FString());
    TestEqual(TEXT("Remote name stays authoritative replicated name"),
        UCFPlayerPresentationComponent::SelectDisplayNickname(TEXT("호크"), TEXT("샤피한"), false), FString(TEXT("호크")));
    TestTrue(TEXT("LAN waiting remains available"), ACFWaitingPlayerController::ShouldShowLegacyWaiting(false, false));
    TestFalse(TEXT("EOS session blocks waiting before GameState replication"), ACFWaitingPlayerController::ShouldShowLegacyWaiting(true, false));
    TestFalse(TEXT("Automatic GameState blocks waiting after session retirement"), ACFWaitingPlayerController::ShouldShowLegacyWaiting(false, true));
    TestFalse(TEXT("EOS automatic match never shows legacy waiting"), ACFWaitingPlayerController::ShouldShowLegacyWaiting(true, true));
    return true;
}
#endif
