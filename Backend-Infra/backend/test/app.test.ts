import type {
  APIGatewayProxyEventV2,
  APIGatewayProxyHandlerV2,
  APIGatewayProxyStructuredResultV2,
  Context,
} from "aws-lambda";
import { describe, expect, it, vi } from "vitest";

import { createHandler } from "../src/app.js";
import type { ClientSecretProvider } from "../src/auth/secret-provider.js";
import {
  TokenVerificationError,
  type TokenVerifier,
} from "../src/auth/token-verifier.js";
import type { SafeLogger } from "../src/logging.js";
import type { ProfileStore } from "../src/profile/profile-store.js";
import { ProfileStoreError } from "../src/profile/dynamo-profile-store.js";
import type { TicketStore } from "../src/matchmaking/ticket-store.js";

const PLAYER_ID = "0123456789abcdef0123456789abcdef";
const OTHER_PLAYER_ID = "fedcba9876543210fedcba9876543210";
const CLIENT_SECRET = "test-client-secret";

function encodeJson(value: unknown): string {
  return Buffer.from(JSON.stringify(value), "utf8").toString("base64url");
}

function makeToken(playerId = PLAYER_ID): string {
  return `${encodeJson({ alg: "RS256", typ: "JWT" })}.${encodeJson({ sub: playerId })}.signature`;
}

function makeEvent(
  overrides: Partial<APIGatewayProxyEventV2> = {},
): APIGatewayProxyEventV2 {
  return {
    version: "2.0",
    routeKey: "GET /v1/me",
    rawPath: "/v1/me",
    rawQueryString: "",
    headers: { authorization: `Bearer ${makeToken()}` },
    requestContext: {
      accountId: "test-account",
      apiId: "test-api",
      domainName: "example.test",
      domainPrefix: "example",
      http: {
        method: "GET",
        path: "/v1/me",
        protocol: "HTTP/1.1",
        sourceIp: "127.0.0.1",
        userAgent: "vitest",
      },
      requestId: "api-request-id",
      routeKey: "GET /v1/me",
      stage: "$default",
      time: "10/Oct/2026:00:00:00 +0000",
      timeEpoch: 1_791_590_400_000,
    },
    isBase64Encoded: false,
    ...overrides,
  };
}

function makeDependencies() {
  const verifier: TokenVerifier = {
    verify: vi.fn().mockResolvedValue({
      playerId: PLAYER_ID,
      verificationMs: 12,
    }),
  };
  const secretProvider: ClientSecretProvider = {
    getClientSecret: vi.fn().mockResolvedValue(CLIENT_SECRET),
  };
  const logger: SafeLogger = {
    write: vi.fn(),
  };
  const profileStore: ProfileStore = {
    get: vi.fn().mockImplementation(async (playerId: string) => ({ playerId })),
    registerNickname: vi.fn().mockImplementation(async ({ playerId, nickname, requestId }) => ({
      playerId,
      nickname,
      requestId,
      created: true,
    })),
  };
  const ticketStore: TicketStore = {
    create: vi.fn().mockImplementation(async ({ requestId }) => ({ ticketId: "019a3185-c000-7aba-8bab-abababababab", status: "WAITING", expiresAt: "2026-11-01T12:03:00.000Z", requestId })),
    current: vi.fn().mockResolvedValue(null),
    cancel: vi.fn().mockImplementation(async ({ requestId, ticketId }) => ({ ticketId, status: "CANCELLED", requestId })),
  };

  return { verifier, secretProvider, logger, profileStore, ticketStore };
}

function nicknameEvent(body: unknown, headers: Record<string, string> = {}) {
  return makeEvent({
    routeKey: "POST /v1/me/nickname",
    rawPath: "/v1/me/nickname",
    headers: {
      authorization: `Bearer ${makeToken()}`,
      "idempotency-key": "123e4567-e89b-42d3-a456-426614174000",
      ...headers,
    },
    body: JSON.stringify(body),
  });
}

async function invoke(
  handler: APIGatewayProxyHandlerV2,
  event: APIGatewayProxyEventV2,
): Promise<APIGatewayProxyStructuredResultV2> {
  const result = await handler(event, {} as Context, () => undefined);
  if (result === undefined || typeof result === "string") {
    throw new Error("Expected a structured API Gateway response");
  }
  return result;
}

function parseBody(result: APIGatewayProxyStructuredResultV2): unknown {
  if (result.body === undefined) {
    throw new Error("Expected a JSON response body");
  }
  return JSON.parse(result.body) as unknown;
}

