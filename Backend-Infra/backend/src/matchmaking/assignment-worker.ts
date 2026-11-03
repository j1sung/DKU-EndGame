import { GetCommand, QueryCommand, TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

import { deadlineIndexKeys, phaseDurationMs } from "./match-contract.js";
import { makeTicketId, MATCHMAKING_CLIENT_VERSION, MATCHMAKING_MODE } from "./ticket-contract.js";

type Item = Record<string, any>;
const QUEUE_PAGE_SIZE = 4;
const CANDIDATE_CHECK_LIMIT = 16;

function isConditionalFailure(error: unknown): boolean {
  const value = error as { name?: string; CancellationReasons?: { Code?: string }[] };
  return value?.name === "ConditionalCheckFailedException" ||
    (value?.name === "TransactionCanceledException" && value.CancellationReasons?.some((reason) => reason.Code === "ConditionalCheckFailed") === true);
}

export class DynamoAssignmentWorker {
  constructor(
    private readonly client: DynamoDBDocumentClient,
    private readonly profileTable: string,
    private readonly ticketTable: string,
    private readonly matchTable: string,
    private readonly now: () => number = Date.now,
    private readonly newMatchId: (now: number) => string = makeTicketId,
  ) {}

  async assignOnce(excludedTicketIds = new Set<string>(), attempt = 0): Promise<string | null> {
    const candidates: Item[] = [];
    for (let bucket = 0; bucket < 16; bucket++) {
      const page = await this.client.send(new QueryCommand({
          TableName: this.ticketTable, IndexName: "QueueIndex",
          KeyConditionExpression: "queueKey = :key",
          ExpressionAttributeValues: { ":key": `${MATCHMAKING_MODE}#${MATCHMAKING_CLIENT_VERSION}#${bucket}` },
          Limit: QUEUE_PAGE_SIZE,
      }));
      candidates.push(...page.Items ?? []);
    }
    candidates.sort((a, b) => a.createdAtMs - b.createdAtMs || String(a.ticketId).localeCompare(String(b.ticketId)));
    const selected: { ticket: Item; guard: Item }[] = [];
    const players = new Set<string>();
    for (const candidate of candidates.slice(0, CANDIDATE_CHECK_LIMIT)) {
      if (excludedTicketIds.has(candidate.ticketId)) continue;
      const ticket = await this.get(this.ticketTable, { ticketId: candidate.ticketId });
      if (!ticket || ticket.status !== "WAITING" || ticket.mode !== MATCHMAKING_MODE || ticket.clientVersion !== MATCHMAKING_CLIENT_VERSION || players.has(ticket.playerId)) continue;
      const guard = await this.get(this.profileTable, { pk: `PLAYER#${ticket.playerId}`, sk: "ACTIVITY" });
      if (!guard || guard.kind !== "WAITING" || guard.ticketId !== ticket.ticketId) continue;
      if (ticket.expiresAtMs <= this.now()) {
        await this.expireWaiting(ticket, guard);
        continue;
      }
      selected.push({ ticket, guard });
      players.add(ticket.playerId);
      if (selected.length === 4) break;
    }
    if (selected.length < 4) return null;
    const now = this.now();
    const deadlineAtMs = now + phaseDurationMs("FORMING");
    const matchId = this.newMatchId(now);
    const memberIds = [...players].sort();
    const ticketIds = selected.map(({ ticket }) => ticket.ticketId);
    const items: any[] = [];
    for (const { ticket } of selected) {
      items.push({ Update: {
        TableName: this.ticketTable, Key: { ticketId: ticket.ticketId },
        UpdateExpression: "SET #status = :assigned, matchId = :matchId, revision = :nextRevision, updatedAtMs = :now REMOVE queueKey, queueSortKey",
        ConditionExpression: "#status = :waiting AND #mode = :mode AND clientVersion = :version AND expiresAtMs > :now AND revision = :revision AND playerId = :playerId",
        ExpressionAttributeNames: { "#status": "status", "#mode": "mode" },
        ExpressionAttributeValues: { ":assigned": "ASSIGNED", ":waiting": "WAITING", ":matchId": matchId, ":nextRevision": ticket.revision + 1, ":now": now, ":revision": ticket.revision, ":mode": MATCHMAKING_MODE, ":version": MATCHMAKING_CLIENT_VERSION, ":playerId": ticket.playerId },
      } });
    }
    for (const { ticket, guard } of selected) {
      items.push({ Update: {
        TableName: this.profileTable, Key: { pk: `PLAYER#${ticket.playerId}`, sk: "ACTIVITY" },
        UpdateExpression: "SET kind = :match, matchId = :matchId, expiresAtMs = :deadline, revision = :nextRevision",
        ConditionExpression: "kind = :waiting AND ticketId = :ticketId AND revision = :revision",
        ExpressionAttributeValues: { ":match": "MATCH", ":waiting": "WAITING", ":matchId": matchId, ":deadline": deadlineAtMs, ":nextRevision": guard.revision + 1, ":ticketId": ticket.ticketId, ":revision": guard.revision },
      } });
    }
    items.push({ Put: {
      TableName: this.matchTable,
      Item: { pk: `MATCH#${matchId}`, sk: "META", matchId, status: "FORMING", mode: MATCHMAKING_MODE, clientVersion: MATCHMAKING_CLIENT_VERSION, memberIds, ticketIds, hostPlayerId: memberIds[0], hostAttempt: 1, joinedCount: 0, createdAtMs: now, updatedAtMs: now, deadlineAtMs, ...deadlineIndexKeys(matchId, deadlineAtMs), revision: 1 },
      ConditionExpression: "attribute_not_exists(pk)",
    } });
    for (const { ticket } of selected) {
      items.push({ Put: {
        TableName: this.matchTable,
        Item: { pk: `MATCH#${matchId}`, sk: `MEMBER#${ticket.playerId}`, playerId: ticket.playerId, ticketId: ticket.ticketId, joinStatus: "PENDING" },
        ConditionExpression: "attribute_not_exists(pk)",
      } });
    }
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: items }));
      return matchId;
    } catch (error) {
      const committed = await this.get(this.matchTable, { pk: `MATCH#${matchId}`, sk: "META" });
      if (committed?.status === "FORMING" && committed.memberIds?.join() === memberIds.join()) return matchId;
      if (isConditionalFailure(error)) {
        if (attempt < 2) return this.assignOnce(new Set([...excludedTicketIds, ...ticketIds]), attempt + 1);
        return null;
      }
      throw error;
    }
  }

  private async expireWaiting(ticket: Item, guard: Item): Promise<void> {
    const now = this.now();
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: [
        { Update: {
          TableName: this.ticketTable, Key: { ticketId: ticket.ticketId },
          UpdateExpression: "SET #status = :expired, revision = :nextRevision, updatedAtMs = :now, expiresAtSeconds = :ttl REMOVE queueKey, queueSortKey",
          ConditionExpression: "#status = :waiting AND playerId = :playerId AND expiresAtMs <= :now AND revision = :revision",
          ExpressionAttributeNames: { "#status": "status" },
          ExpressionAttributeValues: { ":expired": "EXPIRED", ":waiting": "WAITING", ":playerId": ticket.playerId, ":now": now, ":revision": ticket.revision, ":nextRevision": ticket.revision + 1, ":ttl": Math.floor(now / 1000) + 7 * 86_400 },
        } },
        { Delete: {
          TableName: this.profileTable, Key: { pk: `PLAYER#${ticket.playerId}`, sk: "ACTIVITY" },
          ConditionExpression: "kind = :waiting AND ticketId = :ticketId AND revision = :revision",
          ExpressionAttributeValues: { ":waiting": "WAITING", ":ticketId": ticket.ticketId, ":revision": guard.revision },
        } },
      ] }));
    } catch (error) {
      if (!isConditionalFailure(error)) throw error;
      // A cancellation or assignment won the race. The next sweep reads the latest base items.
    }
  }

  private async get(table: string, key: Record<string, string>): Promise<Item | undefined> {
    const result = await this.client.send(new GetCommand({ TableName: table, Key: key, ConsistentRead: true }));
    return result.Item;
  }
}
