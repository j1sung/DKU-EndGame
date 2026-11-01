import { GetCommand, TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { describe, expect, it, vi } from "vitest";

import { DynamoTicketStore } from "../../src/matchmaking/dynamo-ticket-store.js";

const NOW = 1_800_000_000_000;
const PLAYER = "0123456789abcdef0123456789abcdef";
const OTHER = "fedcba9876543210fedcba9876543210";
const TICKET = "019a3185-c000-7aba-8bab-abababababab";
const KEY = "123e4567-e89b-42d3-a456-426614174000";
const INPUT = { playerId: PLAYER, idempotencyKey: KEY, requestHash: "hash", requestId: "request-1" };

function makeStore(send: ReturnType<typeof vi.fn>) {
  return new DynamoTicketStore(
    { send } as unknown as DynamoDBDocumentClient,
    "profiles",
    "tickets",
    () => NOW,
    () => TICKET,
  );
}

describe("DynamoTicketStore", () => {
  it("creates only the caller's waiting ticket, guard, and replay record atomically", async () => {
    const send = vi.fn().mockResolvedValue({});
    const result = await makeStore(send).create(INPUT);
    expect(result).toEqual({
      ticketId: TICKET,
      status: "WAITING",
      expiresAt: new Date(NOW + 180_000).toISOString(),
      requestId: "request-1",
    });
    expect(send.mock.calls[0][0]).toBeInstanceOf(GetCommand);
    expect(send.mock.calls[0][0].input.Key.sk).toBe(`IDEMP#POST /v1/matchmaking/tickets#${KEY}`);
    expect(send.mock.calls[1][0].input).toMatchObject({
      TableName: "profiles", Key: { pk: `PLAYER#${PLAYER}`, sk: "ACTIVITY" }, ConsistentRead: true,
    });
    const transaction = send.mock.calls[2][0];
    expect(transaction).toBeInstanceOf(TransactWriteCommand);
    expect(transaction.input.TransactItems).toHaveLength(3);
    expect(transaction.input.TransactItems[0].Put).toMatchObject({
      TableName: "tickets",
      Item: { ticketId: TICKET, playerId: PLAYER, status: "WAITING", mode: "standard", clientVersion: "0.1.0", expiresAtMs: NOW + 180_000 },
      ConditionExpression: "attribute_not_exists(ticketId)",
    });
    expect(transaction.input.TransactItems[1].Put).toMatchObject({
      TableName: "profiles",
      Item: { pk: `PLAYER#${PLAYER}`, sk: "ACTIVITY", kind: "WAITING", ticketId: TICKET },
      ConditionExpression: "attribute_not_exists(pk)",
    });
    expect(transaction.input.TransactItems[2].Put.Item).toMatchObject({
      pk: `PLAYER#${PLAYER}`,
      response: result,
      expiresAtSeconds: Math.floor(NOW / 1000) + 86_400,
    });
    expect(JSON.stringify(transaction.input)).not.toContain(OTHER);
  });

  it("replays the original response and rejects reuse with another request hash", async () => {
    const response = { ticketId: TICKET, status: "WAITING", expiresAt: "original-time", requestId: "original-request" };
    const replay = vi.fn().mockResolvedValueOnce({ Item: { requestHash: "hash", response } });
    expect(await makeStore(replay).create(INPUT)).toEqual(response);
    expect(replay).toHaveBeenCalledTimes(1);
    const reused = vi.fn().mockResolvedValueOnce({ Item: { requestHash: "different", response } });
    await expect(makeStore(reused).create(INPUT)).rejects.toMatchObject({ code: "IDEMPOTENCY_KEY_REUSED" });
  });

  it("rejects a second active ticket for the same player", async () => {
    const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: TICKET, expiresAtMs: NOW + 1000 } });
    await expect(makeStore(send).create(INPUT)).rejects.toMatchObject({ code: "ALREADY_ACTIVE" });
    expect(send).toHaveBeenCalledTimes(2);
  });

  it("rejects a concurrent create when another request wins the activity guard", async () => {
    const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error("conditional transaction conflict"))
      .mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: "another-ticket" } });
    await expect(makeStore(send).create(INPUT)).rejects.toMatchObject({ code: "ALREADY_ACTIVE" });
  });

  it("replays the original cancellation without another transaction", async () => {
    const response = { ticketId: TICKET, status: "CANCELLED", requestId: "original-request" };
    const send = vi.fn().mockResolvedValueOnce({ Item: { requestHash: "hash", response } });
    expect(await makeStore(send).cancel({ ...INPUT, ticketId: TICKET })).toEqual(response);
    expect(send).toHaveBeenCalledTimes(1);
  });

  it("cleans an expired waiting ticket and guard before reporting no current ticket", async () => {
    const send = vi.fn()
      .mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: TICKET, revision: 1 } })
      .mockResolvedValueOnce({ Item: { ticketId: TICKET, playerId: PLAYER, status: "WAITING", expiresAtMs: NOW - 1, revision: 1 } })
      .mockResolvedValueOnce({});
    expect(await makeStore(send).current(PLAYER)).toBeNull();
    const transaction = send.mock.calls[2][0];
    expect(transaction).toBeInstanceOf(TransactWriteCommand);
    expect(transaction.input.TransactItems).toHaveLength(2);
    expect(transaction.input.TransactItems[0].Update.Key).toEqual({ ticketId: TICKET });
    expect(transaction.input.TransactItems[1].Delete.Key).toEqual({ pk: `PLAYER#${PLAYER}`, sk: "ACTIVITY" });
  });

  it("cancels only the caller's waiting ticket and activity guard", async () => {
    const send = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: TICKET, revision: 1 } })
      .mockResolvedValueOnce({ Item: { ticketId: TICKET, playerId: PLAYER, status: "WAITING", expiresAtMs: NOW + 1000, revision: 1 } })
      .mockResolvedValueOnce({});
    const result = await makeStore(send).cancel({ ...INPUT, ticketId: TICKET });
    expect(result).toEqual({ ticketId: TICKET, status: "CANCELLED", requestId: "request-1" });
    const items = send.mock.calls[3][0].input.TransactItems;
    expect(items).toHaveLength(3);
    expect(items[0].Update).toMatchObject({ TableName: "tickets", Key: { ticketId: TICKET } });
    expect(items[1].Delete).toMatchObject({ TableName: "profiles", Key: { pk: `PLAYER#${PLAYER}`, sk: "ACTIVITY" } });
    expect(JSON.stringify(items)).not.toContain(OTHER);
  });

  it("does not cancel a ticket after assignment wins", async () => {
    const send = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "MATCH", ticketId: TICKET, matchId: "match-1" } });
    await expect(makeStore(send).cancel({ ...INPUT, ticketId: TICKET })).rejects.toMatchObject({ code: "TICKET_ALREADY_ASSIGNED" });
    expect(send).toHaveBeenCalledTimes(2);
  });

  it("maps an assignment that wins during the cancel transaction to a conflict", async () => {
    const send = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: TICKET, revision: 1 } })
      .mockResolvedValueOnce({ Item: { ticketId: TICKET, playerId: PLAYER, status: "WAITING", expiresAtMs: NOW + 1000, revision: 1 } })
      .mockRejectedValueOnce(new Error("transaction conflict"))
      .mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "MATCH", ticketId: TICKET, matchId: "match-1" } })
      .mockResolvedValueOnce({ Item: { ticketId: TICKET, playerId: PLAYER, status: "ASSIGNED", matchId: "match-1" } });
    await expect(makeStore(send).cancel({ ...INPUT, ticketId: TICKET })).rejects.toMatchObject({ code: "TICKET_ALREADY_ASSIGNED" });
  });

  it("rejects a stale cancel after the player has a newer waiting ticket", async () => {
    const send = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { kind: "WAITING", ticketId: "new-ticket", revision: 1 } });
    await expect(makeStore(send).cancel({ ...INPUT, ticketId: TICKET })).rejects.toMatchObject({ code: "TICKET_MISMATCH" });
    expect(send).toHaveBeenCalledTimes(2);
  });

  it("returns the assigned match only when the ticket and guard agree", async () => {
    const send = vi.fn().mockResolvedValueOnce({ Item: { kind: "MATCH", ticketId: TICKET, matchId: "match-1" } })
      .mockResolvedValueOnce({ Item: { ticketId: TICKET, playerId: PLAYER, status: "ASSIGNED", matchId: "match-1" } });
    expect(await makeStore(send).current(PLAYER)).toEqual({ ticketId: TICKET, status: "ASSIGNED", matchId: "match-1" });
  });
});
