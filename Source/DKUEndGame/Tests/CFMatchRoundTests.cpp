#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Network/CFMatchRuntimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchRoundPlanTest, "ChickenFight.MatchRounds.Plan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCFMatchRoundPlanTest::RunTest(const FString& Parameters)
{
    TSet<FString> Orders;
    for (int32 Seed = 0; Seed < 64; ++Seed)
    {
        const TArray<int32> Order = UCFMatchRuntimeSubsystem::ShuffleFour(Seed);
        TSet<int32> Seen;
        FString Key;
        for (int32 Map : Order) { Seen.Add(Map); Key += FString::FromInt(Map); }
        TestEqual(TEXT("Four maps once"), Seen.Num(), 4);
        for (int32 Map = 1; Map <= 4; ++Map) TestTrue(TEXT("Every allowed map"), Seen.Contains(Map));
        Orders.Add(Key);
    }
    TestTrue(TEXT("Different match orders"), Orders.Num() > 1);
    TestEqual(TEXT("Round map whitelist"), UCFMatchRuntimeSubsystem::MapNumber(TEXT("/Game/Map/L_Round4")), 4);
    TestEqual(TEXT("Reject external map"), UCFMatchRuntimeSubsystem::MapNumber(TEXT("/Game/Map/L_Round4?listen")), 0);
    TestEqual(TEXT("Reject practice map"), UCFMatchRuntimeSubsystem::MapNumber(TEXT("/Game/Map/L_Lobby_Playground")), 0);
    TestTrue(TEXT("Different identity colours"), UCFMatchRuntimeSubsystem::PlayerColour(0) != UCFMatchRuntimeSubsystem::PlayerColour(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCFMatchResultCacheTest, "ChickenFight.MatchRounds.ResultCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCFMatchResultCacheTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    auto* Runtime = NewObject<UCFMatchRuntimeSubsystem>(Instance.Get());
    Runtime->BeginMatch({TEXT("a"), TEXT("b"), TEXT("c"), TEXT("d")});
    TSet<int32> Colours;
    for (const FString Player : {FString(TEXT("a")), FString(TEXT("b")), FString(TEXT("c")), FString(TEXT("d"))}) Colours.Add(Runtime->ColourFor(Player));
    TestEqual(TEXT("Four distinct colour assignments"), Colours.Num(), 4);
    TestEqual(TEXT("Unknown player has no colour"), Runtime->ColourFor(TEXT("foreign")), INDEX_NONE);
    TArray<FCFFinalStanding> Rows;
    for (int32 I = 0; I < 4; ++I)
    {
        auto& Row = Rows.AddDefaulted_GetRef(); Row.PlayerId = I; Row.Rank = I + 1;
        Row.PlayerName = FString::Printf(TEXT("플레이어%d"), I); Row.TotalScore = 20 - I;
    }
    TestTrue(TEXT("Four-result copy accepted"), Runtime->CacheResults(Rows));
    Rows[0].PlayerName = TEXT("변경된 원본");
    TestFalse(TEXT("Local copy independent of original state"), Runtime->Results()[0].PlayerName == Rows[0].PlayerName);
    Rows[1].PlayerId = Rows[0].PlayerId;
    TestFalse(TEXT("Duplicate final player rejected"), Runtime->CacheResults(Rows));
    TestTrue(TEXT("Previous results survive malformed update"), Runtime->HasResults());
    TestFalse(TEXT("Cannot confirm while session ownership is unresolved"), Runtime->CanConfirmResults());
    Runtime->BeginMatch({TEXT("a"), TEXT("b"), TEXT("c"), TEXT("d")});
    TestFalse(TEXT("Next match clears old result state"), Runtime->HasResults());
    return true;
}
#endif
