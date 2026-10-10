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

  return { verifier, secretProvider, logger };
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
