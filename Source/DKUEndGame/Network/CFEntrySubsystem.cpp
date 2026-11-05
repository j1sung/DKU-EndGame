#include "Network/CFEntrySubsystem.h"
#include "UI/CFFrontendSubsystem.h"
#include "UI/CFTitleMenuController.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "OnlineSubsystemUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "EOSSettings.h"
#include "IOnlineSubsystemEOS.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "IEOSSDKManager.h"
#include "eos_connect.h"
#include "eos_sdk.h"

DEFINE_LOG_CATEGORY_STATIC(LogCFEntry, Log, All);

namespace CFEntry
{
FText Text(const TCHAR* Value) { return FText::FromString(Value); }
FText ApiError(int32 Status, const FString& Code)
{
    if (Code == TEXT("NICKNAME_TAKEN")) return Text(TEXT("이미 사용 중인 닉네임입니다. 다른 이름을 입력해 주세요."));
    if (Code == TEXT("INVALID_NICKNAME")) return Text(TEXT("2~16자의 한글, 영문, 숫자와 공백으로 입력해 주세요."));
    if (Status == 429) return Text(TEXT("요청이 많습니다. 잠시 후 다시 시도해 주세요."));
    if (Status >= 500) return Text(TEXT("서버에 연결할 수 없습니다. 잠시 후 다시 시도해 주세요."));
    return Text(TEXT("프로필을 확인하지 못했습니다. 다시 시도해 주세요."));
}
}

void UCFEntrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UCFFrontendSubsystem>();
    Frontend = GetGameInstance()->GetSubsystem<UCFFrontendSubsystem>();
    Frontend->OnLoginRequested.AddUniqueDynamic(this, &ThisClass::Login);
    Frontend->OnNicknameRequested.AddUniqueDynamic(this, &ThisClass::RegisterNickname);
    Frontend->OnLobbyRequested.AddUniqueDynamic(this, &ThisClass::EnterLobby);
    Frontend->OnRequestCancelled.AddUniqueDynamic(this, &ThisClass::Cancel);
    MapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::OnMapLoaded);
    TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::OnTravelFailure);
}

void UCFEntrySubsystem::Deinitialize()
{
    bDeinitializing = true;
    ClearOperation();
    if (Frontend)
    {
        Frontend->OnLoginRequested.RemoveDynamic(this, &ThisClass::Login);
        Frontend->OnNicknameRequested.RemoveDynamic(this, &ThisClass::RegisterNickname);
        Frontend->OnLobbyRequested.RemoveDynamic(this, &ThisClass::EnterLobby);
        Frontend->OnRequestCancelled.RemoveDynamic(this, &ThisClass::Cancel);
    }
    if (Identity)
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle);
        Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutHandle);
        Identity->ClearOnLoginStatusChangedDelegate_Handle(0, StatusHandle);
    }
    Identity.Reset(); EOS = nullptr;
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapHandle);
    if (GEngine) GEngine->OnTravelFailure().Remove(TravelHandle);
    Super::Deinitialize();
}

bool UCFEntrySubsystem::IsCurrent(int32 Id) const
{
    if (bDeinitializing || !Frontend || Id <= 0 || RequestId != Id || Frontend->ActiveRequestId != Id) return false;
    return (Operation == EOperation::Login && Frontend->Screen == ECFFrontendScreen::LoggingIn) ||
        (Operation == EOperation::Nickname && Frontend->Screen == ECFFrontendScreen::SavingNickname) ||
        (Operation == EOperation::Travel && Frontend->Screen == ECFFrontendScreen::Connecting);
}

void UCFEntrySubsystem::ClearOperation()
{
    ++HttpSerial;
    if (HttpRequest)
    {
        HttpRequest->OnProcessRequestComplete().Unbind();
        HttpRequest->CancelRequest(); HttpRequest.Reset();
    }
    if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(TravelTimer);
    RequestId = 0; Operation = EOperation::None;
    bResolvingRegistrationConflict = false;
    ExpectedPlayerId.Reset(); PendingNickname.Reset(); LobbyPackage.Reset();
}

void UCFEntrySubsystem::Cancel(int32 Id)
{
    if (Id == RequestId) ClearOperation();
    // Account Portal 자격 입력은 SDK가 소유한다. 닫힌 UI의 응답을 무시하고 로그인 창을 중복 생성하지 않는다.
}

