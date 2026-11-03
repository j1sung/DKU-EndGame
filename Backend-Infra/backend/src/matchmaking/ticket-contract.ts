import { randomBytes } from "node:crypto";

export const TICKET_WAIT_MS = 180_000;
export const MATCHMAKING_MODE = "standard" as const;
export const MATCHMAKING_CLIENT_VERSION = "0.1.0" as const;

export type TicketContractErrorCode =
  | "INVALID_MODE"
  | "CLIENT_VERSION_UNSUPPORTED"
  | "INVALID_TICKET_ID";

export class TicketContractError extends Error {
  public constructor(public readonly code: TicketContractErrorCode) {
    super(code);
    this.name = "TicketContractError";
  }
}

export function parseMode(value: unknown): typeof MATCHMAKING_MODE {
  if (value !== MATCHMAKING_MODE) {
    throw new TicketContractError("INVALID_MODE");
  }
  return MATCHMAKING_MODE;
}

export function parseClientVersion(value: unknown): typeof MATCHMAKING_CLIENT_VERSION {
  if (value !== MATCHMAKING_CLIENT_VERSION) {
    throw new TicketContractError("CLIENT_VERSION_UNSUPPORTED");
  }
  return MATCHMAKING_CLIENT_VERSION;
}

const UUID_V7 = /^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

export function parseTicketId(value: unknown): string {
  if (typeof value !== "string" || !UUID_V7.test(value)) {
    throw new TicketContractError("INVALID_TICKET_ID");
  }
  return value.toLowerCase();
}

export function makeTicketId(timestampMs: number, random: Uint8Array = randomBytes(16)): string {
  if (!Number.isSafeInteger(timestampMs) || timestampMs < 0 ||
      timestampMs > 0xffffffffffff || random.length !== 16) {
    throw new RangeError("Invalid UUID v7 inputs");
  }
  const bytes = Uint8Array.from(random);
  let timestamp = BigInt(timestampMs);
  for (let index = 5; index >= 0; index -= 1) {
    bytes[index] = Number(timestamp & 0xffn);
    timestamp >>= 8n;
  }
  bytes[6] = (bytes[6]! & 0x0f) | 0x70;
  bytes[8] = (bytes[8]! & 0x3f) | 0x80;
  const hex = Buffer.from(bytes).toString("hex");
  return `${hex.slice(0, 8)}-${hex.slice(8, 12)}-${hex.slice(12, 16)}-${hex.slice(16, 20)}-${hex.slice(20)}`;
}
