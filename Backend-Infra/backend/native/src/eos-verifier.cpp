#include "eos-verifier.hpp"

#include <eos_connect.h>
#include <eos_sdk.h>

#include <dlfcn.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits.h>
#include <string>
#include <thread>

namespace {

constexpr auto kVerificationDeadline = std::chrono::seconds(7);
constexpr auto kTickInterval = std::chrono::milliseconds(10);

class StdoutToStderr {
public:
    StdoutToStderr() {
        std::fflush(stdout);
        savedStdout_ = dup(STDOUT_FILENO);
        active_ = savedStdout_ >= 0 && dup2(STDERR_FILENO, STDOUT_FILENO) >= 0;
        if (!active_ && savedStdout_ >= 0) {
            close(savedStdout_);
            savedStdout_ = -1;
        }
    }
    StdoutToStderr(const StdoutToStderr&) = delete;
    StdoutToStderr& operator=(const StdoutToStderr&) = delete;
    ~StdoutToStderr() {
        if (active_) {
            std::fflush(stdout);
            dup2(savedStdout_, STDOUT_FILENO);
            close(savedStdout_);
        }
    }
    bool active() const { return active_; }

private:
    int savedStdout_{-1};
    bool active_{false};
};

class EosApi {
public:
    using InitializeFn = decltype(&EOS_Initialize);
    using ShutdownFn = decltype(&EOS_Shutdown);
    using PlatformCreateFn = decltype(&EOS_Platform_Create);
    using PlatformReleaseFn = decltype(&EOS_Platform_Release);
    using PlatformGetConnectFn = decltype(&EOS_Platform_GetConnectInterface);
    using PlatformTickFn = decltype(&EOS_Platform_Tick);
    using ProductUserIdFromStringFn = decltype(&EOS_ProductUserId_FromString);
    using ProductUserIdIsValidFn = decltype(&EOS_ProductUserId_IsValid);
    using ProductUserIdToStringFn = decltype(&EOS_ProductUserId_ToString);
    using VerifyIdTokenFn = decltype(&EOS_Connect_VerifyIdToken);
    using ResultToStringFn = decltype(&EOS_EResult_ToString);

    EosApi() {
        char executablePath[PATH_MAX + 1]{};
        const ssize_t length = readlink("/proc/self/exe", executablePath, PATH_MAX);
        if (length <= 0) {
            diagnostic_ = "EXECUTABLE_PATH_UNAVAILABLE";
            return;
        }
        executablePath[length] = '\0';
        std::string libraryPath(executablePath);
        const auto separator = libraryPath.find_last_of('/');
        if (separator == std::string::npos) {
            diagnostic_ = "EXECUTABLE_PATH_INVALID";
            return;
        }
        libraryPath.resize(separator + 1);
        libraryPath += "libEOSSDK-Linux-Shipping.so";

        handle_ = dlopen(libraryPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_ == nullptr) {
            diagnostic_ = "EOS_LIBRARY_LOAD_FAILED";
            return;
        }
        loaded_ = load(initialize, "EOS_Initialize") && load(shutdown, "EOS_Shutdown") &&
                  load(platformCreate, "EOS_Platform_Create") &&
                  load(platformRelease, "EOS_Platform_Release") &&
                  load(platformGetConnect, "EOS_Platform_GetConnectInterface") &&
                  load(platformTick, "EOS_Platform_Tick") &&
                  load(productUserIdFromString, "EOS_ProductUserId_FromString") &&
                  load(productUserIdIsValid, "EOS_ProductUserId_IsValid") &&
                  load(productUserIdToString, "EOS_ProductUserId_ToString") &&
                  load(verifyIdToken, "EOS_Connect_VerifyIdToken") &&
                  load(resultToString, "EOS_EResult_ToString");
        if (!loaded_) {
            diagnostic_ = "EOS_LIBRARY_SYMBOL_MISSING";
        }
    }
    EosApi(const EosApi&) = delete;
    EosApi& operator=(const EosApi&) = delete;
    ~EosApi() {
        if (handle_ != nullptr) {
            dlclose(handle_);
        }
    }

    bool loaded() const { return loaded_; }
    const std::string& diagnostic() const { return diagnostic_; }

    InitializeFn initialize{};
    ShutdownFn shutdown{};
    PlatformCreateFn platformCreate{};
    PlatformReleaseFn platformRelease{};
    PlatformGetConnectFn platformGetConnect{};
    PlatformTickFn platformTick{};
    ProductUserIdFromStringFn productUserIdFromString{};
    ProductUserIdIsValidFn productUserIdIsValid{};
    ProductUserIdToStringFn productUserIdToString{};
    VerifyIdTokenFn verifyIdToken{};
    ResultToStringFn resultToString{};

private:
    template <typename Function>
    bool load(Function& function, const char* name) {
        function = reinterpret_cast<Function>(dlsym(handle_, name));
        return function != nullptr;
    }

    void* handle_{nullptr};
    bool loaded_{false};
    std::string diagnostic_;
};

struct VerificationState {
    EosApi* api{nullptr};
    std::atomic<bool> completed{false};
    EOS_EResult result{EOS_EResult::EOS_UnexpectedError};
    std::string productUserId;
    std::string clientId;
    std::string productId;
    std::string sandboxId;
    std::string deploymentId;
};

std::string safeString(const char* value) {
    return value == nullptr ? std::string{} : std::string(value);
}

std::string productUserIdToString(EosApi& api, EOS_ProductUserId id) {
    if (!api.productUserIdIsValid(id)) {
        return {};
    }
    char buffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1]{};
    int32_t length = static_cast<int32_t>(sizeof(buffer));
    return api.productUserIdToString(id, buffer, &length) == EOS_EResult::EOS_Success
               ? std::string(buffer)
               : std::string{};
}

