import { describe, expect, it } from "vitest";

import {
  TICKET_WAIT_MS,
  makeTicketId,
  parseClientVersion,
  parseMode,
  parseTicketId,
} from "../../src/matchmaking/ticket-contract.js";

describe("matchmaking ticket contract", () => {
  it("accepts only the first four-player mode and exact client version", () => {
    expect(parseMode("standard")).toBe("standard");
    expect(parseClientVersion("0.1.0")).toBe("0.1.0");
    expect(() => parseMode("duo")).toThrowError(/INVALID_MODE/);
    expect(() => parseMode(3)).toThrowError(/INVALID_MODE/);
    expect(() => parseClientVersion("0.2.0")).toThrowError(/CLIENT_VERSION_UNSUPPORTED/);
    expect(() => parseClientVersion(undefined)).toThrowError(/CLIENT_VERSION_UNSUPPORTED/);
  });

  it("uses a three-minute waiting deadline", () => {
    expect(TICKET_WAIT_MS).toBe(180_000);
  });

  it("creates a UUID v7 with the supplied millisecond timestamp", () => {
    const timestamp = 1_800_000_000_000;
    const id = makeTicketId(timestamp, new Uint8Array(16).fill(0xab));
    expect(id).toMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-7[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/);
    expect(parseTicketId(id)).toBe(id);
    const timeHex = id.replaceAll("-", "").slice(0, 12);
    expect(Number.parseInt(timeHex, 16)).toBe(timestamp);
    expect(() => parseTicketId("not-a-ticket")).toThrowError(/INVALID_TICKET_ID/);
  });
});
