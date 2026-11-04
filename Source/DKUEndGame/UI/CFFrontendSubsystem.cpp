#include "UI/CFFrontendSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
FText Message(const TCHAR* Text) { return FText::FromString(Text); }
FText OrDefault(const FText& Error, const TCHAR* Default) { return Error.IsEmpty() ? Message(Default) : Error; }
}

void UCFFrontendSubsystem::ClearDeadline()
{
    if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(RequestDeadline);
}

void UCFFrontendSubsystem::Deinitialize()
{
    ClearDeadline(); ActiveRequestId = 0;
    Super::Deinitialize();
}

void UCFFrontendSubsystem::SetScreen(ECFFrontendScreen Next, const FText& Error)
{
    Screen = Next; ErrorMessage = Error; OnStateChanged.Broadcast();
}

int32 UCFFrontendSubsystem::BeginRequest(ECFFrontendScreen Next, float TimeoutSeconds)
{
    ClearDeadline();
    ActiveRequestId = ++RequestSerial;
    const int32 Id = ActiveRequestId;
    GetGameInstance()->GetTimerManager().SetTimer(RequestDeadline, this, &ThisClass::HandleTimeout, TimeoutSeconds, false);
    SetScreen(Next);
    return Id;
}

bool UCFFrontendSubsystem::AcceptResponse(int32 RequestId, ECFFrontendScreen Expected)
{
    // 취소·타임아웃·새 요청 이후 도착한 과거 응답은 화면과 프로필을 바꾸지 못한다.
    if (RequestId <= 0 || ActiveRequestId != RequestId || Screen != Expected) return false;
    ClearDeadline(); ActiveRequestId = 0;
    return true;
}

void UCFFrontendSubsystem::OpenEntry()
{
    if (Screen != ECFFrontendScreen::Title) return;
    if (!bAuthenticated) SetScreen(ECFFrontendScreen::Login);
    else if (ProfileNickname.IsEmpty()) SetScreen(ECFFrontendScreen::Nickname);
    else EnterLobby();
}

