import { describe, expect, it } from "vitest";

import { deadlineIndexKeys, phaseDurationMs } from "../../src/matchmaking/match-contract.js";

describe("match contract", () => {
  it("uses a fresh 60 second deadline for each preparation phase and 30 minutes in game", () => {
    expect(phaseDurationMs("FORMING")).toBe(60_000);
    expect(phaseDurationMs("SESSION_READY")).toBe(60_000);
    expect(phaseDurationMs("READY_TO_START")).toBe(60_000);
    expect(phaseDurationMs("IN_GAME")).toBe(1_800_000);
  });

  it("builds a time ordered sparse deadline key in one of 16 buckets", () => {
    const keys = deadlineIndexKeys("019a3185-c000-7aba-8bab-abababababab", 1_800_000_060_000);
    expect(keys.deadlineKey).toMatch(/^ACTIVE#(?:[0-9]|1[0-5])$/);
    expect(keys.deadlineSortKey).toBe("1800000060000#019a3185-c000-7aba-8bab-abababababab");
  });
});
