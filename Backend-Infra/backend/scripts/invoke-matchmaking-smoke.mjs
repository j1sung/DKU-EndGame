import { randomUUID } from "node:crypto";
import { pathToFileURL } from "node:url";

import { parseOptions, readHiddenLine } from "./set-client-secret.mjs";

const SCENARIOS = new Set(["full", "leave", "phase-expiry", "wait-expiry"]);
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

export function parseSmokeOptions(argv) {
  const options = parseOptions(argv, { values: ["--url", "--scenario"] });
  if (!options?.["--url"] || !SCENARIOS.has(options["--scenario"])) return null;
  try {
    const url = new URL(options["--url"]);
    if (url.protocol !== "https:" || url.username || url.password || url.search || url.hash) return null;
    return { apiUrl: url.href.endsWith("/") ? url.href : `${url.href}/`, scenario: options["--scenario"] };
  } catch {
    return null;
  }
}

export function safeResponseSummary(status, body) {
  const code = typeof body?.error?.code === "string" && /^[A-Z_]{1,64}$/.test(body.error.code)
    ? ` ${body.error.code}` : "";
  const requestId = typeof body?.requestId === "string" && /^[\w=+/-]{1,128}$/.test(body.requestId)
    ? ` requestId=${body.requestId}` : "";
  return `HTTP ${status}${code}${requestId}`;
}

function ensure(condition, description) {
  if (!condition) throw new Error(description);
}

