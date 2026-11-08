import {
  GetCommand,
  TransactWriteCommand,
  type DynamoDBDocumentClient,
} from "@aws-sdk/lib-dynamodb";

import type {
  NicknameRegistration,
  NicknameRegistrationResult,
  PlayerProfile,
  ProfileStore,
} from "./profile-store.js";

export type ProfileStoreErrorCode =
  | "NICKNAME_TAKEN"
  | "NICKNAME_ALREADY_SET"
  | "IDEMPOTENCY_KEY_REUSED"
  | "PROFILE_STORE_FAILURE";

export class ProfileStoreError extends Error {
  public constructor(public readonly code: ProfileStoreErrorCode) {
    super(code);
    this.name = "ProfileStoreError";
  }
}

export class DynamoProfileStore implements ProfileStore {
  public constructor(
    private readonly client: DynamoDBDocumentClient,
    private readonly tableName: string,
    private readonly now: () => number = Date.now,
  ) {}

  public async get(playerId: string): Promise<PlayerProfile> {
    const response = await this.client.send(new GetCommand({
      TableName: this.tableName,
      Key: { pk: `PLAYER#${playerId}`, sk: "PROFILE" },
      ConsistentRead: true,
    }));
    return response.Item?.nickname === undefined
      ? { playerId }
      : { playerId, nickname: response.Item.nickname as string };
  }

  public async registerNickname(input: NicknameRegistration): Promise<NicknameRegistrationResult> {
    const playerPk = `PLAYER#${input.playerId}`;
    const idemKey = {
      pk: playerPk,
      sk: `IDEMP#POST /v1/me/nickname#${input.idempotencyKey}`,
    };
    const existing = await this.getItem(idemKey);
    if (existing !== undefined) {
      return this.replay(existing, input.requestHash);
    }

    const response = { playerId: input.playerId, nickname: input.nickname, requestId: input.requestId };
    const timestamp = this.now();
    try {
      await this.client.send(new TransactWriteCommand({
        TransactItems: [
          {
            Update: {
              TableName: this.tableName,
              Key: { pk: playerPk, sk: "PROFILE" },
              UpdateExpression: "SET playerId = :playerId, nickname = :nickname, nicknameKey = :nicknameKey, createdAt = if_not_exists(createdAt, :now), updatedAt = :now, revision = if_not_exists(revision, :zero) + :one",
              ConditionExpression: "attribute_not_exists(nicknameKey)",
              ExpressionAttributeValues: {
                ":playerId": input.playerId,
                ":nickname": input.nickname,
                ":nicknameKey": input.nicknameKey,
                ":now": timestamp,
                ":zero": 0,
                ":one": 1,
              },
            },
          },
          {
            Put: {
              TableName: this.tableName,
              Item: {
                pk: `NICK#${input.nicknameKey}`,
                sk: "CLAIM",
                playerId: input.playerId,
                createdAt: timestamp,
              },
              ConditionExpression: "attribute_not_exists(pk)",
            },
          },
          {
            Put: {
              TableName: this.tableName,
              Item: {
                ...idemKey,
                requestHash: input.requestHash,
                statusCode: 201,
                response,
                expiresAtSeconds: Math.floor(timestamp / 1000) + 86_400,
              },
              ConditionExpression: "attribute_not_exists(pk)",
            },
          },
        ],
      }));
      return { ...response, created: true };
    } catch {
      const stored = await this.getItem(idemKey);
      if (stored !== undefined) {
        return this.replay(stored, input.requestHash);
      }
      const profile = await this.getItem({ pk: playerPk, sk: "PROFILE" });
      if (profile?.nickname !== undefined) {
        throw new ProfileStoreError("NICKNAME_ALREADY_SET");
      }
      const claim = await this.getItem({ pk: `NICK#${input.nicknameKey}`, sk: "CLAIM" });
      if (claim !== undefined) {
        throw new ProfileStoreError("NICKNAME_TAKEN");
      }
      throw new ProfileStoreError("PROFILE_STORE_FAILURE");
    }
  }

  private async getItem(key: { pk: string; sk: string }): Promise<Record<string, unknown> | undefined> {
    const response = await this.client.send(new GetCommand({
      TableName: this.tableName,
      Key: key,
      ConsistentRead: true,
    }));
    return response.Item;
  }

  private replay(item: Record<string, unknown>, requestHash: string): NicknameRegistrationResult {
    if (item.requestHash !== requestHash) {
      throw new ProfileStoreError("IDEMPOTENCY_KEY_REUSED");
    }
    const response = item.response as { playerId: string; nickname: string; requestId: string };
    return { ...response, created: false };
  }
}