void UCFFrontendSubsystem::RequestLogin()
{
    if (Screen != ECFFrontendScreen::Login) return;
    if (!OnLoginRequested.IsBound())
    {
        SetScreen(Screen, Message(TEXT("현재 로그인할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    const int32 Id = BeginRequest(ECFFrontendScreen::LoggingIn, 120.f);
    if (ActiveRequestId == Id && Screen == ECFFrontendScreen::LoggingIn) OnLoginRequested.Broadcast(Id);
}

void UCFFrontendSubsystem::CompleteLogin(int32 RequestId, bool bSuccess, const FString& RegisteredNickname, const FText& Error)
{
    if (!AcceptResponse(RequestId, ECFFrontendScreen::LoggingIn)) return;
    if (!bSuccess) { SetScreen(ECFFrontendScreen::Login, OrDefault(Error, TEXT("로그인하지 못했습니다. 다시 시도해 주세요."))); return; }
    bAuthenticated = true; ProfileNickname = RegisteredNickname;
    if (ProfileNickname.IsEmpty()) SetScreen(ECFFrontendScreen::Nickname);
    else EnterLobby();
}

bool UCFFrontendSubsystem::ValidateNickname(const FString& Input, FString& Normalized)
{
    Normalized.Reset();
    // 허용 문자에 속하지 않는 줄바꿈·탭도 거절한다. 서버의 최종 정규화/중복 검사가 우선이다.
    for (TCHAR Ch : Input)
    {
        if (!((Ch >= TEXT('가') && Ch <= TEXT('힣')) || (Ch >= 'a' && Ch <= 'z') ||
              (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == ' ')) return false;
        if (Ch != ' ' || (!Normalized.IsEmpty() && !Normalized.EndsWith(TEXT(" ")))) Normalized.AppendChar(Ch);
    }
    Normalized.TrimEndInline();
    return Normalized.Len() >= 2 && Normalized.Len() <= 16;
}

void UCFFrontendSubsystem::RequestNickname(const FString& Nickname)
{
    if (!bAuthenticated || Screen != ECFFrontendScreen::Nickname) return;
    FString Normalized;
    if (!ValidateNickname(Nickname, Normalized))
    {
        SetScreen(Screen, Message(TEXT("2~16자의 한글, 영문, 숫자와 공백으로 입력해 주세요."))); return;
    }
    if (!OnNicknameRequested.IsBound())
    {
        SetScreen(Screen, Message(TEXT("현재 닉네임을 등록할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    const int32 Id = BeginRequest(ECFFrontendScreen::SavingNickname, 30.f);
    if (ActiveRequestId == Id && Screen == ECFFrontendScreen::SavingNickname) OnNicknameRequested.Broadcast(Id, Normalized);
}

void UCFFrontendSubsystem::CompleteNickname(int32 RequestId, bool bSuccess, const FString& RegisteredNickname, const FText& Error)
{
    if (!AcceptResponse(RequestId, ECFFrontendScreen::SavingNickname)) return;
    if (!bSuccess || RegisteredNickname.IsEmpty())
    {
        SetScreen(ECFFrontendScreen::Nickname, OrDefault(Error, TEXT("닉네임을 등록하지 못했습니다. 다시 시도해 주세요."))); return;
    }
    ProfileNickname = RegisteredNickname;
    EnterLobby();
}

void UCFFrontendSubsystem::EnterLobby()
{
    if (!bAuthenticated || ProfileNickname.IsEmpty()) return;
    if (!OnLobbyRequested.IsBound())
    {
        SetScreen(ECFFrontendScreen::Title, Message(TEXT("현재 로비에 접속할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    const int32 Id = BeginRequest(ECFFrontendScreen::Connecting, 60.f);
    if (ActiveRequestId == Id && Screen == ECFFrontendScreen::Connecting) OnLobbyRequested.Broadcast(Id);
}

void UCFFrontendSubsystem::CompleteLobbyEntry(int32 RequestId, bool bSuccess, const FText& Error)
{
    if (!AcceptResponse(RequestId, ECFFrontendScreen::Connecting)) return;
    // 실제 로비 이동은 통합 계층의 책임이다. 다음 타이틀 진입에 이전 로딩 상태를 남기지 않는다.
    SetScreen(ECFFrontendScreen::Title, bSuccess ? FText::GetEmpty() : OrDefault(Error, TEXT("로비에 접속하지 못했습니다. 다시 시도해 주세요.")));
}

void UCFFrontendSubsystem::ReturnToTitle()
{
    const int32 CancelledId = ActiveRequestId;
    ClearDeadline(); ActiveRequestId = 0;
    SetScreen(ECFFrontendScreen::Title);
    if (CancelledId > 0) OnRequestCancelled.Broadcast(CancelledId);
}

void UCFFrontendSubsystem::InvalidateProfile()
{
    bAuthenticated = false; ProfileNickname.Reset(); ReturnToTitle();
}

void UCFFrontendSubsystem::ClearError()
{
    if (!ErrorMessage.IsEmpty()) { ErrorMessage = FText::GetEmpty(); OnStateChanged.Broadcast(); }
}

void UCFFrontendSubsystem::HandleTimeout()
{
    const int32 ExpiredId = ActiveRequestId;
    if (ExpiredId <= 0) return;
    ActiveRequestId = 0;
    const ECFFrontendScreen RetryScreen = Screen == ECFFrontendScreen::LoggingIn ? ECFFrontendScreen::Login :
        Screen == ECFFrontendScreen::SavingNickname ? ECFFrontendScreen::Nickname : ECFFrontendScreen::Title;
    SetScreen(RetryScreen, Message(TEXT("응답 시간이 초과되었습니다. 연결을 확인하고 다시 시도해 주세요.")));
    OnRequestCancelled.Broadcast(ExpiredId);
}
