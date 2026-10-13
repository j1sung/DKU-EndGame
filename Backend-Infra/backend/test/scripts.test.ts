import { createHash } from "node:crypto";
import { PassThrough } from "node:stream";

import { describe, expect, it, vi } from "vitest";

import {
  readHiddenLine,
  runSetClientSecret,
} from "../scripts/set-client-secret.mjs";
import { runAuthSmoke } from "../scripts/invoke-auth-smoke.mjs";
import { runCloudWatchRedactionCheck } from "../scripts/check-cloudwatch-redaction.mjs";

function outputCollector() {
  let value = "";
  return {
    write(chunk: unknown) {
      value += String(chunk);
    },
    value: () => value,
  };
}

describe("operations scripts", () => {
  it("reads sensitive input without echoing it", async () => {
    const input = new PassThrough();
    const output = outputCollector();
    const pending = readHiddenLine("Client Secret: ", { input, output });

    input.end("super-sensitive-value\n");

    await expect(pending).resolves.toBe("super-sensitive-value");
    expect(output.value()).toBe("Client Secret: \n");
    expect(output.value()).not.toContain("super-sensitive-value");
  });

  it("updates Secrets Manager directly without exposing the secret", async () => {
    const secret = "secret-never-log-this";
    const sent: any[] = [];
    const client = { send: vi.fn(async (command: any) => sent.push(command)) };
    const output = outputCollector();
    const argv = ["--secret-id", "arn:aws:secretsmanager:region:account:secret:test"];

    const exitCode = await runSetClientSecret({
      argv,
      prompt: vi.fn(async () => secret),
      client,
      output,
    });

    expect(exitCode).toBe(0);
    expect(client.send).toHaveBeenCalledOnce();
    expect(sent[0].constructor.name).toBe("PutSecretValueCommand");
    expect(sent[0].input).toEqual({
      SecretId: argv[1],
      SecretString: secret,
    });
    expect(argv.join(" ")).not.toContain(secret);
    expect(output.value()).not.toContain(secret);
  });

  it("rejects sensitive or unknown command-line options before side effects", async () => {
    const secretClient = { send: vi.fn() };
    const fetchImpl = vi.fn();
    const logsClient = { send: vi.fn() };

    await expect(
      runSetClientSecret({
        argv: ["--secret-id", "safe-id", "--secret", "must-not-be-an-argument"],
        prompt: vi.fn(),
        client: secretClient,
        output: outputCollector(),
      }),
    ).resolves.toBe(1);
    await expect(
      runAuthSmoke({
        argv: ["--url", "https://example.test", "--token", "must-not-be-an-argument"],
        prompt: vi.fn(),
        fetchImpl,
        output: outputCollector(),
      }),
    ).resolves.toBe(1);
    await expect(
      runCloudWatchRedactionCheck({
        argv: [
          "--request-id",
          "request-123",
          "--function-name",
          "function-name",
          "--secret",
          "must-not-be-an-argument",
        ],
        prompt: vi.fn(),
        client: logsClient,
        output: outputCollector(),
      }),
    ).resolves.toBe(1);
    await expect(
      runCloudWatchRedactionCheck({
        argv: [
          "--request-id",
          'request-123" || *',
          "--function-name",
          "function-name",
        ],
        prompt: vi.fn(),
        client: logsClient,
        output: outputCollector(),
      }),
    ).resolves.toBe(1);

    expect(secretClient.send).not.toHaveBeenCalled();
    expect(fetchImpl).not.toHaveBeenCalled();
    expect(logsClient.send).not.toHaveBeenCalled();
  });

  it.each([
    ["empty", async () => "", 1],
    ["Ctrl+C", async () => null, 130],
  ])("does not write Secrets Manager on %s input", async (_name, prompt, code) => {
    const client = { send: vi.fn() };
    const output = outputCollector();

    await expect(
      runSetClientSecret({
        argv: ["--secret-id", "safe-id"],
        prompt,
        client,
        output,
      }),
    ).resolves.toBe(code);
    expect(client.send).not.toHaveBeenCalled();
  });

  it("sends the token only as an Authorization header and tampers its signature", async () => {
    const token = "header.payload.signature";
    const calls: Array<{ url: string; init: RequestInit | undefined }> = [];
    const responses = [
      new Response(JSON.stringify({ error: { code: "AUTH_MISSING" }, requestId: "r1" }), { status: 401 }),
      new Response(JSON.stringify({ playerId: "0".repeat(32), requestId: "r2" }), { status: 200 }),
      new Response(JSON.stringify({ error: { code: "AUTH_INVALID" }, requestId: "r3" }), { status: 401 }),
    ];
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      calls.push({ url, init });
      return responses.shift()!;
    });
    const output = outputCollector();

    const exitCode = await runAuthSmoke({
      argv: ["--url", "https://example.test/base"],
      prompt: vi.fn(async () => token),
      fetchImpl,
      output,
    });

    expect(exitCode).toBe(0);
    expect(calls).toHaveLength(3);
    expect(calls[0]).toEqual({
      url: "https://example.test/base/v1/me",
      init: { method: "GET", headers: { accept: "application/json" } },
    });
    expect(calls[1]?.init?.headers).toEqual({
      accept: "application/json",
      authorization: `Bearer ${token}`,
    });
    const tamperedHeader = (calls[2]?.init?.headers as Record<string, string>).authorization;
    const tampered = tamperedHeader.slice("Bearer ".length);
    expect(tampered.split(".").slice(0, 2)).toEqual(["header", "payload"]);
    expect(tampered.split(".")[2]).not.toBe("signature");
    expect(tampered.split(".")[2]?.slice(1)).toBe("signature".slice(1));
    expect(output.value()).not.toContain(token);
    for (const call of calls) {
      expect(call.url).not.toContain(token);
      expect(JSON.stringify(call.init)).not.toContain(`\"${token}\"`);
    }
  });

  it("stops smoke checks safely when token input is cancelled", async () => {
    const fetchImpl = vi.fn();
    await expect(
      runAuthSmoke({
        argv: ["--url", "https://example.test"],
        prompt: async () => null,
        fetchImpl,
        output: outputCollector(),
      }),
    ).resolves.toBe(130);
    expect(fetchImpl).not.toHaveBeenCalled();
  });

  it.each([
    ["--expect-invalid", 401, "AUTH_INVALID"],
    ["--expect-unavailable", 503, "AUTH_VERIFIER_UNAVAILABLE"],
  ])("supports the safe %s deployment check", async (mode, status, code) => {
    const token = "header.payload.signature";
    const fetchImpl = vi.fn(async () =>
      new Response(JSON.stringify({ error: { code }, requestId: "mode-request" }), {
        status,
      }),
    );
    const output = outputCollector();

    await expect(
      runAuthSmoke({
        argv: ["--url", "https://example.test", mode],
        prompt: async () => token,
        fetchImpl,
        output,
      }),
    ).resolves.toBe(0);
    expect(fetchImpl).toHaveBeenCalledOnce();
    expect(fetchImpl.mock.calls[0]?.[1]?.headers).toEqual({
      accept: "application/json",
      authorization: `Bearer ${token}`,
    });
    expect(output.value()).not.toContain(token);
  });

  it("fails when CloudWatch messages contain a supplied value or its fingerprint", async () => {
    const token = "header.payload.signature";
    const secret = "client-secret-value";
    const secretFingerprint = createHash("sha256")
      .update(secret, "utf8")
      .digest("hex");
    const client = {
      send: vi.fn(async () => ({
        events: [
          { message: "safe log line" },
          { message: `accidental token leak ${token}` },
          { message: `accidental fingerprint leak ${secretFingerprint}` },
        ],
      })),
    };
    const prompts = [token, secret];
    const output = outputCollector();

    const exitCode = await runCloudWatchRedactionCheck({
      argv: ["--request-id", "request-123", "--function-name", "function-name"],
      prompt: vi.fn(async () => prompts.shift() ?? null),
      client,
      output,
    });

    expect(exitCode).toBe(1);
    expect(client.send).toHaveBeenCalledOnce();
    expect(client.send.mock.calls[0]?.[0].constructor.name).toBe("FilterLogEventsCommand");
    expect(client.send.mock.calls[0]?.[0].input).toMatchObject({
      logGroupName: "/aws/lambda/function-name",
      filterPattern: '"request-123"',
    });
    expect(output.value()).not.toContain(token);
    expect(output.value()).not.toContain(secret);
  });

  it("passes safe CloudWatch logs and avoids AWS calls on empty input", async () => {
    const safeClient = {
      send: vi.fn(async () => ({ events: [{ message: "FLoZUjt0IE0EMOQ= safe" }] })),
    };
    const prompts = ["header.payload.signature", "client-secret-value"];
    await expect(
      runCloudWatchRedactionCheck({
        argv: [
          "--request-id",
          "FLoZUjt0IE0EMOQ=",
          "--function-name",
          "function-name",
        ],
        prompt: async () => prompts.shift() ?? null,
        client: safeClient,
        output: outputCollector(),
      }),
    ).resolves.toBe(0);
    expect(safeClient.send.mock.calls[0]?.[0].input).toMatchObject({
      filterPattern: '"FLoZUjt0IE0EMOQ="',
    });

    const untouchedClient = { send: vi.fn() };
    await expect(
      runCloudWatchRedactionCheck({
        argv: ["--request-id", "request-123", "--function-name", "function-name"],
        prompt: async () => "",
        client: untouchedClient,
        output: outputCollector(),
      }),
    ).resolves.toBe(1);
    expect(untouchedClient.send).not.toHaveBeenCalled();
  });
});
