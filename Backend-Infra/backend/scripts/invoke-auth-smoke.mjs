import { pathToFileURL } from "node:url";

import { parseOptions, readHiddenLine } from "./set-client-secret.mjs";

export function tamperSignature(token) {
  const segments = token.split(".");
  if (segments.length !== 3 || segments.some((segment) => segment.length === 0)) {
    throw new Error("Token must be a three-segment JWT");
  }
  const signature = segments[2];
  const replacement = signature[0] === "A" ? "B" : "A";
  return `${segments[0]}.${segments[1]}.${replacement}${signature.slice(1)}`;
}

async function readJson(response) {
  try {
    return await response.json();
  } catch {
    return undefined;
  }
}

function hasRequestId(body) {
  return typeof body?.requestId === "string" && body.requestId.length > 0;
}

export async function runAuthSmoke({ argv, prompt, fetchImpl, output }) {
  const options = parseOptions(argv, {
    values: ["--url"],
    flags: ["--expect-invalid", "--expect-unavailable"],
  });
  const rawUrl = options?.["--url"]?.trim();
  if (!rawUrl) {
    output.write("Usage: npm run smoke:auth -- --url <api-url>\n");
    return 1;
  }

  let endpoint;
  try {
    endpoint = new URL("v1/me", rawUrl.endsWith("/") ? rawUrl : `${rawUrl}/`).toString();
  } catch {
    output.write("API URL is invalid.\n");
    return 1;
  }

  const token = await prompt("Fresh EOS Connect ID Token: ");
  if (token === null) {
    output.write("Cancelled; no requests were sent.\n");
    return 130;
  }
  if (token.trim().length === 0) {
    output.write("Token cannot be empty; no requests were sent.\n");
    return 1;
  }

  const expectInvalid = options["--expect-invalid"] === true;
  const expectUnavailable = options["--expect-unavailable"] === true;
  if (expectInvalid && expectUnavailable) {
    output.write("Choose only one expected-error mode.\n");
    return 1;
  }
  if (expectInvalid || expectUnavailable) {
    const expectedStatus = expectInvalid ? 401 : 503;
    const expectedCode = expectInvalid
      ? "AUTH_INVALID"
      : "AUTH_VERIFIER_UNAVAILABLE";
    const response = await fetchImpl(endpoint, {
      method: "GET",
      headers: { accept: "application/json", authorization: `Bearer ${token}` },
    });
    const body = await readJson(response);
    if (
      response.status !== expectedStatus ||
      body?.error?.code !== expectedCode ||
      !hasRequestId(body)
    ) {
      output.write(`FAIL expected ${expectedStatus} ${expectedCode}.\n`);
      return 1;
    }
    output.write(`PASS ${expectedCode}; requestId=${body.requestId}\n`);
    return 0;
  }

  let tampered;
  try {
    tampered = tamperSignature(token);
  } catch {
    output.write("Token must be a three-segment JWT.\n");
    return 1;
  }

  const missingResponse = await fetchImpl(endpoint, {
    method: "GET",
    headers: { accept: "application/json" },
  });
  const missingBody = await readJson(missingResponse);
  if (
    missingResponse.status !== 401 ||
    missingBody?.error?.code !== "AUTH_MISSING" ||
    !hasRequestId(missingBody)
  ) {
    output.write("FAIL missing-token contract.\n");
    return 1;
  }

  const validStartedAt = performance.now();
  const validResponse = await fetchImpl(endpoint, {
    method: "GET",
    headers: { accept: "application/json", authorization: `Bearer ${token}` },
  });
  const validLatencyMs = performance.now() - validStartedAt;
  const validBody = await readJson(validResponse);
  if (
    validResponse.status !== 200 ||
    typeof validBody?.playerId !== "string" ||
    !/^[0-9a-f]{32}$/i.test(validBody.playerId) ||
    !hasRequestId(validBody)
  ) {
    output.write("FAIL valid-token contract.\n");
    return 1;
  }

  const tamperedResponse = await fetchImpl(endpoint, {
    method: "GET",
    headers: { accept: "application/json", authorization: `Bearer ${tampered}` },
  });
  const tamperedBody = await readJson(tamperedResponse);
  if (
    tamperedResponse.status !== 401 ||
    tamperedBody?.error?.code !== "AUTH_INVALID" ||
    !hasRequestId(tamperedBody)
  ) {
    output.write("FAIL tampered-token contract.\n");
    return 1;
  }

  output.write(
    `PASS authentication smoke checks; playerId=${validBody.playerId}; requestId=${validBody.requestId}; validLatencyMs=${validLatencyMs.toFixed(1)}\n`,
  );
  return 0;
}

async function main() {
  return runAuthSmoke({
    argv: process.argv.slice(2),
    prompt: (label) => readHiddenLine(label),
    fetchImpl: fetch,
    output: process.stdout,
  });
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  try {
    process.exitCode = await main();
  } catch {
    process.stderr.write("Authentication smoke request failed. No token was logged.\n");
    process.exitCode = 1;
  }
}
