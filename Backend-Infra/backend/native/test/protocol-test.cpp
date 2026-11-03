#include "protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectInvalid(const std::string& input, const char* message) {
    const auto result = parseRequest(input);
    expect(!result.ok, message);
    expect(result.error == "INVALID_INPUT", "invalid input uses stable error code");
}

}  // namespace

int main() {
    constexpr const char* puid = "0123456789abcdef0123456789abcdef";

    expectInvalid("", "empty input is rejected");
    expectInvalid("not-json", "invalid JSON is rejected");
    expectInvalid("[]", "non-object JSON is rejected");
    expectInvalid(R"({"token":1,"candidatePlayerId":"0123456789abcdef0123456789abcdef"})",
                  "wrong token type is rejected");
    expectInvalid(R"({"token":"a.b.c","candidatePlayerId":7})",
                  "wrong PUID type is rejected");
    expectInvalid(R"({"token":"a.b.c","candidatePlayerId":"0123456789abcdef0123456789abcdef","extra":true})",
                  "unknown top-level keys are rejected");
    expectInvalid(R"({"token":"a.b.c","candidatePlayerId":"not-a-puid"})",
                  "invalid PUID is rejected");
    expectInvalid(std::string{"{\"token\":\""} + std::string(16'385, 'a') +
                      "\",\"candidatePlayerId\":\"" + puid + "\"}",
                  "oversized token is rejected");

    const std::string escapedToken = "header.escaped\"payload.signature";
    const auto escaped = parseRequest(
        R"({"token":"header.escaped\"payload.signature","candidatePlayerId":"0123456789ABCDEF0123456789ABCDEF"})");
    expect(escaped.ok, "escaped JSON token is parsed");
    expect(escaped.request.token == escapedToken, "escaped token is decoded exactly once");
    expect(escaped.request.candidatePlayerId == "0123456789abcdef0123456789abcdef",
           "PUID is normalized to lowercase");

    const auto valid = parseRequest(
        R"({"token":"header.payload.signature","candidatePlayerId":"0123456789abcdef0123456789abcdef"})");
    expect(valid.ok, "valid request is accepted");
    expect(valid.request.token == "header.payload.signature", "valid token round-trips");
    expect(valid.request.candidatePlayerId == puid, "valid PUID round-trips");

    const auto success = serializeResult(VerifierResult::success(puid));
    expect(success == R"({"ok":true,"playerId":"0123456789abcdef0123456789abcdef"})",
           "success result is compact and stable");
    expect(success.find('\n') == std::string::npos, "serialized result contains no newline");

    const std::string tokenCanary = "secret-token-canary";
    const std::string secretCanary = "client-secret-canary";
    const auto failure = serializeResult(
        VerifierResult::failure(2, "TOKEN_INVALID", tokenCanary + secretCanary));
    expect(failure == R"({"ok":false,"error":"TOKEN_INVALID"})",
           "failure result exposes only a stable error code");
    expect(failure.find(tokenCanary) == std::string::npos, "serialized result never leaks token");
    expect(failure.find(secretCanary) == std::string::npos,
           "serialized result never leaks client secret");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
