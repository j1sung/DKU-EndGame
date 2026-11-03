#include "eos-verifier.hpp"
#include "protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>

namespace {

constexpr std::size_t kMaxInputBytes = 131'072;

std::optional<std::string> requiredEnvironment(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

std::optional<EosConfig> loadConfig() {
    const auto productId = requiredEnvironment("EOS_PRODUCT_ID");
    const auto sandboxId = requiredEnvironment("EOS_SANDBOX_ID");
    const auto deploymentId = requiredEnvironment("EOS_DEPLOYMENT_ID");
    const auto clientId = requiredEnvironment("EOS_CLIENT_ID");
    const auto clientSecret = requiredEnvironment("EOS_CLIENT_SECRET");
    if (!productId || !sandboxId || !deploymentId || !clientId || !clientSecret) {
        return std::nullopt;
    }
    return EosConfig{*productId, *sandboxId, *deploymentId, *clientId, *clientSecret};
}

int emit(const VerifierResult& result) {
    std::cout << serializeResult(result) << '\n';
    if (!result.diagnostic.empty()) {
        std::cerr << "EOS verifier: " << result.diagnostic << '\n';
    }
    return result.exitCode;
}

}  // namespace

int main() {
    std::string input(std::istreambuf_iterator<char>(std::cin), {});
    if (input.size() > kMaxInputBytes) {
        return emit(VerifierResult::failure(1, "INVALID_INPUT"));
    }

    const ParseResult parsed = parseRequest(input);
    if (!parsed.ok) {
        return emit(VerifierResult::failure(1, parsed.error));
    }

    const auto config = loadConfig();
    if (!config) {
        return emit(VerifierResult::failure(1, "CONFIGURATION_ERROR"));
    }

    return emit(verifyConnectIdToken(parsed.request, *config));
}
