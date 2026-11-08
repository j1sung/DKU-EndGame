#pragma once

#include <string>
#include <string_view>

struct VerifyRequest {
    std::string token;
    std::string candidatePlayerId;
};

struct ParseResult {
    bool ok{false};
    VerifyRequest request;
    std::string error{"INVALID_INPUT"};
};

struct VerifierResult {
    bool ok{false};
    int exitCode{4};
    std::string playerId;
    std::string error{"INTERNAL_FAILURE"};
    std::string diagnostic;

    static VerifierResult success(std::string playerId);
    static VerifierResult failure(int exitCode, std::string error,
                                  std::string diagnostic = {});
};

ParseResult parseRequest(std::string_view input);
std::string serializeResult(const VerifierResult& result);
