import { GetCommand, QueryCommand, TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { expect, it, vi } from "vitest";

import { DynamoAssignmentWorker } from "../../src/matchmaking/assignment-worker.js";

const NOW = 1_800_000_000_000;
const MATCH = "019a3185-c000-7aba-8bab-abababababab";
const tickets = [1, 2, 3, 4].map((i) => ({ ticketId: `t${i}`, playerId: `p${i}`, status: "WAITING", mode: "standard", clientVersion: "0.1.0", createdAtMs: NOW - 10_000 + i, expiresAtMs: NOW + 10_000, revision: 1, queueSortKey: `${NOW - 10_000 + i}#t${i}` }));

function sendForTickets(candidateItems = tickets) {
  return vi.fn(async (command: any) => {
    if (command instanceof QueryCommand) return { Items: command.input.ExpressionAttributeValues[":key"] === "standard#0.1.0#0" ? candidateItems : [] };
    if (command instanceof GetCommand) {
      if (command.input.TableName === "tickets") return { Item: tickets.find((ticket) => ticket.ticketId === command.input.Key.ticketId) };
      const playerId = command.input.Key.pk.slice("PLAYER#".length);
      return { Item: { kind: "WAITING", ticketId: tickets.find((ticket) => ticket.playerId === playerId)?.ticketId, revision: 1 } };
    }
    if (command instanceof TransactWriteCommand) return {};
    throw new Error("unexpected command");
  });
}

it("atomically assigns the oldest four verified waiting tickets and creates one match", async () => {
  const send = sendForTickets();
  const worker = new DynamoAssignmentWorker({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW, () => MATCH);
  expect(await worker.assignOnce()).toBe(MATCH);
  const tx = send.mock.calls.find((call) => call[0] instanceof TransactWriteCommand)![0] as TransactWriteCommand;
  expect(tx.input.TransactItems).toHaveLength(13);
  expect(tx.input.TransactItems?.filter((x: any) => x.Update?.TableName === "tickets")).toHaveLength(4);
  expect(tx.input.TransactItems?.filter((x: any) => x.Update?.TableName === "profiles")).toHaveLength(4);
  expect(tx.input.TransactItems?.filter((x: any) => x.Put?.TableName === "matches")).toHaveLength(5);
  for (const item of tx.input.TransactItems?.filter((x: any) => x.Update?.TableName === "tickets") ?? []) {
    expect(item.Update?.ConditionExpression).toContain("#mode = :mode");
    expect(item.Update?.ExpressionAttributeNames).toMatchObject({ "#mode": "mode" });
  }
  expect(tx.input.TransactItems?.[8]?.Put?.Item).toMatchObject({ status: "FORMING", memberIds: ["p1", "p2", "p3", "p4"], hostPlayerId: "p1", deadlineAtMs: NOW + 60_000 });
});

it("does not assign a cancelled ticket still visible in the queue index", async () => {
  const send = sendForTickets([{ ...tickets[0]!, status: "WAITING" }, ...tickets.slice(1)]);
  const original = send.getMockImplementation()!;
  send.mockImplementation(async (command: any) => {
    if (command instanceof GetCommand && command.input.TableName === "tickets" && command.input.Key.ticketId === "t1") return { Item: { ...tickets[0], status: "CANCELLED" } };
    return original(command);
  });
  const worker = new DynamoAssignmentWorker({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW, () => MATCH);
  expect(await worker.assignOnce()).toBeNull();
  expect(send.mock.calls.some((call) => call[0] instanceof TransactWriteCommand)).toBe(false);
});

it("expires an overdue waiting ticket and its own guard during a sweep", async () => {
  const send = sendForTickets();
  const original = send.getMockImplementation()!;
  send.mockImplementation(async (command: any) => {
    if (command instanceof GetCommand && command.input.TableName === "tickets" && command.input.Key.ticketId === "t1") return { Item: { ...tickets[0], expiresAtMs: NOW - 1 } };
    return original(command);
  });
  const worker = new DynamoAssignmentWorker({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW, () => MATCH);
  expect(await worker.assignOnce()).toBeNull();
  const transactions = send.mock.calls.filter((call) => call[0] instanceof TransactWriteCommand);
  expect(transactions).toHaveLength(1);
  expect(transactions[0]![0].input.TransactItems).toHaveLength(2);
  expect(transactions[0]![0].input.TransactItems[1].Delete.Key).toEqual({ pk: "PLAYER#p1", sk: "ACTIVITY" });
});

it("bounds each queue bucket query and propagates operational transaction failures", async () => {
  const send = sendForTickets();
  const original = send.getMockImplementation()!;
  send.mockImplementation(async (command: any) => {
    if (command instanceof TransactWriteCommand) throw Object.assign(new Error("capacity"), { name: "ProvisionedThroughputExceededException" });
    if (command instanceof GetCommand && command.input.TableName === "matches") return { Item: undefined };
    return original(command);
  });
  const worker = new DynamoAssignmentWorker({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW, () => MATCH);
  await expect(worker.assignOnce()).rejects.toMatchObject({ name: "ProvisionedThroughputExceededException" });
  for (const [command] of send.mock.calls.filter(([command]) => command instanceof QueryCommand)) {
    expect(command.input.Limit).toBe(4);
    expect(command.input.ExclusiveStartKey).toBeUndefined();
  }
});

it("reselects the next four tickets after an assignment race", async () => {
  const eight = [1, 2, 3, 4, 5, 6, 7, 8].map((i) => ({ ...tickets[(i - 1) % 4]!, ticketId: `t${i}`, playerId: `p${i}`, createdAtMs: NOW - 10_000 + i }));
  let transactionCount = 0;
  const send = vi.fn(async (command: any) => {
    if (command instanceof QueryCommand) {
      const bucket = command.input.ExpressionAttributeValues[":key"];
      return { Items: bucket.endsWith("#0") ? eight.slice(0, 4) : bucket.endsWith("#1") ? eight.slice(4) : [] };
    }
    if (command instanceof GetCommand) {
      if (command.input.TableName === "matches") return { Item: undefined };
      if (command.input.TableName === "tickets") return { Item: eight.find((ticket) => ticket.ticketId === command.input.Key.ticketId) };
      const playerId = command.input.Key.pk.slice("PLAYER#".length);
      return { Item: { kind: "WAITING", ticketId: eight.find((ticket) => ticket.playerId === playerId)?.ticketId, revision: 1 } };
    }
    if (command instanceof TransactWriteCommand) {
      transactionCount++;
      if (transactionCount === 1) throw Object.assign(new Error("race"), { name: "TransactionCanceledException", CancellationReasons: [{ Code: "ConditionalCheckFailed" }] });
      return {};
    }
    throw new Error("unexpected command");
  });
  const worker = new DynamoAssignmentWorker({ send } as unknown as DynamoDBDocumentClient, "profiles", "tickets", "matches", () => NOW, () => MATCH);
  expect(await worker.assignOnce()).toBe(MATCH);
  expect(transactionCount).toBe(2);
  const second = send.mock.calls.filter(([command]) => command instanceof TransactWriteCommand)[1]![0] as TransactWriteCommand;
  expect(second.input.TransactItems?.slice(0, 4).map((item: any) => item.Update.Key.ticketId)).toEqual(["t5", "t6", "t7", "t8"]);
});
