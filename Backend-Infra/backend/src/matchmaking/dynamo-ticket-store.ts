import { createHash } from "node:crypto";
import { GetCommand, TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

import { makeTicketId, MATCHMAKING_CLIENT_VERSION, MATCHMAKING_MODE, TICKET_WAIT_MS } from "./ticket-contract.js";
import type { TicketMutation, TicketStore, TicketView } from "./ticket-store.js";

export type TicketStoreErrorCode = "IDEMPOTENCY_KEY_REUSED" | "ALREADY_ACTIVE" | "TICKET_NOT_FOUND" | "TICKET_MISMATCH" | "TICKET_ALREADY_ASSIGNED" | "TICKET_STORE_FAILURE";

export class TicketStoreError extends Error {
  constructor(public readonly code: TicketStoreErrorCode) {
    super(code);
    this.name = "TicketStoreError";
  }
}

type Item = Record<string, any>;
const RETENTION_SECONDS = 7 * 86_400;

export class DynamoTicketStore implements TicketStore {
  constructor(
    private readonly client: DynamoDBDocumentClient,
    private readonly profileTable: string,
    private readonly ticketTable: string,
    private readonly now: () => number = Date.now,
    private readonly newTicketId: (now: number) => string = makeTicketId,
  ) {}

  async create(input: TicketMutation): Promise<TicketView & { requestId: string }> {
    const idem = this.idemKey(input, "POST /v1/matchmaking/tickets");
    const previous = await this.get(this.profileTable, idem);
    if (previous) return this.replay(previous, input.requestHash);
    await this.clearExpiredOrReject(input.playerId);

    const now = this.now();
    const ticketId = this.newTicketId(now);
    const expiresAtMs = now + TICKET_WAIT_MS;
    const bucket = createHash("sha256").update(ticketId).digest()[0]! % 16;
    const response = { ticketId, status: "WAITING" as const, expiresAt: new Date(expiresAtMs).toISOString(), requestId: input.requestId };
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: [
        { Put: { TableName: this.ticketTable, Item: {
          ticketId, playerId: input.playerId, status: "WAITING", mode: MATCHMAKING_MODE,
          clientVersion: MATCHMAKING_CLIENT_VERSION, createdAtMs: now, expiresAtMs,
          updatedAtMs: now, revision: 1,
          queueKey: `${MATCHMAKING_MODE}#${MATCHMAKING_CLIENT_VERSION}#${bucket}`,
          queueSortKey: `${String(now).padStart(13, "0")}#${ticketId}`,
        }, ConditionExpression: "attribute_not_exists(ticketId)" } },
        { Put: { TableName: this.profileTable, Item: {
          ...this.guardKey(input.playerId), kind: "WAITING", ticketId, expiresAtMs, revision: 1,
        }, ConditionExpression: "attribute_not_exists(pk)" } },
        { Put: { TableName: this.profileTable, Item: this.idemItem(idem, input, response, 201, now), ConditionExpression: "attribute_not_exists(pk)" } },
      ] }));
      return response;
    } catch {
      const stored = await this.get(this.profileTable, idem);
      if (stored) return this.replay(stored, input.requestHash);
      const guard = await this.get(this.profileTable, this.guardKey(input.playerId));
      if (guard) throw new TicketStoreError("ALREADY_ACTIVE");
      throw new TicketStoreError("TICKET_STORE_FAILURE");
    }
  }

  async current(playerId: string): Promise<TicketView | null> {
    const guard = await this.get(this.profileTable, this.guardKey(playerId));
    if (!guard) return null;
    const ticket = await this.get(this.ticketTable, { ticketId: guard.ticketId });
    if (!ticket || ticket.playerId !== playerId) throw new TicketStoreError("TICKET_STORE_FAILURE");
    if (ticket.status === "ASSIGNED" && guard.kind === "MATCH" && ticket.matchId === guard.matchId) {
      return { ticketId: ticket.ticketId, status: "ASSIGNED", matchId: ticket.matchId };
    }
    if (ticket.status !== "WAITING" || guard.kind !== "WAITING") throw new TicketStoreError("TICKET_STORE_FAILURE");
    if (ticket.expiresAtMs <= this.now()) {
      try {
        await this.expire(playerId, guard, ticket);
      } catch {
        const latestGuard = await this.get(this.profileTable, this.guardKey(playerId));
        if (!latestGuard) return null;
        const latestTicket = await this.get(this.ticketTable, { ticketId: latestGuard.ticketId });
        if (latestGuard.kind === "MATCH" && latestTicket?.status === "ASSIGNED" && latestGuard.matchId === latestTicket.matchId) {
          return { ticketId: latestTicket.ticketId, status: "ASSIGNED", matchId: latestTicket.matchId };
        }
        throw new TicketStoreError("TICKET_STORE_FAILURE");
      }
      return null;
    }
    return { ticketId: ticket.ticketId, status: "WAITING", expiresAt: new Date(ticket.expiresAtMs).toISOString() };
  }

  async cancel(input: TicketMutation & { ticketId: string }): Promise<{ ticketId: string; status: "CANCELLED"; requestId: string }> {
    const idem = this.idemKey(input, "DELETE /v1/matchmaking/tickets/current");
    const previous = await this.get(this.profileTable, idem);
    if (previous) return this.replay(previous, input.requestHash);
    const guard = await this.get(this.profileTable, this.guardKey(input.playerId));
    if (!guard) throw new TicketStoreError("TICKET_NOT_FOUND");
    if (guard.ticketId !== input.ticketId) throw new TicketStoreError("TICKET_MISMATCH");
    if (guard.kind === "MATCH") throw new TicketStoreError("TICKET_ALREADY_ASSIGNED");
    const ticket = await this.get(this.ticketTable, { ticketId: input.ticketId });
    if (!ticket || ticket.playerId !== input.playerId) throw new TicketStoreError("TICKET_NOT_FOUND");
    if (ticket.status === "ASSIGNED") throw new TicketStoreError("TICKET_ALREADY_ASSIGNED");
    if (ticket.status !== "WAITING" || ticket.expiresAtMs <= this.now()) {
      if (ticket.status === "WAITING") {
        try { await this.expire(input.playerId, guard, ticket); }
        catch {
          const latest = await this.get(this.ticketTable, { ticketId: input.ticketId });
          if (latest?.status === "ASSIGNED") throw new TicketStoreError("TICKET_ALREADY_ASSIGNED");
          throw new TicketStoreError("TICKET_STORE_FAILURE");
        }
      }
      throw new TicketStoreError("TICKET_NOT_FOUND");
    }
    const now = this.now();
    const response = { ticketId: input.ticketId, status: "CANCELLED" as const, requestId: input.requestId };
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: [
        { Update: {
          TableName: this.ticketTable, Key: { ticketId: input.ticketId },
          UpdateExpression: "SET #status = :cancelled, updatedAtMs = :now, revision = :nextRevision, expiresAtSeconds = :ttl REMOVE queueKey, queueSortKey",
          ConditionExpression: "playerId = :playerId AND #status = :waiting AND expiresAtMs > :now AND revision = :revision",
          ExpressionAttributeNames: { "#status": "status" },
          ExpressionAttributeValues: { ":cancelled": "CANCELLED", ":waiting": "WAITING", ":now": now, ":playerId": input.playerId, ":revision": ticket.revision, ":nextRevision": ticket.revision + 1, ":ttl": Math.floor(now / 1000) + RETENTION_SECONDS },
        } },
        { Delete: { TableName: this.profileTable, Key: this.guardKey(input.playerId), ConditionExpression: "kind = :waiting AND ticketId = :ticketId AND revision = :revision", ExpressionAttributeValues: { ":waiting": "WAITING", ":ticketId": input.ticketId, ":revision": guard.revision } } },
        { Put: { TableName: this.profileTable, Item: this.idemItem(idem, input, response, 200, now), ConditionExpression: "attribute_not_exists(pk)" } },
      ] }));
      return response;
    } catch {
      const stored = await this.get(this.profileTable, idem);
      if (stored) return this.replay(stored, input.requestHash);
      const latestGuard = await this.get(this.profileTable, this.guardKey(input.playerId));
      const latestTicket = await this.get(this.ticketTable, { ticketId: input.ticketId });
      if (latestTicket?.status === "ASSIGNED" || latestGuard?.kind === "MATCH") throw new TicketStoreError("TICKET_ALREADY_ASSIGNED");
      if (latestGuard && latestGuard.ticketId !== input.ticketId) throw new TicketStoreError("TICKET_MISMATCH");
      if (!latestGuard || !latestTicket || latestTicket.status !== "WAITING" || latestTicket.expiresAtMs <= this.now()) throw new TicketStoreError("TICKET_NOT_FOUND");
      throw new TicketStoreError("TICKET_STORE_FAILURE");
    }
  }

  private async clearExpiredOrReject(playerId: string): Promise<void> {
    for (let attempt = 0; attempt < 2; attempt++) {
      const guard = await this.get(this.profileTable, this.guardKey(playerId));
      if (!guard) return;
      if (guard.kind !== "WAITING" || guard.expiresAtMs > this.now()) throw new TicketStoreError("ALREADY_ACTIVE");
      const ticket = await this.get(this.ticketTable, { ticketId: guard.ticketId });
      if (!ticket || ticket.playerId !== playerId || ticket.status !== "WAITING") throw new TicketStoreError("TICKET_STORE_FAILURE");
      if (ticket.expiresAtMs > this.now()) throw new TicketStoreError("ALREADY_ACTIVE");
      await this.expire(playerId, guard, ticket);
    }
    if (await this.get(this.profileTable, this.guardKey(playerId))) throw new TicketStoreError("ALREADY_ACTIVE");
  }

  private async expire(playerId: string, guard: Item, ticket: Item): Promise<void> {
    const now = this.now();
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: [
        { Update: { TableName: this.ticketTable, Key: { ticketId: ticket.ticketId },
          UpdateExpression: "SET #status = :expired, updatedAtMs = :now, revision = :nextRevision, expiresAtSeconds = :ttl REMOVE queueKey, queueSortKey",
          ConditionExpression: "playerId = :playerId AND #status = :waiting AND expiresAtMs <= :now AND revision = :revision",
          ExpressionAttributeNames: { "#status": "status" },
          ExpressionAttributeValues: { ":expired": "EXPIRED", ":waiting": "WAITING", ":now": now, ":playerId": playerId, ":revision": ticket.revision, ":nextRevision": ticket.revision + 1, ":ttl": Math.floor(now / 1000) + RETENTION_SECONDS } } },
        { Delete: { TableName: this.profileTable, Key: this.guardKey(playerId),
          ConditionExpression: "kind = :waiting AND ticketId = :ticketId AND revision = :revision",
          ExpressionAttributeValues: { ":waiting": "WAITING", ":ticketId": ticket.ticketId, ":revision": guard.revision } } },
      ] }));
    } catch {
      const latest = await this.get(this.profileTable, this.guardKey(playerId));
      if (latest) throw new TicketStoreError("TICKET_STORE_FAILURE");
    }
  }

  private guardKey(playerId: string) { return { pk: `PLAYER#${playerId}`, sk: "ACTIVITY" }; }
  private idemKey(input: TicketMutation, route: string) { return { pk: `PLAYER#${input.playerId}`, sk: `IDEMP#${route}#${input.idempotencyKey}` }; }
  private idemItem(key: { pk: string; sk: string }, input: TicketMutation, response: object, statusCode: number, now: number) {
    return { ...key, requestHash: input.requestHash, statusCode, response, expiresAtSeconds: Math.floor(now / 1000) + 86_400 };
  }
  private replay<T>(item: Item, hash: string): T {
    if (item.requestHash !== hash) throw new TicketStoreError("IDEMPOTENCY_KEY_REUSED");
    return item.response as T;
  }
  private async get(table: string, key: Record<string, string>): Promise<Item | undefined> {
    const result = await this.client.send(new GetCommand({ TableName: table, Key: key, ConsistentRead: true }));
    return result.Item;
  }
}