void UCFEntrySubsystem::Fail(const FText& Error)
{
    const int32 Id = RequestId;
    const EOperation FailedOperation = Operation;
    const bool bCurrent = IsCurrent(Id);
    ClearOperation();
    if (!bCurrent) return;
    if (FailedOperation == EOperation::Login) Frontend->CompleteLogin(Id, false, FString(), Error);
    else if (FailedOperation == EOperation::Nickname) Frontend->CompleteNickname(Id, false, FString(), Error);
    else if (FailedOperation == EOperation::Travel) Frontend->CompleteLobbyEntry(Id, false, Error);
}

bool UCFEntrySubsystem::ValidApiBase(const FString& Base)
{
    if (!Base.StartsWith(TEXT("https://")) || Base.Len() <= 8) return false;
    for (TCHAR Ch : Base) if (FChar::IsWhitespace(Ch) || Ch == '?' || Ch == '#' || Ch == '@' || Ch == '\\') return false;
    return Base[8] != '/';
}

bool UCFEntrySubsystem::PrepareEOS()
{
    if (Identity.IsValid())
    {
        UE_LOG(LogCFEntry, Display, TEXT("EOS preparation: reusing Identity interface."));
        return true;
    }
    const FEOSSettings Settings = UEOSSettings::GetSettings();
    // 설정값 원문 대신 활성화/존재 여부만 기록한다. 자격 증명과 토큰은 출력하지 않는다.
    UE_LOG(LogCFEntry, Display, TEXT("EOS settings: UseEAS=%d UseEOSConnect=%d UseNamedPlatformConfig=%d"),
        static_cast<int32>(Settings.bUseEAS), static_cast<int32>(Settings.bUseEOSConnect),
        static_cast<int32>(Settings.bUseNamedPlatformConfig));
    if (!Settings.bUseEAS || !Settings.bUseEOSConnect)
    {
        UE_LOG(LogCFEntry, Warning, TEXT("EOS preparation failed: EAS or EOS Connect is disabled."));
        return false;
    }
    FEOSArtifactSettings Artifact;
    if (!Settings.bUseNamedPlatformConfig)
    {
        if (!UEOSSettings::GetSelectedArtifactSettings(Artifact))
        {
            UE_LOG(LogCFEntry, Warning, TEXT("EOS preparation failed: artifact selection failed."));
            return false;
        }
        UE_LOG(LogCFEntry, Display, TEXT("EOS artifact presence: ProductId=%d SandboxId=%d DeploymentId=%d ClientId=%d ClientSecret=%d"),
            !Artifact.ProductId.IsEmpty(), !Artifact.SandboxId.IsEmpty(), !Artifact.DeploymentId.IsEmpty(),
            !Artifact.ClientId.IsEmpty(), !Artifact.ClientSecret.IsEmpty());
        if (Artifact.ProductId.IsEmpty() || Artifact.SandboxId.IsEmpty() || Artifact.DeploymentId.IsEmpty() ||
            Artifact.ClientId.IsEmpty() || Artifact.ClientSecret.IsEmpty())
        {
            UE_LOG(LogCFEntry, Warning, TEXT("EOS preparation failed: required artifact fields are missing."));
            return false;
        }
    }
    else
    {
        UE_LOG(LogCFEntry, Display, TEXT("EOS preparation: using named platform configuration; artifact check skipped."));
    }
    UE_LOG(LogCFEntry, Display, TEXT("EOS preparation: requesting EOS subsystem."));
    IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld(), FName(TEXT("EOS")));
    if (!Subsystem || Subsystem->GetSubsystemName() != FName(TEXT("EOS")))
    {
        UE_LOG(LogCFEntry, Warning, TEXT("EOS preparation failed: EOS subsystem is unavailable or has an unexpected type."));
        return false;
    }
    EOS = static_cast<IOnlineSubsystemEOS*>(Subsystem);
    Identity = EOS->GetIdentityInterface();
    if (!Identity)
    {
        UE_LOG(LogCFEntry, Warning, TEXT("EOS preparation failed: Identity interface is unavailable."));
        return false;
    }
    StatusHandle = Identity->AddOnLoginStatusChangedDelegate_Handle(0,
        FOnLoginStatusChangedDelegate::CreateUObject(this, &ThisClass::OnEOSStatus));
    UE_LOG(LogCFEntry, Display, TEXT("EOS preparation succeeded."));
    return true;
}

