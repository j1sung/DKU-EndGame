#pragma once

#include "CoreMinimal.h"

enum class ECFMatchRequest : uint8 { None, Create, Current, Cancel };
enum class ECFMatchUpdate : uint8 { None, Waiting, Assigned, Idle, Uncertain, AuthenticationRequired };

struct FCFMatchUpdate
{
    ECFMatchUpdate State = ECFMatchUpdate::None;
    ECFMatchRequest Next = ECFMatchRequest::None;
    float Delay = 0.f;
    int32 RemainingSeconds = 0;
    bool bNewAssignment = false;
    FString Message;
};

/** HTTP/UObject와 분리된 티켓 흐름. 쓰기 결과가 불명확하면 같은 키·본문을 유지한다. */
class FCFMatchmakingFlow
{
public:
    void Begin(int32 Id, const FString& PlayerId, bool bCreate);
    void WantCancel();
    void MarkWriteUncertain() { bWriteUncertain = true; }
    ECFMatchRequest RecoveryRequest();
    FCFMatchUpdate Receive(ECFMatchRequest Request, int32 Status, const FString& Body,
        const FDateTime& ServerNow, float RetryAfter = 0.f);
    FString BodyFor(ECFMatchRequest Request) const;
    FString KeyFor(ECFMatchRequest Request) const;
    int32 Id = 0;
    FString PlayerId;
    FString TicketId;
    FString MatchId;
    bool IsAssigned() const { return !MatchId.IsEmpty(); }

private:
    FString CreateKey;
    FString CancelKey;
    FString CancelTicket;
    bool bCreatePending = false;
    bool bCancelPending = false;
    bool bWantCancel = false;
    bool bWriteUncertain = false;
    int32 Failures = 0;
    FCFMatchUpdate Failure(ECFMatchRequest Request, float RetryAfter, const TCHAR* Message);
};
