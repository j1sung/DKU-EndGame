import { GetCommand, QueryCommand, TransactWriteCommand, UpdateCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { describe, expect, it, vi } from "vitest";

import { DynamoMatchStore } from "../../src/matchmaking/dynamo-match-store.js";

const NOW = 1_800_000_000_000;
const MATCH = "019a3185-c000-7aba-8bab-abababababab";
const MEMBERS = ["p1", "p2", "p3", "p4"];
const TICKETS = ["t1", "t2", "t3", "t4"];
const META = { pk: `MATCH#${MATCH}`, sk: "META", matchId: MATCH, status: "FORMING", memberIds: MEMBERS, ticketIds: TICKETS, deadlineAtMs: NOW - 1, revision: 1, hostPlayerId: "p1" };

function store(send: ReturnType<typeof vi.fn>) {
  return new DynamoMatchStore({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW);
}

it("expires a due match and releases four matching guards in one transaction", async () => {
  const send = vi.fn().mockResolvedValueOnce({ Item: META });
  for (const playerId of MEMBERS) send.mockResolvedValueOnce({ Item: { pk: `PLAYER#${playerId}`, sk: "ACTIVITY", kind: "MATCH", matchId: MATCH, revision: 2 } });
  send.mockResolvedValueOnce({});
  expect(await store(send).expire(MATCH)).toBe("EXPIRED");
  const tx = send.mock.calls.at(-1)![0];
  expect(tx).toBeInstanceOf(TransactWriteCommand);
  expect(tx.input.TransactItems).toHaveLength(13);
  expect(tx.input.TransactItems[0].Update).toMatchObject({ TableName: "matches", Key: { pk: `MATCH#${MATCH}`, sk: "META" } });
  expect(tx.input.TransactItems[0].Update.UpdateExpression).toContain("REMOVE deadlineKey, deadlineSortKey, deadlineAtMs");
  expect(tx.input.TransactItems.filter((x: any) => x.Delete)).toHaveLength(4);
  for (const [index, playerId] of MEMBERS.entries()) {
    expect(tx.input.TransactItems[1 + index].Delete).toMatchObject({
      TableName: "profiles", Key: { pk: `PLAYER#${playerId}`, sk: "ACTIVITY" },
      ExpressionAttributeValues: expect.objectContaining({ ":matchId": MATCH, ":revision": 2 }),
    });
  }
  expect(tx.input.TransactItems.filter((x: any) => x.Update?.TableName === "tickets")).toHaveLength(4);
  expect(tx.input.TransactItems.filter((x: any) => x.Update?.TableName === "matches")).toHaveLength(5);
});

it("ignores a stale GSI candidate after the match deadline moved", async () => {
  const send = vi.fn().mockResolvedValueOnce({ Item: { ...META, deadlineAtMs: NOW + 60_000 } });
  expect(await store(send).expire(MATCH)).toBe("NOT_DUE");
  expect(send).toHaveBeenCalledTimes(1);
  expect(send.mock.calls[0]![0]).toBeInstanceOf(GetCommand);
});

it("refuses to remove a guard that points at a different match", async () => {
  const send = vi.fn().mockResolvedValueOnce({ Item: META })
    .mockResolvedValueOnce({ Item: { kind: "MATCH", matchId: "other-match", revision: 2 } });
  await expect(store(send).expire(MATCH)).rejects.toMatchObject({ code: "MATCH_GUARD_MISMATCH" });
  expect(send).toHaveBeenCalledTimes(2);
});

it("queries all 16 deadline buckets and returns only match ids", async () => {
  const send = vi.fn().mockResolvedValue({ Items: [] });
  const original = send.getMockImplementation()!;
  send.mockImplementation(async (command: any) => command instanceof QueryCommand && command.input.ExpressionAttributeValues[":key"] === "ACTIVE#0"
    ? { Items: [{ pk: `MATCH#${MATCH}`, sk: "META" }] } : original(command));
  const subject = store(send);
  const expire = vi.spyOn(subject, "expire").mockResolvedValue("EXPIRED");
  await subject.recoverDue(vi.fn(), NOW);
  expect(expire).toHaveBeenCalledWith(MATCH);
  const queries = send.mock.calls.filter(([command]) => command instanceof QueryCommand);
  expect(queries).toHaveLength(16);
  for (const [index, call] of queries.entries()) {
    expect(call[0]).toBeInstanceOf(QueryCommand);
    expect(call[0].input).toMatchObject({ TableName: "matches", IndexName: "DeadlineIndex", ExpressionAttributeValues: { ":key": `ACTIVE#${index}`, ":through": `${NOW}#~` } });
    expect(call[0].input.Limit).toBe(2);
  }
});

it("persists a deadline cursor so a failed oldest match cannot hide later matches", async () => {
  const checkpoint: { cursor?: Record<string, string>; revision?: number } = {};
  const later = "019a3185-c000-7aba-8bab-abababababac";
  const send = vi.fn(async (command: any) => {
    if (command instanceof GetCommand) return { Item: command.input.Key.sk === "BUCKET#0" ? { ...checkpoint } : undefined };
    if (command instanceof QueryCommand) {
      if (command.input.ExpressionAttributeValues[":key"] !== "ACTIVE#0") return { Items: [] };
      return command.input.ExclusiveStartKey
        ? { Items: [{ pk: `MATCH#${later}`, sk: "META" }] }
        : { Items: [{ pk: `MATCH#${MATCH}`, sk: "META" }], LastEvaluatedKey: { pk: `MATCH#${MATCH}`, sk: "META", deadlineKey: "ACTIVE#0", deadlineSortKey: `${NOW}#${MATCH}` } };
    }
    if (command instanceof UpdateCommand) {
      checkpoint.cursor = command.input.ExpressionAttributeValues[":cursor"];
      checkpoint.revision = (checkpoint.revision ?? 0) + 1;
      return {};
    }
    throw new Error("unexpected command");
  });
  const subject = store(send);
  const expire = vi.spyOn(subject, "expire").mockResolvedValue("EXPIRED");
  await subject.recoverDue(vi.fn(), NOW);
  await subject.recoverDue(vi.fn(), NOW);
  expect(expire.mock.calls.map((call) => call[0])).toEqual([MATCH, later]);
  expect(send.mock.calls.some(([command]) => command instanceof QueryCommand && command.input.ExclusiveStartKey?.pk === `MATCH#${MATCH}`)).toBe(true);
});

it("keeps the deadline cursor when expiry fails and retry enqueue also fails", async () => {
  const send = vi.fn(async (command: any) => {
    if (command instanceof GetCommand) return { Item: undefined };
    if (command instanceof QueryCommand) return command.input.ExpressionAttributeValues[":key"] === "ACTIVE#0"
      ? { Items: [{ pk: `MATCH#${MATCH}`, sk: "META" }], LastEvaluatedKey: { pk: `MATCH#${MATCH}`, sk: "META" } }
      : { Items: [] };
    if (command instanceof UpdateCommand) return {};
    throw new Error("unexpected command");
  });
  const subject = store(send);
  vi.spyOn(subject, "expire").mockRejectedValue(new Error("capacity"));
  await expect(subject.recoverDue(async () => { throw new Error("queue down"); }, NOW)).rejects.toThrow("queue down");
  expect(send.mock.calls.some(([command]) => command instanceof UpdateCommand)).toBe(false);
});