void UCFEntrySubsystem::Login(int32 Id)
{
    ClearOperation(); RequestId = Id; Operation = EOperation::Login;
    if (!IsCurrent(Id)) { ClearOperation(); return; }
    UE_LOG(LogCFEntry, Display, TEXT("Login requested: ApiBaseValid=%d"), ValidApiBase(ProfileApiBaseUrl));
    if (!ValidApiBase(ProfileApiBaseUrl) || !PrepareEOS())
    {
        Fail(CFEntry::Text(TEXT("현재 로그인 서비스를 사용할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    if (bLoginPending || bLogoutPending) return;
    if (bNeedsReauthentication && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
    {
        bLogoutPending = true;
        LogoutHandle = Identity->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateUObject(this, &ThisClass::OnEOSLogout));
        if (!Identity->Logout(0)) OnEOSLogout(0, false);
        return;
    }
    if (Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn) SendProfileRequest(false);
    else StartEOSLogin();
}

void UCFEntrySubsystem::StartEOSLogin()
{
    if (!IsCurrent(RequestId) || Operation != EOperation::Login || bLoginPending) return;
    bLoginPending = true;
    LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::OnEOSLogin));
    FOnlineAccountCredentials Credentials;
    Credentials.Type = TEXT("accountportal");
    UE_LOG(LogCFEntry, Display, TEXT("Starting Account Portal login."));
    if (!Identity->Login(0, Credentials) && bLoginPending)
    {
        Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle); LoginHandle.Reset(); bLoginPending = false;
        Fail(CFEntry::Text(TEXT("로그인을 시작하지 못했습니다. 다시 시도해 주세요.")));
    }
}

void UCFEntrySubsystem::OnEOSLogin(int32 LocalUserNum, bool bSuccess, const FUniqueNetId& UserId, const FString& Error)
{
    if (LocalUserNum != 0) return;
    Identity->ClearOnLoginCompleteDelegate_Handle(0, LoginHandle); LoginHandle.Reset(); bLoginPending = false;
    if (!IsCurrent(RequestId) || Operation != EOperation::Login) return;
    if (!bSuccess) { Fail(CFEntry::Text(TEXT("로그인이 완료되지 않았습니다. 다시 시도해 주세요."))); return; }
    bNeedsReauthentication = false;
    SendProfileRequest(false);
}

void UCFEntrySubsystem::OnEOSLogout(int32 LocalUserNum, bool bSuccess)
{
    if (LocalUserNum != 0 || !bLogoutPending) return;
    Identity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutHandle); LogoutHandle.Reset(); bLogoutPending = false;
    if (!IsCurrent(RequestId) || Operation != EOperation::Login) return;
    if (!bSuccess) { Fail(CFEntry::Text(TEXT("로그인 정보를 갱신하지 못했습니다. 다시 시도해 주세요."))); return; }
    StartEOSLogin();
}

void UCFEntrySubsystem::OnEOSStatus(int32 LocalUserNum, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId& UserId)
{
    if (LocalUserNum != 0 || Previous != ELoginStatus::LoggedIn || Current == ELoginStatus::LoggedIn || bLogoutPending || bDeinitializing) return;
    RequireLogin();
}

bool UCFEntrySubsystem::CopyConnectToken(FString& Token)
{
    Token.Reset();
    if (!EOS || !Identity || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) return false;
    const FUniqueNetIdPtr User = Identity->GetUniquePlayerId(0);
    const IEOSPlatformHandlePtr Platform = EOS->GetEOSPlatformHandle();
    if (!User || User->GetType() != FName(TEXT("EOS")) || !Platform) return false;
    const EOS_ProductUserId Player = static_cast<const IUniqueNetIdEOS&>(*User).GetProductUserId();
    if (!EOS_ProductUserId_IsValid(Player)) return false;
    char PlayerBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
    int32 BufferLength = UE_ARRAY_COUNT(PlayerBuffer);
    if (EOS_ProductUserId_ToString(Player, PlayerBuffer, &BufferLength) != EOS_EResult::EOS_Success) return false;
    const FString PlayerId(UTF8_TO_TCHAR(PlayerBuffer));
    if (!ExpectedPlayerId.IsEmpty() && ExpectedPlayerId != PlayerId) return false;
    ExpectedPlayerId = PlayerId;
    EOS_Connect_CopyIdTokenOptions Options = {};
    Options.ApiVersion = EOS_CONNECT_COPYIDTOKEN_API_LATEST; Options.LocalUserId = Player;
    EOS_Connect_IdToken* IdToken = nullptr;
    const EOS_EResult Result = EOS_Connect_CopyIdToken(EOS_Platform_GetConnectInterface(*Platform), &Options, &IdToken);
    if (Result != EOS_EResult::EOS_Success || !IdToken) return false;
    // OSS GetAuthToken은 Epic Access Token이다. 백엔드에는 반드시 Connect ID Token을 보낸다.
    if (IdToken->JsonWebToken) Token = UTF8_TO_TCHAR(IdToken->JsonWebToken);
    EOS_Connect_IdToken_Release(IdToken);
    return !Token.IsEmpty();
}

