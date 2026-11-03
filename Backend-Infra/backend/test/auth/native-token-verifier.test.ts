import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";

import { describe, expect, it, vi } from "vitest";

import { NativeTokenVerifier } from "../../src/auth/native-token-verifier.js";
import { TokenVerificationError } from "../../src/auth/token-verifier.js";

const PLAYER_ID = "0123456789abcdef0123456789abcdef";
const OTHER_PLAYER_ID = "fedcba9876543210fedcba9876543210";
const TOKEN = "header.payload.signature";
const SECRET = "client-secret-must-not-leak";
const fixture = (name: string) =>
  fileURLToPath(new URL(`../fixtures/${name}`, import.meta.url));

function options(script: string, ...args: string[]) {
  return {
    executablePath: process.execPath,
    executableArguments: [fixture(script), ...args],
    eos: {
      productId: "product-id",
      sandboxId: "sandbox-id",
      deploymentId: "deployment-id",
      clientId: "client-id",
    },
    timeoutMs: 1_000,
    maxOutputBytes: 65_536,
  };
}

describe("NativeTokenVerifier", () => {
  it("passes credentials only through stdin and an allow-listed environment", async () => {
    const calls: Array<{ command: string; args: readonly string[]; options: unknown }> = [];
    const spawnProcess: typeof spawn = ((command, args, spawnOptions) => {
      calls.push({ command, args: args ?? [], options: spawnOptions });
      return spawn(command, args, spawnOptions);
    }) as typeof spawn;
    const verifier = new NativeTokenVerifier({
      ...options("verifier-success.mjs"),
      spawnProcess,
    });

    await expect(
      verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET }),
    ).resolves.toMatchObject({ playerId: PLAYER_ID });

    expect(calls).toHaveLength(1);
    expect(calls[0]?.command).toBe(process.execPath);
    expect(calls[0]?.args).not.toContain(TOKEN);
    expect(calls[0]?.args).not.toContain(SECRET);
    expect(calls[0]?.options).toMatchObject({
      shell: false,
      env: {
        EOS_PRODUCT_ID: "product-id",
        EOS_SANDBOX_ID: "sandbox-id",
        EOS_DEPLOYMENT_ID: "deployment-id",
        EOS_CLIENT_ID: "client-id",
        EOS_CLIENT_SECRET: SECRET,
      },
    });
  });

  it("rejects a successful process that returns a different PUID", async () => {
    const verifier = new NativeTokenVerifier({
      ...options("verifier-success.mjs", OTHER_PLAYER_ID),
    });

    await expect(
      verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET }),
    ).rejects.toMatchObject({ kind: "invalid" });
  });

  it.each([
    ["invalid", options("verifier-invalid.mjs"), "invalid"],
    ["unavailable", { ...options("verifier-invalid.mjs"), executableArguments: [fixture("verifier-invalid.mjs"), "unavailable"] }, "unavailable"],
    ["internal", { ...options("verifier-invalid.mjs"), executableArguments: [fixture("verifier-invalid.mjs"), "internal"] }, "failure"],
    ["signal", { ...options("verifier-invalid.mjs"), executableArguments: [fixture("verifier-invalid.mjs"), "signal"] }, "failure"],
    ["malformed output", { ...options("verifier-invalid.mjs"), executableArguments: [fixture("verifier-invalid.mjs"), "malformed"] }, "failure"],
  ])("maps %s process results", async (_name, verifierOptions, kind) => {
    const verifier = new NativeTokenVerifier(verifierOptions);
    await expect(
      verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET }),
    ).rejects.toMatchObject({ kind });
  });

  it("kills a verifier that exceeds its deadline", async () => {
    const verifier = new NativeTokenVerifier({
      ...options("verifier-hang.mjs"),
      timeoutMs: 40,
    });
    await expect(
      verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET }),
    ).rejects.toMatchObject({ kind: "unavailable" });
  });

  it("kills a verifier that exceeds its output limit", async () => {
    const verifier = new NativeTokenVerifier({
      ...options("verifier-flood.mjs"),
      maxOutputBytes: 1_024,
    });
    await expect(
      verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET }),
    ).rejects.toMatchObject({ kind: "failure" });
  });

  it("hides process errors and credentials", async () => {
    const verifier = new NativeTokenVerifier({
      ...options("verifier-success.mjs"),
      executablePath: "Z:/missing/eos-token-verifier",
    });
    let caught: unknown;
    try {
      await verifier.verify({ token: TOKEN, candidatePlayerId: PLAYER_ID, clientSecret: SECRET });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(TokenVerificationError);
    expect(caught).toMatchObject({ kind: "failure" });
    expect(JSON.stringify(caught)).not.toContain(TOKEN);
    expect(JSON.stringify(caught)).not.toContain(SECRET);
  });
});
