import type {
  APIGatewayProxyEventV2,
  APIGatewayProxyHandlerV2,
} from "aws-lambda";

import { AuthError } from "./auth/auth-errors.js";
import { parseAuthorization } from "./auth/parse-authorization.js";
import type { ClientSecretProvider } from "./auth/secret-provider.js";
import {
  TokenVerificationError,
  type TokenVerifier,
} from "./auth/token-verifier.js";
import { getRequestId } from "./http/request-context.js";
import { errorResponse, jsonResponse } from "./http/responses.js";
import type { AuthOutcome, SafeLogger } from "./logging.js";

export interface HandlerDependencies {
  verifier: TokenVerifier;
  secretProvider: ClientSecretProvider;
  logger: SafeLogger;
}

interface ErrorMapping {
  statusCode: number;
  code: string;
  message: string;
  authOutcome: Exclude<AuthOutcome, "success">;
}

function routeName(event: APIGatewayProxyEventV2): string {
  return event.routeKey.length > 0
    ? event.routeKey
    : `${event.requestContext.http.method} ${event.rawPath}`;
}

function mapError(error: unknown): ErrorMapping {
  if (error instanceof AuthError) {
    const outcomes: Record<AuthError["code"], ErrorMapping["authOutcome"]> = {
      AUTH_MISSING: "missing_header",
      AUTH_MALFORMED: "malformed_header",
      AUTH_INVALID: "invalid_token",
    };
    return {
      statusCode: error.statusCode,
      code: error.code,
      message: error.message,
      authOutcome: outcomes[error.code],
    };
  }

  if (error instanceof TokenVerificationError) {
    if (error.kind === "invalid") {
      return {
        statusCode: 401,
        code: "AUTH_INVALID",
        message: "Authentication failed",
        authOutcome: "invalid_token",
      };
    }
    if (error.kind === "unavailable") {
      return {
        statusCode: 503,
        code: "AUTH_VERIFIER_UNAVAILABLE",
        message: "Authentication service unavailable",
        authOutcome: "verifier_unavailable",
      };
    }
    return {
      statusCode: 500,
      code: "AUTH_VERIFIER_FAILURE",
      message: "Authentication failed",
      authOutcome: "verifier_failure",
    };
  }

  return {
    statusCode: 500,
    code: "INTERNAL_CONFIGURATION_ERROR",
    message: "Internal server error",
    authOutcome: "configuration_error",
  };
}

export function createHandler(
  dependencies: HandlerDependencies,
): APIGatewayProxyHandlerV2 {
  return async (event) => {
    const requestId = getRequestId(event);
    const route = routeName(event);

    try {
      const { token, candidatePlayerId } = parseAuthorization(event.headers);
      const clientSecret = await dependencies.secretProvider.getClientSecret();
      const verified = await dependencies.verifier.verify({
        token,
        candidatePlayerId,
        clientSecret,
      });

      dependencies.logger.write({
        requestId,
        route,
        statusCode: 200,
        authOutcome: "success",
        playerId: verified.playerId,
        verificationMs: verified.verificationMs,
      });

      return jsonResponse(200, {
        playerId: verified.playerId,
        requestId,
      });
    } catch (error) {
      const mapped = mapError(error);
      dependencies.logger.write({
        requestId,
        route,
        statusCode: mapped.statusCode,
        authOutcome: mapped.authOutcome,
      });
      return errorResponse(
        mapped.statusCode,
        mapped.code,
        mapped.message,
        requestId,
      );
    }
  };
}
