import { describe, expect, it } from "vitest";

import { parseSmokeOptions, runMatchmakingSmoke, safeResponseSummary } from "../../scripts/invoke-matchmaking-smoke.mjs";

describe("matchmaking smoke CLI", () => {
  it("requires an explicit API URL and a supported scenario", () => {
    expect(parseSmokeOptions([])).toBeNull();
    expect(parseSmokeOptions(["--url", "https://example.test", "--scenario", "full"])).toEqual({
      apiUrl: "https://example.test/", scenario: "full",
    });
    expect(parseSmokeOptions(["--url", "https://example.test", "--scenario", "destroy"])).toBeNull();
    expect(parseSmokeOptions(["--url", "https://example.test", "--scenario", "leave"])).toEqual({
      apiUrl: "https://example.test/", scenario: "leave",
    });
  });

  it("summarizes failures without logging response bodies or bearer tokens", () => {
    expect(safeResponseSummary(409, { error: { code: "ALREADY_ACTIVE", message: "Bearer secret" }, requestId: "req-1", token: "secret" }))
      .toBe("HTTP 409 ALREADY_ACTIVE requestId=req-1");
  });

  it("rejects duplicate accounts before any write and never prints tokens", async () => {
    const token = "secret.header.signature";
    const writes: string[] = [];
    let output = "";
    const result = await runMatchmakingSmoke({
      argv: ["--url", "https://example.test", "--scenario", "full"],
      prompt: async () => token,
      output: { write: (chunk: string) => { output += chunk; } },
      fetchImpl: async (url: URL, init: RequestInit) => {
        if (init.method !== "GET") writes.push(url.pathname);
        if (url.pathname === "/v1/me") return { status: 200, json: async () => ({ playerId: "same-player", nickname: "name", requestId: "req-1" }) };
        return { status: 200, json: async () => ({ ticket: null, requestId: "req-2" }) };
      },
    });
    expect(result).toBe(1);
    expect(writes).toEqual([]);
    expect(output).toContain("same PUID");
    expect(output).not.toContain(token);
  });
});