void UCFEntrySubsystem::RegisterNickname(int32 Id, const FString& Nickname)
{
    ClearOperation(); RequestId = Id; Operation = EOperation::Nickname; PendingNickname = Nickname;
    if (!IsCurrent(Id)) { ClearOperation(); return; }
    if (!ValidApiBase(ProfileApiBaseUrl) || !PrepareEOS())
    {
        Fail(CFEntry::Text(TEXT("현재 닉네임을 등록할 수 없습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    // 이전 등록의 응답만 유실됐을 수 있다. 실제 프로필을 조회한 다음 최초 등록을 수행한다.
    SendProfileRequest(false);
}

void UCFEntrySubsystem::SendProfileRequest(bool bRegister)
{
    if (!IsCurrent(RequestId)) return;
    FString Token;
    if (!CopyConnectToken(Token)) { RequireLogin(); return; }
    FString Base = ProfileApiBaseUrl;
    while (Base.EndsWith(TEXT("/"))) Base.LeftChopInline(1);
    const int32 Id = RequestId;
    const uint64 Serial = ++HttpSerial;
    HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Base + (bRegister ? TEXT("/v1/me/nickname") : TEXT("/v1/me")));
    HttpRequest->SetVerb(bRegister ? TEXT("POST") : TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
    HttpRequest->SetTimeout(12.f);
    if (bRegister)
    {
        if (RegistrationKey.IsEmpty() || RegistrationPlayerId != ExpectedPlayerId || RegistrationNickname != PendingNickname)
        {
            RegistrationPlayerId = ExpectedPlayerId; RegistrationNickname = PendingNickname;
            RegistrationKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
        }
        HttpRequest->SetHeader(TEXT("Idempotency-Key"), RegistrationKey);
        HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("nickname"), PendingNickname);
        FString Body; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Body));
        HttpRequest->SetContentAsString(Body);
    }
    HttpRequest->OnProcessRequestComplete().BindWeakLambda(this,
        [this, Id, Serial, bRegister](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnected)
        {
            ReceiveProfile(Id, Serial, bRegister, bConnected && Response.IsValid(),
                Response ? Response->GetResponseCode() : 0, Response ? Response->GetContentAsString() : FString());
        });
    if (!HttpRequest->ProcessRequest()) ReceiveProfile(Id, Serial, bRegister, false, 0, FString());
}

void UCFEntrySubsystem::ReceiveProfile(int32 Id, uint64 Serial, bool bRegister, bool bConnected, int32 Status, const FString& Body)
{
    if (!IsCurrent(Id) || Serial != HttpSerial) return;
    HttpRequest.Reset();
    if (!bConnected) { Fail(CFEntry::Text(TEXT("연결이 끊겼거나 응답 시간이 초과되었습니다. 다시 시도해 주세요."))); return; }
    if (Status == 401) { RequireLogin(); return; }
    TSharedPtr<FJsonObject> Json;
    const bool bJson = Body.Len() <= 65536 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) && Json.IsValid();
    if (Status != (bRegister ? 201 : 200))
    {
        FString Code;
        const TSharedPtr<FJsonObject>* Error = nullptr;
        if (bJson && Json->TryGetObjectField(TEXT("error"), Error) && Error && Error->IsValid()) (*Error)->TryGetStringField(TEXT("code"), Code);
        if (bRegister && Status == 409 && Code == TEXT("NICKNAME_ALREADY_SET"))
        {
            bResolvingRegistrationConflict = true; SendProfileRequest(false); return;
        }
        Fail(CFEntry::ApiError(Status, Code)); return;
    }
    FString PlayerId, Nickname, Normalized;
    const TSharedPtr<FJsonValue> Player = bJson ? Json->TryGetField(TEXT("playerId")) : nullptr;
    if (!Player || Player->Type != EJson::String || !Player->TryGetString(PlayerId) || ExpectedPlayerId.IsEmpty() || PlayerId != ExpectedPlayerId)
    {
        Fail(CFEntry::Text(TEXT("프로필 응답을 확인하지 못했습니다. 다시 시도해 주세요."))); return;
    }
    const TSharedPtr<FJsonValue> Name = Json->TryGetField(TEXT("nickname"));
    if (Name && !Name->IsNull() && (Name->Type != EJson::String || !Name->TryGetString(Nickname) || !UCFFrontendSubsystem::ValidateNickname(Nickname, Normalized) || Nickname != Normalized))
    {
        Fail(CFEntry::Text(TEXT("닉네임 정보를 확인하지 못했습니다. 다시 시도해 주세요."))); return;
    }
    if (bRegister && Nickname.IsEmpty()) { Fail(CFEntry::Text(TEXT("닉네임 등록을 확인하지 못했습니다. 다시 시도해 주세요."))); return; }
    ResolveProfile(Nickname);
}

