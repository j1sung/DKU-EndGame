import { AuthError } from "./auth-errors.js";

const MAX_TOKEN_BYTES = 16_384;
const PRODUCT_USER_ID_PATTERN = /^[0-9a-f]{32}$/i;
const BASE64URL_PATTERN = /^[A-Za-z0-9_-]+$/;

export interface ParsedAuthorization {
  token: string;
  candidatePlayerId: string;
}

function findAuthorizationHeader(
  headers: Record<string, string | undefined> | undefined,
): string {
  if (headers === undefined) {
    throw new AuthError("AUTH_MISSING", 401, "Authentication required");
  }

  const values = Object.entries(headers)
    .filter(([name]) => name.toLowerCase() === "authorization")
    .map(([, value]) => value);

  if (values.length === 0) {
    throw new AuthError("AUTH_MISSING", 401, "Authentication required");
  }

  if (values.length !== 1 || values[0] === undefined || values[0].length === 0) {
    throw new AuthError("AUTH_MALFORMED", 401, "Authentication failed");
  }

  return values[0];
}

function readCandidatePlayerId(token: string): string {
  if (Buffer.byteLength(token, "utf8") > MAX_TOKEN_BYTES) {
    throw new AuthError("AUTH_INVALID", 401, "Authentication failed");
  }

  const segments = token.split(".");
  if (segments.length !== 3 || segments.some((segment) => segment.length === 0)) {
    throw new AuthError("AUTH_INVALID", 401, "Authentication failed");
  }

  const payloadSegment = segments[1];
  if (payloadSegment === undefined || !BASE64URL_PATTERN.test(payloadSegment)) {
    throw new AuthError("AUTH_INVALID", 401, "Authentication failed");
  }

  try {
    const payload: unknown = JSON.parse(
      Buffer.from(payloadSegment, "base64url").toString("utf8"),
    );

    if (payload === null || Array.isArray(payload) || typeof payload !== "object") {
      throw new Error("JWT payload is not an object");
    }

    const candidatePlayerId = (payload as Record<string, unknown>).sub;
    if (
      typeof candidatePlayerId !== "string" ||
      !PRODUCT_USER_ID_PATTERN.test(candidatePlayerId)
    ) {
      throw new Error("JWT subject is not a Product User ID");
    }

    return candidatePlayerId;
  } catch {
    throw new AuthError("AUTH_INVALID", 401, "Authentication failed");
  }
}

export function parseAuthorization(
  headers: Record<string, string | undefined> | undefined,
): ParsedAuthorization {
  const authorization = findAuthorizationHeader(headers);
  const match = /^\s*Bearer[\t ]+(\S+)\s*$/i.exec(authorization);

  if (match === null || match[1] === undefined || match[1].includes(",")) {
    throw new AuthError("AUTH_MALFORMED", 401, "Authentication failed");
  }

  const token = match[1];
  return {
    token,
    candidatePlayerId: readCandidatePlayerId(token),
  };
}