void EOS_CALL onVerifyIdToken(const EOS_Connect_VerifyIdTokenCallbackInfo* data) {
    if (data == nullptr || data->ClientData == nullptr) {
        return;
    }
    auto* state = static_cast<VerificationState*>(data->ClientData);
    state->result = data->ResultCode;
    state->productUserId = productUserIdToString(*state->api, data->ProductUserId);
    state->clientId = safeString(data->ClientId);
    state->productId = safeString(data->ProductId);
    state->sandboxId = safeString(data->SandboxId);
    state->deploymentId = safeString(data->DeploymentId);
    state->completed.store(true, std::memory_order_release);
}

bool isUnavailable(EOS_EResult result) {
    return result == EOS_EResult::EOS_NoConnection ||
           result == EOS_EResult::EOS_ServiceFailure ||
           result == EOS_EResult::EOS_TimedOut ||
           result == EOS_EResult::EOS_TooManyRequests;
}

bool scopeMatches(const VerificationState& state, const EosConfig& config) {
    return !state.clientId.empty() && !state.productId.empty() && !state.sandboxId.empty() &&
           !state.deploymentId.empty() && state.clientId == config.clientId &&
           state.productId == config.productId && state.sandboxId == config.sandboxId &&
           state.deploymentId == config.deploymentId;
}

class EosLifetime {
public:
    EosLifetime(EosApi& api, bool initialized) : api_(api), initialized_(initialized) {}
    EosLifetime(const EosLifetime&) = delete;
    EosLifetime& operator=(const EosLifetime&) = delete;
    ~EosLifetime() {
        if (platform_ != nullptr) {
            api_.platformRelease(platform_);
        }
        if (initialized_) {
            api_.shutdown();
        }
    }

    void setPlatform(EOS_HPlatform platform) { platform_ = platform; }

private:
    EosApi& api_;
    bool initialized_{false};
    EOS_HPlatform platform_{nullptr};
};

}  // namespace

VerifierResult verifyConnectIdToken(const VerifyRequest& request, const EosConfig& config) {
    StdoutToStderr redirect;
    if (!redirect.active()) {
        return VerifierResult::failure(4, "INTERNAL_FAILURE", "STDOUT_REDIRECT_FAILED");
    }
    EosApi api;
    if (!api.loaded()) {
        return VerifierResult::failure(4, "INTERNAL_FAILURE", api.diagnostic());
    }

    EOS_InitializeOptions initializeOptions{};
    initializeOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
    initializeOptions.ProductName = "chickengame-backend";
    initializeOptions.ProductVersion = "1.0.0";

    const EOS_EResult initializeResult = api.initialize(&initializeOptions);
    if (initializeResult != EOS_EResult::EOS_Success) {
        return VerifierResult::failure(4, "INTERNAL_FAILURE",
                                       api.resultToString(initializeResult));
    }
    EosLifetime lifetime(api, true);

    EOS_Platform_Options platformOptions{};
    platformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    platformOptions.ProductId = config.productId.c_str();
    platformOptions.SandboxId = config.sandboxId.c_str();
    platformOptions.DeploymentId = config.deploymentId.c_str();
    platformOptions.ClientCredentials.ClientId = config.clientId.c_str();
    platformOptions.ClientCredentials.ClientSecret = config.clientSecret.c_str();
    platformOptions.bIsServer = EOS_TRUE;

    EOS_HPlatform platform = api.platformCreate(&platformOptions);
    if (platform == nullptr) {
        return VerifierResult::failure(1, "CONFIGURATION_ERROR", "PLATFORM_CREATE_FAILED");
    }
    lifetime.setPlatform(platform);

    EOS_HConnect connect = api.platformGetConnect(platform);
    const EOS_ProductUserId expectedUserId =
        api.productUserIdFromString(request.candidatePlayerId.c_str());
    if (connect == nullptr || !api.productUserIdIsValid(expectedUserId)) {
        return VerifierResult::failure(1, "CONFIGURATION_ERROR",
                                       "INVALID_CONNECT_HANDLE_OR_PUID");
    }

    EOS_Connect_IdToken idToken{};
    idToken.ApiVersion = EOS_CONNECT_IDTOKEN_API_LATEST;
    idToken.ProductUserId = expectedUserId;
    idToken.JsonWebToken = request.token.c_str();

    EOS_Connect_VerifyIdTokenOptions verifyOptions{};
    verifyOptions.ApiVersion = EOS_CONNECT_VERIFYIDTOKEN_API_LATEST;
    verifyOptions.IdToken = &idToken;

    VerificationState state;
    state.api = &api;
    api.verifyIdToken(connect, &verifyOptions, &state, onVerifyIdToken);

    const auto deadline = std::chrono::steady_clock::now() + kVerificationDeadline;
    while (!state.completed.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        api.platformTick(platform);
        std::this_thread::sleep_for(kTickInterval);
    }

    if (!state.completed.load(std::memory_order_acquire)) {
        return VerifierResult::failure(3, "EOS_UNAVAILABLE", "NATIVE_TIMEOUT");
    }
    if (isUnavailable(state.result)) {
        return VerifierResult::failure(3, "EOS_UNAVAILABLE", api.resultToString(state.result));
    }
    if (state.result != EOS_EResult::EOS_Success) {
        return VerifierResult::failure(2, "TOKEN_INVALID", api.resultToString(state.result));
    }
    if (state.productUserId != request.candidatePlayerId) {
        return VerifierResult::failure(2, "PUID_MISMATCH");
    }
    if (!scopeMatches(state, config)) {
        return VerifierResult::failure(2, "SCOPE_MISMATCH");
    }
    return VerifierResult::success(state.productUserId);
}
