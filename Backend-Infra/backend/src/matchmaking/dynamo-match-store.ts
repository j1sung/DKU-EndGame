import { GetCommand, QueryCommand, TransactWriteCommand, UpdateCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

import type { MatchStatus } from "./match-contract.js";
import { deadlineIndexKeys, phaseDurationMs, type ActiveMatchStatus } from "./match-contract.js";

export interface MatchMutation {
  matchId: string;
  playerId: string;
  hostAttempt: number;
  idempotencyKey: string;
  requestHash: string;
  requestId: string;
}

export class MatchStoreError extends Error {
  constructor(public readonly code: "MATCH_NOT_FOUND" | "MATCH_GUARD_MISMATCH" | "MATCH_STORE_FAILURE" | "NOT_MATCH_MEMBER" | "NOT_MATCH_HOST" | "MATCH_STATE_CONFLICT" | "HOST_ATTEMPT_MISMATCH" | "IDEMPOTENCY_KEY_REUSED") {
    super(code);
    this.name = "MatchStoreError";
  }
}

type Item = Record<string, any>;
const TERMINAL_TTL_SECONDS = 7 * 86_400;

function isConditionalFailure(error: unknown): boolean {
  const value = error as { name?: string; CancellationReasons?: { Code?: string }[] };
  return value?.name === "ConditionalCheckFailedException" ||
    (value?.name === "TransactionCanceledException" && value.CancellationReasons?.some((reason) => reason.Code === "ConditionalCheckFailed") === true);
}

export class DynamoMatchStore {
  constructor(
    private readonly client: DynamoDBDocumentClient,
    private readonly profileTable: string,
    private readonly ticketTable: string,
    private readonly matchTable: string,
    private readonly now: () => number = Date.now,
  ) {}

  async getForMember(matchId: string, playerId: string): Promise<Record<string, unknown>> {
    const meta = await this.requireMeta(matchId);
    if (!Array.isArray(meta.memberIds) || !meta.memberIds.includes(playerId)) throw new MatchStoreError("NOT_MATCH_MEMBER");
    if (!this.isTerminal(meta.status) && meta.deadlineAtMs <= this.now()) {
      await this.expire(matchId);
      return this.getForMember(matchId, playerId);
    }
    return {
      matchId, status: meta.status, memberIds: meta.memberIds, hostPlayerId: meta.hostPlayerId,
      hostAttempt: meta.hostAttempt, ...(meta.sessionId ? { sessionId: meta.sessionId } : {}),
      ...(meta.deadlineAtMs ? { deadlineAt: new Date(meta.deadlineAtMs).toISOString() } : {}),
      joinedCount: meta.joinedCount ?? 0,
    };
  }

  async registerSession(input: MatchMutation & { sessionId: string }): Promise<Record<string, unknown>> {
    return this.transition(input, "POST /v1/matches/{matchId}/session", "FORMING", "SESSION_READY", true, input.sessionId);
  }

  async start(input: MatchMutation): Promise<Record<string, unknown>> {
    return this.transition(input, "POST /v1/matches/{matchId}/started", "READY_TO_START", "IN_GAME", true);
  }

  async finish(input: MatchMutation): Promise<Record<string, unknown>> {
    return this.reportEnd(input, "FINISHED", "POST /v1/matches/{matchId}/finished", true);
  }

  async leave(input: MatchMutation): Promise<Record<string, unknown>> {
    return this.reportEnd(input, "FAILED", "POST /v1/matches/{matchId}/left", false);
  }

  private async reportEnd(input: MatchMutation, status: "FINISHED" | "FAILED", route: string, hostOnly: boolean): Promise<Record<string, unknown>> {
    const idem = this.idemKey(input, route);
    const previous = await this.get(this.profileTable, idem);
    if (previous) return this.replay(previous, input.requestHash);
    const meta = await this.requireMeta(input.matchId);
    this.assertActor(meta, input, hostOnly);
    if (status === "FINISHED" ? meta.status !== "IN_GAME" : this.isTerminal(meta.status)) throw new MatchStoreError("MATCH_STATE_CONFLICT");
    if (meta.deadlineAtMs <= this.now()) throw new MatchStoreError("MATCH_STATE_CONFLICT");
    const response = { matchId: input.matchId, status, requestId: input.requestId };
    await this.terminate(input.matchId, meta, status, idem, input, response);
    const stored = await this.get(this.profileTable, idem);
    if (!stored) throw new MatchStoreError("MATCH_STORE_FAILURE");
    return this.replay(stored, input.requestHash);
  }

  async join(input: MatchMutation): Promise<Record<string, unknown>> {
    for (let attempt = 0; attempt < 4; attempt++) {
      try { return await this.joinOnce(input); }
      catch (error) {
        if (!(error instanceof MatchStoreError) || error.code !== "MATCH_STATE_CONFLICT" || attempt === 3) throw error;
      }
    }
    throw new MatchStoreError("MATCH_STATE_CONFLICT");
  }

  private async joinOnce(input: MatchMutation): Promise<Record<string, unknown>> {
    const route = "POST /v1/matches/{matchId}/joined";
    const idem = this.idemKey(input, route);
    const previous = await this.get(this.profileTable, idem);
    if (previous) return this.replay(previous, input.requestHash);
    const meta = await this.requireMeta(input.matchId);
    this.assertActor(meta, input, false);
    this.assertState(meta, "SESSION_READY");
    const memberKey = { pk: `MATCH#${input.matchId}`, sk: `MEMBER#${input.playerId}` };
    const member = await this.get(this.matchTable, memberKey);
    if (!member) throw new MatchStoreError("NOT_MATCH_MEMBER");
    if (member.joinStatus === "JOINED") throw new MatchStoreError("MATCH_STATE_CONFLICT");
    if (!Number.isInteger(meta.joinedCount) || meta.joinedCount < 0 || meta.joinedCount > 3) throw new MatchStoreError("MATCH_STORE_FAILURE");
    const fourth = meta.joinedCount === 3;
    const now = this.now();
    const deadline = now + phaseDurationMs("READY_TO_START");
    const response = { matchId: input.matchId, status: fourth ? "READY_TO_START" : "SESSION_READY", joinedCount: meta.joinedCount + 1, ...(fourth ? { deadlineAt: new Date(deadline).toISOString() } : {}), requestId: input.requestId };
    const items: any[] = [
      { Update: {
        TableName: this.matchTable, Key: memberKey,
        UpdateExpression: "SET joinStatus = :joined, joinedAtMs = :now",
        ConditionExpression: "joinStatus = :pending",
        ExpressionAttributeValues: { ":joined": "JOINED", ":pending": "PENDING", ":now": now },
      } },
      { Update: {
        TableName: this.matchTable, Key: { pk: `MATCH#${input.matchId}`, sk: "META" },
        UpdateExpression: fourth
          ? "SET #status = :next, joinedCount = :nextCount, revision = :nextRevision, updatedAtMs = :now, deadlineAtMs = :deadline, deadlineKey = :deadlineKey, deadlineSortKey = :deadlineSortKey"
          : "SET joinedCount = :nextCount, revision = :nextRevision, updatedAtMs = :now",
        ConditionExpression: "#status = :expected AND revision = :revision AND deadlineAtMs > :now AND joinedCount = :count",
        ExpressionAttributeNames: { "#status": "status" },
        ExpressionAttributeValues: { ":expected": "SESSION_READY", ":revision": meta.revision, ":nextRevision": meta.revision + 1, ":now": now, ":count": meta.joinedCount, ":nextCount": meta.joinedCount + 1, ...(fourth ? { ":next": "READY_TO_START", ":deadline": deadline, ":deadlineKey": deadlineIndexKeys(input.matchId, deadline).deadlineKey, ":deadlineSortKey": deadlineIndexKeys(input.matchId, deadline).deadlineSortKey } : {}) },
      } },
    ];
    if (fourth) items.push(...await this.guardDeadlineUpdates(meta, input.matchId, deadline));
    items.push(this.idemPut(idem, input, response, now));
    return this.commit(items, idem, input.requestHash, response);
  }

  private async transition(input: MatchMutation, route: string, expected: ActiveMatchStatus, next: ActiveMatchStatus, hostOnly: boolean, sessionId?: string): Promise<Record<string, unknown>> {
    const idem = this.idemKey(input, route);
    const previous = await this.get(this.profileTable, idem);
    if (previous) return this.replay(previous, input.requestHash);
    const meta = await this.requireMeta(input.matchId);
    this.assertActor(meta, input, hostOnly);
    this.assertState(meta, expected);
    const now = this.now();
    const deadline = now + phaseDurationMs(next);
    const keys = deadlineIndexKeys(input.matchId, deadline);
    const response = { matchId: input.matchId, status: next, deadlineAt: new Date(deadline).toISOString(), requestId: input.requestId };
    const items: any[] = [{ Update: {
      TableName: this.matchTable, Key: { pk: `MATCH#${input.matchId}`, sk: "META" },
      UpdateExpression: "SET #status = :next, revision = :nextRevision, updatedAtMs = :now, deadlineAtMs = :deadline, deadlineKey = :deadlineKey, deadlineSortKey = :deadlineSortKey" + (sessionId === undefined ? "" : ", sessionId = :sessionId"),
      ConditionExpression: "#status = :expected AND revision = :revision AND deadlineAtMs > :now AND hostAttempt = :hostAttempt",
      ExpressionAttributeNames: { "#status": "status" },
      ExpressionAttributeValues: { ":next": next, ":expected": expected, ":revision": meta.revision, ":nextRevision": meta.revision + 1, ":now": now, ":deadline": deadline, ":deadlineKey": keys.deadlineKey, ":deadlineSortKey": keys.deadlineSortKey, ":hostAttempt": input.hostAttempt, ...(sessionId === undefined ? {} : { ":sessionId": sessionId }) },
    } }];
    items.push(...await this.guardDeadlineUpdates(meta, input.matchId, deadline));
    items.push(this.idemPut(idem, input, response, now));
    return this.commit(items, idem, input.requestHash, response);
  }

  private async guardDeadlineUpdates(meta: Item, matchId: string, deadline: number): Promise<any[]> {
    const updates: any[] = [];
    for (const playerId of meta.memberIds as string[]) {
      const key = { pk: `PLAYER#${playerId}`, sk: "ACTIVITY" };
      const guard = await this.get(this.profileTable, key);
      if (!guard || guard.kind !== "MATCH" || guard.matchId !== matchId) throw new MatchStoreError("MATCH_GUARD_MISMATCH");
      updates.push({ Update: {
        TableName: this.profileTable, Key: key,
        UpdateExpression: "SET expiresAtMs = :deadline, revision = :nextRevision",
        ConditionExpression: "kind = :kind AND matchId = :matchId AND revision = :revision",
        ExpressionAttributeValues: { ":deadline": deadline, ":kind": "MATCH", ":matchId": matchId, ":revision": guard.revision, ":nextRevision": guard.revision + 1 },
      } });
    }
    return updates;
  }

  private assertActor(meta: Item, input: MatchMutation, hostOnly: boolean): void {
    if (!Array.isArray(meta.memberIds) || !meta.memberIds.includes(input.playerId)) throw new MatchStoreError("NOT_MATCH_MEMBER");
    if (hostOnly && meta.hostPlayerId !== input.playerId) throw new MatchStoreError("NOT_MATCH_HOST");
    if (meta.hostAttempt !== input.hostAttempt) throw new MatchStoreError("HOST_ATTEMPT_MISMATCH");
  }

  private assertState(meta: Item, expected: ActiveMatchStatus): void {
    if (meta.status !== expected || meta.deadlineAtMs <= this.now()) throw new MatchStoreError("MATCH_STATE_CONFLICT");
  }

  private idemKey(input: MatchMutation, route: string) { return { pk: `PLAYER#${input.playerId}`, sk: `IDEMP#${route}#${input.idempotencyKey}` }; }
  private idemPut(key: { pk: string; sk: string }, input: MatchMutation, response: object, now: number) {
    return { Put: { TableName: this.profileTable, Item: { ...key, requestHash: input.requestHash, statusCode: 200, response, expiresAtSeconds: Math.floor(now / 1000) + 86_400 }, ConditionExpression: "attribute_not_exists(pk)" } };
  }
  private replay(item: Item, hash: string): Record<string, unknown> {
    if (item.requestHash !== hash) throw new MatchStoreError("IDEMPOTENCY_KEY_REUSED");
    return item.response;
  }
  private async commit(items: any[], idem: { pk: string; sk: string }, hash: string, response: Record<string, unknown>): Promise<Record<string, unknown>> {
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: items }));
      return response;
    } catch (error) {
      const stored = await this.get(this.profileTable, idem);
      if (stored) return this.replay(stored, hash);
      throw new MatchStoreError(isConditionalFailure(error) ? "MATCH_STATE_CONFLICT" : "MATCH_STORE_FAILURE");
    }
  }

  private async requireMeta(matchId: string): Promise<Item> {
    const meta = await this.get(this.matchTable, { pk: `MATCH#${matchId}`, sk: "META" });
    if (!meta) throw new MatchStoreError("MATCH_NOT_FOUND");
    return meta;
  }

  async recoverDue(publishRetry: (matchId: string) => Promise<void>, now = this.now()): Promise<void> {
    for (let bucket = 0; bucket < 16; bucket++) {
      const cursorKey = { pk: "WORKER#DEADLINE", sk: `BUCKET#${bucket}` };
      const checkpoint = await this.get(this.matchTable, cursorKey);
      const page = await this.client.send(new QueryCommand({
          TableName: this.matchTable,
          IndexName: "DeadlineIndex",
          KeyConditionExpression: "deadlineKey = :key AND deadlineSortKey <= :through",
          ExpressionAttributeValues: { ":key": `ACTIVE#${bucket}`, ":through": `${String(now).padStart(13, "0")}#~` },
          Limit: 2,
          ...(checkpoint?.cursor ? { ExclusiveStartKey: checkpoint.cursor } : {}),
      }));
      for (const item of page.Items ?? []) {
        if (item.sk !== "META" || typeof item.pk !== "string" || !item.pk.startsWith("MATCH#")) continue;
        const matchId = item.pk.slice("MATCH#".length);
        try { await this.expire(matchId); }
        catch { await publishRetry(matchId); }
      }
      // A failed expiry is queued before the cursor advances. If enqueueing fails,
      // this bucket is retried from the same position on the next scheduled run.
      if (page.LastEvaluatedKey || checkpoint?.cursor) {
        await this.client.send(new UpdateCommand({
          TableName: this.matchTable,
          Key: cursorKey,
          UpdateExpression: page.LastEvaluatedKey
            ? "SET cursor = :cursor, revision = if_not_exists(revision, :zero) + :one"
            : "SET revision = if_not_exists(revision, :zero) + :one REMOVE cursor",
          ConditionExpression: checkpoint?.revision === undefined ? "attribute_not_exists(revision)" : "revision = :revision",
          ExpressionAttributeValues: { ":zero": 0, ":one": 1, ...(page.LastEvaluatedKey ? { ":cursor": page.LastEvaluatedKey } : {}), ...(checkpoint?.revision === undefined ? {} : { ":revision": checkpoint.revision }) },
        }));
      }
    }
  }

  async expire(matchId: string): Promise<"EXPIRED" | "ALREADY_TERMINAL" | "NOT_DUE"> {
    const meta = await this.get(this.matchTable, { pk: `MATCH#${matchId}`, sk: "META" });
    if (!meta) throw new MatchStoreError("MATCH_NOT_FOUND");
    if (this.isTerminal(meta.status)) return "ALREADY_TERMINAL";
    if (typeof meta.deadlineAtMs !== "number" || meta.deadlineAtMs > this.now()) return "NOT_DUE";
    return this.terminate(matchId, meta, "EXPIRED");
  }

  private async terminate(matchId: string, meta: Item, target: "EXPIRED" | "FAILED" | "FINISHED", idem?: { pk: string; sk: string }, input?: MatchMutation, response?: Record<string, unknown>): Promise<"EXPIRED" | "ALREADY_TERMINAL" | "NOT_DUE"> {
    const memberIds = meta.memberIds as string[];
    const ticketIds = meta.ticketIds as string[];
    if (!Array.isArray(memberIds) || !Array.isArray(ticketIds) || memberIds.length !== 4 || ticketIds.length !== 4) throw new MatchStoreError("MATCH_STORE_FAILURE");
    const guards: Item[] = [];
    for (const playerId of memberIds) {
      const guard = await this.get(this.profileTable, { pk: `PLAYER#${playerId}`, sk: "ACTIVITY" });
      if (!guard || guard.kind !== "MATCH" || guard.matchId !== matchId) throw new MatchStoreError("MATCH_GUARD_MISMATCH");
      guards.push(guard);
    }
    const now = this.now();
    const ttl = Math.floor(now / 1000) + TERMINAL_TTL_SECONDS;
    const items: any[] = [{ Update: {
      TableName: this.matchTable,
      Key: { pk: `MATCH#${matchId}`, sk: "META" },
      UpdateExpression: "SET #status = :target, revision = :nextRevision, updatedAtMs = :now, expiresAtSeconds = :ttl REMOVE deadlineKey, deadlineSortKey, deadlineAtMs",
      ConditionExpression: "#status = :status AND revision = :revision AND deadlineAtMs " + (target === "EXPIRED" ? "<= :now" : "> :now"),
      ExpressionAttributeNames: { "#status": "status" },
      ExpressionAttributeValues: { ":target": target, ":status": meta.status, ":revision": meta.revision, ":nextRevision": meta.revision + 1, ":now": now, ":ttl": ttl },
    } }];
    for (const [index, playerId] of memberIds.entries()) {
      items.push({ Delete: {
        TableName: this.profileTable,
        Key: { pk: `PLAYER#${playerId}`, sk: "ACTIVITY" },
        ConditionExpression: "kind = :kind AND matchId = :matchId AND revision = :revision",
        ExpressionAttributeValues: { ":kind": "MATCH", ":matchId": matchId, ":revision": guards[index]!.revision },
      } });
    }
    for (const ticketId of ticketIds) {
      items.push({ Update: {
        TableName: this.ticketTable,
        Key: { ticketId },
        UpdateExpression: "SET expiresAtSeconds = :ttl",
        ConditionExpression: "#status = :assigned AND matchId = :matchId",
        ExpressionAttributeNames: { "#status": "status" },
        ExpressionAttributeValues: { ":ttl": ttl, ":assigned": "ASSIGNED", ":matchId": matchId },
      } });
    }
    for (const playerId of memberIds) {
      items.push({ Update: {
        TableName: this.matchTable,
        Key: { pk: `MATCH#${matchId}`, sk: `MEMBER#${playerId}` },
        UpdateExpression: "SET expiresAtSeconds = :ttl",
        ConditionExpression: "attribute_exists(pk)",
        ExpressionAttributeValues: { ":ttl": ttl },
      } });
    }
    if (idem && input && response) items.push(this.idemPut(idem, input, response, now));
    try {
      await this.client.send(new TransactWriteCommand({ TransactItems: items }));
      return target === "EXPIRED" ? "EXPIRED" : "ALREADY_TERMINAL";
    } catch {
      if (idem && input) {
        const stored = await this.get(this.profileTable, idem);
        if (stored) {
          this.replay(stored, input.requestHash);
          return "ALREADY_TERMINAL";
        }
      }
      const latest = await this.get(this.matchTable, { pk: `MATCH#${matchId}`, sk: "META" });
      if (latest && this.isTerminal(latest.status)) {
        if (target === "EXPIRED") return "ALREADY_TERMINAL";
        throw new MatchStoreError("MATCH_STATE_CONFLICT");
      }
      if (latest?.deadlineAtMs > now && target === "EXPIRED") return "NOT_DUE";
      throw new MatchStoreError("MATCH_STORE_FAILURE");
    }
  }

  private isTerminal(status: MatchStatus): boolean {
    return status === "FINISHED" || status === "FAILED" || status === "EXPIRED";
  }

  private async get(table: string, key: Record<string, string>): Promise<Item | undefined> {
    const result = await this.client.send(new GetCommand({ TableName: table, Key: key, ConsistentRead: true }));
    return result.Item;
  }
}
