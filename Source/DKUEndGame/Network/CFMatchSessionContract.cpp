#include "Network/CFMatchSessionContract.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FCFSessionMatch::IsPlayerId(const FString& Value)
{
    if (Value.Len() != 32) return false;
    for (TCHAR C : Value)
        if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
    return true;
}

bool FCFSessionMatch::IsTerminal() const
{
    return Status == TEXT("FAILED") || Status == TEXT("EXPIRED") || Status == TEXT("FINISHED");
}

bool FCFSessionMatch::Parse(const FString& Body, const FString& ExpectedId, const FString& LocalPlayer, FCFSessionMatch& Out)
{
    TSharedPtr<FJsonObject> Json;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) || !Json) return false;
    FCFSessionMatch Value;
    double AttemptValue = 0, JoinedValue = 0;
    const TArray<TSharedPtr<FJsonValue>>* MembersValue = nullptr;
    if (!Json->TryGetStringField(TEXT("matchId"), Value.Id) || Value.Id != ExpectedId ||
        !Json->TryGetStringField(TEXT("status"), Value.Status) ||
        !Json->TryGetStringField(TEXT("hostPlayerId"), Value.Host) ||
        !Json->TryGetNumberField(TEXT("hostAttempt"), AttemptValue) || AttemptValue != 1 ||
        !Json->TryGetNumberField(TEXT("joinedCount"), JoinedValue) || !FMath::IsFinite(JoinedValue) || JoinedValue < 0 || JoinedValue > 4 || JoinedValue != static_cast<int32>(JoinedValue) ||
        !Json->TryGetArrayField(TEXT("memberIds"), MembersValue) || MembersValue->Num() != 4) return false;
    for (const auto& Item : *MembersValue)
    {
        FString Player;
        if (!Item || !Item->TryGetString(Player) || !IsPlayerId(Player) || Value.Members.Contains(Player)) return false;
        Value.Members.Add(Player);
    }
    if (!Value.Contains(LocalPlayer) || !Value.Contains(Value.Host)) return false;
    Value.Attempt = 1; Value.Joined = static_cast<int32>(JoinedValue);
    Json->TryGetStringField(TEXT("sessionId"), Value.SessionId);
    if (!Value.IsTerminal())
    {
        if (Value.Status != TEXT("FORMING") && Value.Status != TEXT("SESSION_READY") &&
            Value.Status != TEXT("READY_TO_START") && Value.Status != TEXT("IN_GAME")) return false;
        FString DeadlineValue;
        if (!Json->TryGetStringField(TEXT("deadlineAt"), DeadlineValue) || !FDateTime::ParseIso8601(*DeadlineValue, Value.Deadline)) return false;
        if (Value.Status != TEXT("FORMING") && (Value.SessionId.IsEmpty() || Value.SessionId.Len() > 128)) return false;
        for (TCHAR C : Value.SessionId) if (C < 32 || C == 127) return false;
        if ((Value.Status == TEXT("READY_TO_START") || Value.Status == TEXT("IN_GAME")) && Value.Joined != 4) return false;
    }
    Out = MoveTemp(Value);
    return true;
}
