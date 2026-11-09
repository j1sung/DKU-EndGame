#pragma once

#include "CoreMinimal.h"

/** 백엔드의 멤버 전용 GET 응답. mutation 응답을 이 스냅샷 대신 사용하지 않는다. */
struct FCFSessionMatch
{
    FString Id, Status, Host, SessionId;
    TArray<FString> Members;
    int32 Attempt = 0;
    int32 Joined = 0;
    FDateTime Deadline;

    bool Contains(const FString& Player) const { return Members.Contains(Player); }
    bool IsTerminal() const;
    static bool IsPlayerId(const FString& Value);
    static bool Parse(const FString& Body, const FString& ExpectedId, const FString& LocalPlayer, FCFSessionMatch& Out);
};
