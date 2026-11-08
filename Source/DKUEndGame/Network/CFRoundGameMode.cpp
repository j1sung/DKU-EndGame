#include "Network/CFRoundGameMode.h"
#include "Character/ChickenCharacter.h"
#include "Character/CFWaitingCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Network/CFMatchSessionSubsystem.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFRoundSetup, Log, All);

namespace
{
const FName PreviewTag(TEXT("CFRoundSpawnPreview"));
const FName SpawnTag(TEXT("ArenaSpawn"));
}

ACFRoundGameMode::ACFRoundGameMode()
{
    // 기존 입력 컨텍스트와 Designer 대기 UI를 보유한 Blueprint를 재사용한다.
    static ConstructorHelpers::FClassFinder<ACFWaitingCharacter> Waiting(TEXT("/Game/Characters/WhiteRound/Blueprints/BP_CF_WaitingCharacter"));
    static ConstructorHelpers::FClassFinder<ACFWaitingPlayerController> Controller(TEXT("/Game/UI/WaitingRoom/BP_CF_ArenaController"));
    DefaultPawnClass = Waiting.Class;
    PlayerControllerClass = Controller.Class;
}

void ACFRoundGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    bRoundSetupReady = false;
    if (!ErrorMessage.IsEmpty()) return;
    bAssignedMatch = !UGameplayStatics::ParseOption(Options, TEXT("CFMatch")).IsEmpty();
    if (bAssignedMatch)
    {
        const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
        const auto* Local = GetGameInstance()->GetFirstGamePlayer();
        if (!Bridge || !Local || !Bridge->CanAdmit(Local->GetPreferredUniqueNetId(), Options) ||
            UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) != TEXT("/Game/Map/L_Round1"))
        { ErrorMessage = TEXT("EOS_MATCH_CONTEXT_REQUIRED"); return; }
    }
    const FString Package = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
    bool bRoundMap = false;
    for (int32 Index = 1; Index <= 4; ++Index)
        bRoundMap |= Package == FString::Printf(TEXT("/Game/Map/L_Round%d"), Index);
    if (!bRoundMap || !DefaultPawnClass || !PlayerControllerClass || !CombatPawnClass)
    {
        ErrorMessage = TEXT("ROUND_SETUP_INVALID_MAP_OR_CLASSES"); return;
    }
    int32 SpawnCount = 0;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(SpawnTag)) ++SpawnCount;
    if (SpawnCount != 4) { ErrorMessage = TEXT("ROUND_SETUP_REQUIRES_FOUR_SPAWNS"); return; }

    TArray<AChickenCharacter*> Previews;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        auto* Chicken = Cast<AChickenCharacter>(Actor);
        if (!Chicken && !Actor->ActorHasTag(PreviewTag)) continue;
        // 표식 없는 캐릭터나 이미 소유된 플레이어 Pawn을 추측으로 삭제하지 않는다.
        if (!Chicken || !Actor->ActorHasTag(PreviewTag) || Chicken->GetController() || Actor->GetOwner() || Actor->GetAttachParentActor())
        {
            ErrorMessage = TEXT("ROUND_SETUP_UNEXPECTED_CHARACTER"); return;
        }
        TArray<AActor*> AttachedActors;
        Actor->GetAttachedActors(AttachedActors);
        if (!AttachedActors.IsEmpty()) { ErrorMessage = TEXT("ROUND_SETUP_ATTACHED_PREVIEW"); return; }
        // 초기 삭제의 전파에만 의존하지 않는다. 저장된 표식은 원격 클라이언트가 맵에서 로드하지 않는다.
        if (Actor->bNetLoadOnClient) { ErrorMessage = TEXT("ROUND_SETUP_PREVIEW_CLIENT_LOAD_ENABLED"); return; }
        Previews.Add(Chicken);
    }
    if (Previews.Num() != 0 && Previews.Num() != 4)
    {
        ErrorMessage = TEXT("ROUND_SETUP_INCOMPLETE_PREVIEWS"); return;
    }
    // 맵 에셋은 보존한다. 서버 초기화에서만 명시된 표시용 캐릭터를 제거해 스폰 충돌/자동 소유를 막는다.
    for (auto* Preview : Previews)
        if (!Preview->Destroy()) { ErrorMessage = TEXT("ROUND_SETUP_PREVIEW_CLEANUP_FAILED"); return; }
    bRoundSetupReady = true;
    UE_LOG(LogCFRoundSetup, Display, TEXT("Round map ready: ArenaSpawns=%d PreviewsRemoved=%d"), SpawnCount, Previews.Num());
}

void ACFRoundGameMode::InitGameState()
{
    Super::InitGameState();
    if (auto* GS = GetGameState<ACFWaitingGameState>())
    {
        GS->RoomCapacity = 4; GS->MinimumPlayers = 4; GS->TotalRounds = 4;
    }
}

