import { TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { expect, it, vi } from "vitest";

import { DynamoMatchStore } from "../../src/matchmaking/dynamo-match-store.js";

const NOW = 1_800_000_000_000;
const MATCH = "019a3185-c000-7aba-8bab-abababababab";
const MEMBERS = ["p1", "p2", "p3", "p4"];
const TICKETS = ["t1", "t2", "t3", "t4"];
const BASE = { matchId: MATCH, playerId: "p1", hostAttempt: 1, idempotencyKey: "123e4567-e89b-42d3-a456-426614174000", requestHash: "hash", requestId: "request-1" };
const META = { pk: `MATCH#${MATCH}`, sk: "META", matchId: MATCH, status: "FORMING", memberIds: MEMBERS, ticketIds: TICKETS, deadlineAtMs: NOW + 1_000, revision: 1, hostPlayerId: "p1", hostAttempt: 1, joinedCount: 0 };

function store(send: ReturnType<typeof vi.fn>) {
  return new DynamoMatchStore({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW);
}

it("registers the host session and immediately moves to the next 60 second phase", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: META });
  for (let i = 0; i < 4; i++) send.mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  const result = await store(send).registerSession({ ...BASE, sessionId: "session-123" });
  expect(result).toMatchObject({ matchId: MATCH, status: "SESSION_READY", deadlineAt: new Date(NOW + 60_000).toISOString() });
  const tx = send.mock.calls.at(-1)![0];
  expect(tx).toBeInstanceOf(TransactWriteCommand);
  expect(tx.input.TransactItems).toHaveLength(6);
  expect(tx.input.TransactItems[0].Update.ExpressionAttributeValues).toMatchObject({ ":next": "SESSION_READY", ":deadline": NOW + 60_000, ":sessionId": "session-123" });
});

it("rejects a session report from a non-host", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: META });
  await expect(store(send).registerSession({ ...BASE, playerId: "p2", sessionId: "session-123" })).rejects.toMatchObject({ code: "NOT_MATCH_HOST" });
  expect(send).toHaveBeenCalledTimes(2);
});

it("moves to READY_TO_START on the fourth join without waiting for a timer", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: { ...META, status: "SESSION_READY", joinedCount: 3 } })
    .mockResolvedValueOnce({ Item: { sk: "MEMBER#p1", joinStatus: "PENDING" } });
  for (let i = 0; i < 4; i++) send.mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  const result = await store(send).join(BASE);
  expect(result).toMatchObject({ matchId: MATCH, status: "READY_TO_START", joinedCount: 4 });
  const tx = send.mock.calls.at(-1)![0];
  expect(tx).toBeInstanceOf(TransactWriteCommand);
  expect(tx.input.TransactItems).toHaveLength(7);
});

it("starts the game immediately after the host reports start", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: { ...META, status: "READY_TO_START", joinedCount: 4 } });
  for (let i = 0; i < 4; i++) send.mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  const result = await store(send).start(BASE);
  expect(result).toMatchObject({ status: "IN_GAME", deadlineAt: new Date(NOW + 1_800_000).toISOString() });
});

it("lets the host finish the game and atomically release everyone", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: { ...META, status: "IN_GAME" } });
  for (let i = 0; i < 4; i++) send.mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  send.mockResolvedValueOnce({ Item: { requestHash: "hash", response: { matchId: MATCH, status: "FINISHED", requestId: "request-1" } } });
  const result = await store(send).finish(BASE);
  expect(result).toMatchObject({ status: "FINISHED" });
  const tx = send.mock.calls.findLast((call) => call[0] instanceof TransactWriteCommand)![0];
  expect(tx).toBeInstanceOf(TransactWriteCommand);
  expect(tx.input.TransactItems).toHaveLength(14);
});

it("re-reads the count and retries when another member joins concurrently", async () => {
  let transactionCount = 0;
  let metaReads = 0;
  const send = vi.fn(async (command: any) => {
    if (command instanceof TransactWriteCommand) {
      transactionCount++;
      if (transactionCount === 1) throw Object.assign(new Error("race"), { name: "TransactionCanceledException", CancellationReasons: [{ Code: "ConditionalCheckFailed" }] });
      return {};
    }
    if (command.input.TableName === "profiles") return {};
    if (command.input.Key.sk === "META") return { Item: { ...META, status: "SESSION_READY", joinedCount: metaReads++, revision: metaReads } };
    return { Item: { joinStatus: "PENDING" } };
  });
  const result = await store(send).join(BASE);
  expect(result).toMatchObject({ status: "SESSION_READY", joinedCount: 2 });
  expect(transactionCount).toBe(2);
});

it("ends the whole match when any member reports leaving", async () => {
  const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: { ...META, status: "SESSION_READY" } });
  for (let i = 0; i < 4; i++) send.mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  send.mockResolvedValueOnce({ Item: { requestHash: "hash", response: { matchId: MATCH, status: "FAILED", requestId: "request-1" } } });
  expect(await store(send).leave({ ...BASE, playerId: "p2" })).toMatchObject({ status: "FAILED" });
});
