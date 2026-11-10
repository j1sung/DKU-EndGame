#include "Network/CFMatchRuntimeSubsystem.h"
#include "Network/CFMatchSessionSubsystem.h"
#include "UI/CFCompletedMatchWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFMatchRounds, Log, All);

void UCFMatchRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    MapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::MapLoaded);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), .25f);
}
void UCFMatchRuntimeSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
    if (ResultWidget) ResultWidget->RemoveFromParent();
    ResultWidget = nullptr;
    Super::Deinitialize();
}
TArray<int32> UCFMatchRuntimeSubsystem::ShuffleFour(int32 Seed)
{
    TArray<int32> Values{1, 2, 3, 4};
    FRandomStream Random(Seed);
    for (int32 I = 3; I > 0; --I) Values.Swap(I, Random.RandRange(0, I));
    return Values;
}
int32 UCFMatchRuntimeSubsystem::MapNumber(const FString& Package)
{
    for (int32 I = 1; I <= 4; ++I) if (Package == MapPath(I)) return I;
    return 0;
}
FString UCFMatchRuntimeSubsystem::MapPath(int32 Number)
{
    return Number >= 1 && Number <= 4 ? FString::Printf(TEXT("/Game/Map/L_Round%d"), Number) : FString();
}
FLinearColor UCFMatchRuntimeSubsystem::PlayerColour(int32 Slot)
{
    const FColor Colours[]{FColor(0, 140, 235), FColor(245, 125, 25), FColor(200, 65, 170), FColor(35, 180, 125)};
    return Slot >= 0 && Slot < 4 ? FLinearColor::FromSRGBColor(Colours[Slot]) : FLinearColor::White;
}
void UCFMatchRuntimeSubsystem::BeginMatch(const TArray<FString>& Members)
{
    MapOrder = ShuffleFour(FMath::Rand());
    const TArray<int32> Slots = ShuffleFour(FMath::Rand());
    Colours.Reset(); PlayerIds.Reset(); PlayerNames.Reset(); Scores.Reset(); ScoredRound = 0;
    for (int32 I = 0; I < Members.Num() && I < 4; ++I) Colours.Add(Members[I], Slots[I] - 1);
    RoundNumber = 1; bTravelPending = false;
    UE_LOG(LogCFMatchRounds, Display, TEXT("Match plan: maps=%d,%d,%d,%d colours=%d"), MapOrder[0], MapOrder[1], MapOrder[2], MapOrder[3], Colours.Num());
    FinalRows.Reset(); bResultConfirmed = true; bResultsSuspended = false;
    if (ResultWidget) ResultWidget->RemoveFromParent();
    ResultWidget = nullptr;
}
FString UCFMatchRuntimeSubsystem::CurrentMap() const
{
    return MapOrder.IsValidIndex(RoundNumber - 1) ? MapPath(MapOrder[RoundNumber - 1]) : FString();
}
int32 UCFMatchRuntimeSubsystem::ColourFor(const FString& Player) const
{
    const int32* Found = Colours.Find(Player);
    return Found ? *Found : INDEX_NONE;
}
bool UCFMatchRuntimeSubsystem::TravelToNext(ACFWaitingGameState* State)
{
    UWorld* World = GetWorld();
    const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
    if (!World || !State || !Bridge || !Bridge->IsActive() || World->GetNetMode() != NM_ListenServer ||
        bTravelPending || State->Phase != ECFMatchPhase::RoundResult || State->CurrentRound != RoundNumber ||
        State->ScoredRound != RoundNumber || RoundNumber >= 4 || State->Scoreboard.Num() != 4) return false;
    Scores = State->Scoreboard; ScoredRound = State->ScoredRound;
    PlayerIds.Reset(); PlayerNames.Reset();
    for (APlayerState* Player : State->PlayerArray)
        if (Player && Player->GetUniqueId().IsValid())
        {
            const FString Id = UCFMatchSessionSubsystem::ProductUserId(*Player->GetUniqueId());
            PlayerIds.Add(Id, Player->GetPlayerId()); PlayerNames.Add(Id, Player->GetPlayerName());
        }
    ++RoundNumber; bTravelPending = true; TravelDeadline = FPlatformTime::Seconds() + 60;
    const FString URL = CurrentMap() + Bridge->TravelOptions();
    UE_LOG(LogCFMatchRounds, Display, TEXT("Travelling: round=%d map=%s scores=%d"), RoundNumber, *CurrentMap(), Scores.Num());
    if (!World->ServerTravel(URL, true)) { bTravelPending = false; return false; }
    return true;
}
void UCFMatchRuntimeSubsystem::Restore(ACFWaitingGameState* State) const
{
    if (!State || !bTravelPending) return;
    State->Scoreboard = Scores; State->ScoredRound = ScoredRound;
    State->CurrentRound = RoundNumber;
}
void UCFMatchRuntimeSubsystem::RestoreIdentity(APlayerState* State) const
{
    if (!State || !State->GetUniqueId().IsValid()) return;
    const FString Player = UCFMatchSessionSubsystem::ProductUserId(*State->GetUniqueId());
    if (const int32* Id = PlayerIds.Find(Player))
    { State->SetPlayerId(*Id); State->ForceNetUpdate(); }
    if (const FString* Name = PlayerNames.Find(Player)) { State->SetPlayerName(*Name); State->ForceNetUpdate(); }
}
bool UCFMatchRuntimeSubsystem::TravelTimedOut() const { return bTravelPending && FPlatformTime::Seconds() > TravelDeadline; }
bool UCFMatchRuntimeSubsystem::CacheResults(const TArray<FCFFinalStanding>& Rows)
{
    if (Rows.Num() != 4) return false;
    TSet<int32> Ids;
    for (const auto& Row : Rows)
    {
        if (Row.PlayerId < 0 || Ids.Contains(Row.PlayerId) || Row.PlayerName.IsEmpty() || Row.Rank < 1 || Row.Rank > 4 || Row.TotalScore < 0) return false;
        Ids.Add(Row.PlayerId);
    }
    FinalRows = Rows; bResultConfirmed = false; bResultsSuspended = false;
    return true;
}
bool UCFMatchRuntimeSubsystem::CanConfirmResults() const
{
    const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
    return HasResults() && Bridge && !Bridge->IsActive();
}
void UCFMatchRuntimeSubsystem::ConfirmResults()
{
    if (!CanConfirmResults()) return;
    bResultConfirmed = true;
    if (ResultWidget) ResultWidget->RemoveFromParent();
    ResultWidget = nullptr;
    if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController())
    { PC->SetInputMode(FInputModeGameAndUI()); PC->bShowMouseCursor = true; PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput(); }
}
void UCFMatchRuntimeSubsystem::MapLoaded(UWorld* World)
{
    if (!World || World->GetGameInstance() != GetGameInstance()) return;
    if (ResultWidget) ResultWidget->RemoveFromParent();
    ResultWidget = nullptr;
}
void UCFMatchRuntimeSubsystem::SuspendResultsForLogin()
{
    bResultsSuspended = true;
    if (ResultWidget) ResultWidget->RemoveFromParent();
    ResultWidget = nullptr;
}
bool UCFMatchRuntimeSubsystem::Tick(float Delta)
{
    if (bResultsSuspended)
    {
        const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
        if (Bridge && Bridge->IsActive()) return true;
        bResultsSuspended = false;
    }
    if (HasResults() && GetWorld() && GetWorld()->HasBegunPlay())
    {
        if (!ResultWidget)
        {
            ResultWidget = CreateWidget<UCFCompletedMatchWidget>(GetGameInstance(), UCFCompletedMatchWidget::StaticClass());
            if (ResultWidget) ResultWidget->AddToViewport(1000);
        }
        if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController())
        { PC->SetInputMode(FInputModeUIOnly()); PC->bShowMouseCursor = true; }
    }
    return true;
}