int32 ACFRoundGameMode::FindFreeSpawn(const TArray<FName>& SortedSpawns, const TSet<FName>& Reserved)
{
    if (SortedSpawns.Num() != 4) return INDEX_NONE;
    TSet<FName> Unique;
    for (FName Name : SortedSpawns)
    {
        if (Name.IsNone() || Unique.Contains(Name)) return INDEX_NONE;
        Unique.Add(Name);
    }
    for (int32 Index = 0; Index < SortedSpawns.Num(); ++Index)
        if (!Reserved.Contains(SortedSpawns[Index])) return Index;
    return INDEX_NONE;
}

AActor* ACFRoundGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (!bRoundSetupReady || !IsValid(Player)) return nullptr;
    const TWeakObjectPtr<AController> Key(Player);
    if (const auto* Assigned = AssignedStarts.Find(Key); Assigned && Assigned->IsValid()) return Assigned->Get();
    TArray<APlayerStart*> Starts;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(SpawnTag)) Starts.Add(*It);
    Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetName() < B.GetName(); });
    TArray<FName> Names;
    for (const auto* Start : Starts) Names.Add(Start->GetFName());
    TSet<FName> Reserved;
    for (auto It = AssignedStarts.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); continue; }
        Reserved.Add(It.Value()->GetFName());
    }
    const int32 Index = FindFreeSpawn(Names, Reserved);
    if (Index == INDEX_NONE) return nullptr;
    AssignedStarts.Add(Key, TWeakObjectPtr<APlayerStart>(Starts[Index]));
    return Starts[Index];
}

void ACFRoundGameMode::Logout(AController* Exiting)
{
    const bool bMemberLeft = AdmittedPlayers.Remove(TWeakObjectPtr<AController>(Exiting)) > 0;
    AssignedStarts.Remove(TWeakObjectPtr<AController>(Exiting));
    Super::Logout(Exiting);
    if (bAssignedMatch && bMemberLeft)
        if (auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>()) Bridge->NotifyPlayerLeft();
}

void ACFRoundGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty() || !bAssignedMatch) return;
    const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
    if (!Bridge || !Bridge->CanAdmit(UniqueId, Options)) { ErrorMessage = TEXT("NOT_ASSIGNED_MEMBER"); return; }
    for (const auto& Existing : AdmittedPlayers)
        if (Existing.IsValid() && Existing->PlayerState && Existing->PlayerState->GetUniqueId() == UniqueId)
        { ErrorMessage = TEXT("DUPLICATE_MEMBER"); return; }
}

void ACFRoundGameMode::PostLogin(APlayerController* NewPlayer)
{
    if (bAssignedMatch)
    {
        const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
        bool bValidMember = Bridge && NewPlayer && NewPlayer->PlayerState &&
            Bridge->CanAdmit(NewPlayer->PlayerState->GetUniqueId(), OptionsString);
        for (const auto& Existing : AdmittedPlayers)
            if (Existing.IsValid() && Existing->PlayerState && NewPlayer && NewPlayer->PlayerState &&
                Existing->PlayerState->GetUniqueId() == NewPlayer->PlayerState->GetUniqueId()) bValidMember = false;
        if (!bValidMember)
        {
            if (GameSession && NewPlayer) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("NOT_ASSIGNED_MEMBER")));
            return;
        }
        AdmittedPlayers.Add(TWeakObjectPtr<AController>(NewPlayer));
    }
    Super::PostLogin(NewPlayer);
}

bool ACFRoundGameMode::TryStartRound(ACFWaitingPlayerController* Requester)
{
    // 기존 Designer 시작 버튼/RPC가 백엔드 준비 승인을 우회하지 못하게 한다.
    if (bAssignedMatch && !bStartingAssignedRound) return false;
    return Super::TryStartRound(Requester);
}

bool ACFRoundGameMode::StartAssignedRound(ACFWaitingPlayerController* Requester)
{
    const auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>();
    if (!bAssignedMatch || !bRoundSetupReady || !Bridge || !Bridge->CanAutoStart()) return false;
    TGuardValue<bool> Starting(bStartingAssignedRound, true);
    UE_LOG(LogCFRoundSetup, Display, TEXT("EOS four-player readiness accepted; starting Round1 countdown."));
    return TryStartRound(Requester);
}

void ACFRoundGameMode::OnRoundPlaying()
{
    Super::OnRoundPlaying();
    const auto* GS = GetGameState<ACFWaitingGameState>();
    if (bAssignedMatch && GS && GS->CurrentRound == 1)
        if (auto* Bridge = GetGameInstance()->GetSubsystem<UCFMatchSessionSubsystem>()) Bridge->NotifyPlaying();
}
