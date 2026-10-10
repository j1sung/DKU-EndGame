import { describe, expect, it } from "vitest";

import { AuthError } from "../../src/auth/auth-errors.js";
import { parseAuthorization } from "../../src/auth/parse-authorization.js";

const PLAYER_ID = "0123456789abcdef0123456789abcdef";

function encodeJson(value: unknown): string {
  return Buffer.from(JSON.stringify(value), "utf8").toString("base64url");
}

function makeToken(payload: unknown = { sub: PLAYER_ID }): string {
  return `${encodeJson({ alg: "RS256", typ: "JWT" })}.${encodeJson(payload)}.signature`;
}

function expectAuthError(
  action: () => unknown,
  code: string,
  statusCode = 401,
): void {
  try {
    action();
    throw new Error("Expected parseAuthorization to throw");
  } catch (error) {
    expect(error).toBeInstanceOf(AuthError);
    expect(error).toMatchObject({ code, statusCode });
  }
}

describe("parseAuthorization", () => {
  it("rejects a missing Authorization header", () => {
    expectAuthError(() => parseAuthorization(undefined), "AUTH_MISSING");
    expectAuthError(() => parseAuthorization({}), "AUTH_MISSING");
  });

  it.each([
    ["wrong scheme", `Basic ${makeToken()}`],
    ["comma-separated duplicate values", `Bearer ${makeToken()}, Bearer ${makeToken()}`],
    ["empty token", "Bearer "],
    ["extra fields", `Bearer ${makeToken()} extra`],
  ])("rejects malformed Bearer syntax: %s", (_name, authorization) => {
    expectAuthError(
      () => parseAuthorization({ authorization }),
      "AUTH_MALFORMED",
    );
  });

  it.each([
    ["two JWT segments", "abc.def"],
    ["an empty JWT segment", "abc..def"],
    ["invalid base64url payload", "abc.***.def"],
    ["non-JSON payload", `abc.${Buffer.from("not-json").toString("base64url")}.def`],
    ["array payload", makeToken([PLAYER_ID])],
    ["null payload", makeToken(null)],
    ["missing sub", makeToken({ aud: "client" })],
    ["non-string sub", makeToken({ sub: 123 })],
    ["non-PUID sub", makeToken({ sub: "not-a-puid" })],
  ])("rejects an invalid token: %s", (_name, token) => {
    expectAuthError(
      () => parseAuthorization({ authorization: `Bearer ${token}` }),
      "AUTH_INVALID",
    );
  });

  it("rejects a token larger than 16 KiB", () => {
    const baseToken = makeToken();
    const token = `${baseToken}${"a".repeat(16_385 - Buffer.byteLength(baseToken, "utf8"))}`;

    expect(Buffer.byteLength(token, "utf8")).toBe(16_385);
    expectAuthError(
      () => parseAuthorization({ authorization: `Bearer ${token}` }),
      "AUTH_INVALID",
    );
  });

  it("accepts a token exactly 16 KiB long", () => {
    const baseToken = makeToken();
    const token = `${baseToken}${"a".repeat(16_384 - Buffer.byteLength(baseToken, "utf8"))}`;

    expect(Buffer.byteLength(token, "utf8")).toBe(16_384);
    expect(parseAuthorization({ authorization: `Bearer ${token}` })).toEqual({
      token,
      candidatePlayerId: PLAYER_ID,
    });
  });

  it("accepts a case-insensitive Bearer scheme and preserves the token", () => {
    const token = makeToken();

    expect(parseAuthorization({ Authorization: `  bEaReR\t${token}  ` })).toEqual({
      token,
      candidatePlayerId: PLAYER_ID,
    });
  });

  it("rejects two Authorization keys with different casing", () => {
    const token = makeToken();

    expectAuthError(
      () =>
        parseAuthorization({
          Authorization: `Bearer ${token}`,
          authorization: `Bearer ${token}`,
        }),
      "AUTH_MALFORMED",
    );
  });
});
