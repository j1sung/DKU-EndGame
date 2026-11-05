#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "CFEntrySubsystem.generated.h"

class UCFFrontendSubsystem;
class IHttpRequest;
class IOnlineSubsystemEOS;

/** Epic 인증과 프로필 API, 연습 로비 이동을 기존 Frontend UI에 연결한다. */
UCLASS(Config=Game)
class DKUENDGAME_API UCFEntrySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // 네이티브 통합 계층에서만 토큰을 사용한다. Blueprint/화면/로그에 노출하지 않는다.
    bool HasBackendSession() const;
    bool GetBackendAccess(FString& BaseUrl, FString& Token, FString& PlayerId);
    void RequireBackendLogin();

private:
    enum class EOperation : uint8 { None, Login, Nickname, Travel };
    UPROPERTY(Config) FString ProfileApiBaseUrl;
    UPROPERTY(Transient) TObjectPtr<UCFFrontendSubsystem> Frontend;
    IOnlineSubsystemEOS* EOS = nullptr;
    IOnlineIdentityPtr Identity;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
    FDelegateHandle LoginHandle, LogoutHandle, StatusHandle, MapHandle, TravelHandle;
    FTimerHandle TravelTimer;
    EOperation Operation = EOperation::None;
    int32 RequestId = 0;
    uint64 HttpSerial = 0;
    bool bLoginPending = false;
    bool bLogoutPending = false;
    bool bNeedsReauthentication = false;
    bool bDeinitializing = false;
    bool bResolvingRegistrationConflict = false;
    FString ExpectedPlayerId;
    FString PendingNickname;
    FString RegistrationPlayerId, RegistrationNickname, RegistrationKey;
    FString LobbyPackage;

    UFUNCTION() void Login(int32 Id);
    UFUNCTION() void RegisterNickname(int32 Id, const FString& Nickname);
    UFUNCTION() void EnterLobby(int32 Id);
    UFUNCTION() void Cancel(int32 Id);
    bool IsCurrent(int32 Id) const;
    void ClearOperation();
    void Fail(const FText& Error);
    bool PrepareEOS();
    void StartEOSLogin();
    void OnEOSLogin(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error);
    void OnEOSLogout(int32 LocalUserNum, bool bSuccess);
    void OnEOSStatus(int32 LocalUserNum, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId& UserId);
    bool CopyConnectToken(FString& Token);
    bool CopyBackendToken(FString& Token, FString& PlayerId);
    void SendProfileRequest(bool bRegister);
    void ReceiveProfile(int32 Id, uint64 Serial, bool bRegister, bool bConnected, int32 Status, const FString& Body);
    void ResolveProfile(const FString& Nickname);
    void RequireLogin();
    void OnMapLoaded(UWorld* World);
    void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Detail);
    static bool ValidApiBase(const FString& Base);

    // 응답·취소 처리를 검증하며 제품에 인증 우회용 실행 경로를 추가하지 않는다.
    friend class FCFEntryResponseTest;
};