describe("createHandler", () => {
  it("requires a nickname and exact client version before creating a ticket", async () => {
    const dependencies = makeDependencies();
    const event = makeEvent({ routeKey: "POST /v1/matchmaking/tickets", rawPath: "/v1/matchmaking/tickets", headers: { authorization: `Bearer ${makeToken()}`, "idempotency-key": "123e4567-e89b-42d3-a456-426614174000", "x-client-version": "0.1.0" }, body: JSON.stringify({ mode: "standard" }) });
    const first = await invoke(createHandler(dependencies), event);
    expect(first.statusCode).toBe(409);
    expect(parseBody(first)).toMatchObject({ error: { code: "NICKNAME_REQUIRED" } });
    expect(dependencies.ticketStore.create).not.toHaveBeenCalled();
    vi.mocked(dependencies.profileStore.get).mockResolvedValue({ playerId: PLAYER_ID, nickname: "닭싸움왕" });
    const second = await invoke(createHandler(dependencies), event);
    expect(second.statusCode).toBe(201);
    expect(parseBody(second)).toMatchObject({ status: "WAITING", requestId: "api-request-id" });
    expect(dependencies.ticketStore.create).toHaveBeenCalledWith(expect.objectContaining({ playerId: PLAYER_ID, requestHash: expect.stringMatching(/^[a-f0-9]{64}$/) }));
    const wrong = await invoke(createHandler(dependencies), { ...event, headers: { ...event.headers, "x-client-version": "0.2.0" } });
    expect(wrong.statusCode).toBe(426);
    expect(parseBody(wrong)).toMatchObject({ error: { code: "CLIENT_VERSION_UNSUPPORTED" } });
  });

  it("gets the caller's current ticket and cancels only a specified waiting ticket", async () => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.ticketStore.current).mockResolvedValue({ ticketId: "019a3185-c000-7aba-8bab-abababababab", status: "WAITING", expiresAt: "2026-11-01T12:03:00.000Z" });
    const getResult = await invoke(createHandler(dependencies), makeEvent({ routeKey: "GET /v1/matchmaking/tickets/current", rawPath: "/v1/matchmaking/tickets/current" }));
    expect(getResult.statusCode).toBe(200);
    expect(parseBody(getResult)).toMatchObject({ status: "WAITING", requestId: "api-request-id" });
    expect(dependencies.ticketStore.current).toHaveBeenCalledWith(PLAYER_ID);
    const deleteResult = await invoke(createHandler(dependencies), makeEvent({ routeKey: "DELETE /v1/matchmaking/tickets/current", rawPath: "/v1/matchmaking/tickets/current", headers: { authorization: `Bearer ${makeToken()}`, "idempotency-key": "123e4567-e89b-42d3-a456-426614174000" }, body: JSON.stringify({ ticketId: "019a3185-c000-7aba-8bab-abababababab", playerId: OTHER_PLAYER_ID }) }));
    expect(deleteResult.statusCode).toBe(200);
    expect(parseBody(deleteResult)).toMatchObject({ status: "CANCELLED" });
    expect(dependencies.ticketStore.cancel).toHaveBeenCalledWith(expect.objectContaining({ playerId: PLAYER_ID, ticketId: "019a3185-c000-7aba-8bab-abababababab" }));
  });

  it("rejects invalid ticket requests without calling the store", async () => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.profileStore.get).mockResolvedValue({ playerId: PLAYER_ID, nickname: "닭싸움왕" });
    const create = makeEvent({ routeKey: "POST /v1/matchmaking/tickets", rawPath: "/v1/matchmaking/tickets", headers: { authorization: `Bearer ${makeToken()}`, "idempotency-key": "123e4567-e89b-42d3-a456-426614174000", "x-client-version": "0.1.0" }, body: JSON.stringify({ mode: "unknown" }) });
    expect(parseBody(await invoke(createHandler(dependencies), create))).toMatchObject({ error: { code: "INVALID_MODE" } });
    const cancel = makeEvent({ routeKey: "DELETE /v1/matchmaking/tickets/current", rawPath: "/v1/matchmaking/tickets/current", headers: { authorization: `Bearer ${makeToken()}`, "idempotency-key": "123e4567-e89b-42d3-a456-426614174000" }, body: JSON.stringify({ ticketId: "bad" }) });
    expect(parseBody(await invoke(createHandler(dependencies), cancel))).toMatchObject({ error: { code: "INVALID_TICKET_ID" } });
    const large = { ...create, body: JSON.stringify({ mode: "standard", padding: "x".repeat(5000) }) };
    expect(parseBody(await invoke(createHandler(dependencies), large))).toMatchObject({ error: { code: "INVALID_BODY" } });
    expect(dependencies.ticketStore.create).not.toHaveBeenCalled();
    expect(dependencies.ticketStore.cancel).not.toHaveBeenCalled();
  });

  it("does not touch ticket storage when authentication fails", async () => {
    const dependencies = makeDependencies();
    const event = makeEvent({ routeKey: "GET /v1/matchmaking/tickets/current", rawPath: "/v1/matchmaking/tickets/current", headers: {} });
    const result = await invoke(createHandler(dependencies), event);
    expect(result.statusCode).toBe(401);
    expect(dependencies.ticketStore.current).not.toHaveBeenCalled();
  });
  it("returns the verified player identity and API Gateway request ID", async () => {
    const dependencies = makeDependencies();
    const handler = createHandler(dependencies);

    const result = await invoke(handler, makeEvent());

    expect(result.statusCode).toBe(200);
    expect(result.headers).toMatchObject({ "content-type": "application/json" });
    expect(parseBody(result)).toEqual({
      playerId: PLAYER_ID,
      requestId: "api-request-id",
    });
    expect(dependencies.verifier.verify).toHaveBeenCalledWith({
      token: makeToken(),
      candidatePlayerId: PLAYER_ID,
      clientSecret: CLIENT_SECRET,
    });
  });

  it("generates a UUID request ID when API Gateway does not provide one", async () => {
    const dependencies = makeDependencies();
    const handler = createHandler(dependencies);
    const event = makeEvent({
      requestContext: {
        ...makeEvent().requestContext,
        requestId: "",
      },
    });

    const result = await invoke(handler, event);
    const body = parseBody(result) as { requestId: string };

    expect(body.requestId).toMatch(
      /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i,
    );
  });

  it("ignores player IDs supplied in the query string or request body", async () => {
    const dependencies = makeDependencies();
    const handler = createHandler(dependencies);
    const result = await invoke(
      handler,
      makeEvent({
        rawQueryString: `playerId=${OTHER_PLAYER_ID}`,
        queryStringParameters: { playerId: OTHER_PLAYER_ID },
        body: JSON.stringify({ playerId: OTHER_PLAYER_ID }),
      }),
    );

    expect(parseBody(result)).toMatchObject({ playerId: PLAYER_ID });
  });

  it("includes an existing nickname in the authenticated profile response", async () => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.profileStore.get).mockResolvedValue({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
    });
    const result = await invoke(createHandler(dependencies), makeEvent());
    expect(parseBody(result)).toEqual({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
      requestId: "api-request-id",
    });
  });

  it("registers a nickname for the verified player and returns 201", async () => {
    const dependencies = makeDependencies();
    const result = await invoke(
      createHandler(dependencies),
      nicknameEvent({ nickname: "  ＣＨＩＣＫＥＮ   1 ", playerId: OTHER_PLAYER_ID }),
    );
    expect(result.statusCode).toBe(201);
    expect(parseBody(result)).toEqual({
      playerId: PLAYER_ID,
      nickname: "CHICKEN 1",
      requestId: "api-request-id",
    });
    expect(dependencies.profileStore.registerNickname).toHaveBeenCalledWith(
      expect.objectContaining({
        playerId: PLAYER_ID,
        nickname: "CHICKEN 1",
        nicknameKey: "chicken 1",
        idempotencyKey: "123e4567-e89b-42d3-a456-426614174000",
        requestHash: expect.stringMatching(/^[a-f0-9]{64}$/),
        requestId: "api-request-id",
      }),
    );
  });

  it("replays the original request ID on a same-key retry", async () => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.profileStore.registerNickname).mockResolvedValue({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
      requestId: "original-request-id",
      created: false,
    });
    const result = await invoke(createHandler(dependencies), nicknameEvent({ nickname: "닭싸움왕" }));
    expect(parseBody(result)).toEqual({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
      requestId: "original-request-id",
    });
  });

  it.each([
    [nicknameEvent({ nickname: "!" }), 400, "INVALID_NICKNAME"],
    [nicknameEvent({ nickname: "닭싸움왕" }, { "idempotency-key": "bad" }), 400, "INVALID_IDEMPOTENCY_KEY"],
    [nicknameEvent({ nickname: "닭싸움왕" }, { "idempotency-key": "" }), 400, "INVALID_IDEMPOTENCY_KEY"],
  ])("rejects invalid nickname requests before a write", async (event, status, code) => {
    const dependencies = makeDependencies();
    const result = await invoke(createHandler(dependencies), event);
    expect(result.statusCode).toBe(status);
    expect(parseBody(result)).toMatchObject({ error: { code } });
    expect(dependencies.profileStore.registerNickname).not.toHaveBeenCalled();
  });

  it.each([
    ["NICKNAME_TAKEN", 409],
    ["NICKNAME_ALREADY_SET", 409],
    ["IDEMPOTENCY_KEY_REUSED", 409],
  ] as const)("maps %s to a safe conflict response", async (code, status) => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.profileStore.registerNickname).mockRejectedValue(
      new ProfileStoreError(code),
    );
    const result = await invoke(createHandler(dependencies), nicknameEvent({ nickname: "닭싸움왕" }));
    expect(result.statusCode).toBe(status);
    expect(parseBody(result)).toMatchObject({ error: { code }, requestId: "api-request-id" });
  });

  it.each([
    ["missing", {}, "AUTH_MISSING"],
    ["malformed", { authorization: "Basic abc" }, "AUTH_MALFORMED"],
    ["invalid", { authorization: "Bearer abc.def.ghi" }, "AUTH_INVALID"],
  ])(
    "rejects %s authorization before reading a secret or starting verification",
    async (_name, headers, expectedCode) => {
      const dependencies = makeDependencies();
      const handler = createHandler(dependencies);

      const result = await invoke(handler, makeEvent({ headers }));

      expect(result.statusCode).toBe(401);
      expect(parseBody(result)).toEqual({
        error: {
          code: expectedCode,
          message:
            expectedCode === "AUTH_MISSING"
              ? "Authentication required"
              : "Authentication failed",
        },
        requestId: "api-request-id",
      });
      expect(dependencies.secretProvider.getClientSecret).not.toHaveBeenCalled();
      expect(dependencies.verifier.verify).not.toHaveBeenCalled();
    },
  );

  it.each([
    ["invalid", 401, "AUTH_INVALID"],
    ["unavailable", 503, "AUTH_VERIFIER_UNAVAILABLE"],
    ["failure", 500, "AUTH_VERIFIER_FAILURE"],
  ] as const)(
    "maps verifier %s errors to a safe HTTP response",
    async (kind, expectedStatus, expectedCode) => {
      const dependencies = makeDependencies();
      vi.mocked(dependencies.verifier.verify).mockRejectedValue(
        new TokenVerificationError(kind),
      );
      const handler = createHandler(dependencies);

      const result = await invoke(handler, makeEvent());

      expect(result.statusCode).toBe(expectedStatus);
      expect(parseBody(result)).toEqual({
        error: {
          code: expectedCode,
          message:
            kind === "unavailable"
              ? "Authentication service unavailable"
              : "Authentication failed",
        },
        requestId: "api-request-id",
      });
    },
  );

  it("logs only allow-listed request metadata", async () => {
    const dependencies = makeDependencies();
    const handler = createHandler(dependencies);

    await invoke(handler, makeEvent());

    expect(dependencies.logger.write).toHaveBeenCalledWith({
      requestId: "api-request-id",
      route: "GET /v1/me",
      statusCode: 200,
      authOutcome: "success",
      playerId: PLAYER_ID,
      verificationMs: 12,
    });
    const serializedLogs = JSON.stringify(
      vi.mocked(dependencies.logger.write).mock.calls,
    );
    expect(serializedLogs).not.toContain(makeToken());
    expect(serializedLogs).not.toContain(CLIENT_SECRET);
  });

  it("hides Secret provider failures from the response and logs", async () => {
    const dependencies = makeDependencies();
    vi.mocked(dependencies.secretProvider.getClientSecret).mockRejectedValue(
      new Error(`failed to load ${CLIENT_SECRET}`),
    );
    const handler = createHandler(dependencies);

    const result = await invoke(handler, makeEvent());

    expect(result.statusCode).toBe(500);
    expect(parseBody(result)).toEqual({
      error: {
        code: "INTERNAL_CONFIGURATION_ERROR",
        message: "Internal server error",
      },
      requestId: "api-request-id",
    });
    const serialized = JSON.stringify({
      body: result.body,
      logs: vi.mocked(dependencies.logger.write).mock.calls,
    });
    expect(serialized).not.toContain(CLIENT_SECRET);
  });
});
