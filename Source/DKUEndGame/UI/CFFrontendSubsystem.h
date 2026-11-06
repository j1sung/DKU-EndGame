#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CFFrontendSubsystem.generated.h"

UENUM(BlueprintType)
enum class ECFFrontendScreen : uint8 { Title, Login, LoggingIn, Nickname, SavingNickname, Connecting };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCFFrontendChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCFFrontendRequest, int32, RequestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCFNicknameRequest, int32, RequestId, const FString&, Nickname);

/** UI 상태와 통합 담당의 비동기 작업 사이 연결점. 인증/API/세션을 직접 구현하지 않는다. */
UCLASS()
class DKUENDGAME_API UCFFrontendSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    UPROPERTY(BlueprintReadOnly, Category="CF|Frontend") ECFFrontendScreen Screen = ECFFrontendScreen::Title;
    UPROPERTY(BlueprintReadOnly, Category="CF|Frontend") FText ErrorMessage;
    UPROPERTY(BlueprintReadOnly, Category="CF|Frontend") FString ProfileNickname;
    UPROPERTY(BlueprintReadOnly, Category="CF|Frontend") int32 ActiveRequestId = 0;
    UPROPERTY(BlueprintAssignable, Category="CF|Frontend") FCFFrontendChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable, Category="CF|Frontend") FCFFrontendRequest OnLoginRequested;
    UPROPERTY(BlueprintAssignable, Category="CF|Frontend") FCFNicknameRequest OnNicknameRequested;
    UPROPERTY(BlueprintAssignable, Category="CF|Frontend") FCFFrontendRequest OnLobbyRequested;
    UPROPERTY(BlueprintAssignable, Category="CF|Frontend") FCFFrontendRequest OnRequestCancelled;

    UFUNCTION(BlueprintCallable, Category="CF|Frontend") void OpenEntry();
    UFUNCTION(BlueprintCallable, Category="CF|Frontend") void RequestLogin();
    UFUNCTION(BlueprintCallable, Category="CF|Frontend") void RequestNickname(const FString& Nickname);
    UFUNCTION(BlueprintCallable, Category="CF|Frontend") void ReturnToTitle();
    UFUNCTION(BlueprintCallable, Category="CF|Frontend") void ClearError();
    bool HasAuthenticatedProfile() const { return bAuthenticated && !ProfileNickname.IsEmpty(); }
    UFUNCTION(BlueprintPure, Category="CF|Frontend") static bool ValidateNickname(const FString& Input, FString& Normalized);

    // 통합 계층은 요청 ID를 보존하고, 실제 작업이 끝난 경우에만 응답한다. 토큰은 UI에 전달하지 않는다.
    UFUNCTION(BlueprintCallable, Category="CF|Frontend|Integration")
    void CompleteLogin(int32 RequestId, bool bSuccess, const FString& RegisteredNickname, const FText& Error);
    UFUNCTION(BlueprintCallable, Category="CF|Frontend|Integration")
    void CompleteNickname(int32 RequestId, bool bSuccess, const FString& RegisteredNickname, const FText& Error);
    UFUNCTION(BlueprintCallable, Category="CF|Frontend|Integration")
    void CompleteLobbyEntry(int32 RequestId, bool bSuccess, const FText& Error);
    UFUNCTION(BlueprintCallable, Category="CF|Frontend|Integration") void InvalidateProfile();
    UFUNCTION(BlueprintCallable, Category="CF|Frontend|Integration") void RequireLogin(const FText& Error);

private:
    bool bAuthenticated = false;
    int32 RequestSerial = 0;
    FTimerHandle RequestDeadline;
    void SetScreen(ECFFrontendScreen Next, const FText& Error = FText::GetEmpty());
    int32 BeginRequest(ECFFrontendScreen Next, float TimeoutSeconds);
    bool AcceptResponse(int32 RequestId, ECFFrontendScreen Expected);
    void EnterLobby();
    void HandleTimeout();
    void ClearDeadline();
};
