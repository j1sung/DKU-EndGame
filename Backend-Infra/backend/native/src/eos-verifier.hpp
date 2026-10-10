#pragma once

#include "protocol.hpp"

#include <string>

struct EosConfig {
    std::string productId;
    std::string sandboxId;
    std::string deploymentId;
    std::string clientId;
    std::string clientSecret;
};

VerifierResult verifyConnectIdToken(const VerifyRequest& request, const EosConfig& config);
