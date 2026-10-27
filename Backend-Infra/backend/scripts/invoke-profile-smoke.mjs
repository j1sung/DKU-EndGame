import { randomUUID } from "node:crypto";
import { pathToFileURL } from "node:url";

import { parseOptions, readHiddenLine } from "./set-client-secret.mjs";

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

export async function runProfileSmoke({ argv, prompt, fetchImpl, output }) {
  const options = parseOptions(argv, {
    values: ["--url", "--nickname"],
    flags: ["--expect-taken"],
  });
  const rawUrl = options?.["--url"]?.trim();
  const nickname = options?.["--nickname"]?.trim();
  if (!rawUrl || !nickname) {
    output.write("Usage: npm run smoke:profile -- --url <api-url> --nickname <new-nickname> [--expect-taken]\n");
    return 1;
  }

  let meUrl;
  let nicknameUrl;
  try {
    const baseUrl = rawUrl.endsWith("/") ? rawUrl : `${rawUrl}/`;
    meUrl = new URL("v1/me", baseUrl).toString();
    nicknameUrl = new URL("v1/me/nickname", baseUrl).toString();
  } catch {
    output.write("API URL is invalid.\n");
    return 1;
  }

  const token = await prompt("Fresh EOS Connect ID Token: ");
  if (token === null) {
    output.write("Cancelled; no requests were sent.\n");
    return 130;
  }
  if (!token.trim()) {
    output.write("Token cannot be empty; no requests were sent.\n");
    return 1;
  }

  const authorization = `Bearer ${token}`;
  const getProfile = () => fetchImpl(meUrl, {
    method: "GET",
    headers: { accept: "application/json", authorization },
  });
  const first = await getProfile();
  const firstBody = await readJson(first);
  if (first.status !== 200 || typeof firstBody?.playerId !== "string" || !hasRequestId(firstBody)) {
    output.write("FAIL initial profile read.\n");
    return 1;
  }

  const key = randomUUID();
  const body = JSON.stringify({ nickname });
  const postNickname = () => fetchImpl(nicknameUrl, {
    method: "POST",
    headers: {
      accept: "application/json",
      "content-type": "application/json",
      authorization,
      "idempotency-key": key,
    },
    body,
  });
  const registered = await postNickname();
  const registeredBody = await readJson(registered);
  if (options["--expect-taken"] === true) {
    if (registered.status !== 409 || registeredBody?.error?.code !== "NICKNAME_TAKEN" || !hasRequestId(registeredBody)) {
      output.write("FAIL expected NICKNAME_TAKEN.\n");
      return 1;
    }
    output.write(`PASS nickname collision; requestId=${registeredBody.requestId}\n`);
    return 0;
  }
  if (registered.status !== 201 || registeredBody?.nickname !== nickname ||
      registeredBody?.playerId !== firstBody.playerId || !hasRequestId(registeredBody)) {
    output.write("FAIL nickname registration.\n");
    return 1;
  }

  const retried = await postNickname();
  const retriedBody = await readJson(retried);
  if (retried.status !== 201 || JSON.stringify(retriedBody) !== JSON.stringify(registeredBody)) {
    output.write("FAIL idempotent nickname retry.\n");
    return 1;
  }
  const profile = await getProfile();
  const profileBody = await readJson(profile);
  if (profile.status !== 200 || profileBody?.nickname !== nickname ||
      profileBody?.playerId !== firstBody.playerId || !hasRequestId(profileBody)) {
    output.write("FAIL persisted profile read.\n");
    return 1;
  }

  output.write(`PASS profile smoke checks; playerId=${firstBody.playerId}; registerRequestId=${registeredBody.requestId}; readRequestId=${profileBody.requestId}\n`);
  return 0;
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  try {
    process.exitCode = await runProfileSmoke({
      argv: process.argv.slice(2),
      prompt: (label) => readHiddenLine(label),
      fetchImpl: fetch,
      output: process.stdout,
    });
  } catch {
    process.stderr.write("Profile smoke request failed. No token was logged.\n");
    process.exitCode = 1;
  }
}