void UCFEntrySubsystem::ResolveProfile(const FString& Nickname)
{
    if (bResolvingRegistrationConflict && Nickname.IsEmpty())
    {
        Fail(CFEntry::Text(TEXT("등록된 닉네임을 확인하지 못했습니다. 잠시 후 다시 시도해 주세요."))); return;
    }
    if (Operation == EOperation::Nickname && Nickname.IsEmpty()) { SendProfileRequest(true); return; }
    const int32 Id = RequestId;
    const EOperation CompletedOperation = Operation;
    if (!Nickname.IsEmpty()) { RegistrationKey.Reset(); RegistrationNickname.Reset(); RegistrationPlayerId.Reset(); }
    ClearOperation();
    if (CompletedOperation == EOperation::Login) Frontend->CompleteLogin(Id, true, Nickname, FText::GetEmpty());
    else Frontend->CompleteNickname(Id, true, Nickname, FText::GetEmpty());
}

void UCFEntrySubsystem::RequireLogin()
{
    bNeedsReauthentication = true;
    ClearOperation();
    if (Frontend) Frontend->RequireLogin(CFEntry::Text(TEXT("로그인 정보가 만료되었습니다. 다시 로그인해 주세요.")));
}

void UCFEntrySubsystem::EnterLobby(int32 Id)
{
    ClearOperation(); RequestId = Id; Operation = EOperation::Travel;
    if (!IsCurrent(Id)) { ClearOperation(); return; }
    const auto* Controller = Cast<ACFTitleMenuController>(GetGameInstance()->GetFirstLocalPlayerController());
    const TSoftObjectPtr<UWorld> Map = Controller ? Controller->LobbyMap : TSoftObjectPtr<UWorld>();
    LobbyPackage = Map.ToSoftObjectPath().GetLongPackageName();
    if (LobbyPackage.IsEmpty() || !FPackageName::DoesPackageExist(LobbyPackage))
    {
        Fail(CFEntry::Text(TEXT("로비를 불러올 수 없습니다. 게임 파일을 확인하고 다시 시도해 주세요."))); return;
    }
    // 완료 신호는 OpenLevel 호출 시점이 아니라 같은 GameInstance의 로비 로딩 완료 시점에 보낸다.
    GetGameInstance()->GetTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this, [this, Id, Map]()
    {
        if (IsCurrent(Id)) UGameplayStatics::OpenLevelBySoftObjectPtr(this, Map, true);
    }), 0.1f, false);
}

void UCFEntrySubsystem::OnMapLoaded(UWorld* World)
{
    if (!World || World->GetGameInstance() != GetGameInstance() || Operation != EOperation::Travel || !IsCurrent(RequestId)) return;
    if (UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != LobbyPackage)
    {
        Fail(CFEntry::Text(TEXT("로비에 접속하지 못했습니다. 다시 시도해 주세요."))); return;
    }
    const int32 Id = RequestId;
    ClearOperation(); Frontend->CompleteLobbyEntry(Id, true, FText::GetEmpty());
}

void UCFEntrySubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Detail)
{
    if (World && World->GetGameInstance() == GetGameInstance() && Operation == EOperation::Travel)
        Fail(CFEntry::Text(TEXT("로비에 접속하지 못했습니다. 다시 시도해 주세요.")));
}
