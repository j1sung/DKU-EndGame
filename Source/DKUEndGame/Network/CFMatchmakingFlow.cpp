#include "Network/CFMatchmakingFlow.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool IsId(const FString& Value, bool bTicket = false)
{
    FGuid Guid;
    return Value.Len() == 36 && FGuid::ParseExact(Value, EGuidFormats::DigitsWithHyphens, Guid) &&
        Guid.IsValid() && (!bTicket || (Value[14] == '7' && (Value[19] == '8' || Value[19] == '9' ||
            Value[19] == 'a' || Value[19] == 'A' || Value[19] == 'b' || Value[19] == 'B')));
}
FCFMatchUpdate Next(ECFMatchRequest Request, float Delay = 0.f)
{
    FCFMatchUpdate Update;
    Update.Next = Request; Update.Delay = Delay;
    return Update;
}
}

void FCFMatchmakingFlow::Begin(int32 InId, const FString& InPlayerId, bool bCreate)
{
    *this = FCFMatchmakingFlow();
    Id = InId; PlayerId = InPlayerId; bCreatePending = bCreate;
    if (bCreate) CreateKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

void FCFMatchmakingFlow::WantCancel()
{
    if (!IsAssigned()) bWantCancel = true;
}

ECFMatchRequest FCFMatchmakingFlow::RecoveryRequest()
{
    if (bCreatePending) return ECFMatchRequest::Create;
    if (IsAssigned()) return ECFMatchRequest::Current;
    if (bCancelPending) return ECFMatchRequest::Cancel;
    if (bWantCancel && !TicketId.IsEmpty())
    {
        CancelTicket = TicketId;
        CancelKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        bCancelPending = true;
        return ECFMatchRequest::Cancel;
    }
    return ECFMatchRequest::Current;
}

FString FCFMatchmakingFlow::BodyFor(ECFMatchRequest Request) const
{
    if (Request == ECFMatchRequest::Create) return TEXT("{\"mode\":\"standard\"}");
    if (Request == ECFMatchRequest::Cancel) return FString::Printf(TEXT("{\"ticketId\":\"%s\"}"), *CancelTicket);
    return FString();
}

FString FCFMatchmakingFlow::KeyFor(ECFMatchRequest Request) const
{
    return Request == ECFMatchRequest::Create ? CreateKey : Request == ECFMatchRequest::Cancel ? CancelKey : FString();
}

FCFMatchUpdate FCFMatchmakingFlow::Failure(ECFMatchRequest Request, float RetryAfter, const TCHAR* Message)
{
    if (Request != ECFMatchRequest::Current) bWriteUncertain = true;
    FCFMatchUpdate Update;
    Update.State = ECFMatchUpdate::Uncertain; Update.Message = Message;
    ++Failures;
    Update.Delay = FMath::Max(RetryAfter, FMath::Min(1.f + 2.f * Failures, 5.f));
    if (Failures < 3) Update.Next = Request;
    return Update;
}

FCFMatchUpdate FCFMatchmakingFlow::Receive(ECFMatchRequest Request, int32 Status, const FString& Body,
    const FDateTime& ServerNow, float RetryAfter)
{
    if (Status == 401)
    {
        FCFMatchUpdate Update;
        Update.State = ECFMatchUpdate::AuthenticationRequired;
        return Update;
    }
    if (Status == 0 || Status == 429 || Status >= 500)
        return Failure(Request, RetryAfter, TEXT("서버 응답을 확인하지 못했습니다. 매칭 상태를 다시 확인해 주세요."));

    TSharedPtr<FJsonObject> Json;
    const bool bJson = Body.Len() <= 65536 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) && Json.IsValid();
    if (Status != (Request == ECFMatchRequest::Create ? 201 : 200))
    {
        FString Code;
        const TSharedPtr<FJsonObject>* Error = nullptr;
        if (bJson && Json->TryGetObjectField(TEXT("error"), Error) && Error && Error->IsValid())
            (*Error)->TryGetStringField(TEXT("code"), Code);
        if (Request == ECFMatchRequest::Create && Status == 409 && Code == TEXT("ALREADY_ACTIVE"))
        {
            bCreatePending = bWriteUncertain = false; Failures = 0;
            return Next(ECFMatchRequest::Current, RetryAfter);
        }
        if (Request == ECFMatchRequest::Cancel &&
            ((Status == 404 && Code == TEXT("TICKET_NOT_FOUND")) ||
             (Status == 409 && (Code == TEXT("TICKET_ALREADY_ASSIGNED") || Code == TEXT("TICKET_MISMATCH")))))
        {
            bCancelPending = bWriteUncertain = false; Failures = 0;
            return Next(ECFMatchRequest::Current, RetryAfter);
        }
        if (Request == ECFMatchRequest::Create && !bWriteUncertain &&
            ((Status == 409 && Code == TEXT("NICKNAME_REQUIRED")) ||
             (Status == 426 && Code == TEXT("CLIENT_VERSION_UNSUPPORTED"))))
        {
            FCFMatchUpdate Update;
            Update.State = Status == 426 ? ECFMatchUpdate::Idle : ECFMatchUpdate::AuthenticationRequired;
            Update.Message = Status == 426 ? TEXT("게임을 최신 버전으로 업데이트한 뒤 다시 시도해 주세요.") : TEXT("닉네임 등록이 필요합니다. 타이틀로 돌아가 다시 로그인해 주세요.");
            return Update;
        }
        return Failure(Request, RetryAfter, TEXT("매칭 요청을 확인하지 못했습니다. 연결 상태를 확인하고 다시 시도해 주세요."));
    }
    if (!bJson) return Failure(Request, RetryAfter, TEXT("매칭 응답을 확인하지 못했습니다. 현재 상태를 다시 확인해 주세요."));

    if (Request == ECFMatchRequest::Current)
    {
        const TSharedPtr<FJsonValue> Empty = Json->TryGetField(TEXT("ticket"));
        if (Empty && Empty->IsNull())
        {
            if (Json->HasField(TEXT("status")) || Json->HasField(TEXT("ticketId")))
                return Failure(Request, RetryAfter, TEXT("매칭 응답이 올바르지 않습니다. 다시 확인해 주세요."));
            FCFMatchUpdate Update;
            Update.State = ECFMatchUpdate::Idle;
            Update.Message = IsAssigned() ? TEXT("매치가 종료되었거나 준비 시간이 만료되었습니다. 다시 매칭할 수 있습니다.") :
                bWantCancel ? TEXT("매칭이 취소되었습니다.") : TicketId.IsEmpty() ? TEXT("") : TEXT("매칭 대기가 끝났습니다. 다시 시작해 주세요.");
            return Update;
        }
    }
    FString ReceivedTicket, TicketStatus;
    if (!Json->TryGetStringField(TEXT("ticketId"), ReceivedTicket) || !IsId(ReceivedTicket, true) ||
        !Json->TryGetStringField(TEXT("status"), TicketStatus))
        return Failure(Request, RetryAfter, TEXT("티켓 응답을 확인하지 못했습니다. 다시 확인해 주세요."));

    if (Request == ECFMatchRequest::Cancel)
    {
        if (TicketStatus != TEXT("CANCELLED") || ReceivedTicket != CancelTicket)
            return Failure(Request, RetryAfter, TEXT("매칭 취소를 확인하지 못했습니다. 다시 확인해 주세요."));
        bCancelPending = bWriteUncertain = false; Failures = 0;
        return Next(ECFMatchRequest::Current, RetryAfter);
    }
    if (TicketStatus == TEXT("ASSIGNED") && Request == ECFMatchRequest::Current)
    {
        FString ReceivedMatch;
        if (!Json->TryGetStringField(TEXT("matchId"), ReceivedMatch) || !IsId(ReceivedMatch) ||
            (IsAssigned() && (MatchId != ReceivedMatch || TicketId != ReceivedTicket)))
            return Failure(Request, RetryAfter, TEXT("배정 정보를 확인하지 못했습니다. 다시 확인해 주세요."));
        FCFMatchUpdate Update = Next(ECFMatchRequest::Current, FMath::Max(2.f, RetryAfter));
        Update.State = ECFMatchUpdate::Assigned; Update.bNewAssignment = !IsAssigned();
        TicketId = ReceivedTicket; MatchId = ReceivedMatch;
        bWantCancel = bCancelPending = bWriteUncertain = false; Failures = 0;
        return Update;
    }
    FString Expiry;
    FDateTime Deadline;
    if (TicketStatus != TEXT("WAITING") || IsAssigned() ||
        !Json->TryGetStringField(TEXT("expiresAt"), Expiry) || !Expiry.EndsWith(TEXT("Z")) ||
        !FDateTime::ParseIso8601(*Expiry, Deadline))
        return Failure(Request, RetryAfter, TEXT("대기 정보를 확인하지 못했습니다. 다시 확인해 주세요."));

    TicketId = ReceivedTicket; Failures = 0;
    if (Request == ECFMatchRequest::Create)
    {
        // 생성 멱등 응답은 과거 WAITING일 수 있다. 표시/취소 전에 현재 상태를 확인한다.
        bCreatePending = bWriteUncertain = false;
        return Next(ECFMatchRequest::Current, RetryAfter);
    }
    if (bCancelPending && CancelTicket != TicketId) bCancelPending = false;
    FCFMatchUpdate Update = Next(ECFMatchRequest::Current, FMath::Max(2.f, RetryAfter));
    Update.State = ECFMatchUpdate::Waiting;
    Update.RemainingSeconds = FMath::CeilToInt(FMath::Clamp((Deadline - ServerNow).GetTotalSeconds(), 0.0, 180.0));
    if (bWantCancel) { Update.Next = RecoveryRequest(); Update.Delay = RetryAfter; }
    return Update;
}
