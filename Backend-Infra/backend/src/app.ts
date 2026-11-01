import type {
  APIGatewayProxyEventV2,
  APIGatewayProxyHandlerV2,
} from "aws-lambda";
import { createHash } from "node:crypto";

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
import { parseClientVersion, parseMode, parseTicketId, TicketContractError } from "./matchmaking/ticket-contract.js";
import { TicketStoreError } from "./matchmaking/dynamo-ticket-store.js";
import type { TicketStore } from "./matchmaking/ticket-store.js";
import { ProfileStoreError } from "./profile/dynamo-profile-store.js";
import { NicknameValidationError, normalizeNickname } from "./profile/nickname.js";
import type { ProfileStore } from "./profile/profile-store.js";

export interface HandlerDependencies {
  verifier: TokenVerifier;
  secretProvider: ClientSecretProvider;
  logger: SafeLogger;
  profileStore: ProfileStore;
  ticketStore: TicketStore;
}

class RequestError extends Error {
  public constructor(public readonly code: string) {
    super(code);
  }
}

function header(headers: Record<string, string | undefined>, name: string): string | undefined {
  return Object.entries(headers).find(([key]) => key.toLowerCase() === name)?.[1];
}

function nicknameRequest(event: APIGatewayProxyEventV2): {
  idempotencyKey: string;
  nickname: string;
  nicknameKey: string;
  requestHash: string;
} {
  const idempotencyKey = header(event.headers, "idempotency-key");
  if (idempotencyKey === undefined ||
      !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(idempotencyKey)) {
    throw new RequestError("INVALID_IDEMPOTENCY_KEY");
  }
  try {
    const rawBody = event.isBase64Encoded
      ? Buffer.from(event.body ?? "", "base64").toString("utf8")
      : event.body ?? "";
    const parsed = JSON.parse(rawBody) as unknown;
    if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) {
      throw new NicknameValidationError();
    }
    const { nickname, nicknameKey } = normalizeNickname((parsed as { nickname?: unknown }).nickname);
    const requestHash = createHash("sha256")
      .update(`POST /v1/me/nickname\0${rawBody}`, "utf8")
      .digest("hex");
    return { idempotencyKey, nickname, nicknameKey, requestHash };
  } catch (error) {
    if (error instanceof NicknameValidationError) {
      throw error;
    }
    throw new NicknameValidationError();
  }
}

function mutationRequest(event: APIGatewayProxyEventV2, route: string): { idempotencyKey: string; requestHash: string; body: Record<string, unknown> } {
  const idempotencyKey = header(event.headers, "idempotency-key");
  if (!idempotencyKey || !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(idempotencyKey)) {
    throw new RequestError("INVALID_IDEMPOTENCY_KEY");
  }
  const rawBody = event.isBase64Encoded ? Buffer.from(event.body ?? "", "base64").toString("utf8") : event.body ?? "";
  if (Buffer.byteLength(rawBody, "utf8") > 4096) throw new RequestError("INVALID_BODY");
  let body: unknown;
  try { body = JSON.parse(rawBody); } catch { throw new RequestError("INVALID_BODY"); }
  if (!body || typeof body !== "object" || Array.isArray(body)) throw new RequestError("INVALID_BODY");
  return { idempotencyKey, requestHash: createHash("sha256").update(`${route}\0${rawBody}`, "utf8").digest("hex"), body: body as Record<string, unknown> };
}

function profileError(error: unknown): { statusCode: number; code: string; message: string } {
  if (error instanceof TicketContractError) {
    return { statusCode: error.code === "CLIENT_VERSION_UNSUPPORTED" ? 426 : 400, code: error.code, message: "Invalid matchmaking request" };
  }
  if (error instanceof TicketStoreError) {
    const statusCode = error.code === "TICKET_NOT_FOUND" ? 404 : error.code === "TICKET_STORE_FAILURE" ? 500 : 409;
    return { statusCode, code: error.code, message: statusCode === 500 ? "Internal server error" : "Matchmaking conflict" };
  }
  if (error instanceof RequestError) {
    return { statusCode: error.code === "NICKNAME_REQUIRED" ? 409 : 400, code: error.code, message: error.code === "NICKNAME_REQUIRED" ? "Nickname required" : "Invalid request" };
  }
  if (error instanceof NicknameValidationError) {
    return { statusCode: 400, code: error.code, message: error.message };
  }
  if (error instanceof ProfileStoreError && error.code !== "PROFILE_STORE_FAILURE") {
    return { statusCode: 409, code: error.code, message: "Profile conflict" };
  }
  return { statusCode: 500, code: "PROFILE_STORE_FAILURE", message: "Internal server error" };
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

      let statusCode = 200;
      let response;
      try {
        if (route === "GET /v1/me") {
          const profile = await dependencies.profileStore.get(verified.playerId);
          response = jsonResponse(200, { ...profile, requestId });
        } else if (route === "POST /v1/me/nickname") {
          const input = nicknameRequest(event);
          const registered = await dependencies.profileStore.registerNickname({
            playerId: verified.playerId,
            requestId,
            ...input,
          });
          statusCode = 201;
          response = jsonResponse(201, {
            playerId: registered.playerId,
            nickname: registered.nickname,
            requestId: registered.requestId,
          });
        } else if (route === "POST /v1/matchmaking/tickets") {
          parseClientVersion(header(event.headers, "x-client-version"));
          const input = mutationRequest(event, route);
          parseMode(input.body.mode);
          const profile = await dependencies.profileStore.get(verified.playerId);
          if (!profile.nickname) throw new RequestError("NICKNAME_REQUIRED");
          const created = await dependencies.ticketStore.create({ playerId: verified.playerId, requestId, idempotencyKey: input.idempotencyKey, requestHash: input.requestHash });
          statusCode = 201;
          response = jsonResponse(201, created);
        } else if (route === "GET /v1/matchmaking/tickets/current") {
          const current = await dependencies.ticketStore.current(verified.playerId);
          response = jsonResponse(200, current ? { ...current, requestId } : { ticket: null, requestId });
        } else if (route === "DELETE /v1/matchmaking/tickets/current") {
          const input = mutationRequest(event, route);
          const ticketId = parseTicketId(input.body.ticketId);
          const cancelled = await dependencies.ticketStore.cancel({ playerId: verified.playerId, requestId, ticketId, idempotencyKey: input.idempotencyKey, requestHash: input.requestHash });
          response = jsonResponse(200, cancelled);
        } else {
          statusCode = 404;
          response = errorResponse(404, "NOT_FOUND", "Not found", requestId);
        }
      } catch (error) {
        const mapped = profileError(error);
        statusCode = mapped.statusCode;
        response = errorResponse(mapped.statusCode, mapped.code, mapped.message, requestId);
      }

      dependencies.logger.write({
        requestId,
        route,
        statusCode,
        authOutcome: "success",
        playerId: verified.playerId,
        verificationMs: verified.verificationMs,
      });

      return response;
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
