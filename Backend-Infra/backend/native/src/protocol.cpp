#include "protocol.hpp"

#include <json-c/json.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace {

constexpr std::size_t kMaxTokenBytes = 16'384;
constexpr std::size_t kPuidLength = 32;

struct JsonObjectDeleter {
    void operator()(json_object* value) const {
        if (value != nullptr) {
            json_object_put(value);
        }
    }
};

using JsonObjectPtr = std::unique_ptr<json_object, JsonObjectDeleter>;

ParseResult invalid() {
    return {};
}

bool isHexPuid(std::string_view value) {
    return value.size() == kPuidLength &&
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isxdigit(character) != 0;
           });
}

bool hasOnlyWhitespace(std::string_view input, std::size_t start) {
    return std::all_of(input.begin() + static_cast<std::ptrdiff_t>(start), input.end(),
                       [](unsigned char character) { return std::isspace(character) != 0; });
}

std::string compactJson(json_object* object) {
    return json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN);
}

}  // namespace

VerifierResult VerifierResult::success(std::string playerId) {
    VerifierResult result;
    result.ok = true;
    result.exitCode = 0;
    result.playerId = std::move(playerId);
    result.error.clear();
    return result;
}

VerifierResult VerifierResult::failure(int exitCode, std::string error,
                                       std::string diagnostic) {
    VerifierResult result;
    result.exitCode = exitCode;
    result.error = std::move(error);
    result.diagnostic = std::move(diagnostic);
    return result;
}

ParseResult parseRequest(std::string_view input) {
    if (input.empty()) {
        return invalid();
    }

    std::unique_ptr<json_tokener, decltype(&json_tokener_free)> tokener(
        json_tokener_new(), &json_tokener_free);
    if (!tokener) {
        return invalid();
    }

    JsonObjectPtr root(json_tokener_parse_ex(tokener.get(), input.data(),
                                             static_cast<int>(input.size())));
    const auto parseEnd = static_cast<std::size_t>(json_tokener_get_parse_end(tokener.get()));
    if (json_tokener_get_error(tokener.get()) != json_tokener_success || !root ||
        !hasOnlyWhitespace(input, parseEnd) ||
        json_object_get_type(root.get()) != json_type_object ||
        json_object_object_length(root.get()) != 2) {
        return invalid();
    }

    json_object* tokenValue = nullptr;
    json_object* puidValue = nullptr;
    if (!json_object_object_get_ex(root.get(), "token", &tokenValue) ||
        !json_object_object_get_ex(root.get(), "candidatePlayerId", &puidValue) ||
        json_object_get_type(tokenValue) != json_type_string ||
        json_object_get_type(puidValue) != json_type_string) {
        return invalid();
    }

    const auto tokenLength = static_cast<std::size_t>(json_object_get_string_len(tokenValue));
    const auto puidLength = static_cast<std::size_t>(json_object_get_string_len(puidValue));
    const std::string token(json_object_get_string(tokenValue), tokenLength);
    std::string puid(json_object_get_string(puidValue), puidLength);
    if (token.empty() || token.size() > kMaxTokenBytes ||
        token.find('\0') != std::string::npos || !isHexPuid(puid)) {
        return invalid();
    }

    std::transform(puid.begin(), puid.end(), puid.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    ParseResult result;
    result.ok = true;
    result.error.clear();
    result.request = {token, puid};
    return result;
}

std::string serializeResult(const VerifierResult& result) {
    JsonObjectPtr root(json_object_new_object());
    json_object_object_add(root.get(), "ok", json_object_new_boolean(result.ok));
    if (result.ok) {
        json_object_object_add(root.get(), "playerId",
                               json_object_new_string_len(result.playerId.data(),
                                                          result.playerId.size()));
    } else {
        json_object_object_add(root.get(), "error",
                               json_object_new_string_len(result.error.data(),
                                                          result.error.size()));
    }
    return compactJson(root.get());
}