export async function runMatchmakingSmoke({ argv, prompt, fetchImpl = fetch, output = process.stdout, sleep = delay }) {
  const options = parseSmokeOptions(argv);
  if (!options) {
    output.write("Usage: npm run smoke:matchmaking -- --url <https-api-url> --scenario <full|leave|phase-expiry|wait-expiry>\n");
    return 1;
  }

  const count = options.scenario === "wait-expiry" ? 1 : 4;
  const actors = [];
  for (let index = 0; index < count; index += 1) {
    const token = await prompt(`Account ${index + 1} fresh EOS Connect ID Token: `);
    if (token === null) {
      output.write("Cancelled; no requests were sent.\n");
      return 130;
    }
    if (!/^[^\s.]+\.[^\s.]+\.[^\s.]+$/.test(token.trim())) {
      output.write("Invalid token format; no requests were sent.\n");
      return 1;
    }
    actors.push({ label: `A${index + 1}`, token: token.trim() });
  }

  const call = async (actor, method, path, body, expectedStatus = 200) => {
    const headers = { authorization: `Bearer ${actor.token}`, accept: "application/json" };
    if (body !== undefined) {
      headers["content-type"] = "application/json";
      headers["idempotency-key"] = randomUUID();
      if (path === "v1/matchmaking/tickets") headers["x-client-version"] = "0.1.0";
    }
    let response;
    try {
      response = await fetchImpl(new URL(path, options.apiUrl), {
        method, headers, ...(body === undefined ? {} : { body: JSON.stringify(body) }),
        signal: AbortSignal.timeout(20_000),
      });
    } catch {
      throw new Error(`${actor.label} ${method} ${path}: network request failed`);
    }
    let result;
    try { result = await response.json(); } catch { result = undefined; }
    if (response.status !== expectedStatus) {
      throw new Error(`${actor.label} ${method} ${path}: ${safeResponseSummary(response.status, result)}; expected HTTP ${expectedStatus}`);
    }
    ensure(typeof result?.requestId === "string", `${actor.label} ${method} ${path}: missing requestId`);
    output.write(`${actor.label} ${method} ${path}: ${safeResponseSummary(response.status, result)}\n`);
    return result;
  };

  const current = (actor) => call(actor, "GET", "v1/matchmaking/tickets/current");
  const start = (actor) => call(actor, "POST", "v1/matchmaking/tickets", { mode: "standard" }, 201);
  const cancel = (actor, ticketId) => call(actor, "DELETE", "v1/matchmaking/tickets/current", { ticketId });
  const match = (actor, matchId) => call(actor, "GET", `v1/matches/${encodeURIComponent(matchId)}`);
  const report = (actor, matchId, action, hostAttempt, extra = {}) =>
    call(actor, "POST", `v1/matches/${encodeURIComponent(matchId)}/${action}`, { hostAttempt, ...extra });

  const awaitState = async (actor, wanted, timeoutMs) => {
    const until = Date.now() + timeoutMs;
    while (Date.now() < until) {
      const state = await current(actor);
      if (state.status === wanted || (wanted === "NONE" && state.ticket === null)) return state;
      await sleep(2_000);
    }
    throw new Error(`${actor.label}: timed out waiting for ${wanted}`);
  };

  try {
    const ids = new Set();
    for (const actor of actors) {
      const profile = await call(actor, "GET", "v1/me");
      ensure(typeof profile.playerId === "string" && profile.playerId.length > 0, `${actor.label}: missing playerId`);
      ensure(typeof profile.nickname === "string" && profile.nickname.length > 0, `${actor.label}: nickname must be registered first`);
      ensure(!ids.has(profile.playerId), "Two supplied tokens resolve to the same PUID");
      ids.add(profile.playerId);
      actor.playerId = profile.playerId;
      ensure((await current(actor)).ticket === null, `${actor.label}: already has an active ticket or match; wait for expiry or finish it first`);
    }
    output.write(`Preflight passed: ${count} distinct accounts with nicknames and no active matches.\n`);

    if (options.scenario === "wait-expiry") {
      const ticket = await start(actors[0]);
      ensure(ticket.status === "WAITING", "Expected WAITING ticket");
      const ended = await awaitState(actors[0], "NONE", 215_000);
      ensure(ended.ticket === null, "Waiting ticket did not expire");
      output.write("PASS wait-expiry: one account returned to idle after the 3-minute limit.\n");
      return 0;
    }

    if (options.scenario === "full") {
      const first = [];
      for (const actor of actors.slice(0, 3)) {
        const ticket = await start(actor);
        ensure(ticket.status === "WAITING", `${actor.label}: expected WAITING`);
        first.push(ticket);
      }
      const cancelled = await cancel(actors[2], first[2].ticketId);
      ensure(cancelled.status === "CANCELLED", "Cancellation did not succeed");
      ensure((await current(actors[2])).ticket === null, "Cancelled account still has a ticket");
      for (const actor of actors.slice(0, 2)) {
        ensure((await current(actor)).status === "WAITING", `${actor.label}: cancellation affected another player`);
      }
      output.write("PASS cancel: only A3 left the queue; A1 and A2 remain waiting.\n");
      await start(actors[2]);
      await start(actors[3]);
    } else {
      for (const actor of actors) await start(actor);
    }

    const assigned = [];
    for (const actor of actors) assigned.push(await awaitState(actor, "ASSIGNED", 190_000));
    const matchId = assigned[0].matchId;
    ensure(typeof matchId === "string" && assigned.every((item) => item.matchId === matchId), "Four accounts did not receive the same matchId");
    const state = await match(actors[0], matchId);
    ensure(state.status === "FORMING" && state.memberIds?.length === 4, "Expected four-member FORMING match");
    ensure(state.memberIds.every((id) => ids.has(id)), "Match includes an unexpected account");
    const host = actors.find((actor) => actor.playerId === state.hostPlayerId);
    ensure(host && Number.isInteger(state.hostAttempt), "Missing host or hostAttempt");
    output.write(`PASS assignment: four distinct accounts share matchId=${matchId}; host=${host.label}.\n`);

    if (options.scenario === "phase-expiry") {
      const until = Date.parse(state.deadlineAt);
      ensure(Number.isFinite(until), "Missing FORMING deadline");
      await sleep(Math.max(0, until - Date.now() + 2_000));
      const expired = await match(actors[0], matchId);
      ensure(expired.status === "EXPIRED", "FORMING match did not expire");
      for (const actor of actors) ensure((await current(actor)).ticket === null, `${actor.label}: active guard remains after expiry`);
      output.write("PASS phase-expiry: match expired and all four accounts returned to idle.\n");
      return 0;
    }

    if (options.scenario === "leave") {
      const member = actors.find((actor) => actor !== host);
      ensure((await report(member, matchId, "left", state.hostAttempt)).status === "FAILED", "Leave report did not fail the match");
      ensure((await match(host, matchId)).status === "FAILED", "Match did not become FAILED");
      for (const actor of actors) ensure((await current(actor)).ticket === null, `${actor.label}: active guard remains after leave`);
      output.write("PASS leave: one member's simulated departure failed the match and released all four guards.\n");
      return 0;
    }

    const member = actors.find((actor) => actor !== host);
    const denied = await call(member, "POST", `v1/matches/${encodeURIComponent(matchId)}/started`, { hostAttempt: state.hostAttempt }, 403);
    ensure(denied.error?.code === "NOT_MATCH_HOST", "Host-only operation was not denied correctly");
    const session = await report(host, matchId, "session", state.hostAttempt, { sessionId: `smoke-${randomUUID()}` });
    ensure(session.status === "SESSION_READY", "Session report did not advance match");
    for (const actor of actors) await report(actor, matchId, "joined", state.hostAttempt);
    ensure((await match(host, matchId)).status === "READY_TO_START", "Four joins did not advance match");
    ensure((await report(host, matchId, "started", state.hostAttempt)).status === "IN_GAME", "Start report failed");
    ensure((await report(host, matchId, "finished", state.hostAttempt)).status === "FINISHED", "Finish report failed");
    for (const actor of actors) ensure((await current(actor)).ticket === null, `${actor.label}: active guard remains after finish`);
    output.write("PASS full: host permission, simulated session/join reports, start, finish, and guard release.\n");
    return 0;
  } catch (error) {
    output.write(`FAIL ${error.message}\n`);
    return 1;
  }
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  process.exitCode = await runMatchmakingSmoke({ argv: process.argv.slice(2), prompt: readHiddenLine });
}
