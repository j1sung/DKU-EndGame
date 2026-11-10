#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFRoundGameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFRoundSpawnReservationTest, "ChickenFight.RoundSetup.SpawnReservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCFRoundSpawnReservationTest::RunTest(const FString& Parameters)
{
    const TArray<FName> Starts = { TEXT("Start1"), TEXT("Start2"), TEXT("Start3"), TEXT("Start4") };
    TSet<FName> Reserved;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TestEqual(TEXT("Each connection gets a distinct slot"), ACFRoundGameMode::FindFreeSpawn(Starts, Reserved), Index);
        Reserved.Add(Starts[Index]);
    }
    TestEqual(TEXT("Fifth connection has no slot"), ACFRoundGameMode::FindFreeSpawn(Starts, Reserved), INDEX_NONE);
    Reserved.Remove(Starts[1]);
    TestEqual(TEXT("Disconnected slot is reusable"), ACFRoundGameMode::FindFreeSpawn(Starts, Reserved), 1);
    TestEqual(TEXT("Three spawns are rejected"), ACFRoundGameMode::FindFreeSpawn({ TEXT("A"), TEXT("B"), TEXT("C") }, {}), INDEX_NONE);
    TestEqual(TEXT("Duplicate spawns are rejected"), ACFRoundGameMode::FindFreeSpawn({ TEXT("A"), TEXT("A"), TEXT("C"), TEXT("D") }, {}), INDEX_NONE);
    return true;
}
#endif
