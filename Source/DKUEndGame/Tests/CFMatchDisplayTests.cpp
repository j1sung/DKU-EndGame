#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFWaitingNetwork.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchDisplayOwnershipTest, "ChickenFight.MatchRounds.DisplayOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFMatchDisplayOwnershipTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient test world"), World)) return false;
    auto* Previous = World->SpawnActor<ACFWaitingPlayerController>();
    auto* Current = World->SpawnActor<ACFWaitingPlayerController>();
    if (!Previous || !Current) { World->DestroyWorld(false); return false; }
    Previous->SetAsLocalPlayerController(); Current->SetAsLocalPlayerController();
    TestFalse(TEXT("Local controller without attached player must defer HUD"), Previous->CanUpdateMatchDisplay());
    TStrongObjectPtr<ULocalPlayer> Local(NewObject<ULocalPlayer>(GEngine));
    Previous->Player = Local.Get(); Local->PlayerController = Previous;
    TestTrue(TEXT("Attached current controller may update HUD"), Previous->CanUpdateMatchDisplay());
    Current->Player = Local.Get(); Local->PlayerController = Current;
    TestFalse(TEXT("Retired controller cannot update transferred player's HUD"), Previous->CanUpdateMatchDisplay());
    TestTrue(TEXT("Replacement controller may update HUD"), Current->CanUpdateMatchDisplay());
    Current->Player = nullptr;
    TestFalse(TEXT("Transient detach during travel must defer HUD"), Current->CanUpdateMatchDisplay());
    Current->Player = Local.Get();
    TestTrue(TEXT("HUD resumes after reattachment"), Current->CanUpdateMatchDisplay());
    World->bIsTearingDown = true;
    TestFalse(TEXT("World teardown must defer HUD"), Current->CanUpdateMatchDisplay());
    World->bIsTearingDown = false;
    Previous->Player = nullptr; Current->Player = nullptr; Local->PlayerController = nullptr;
    World->DestroyWorld(false);
    return true;
}
#endif
