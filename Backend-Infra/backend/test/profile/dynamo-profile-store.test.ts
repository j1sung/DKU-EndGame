import { GetCommand, TransactWriteCommand, type DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { describe, expect, it, vi } from "vitest";

import { DynamoProfileStore, ProfileStoreError } from "../../src/profile/dynamo-profile-store.js";

const PLAYER_ID = "0123456789abcdef0123456789abcdef";
const INPUT = {
  playerId: PLAYER_ID,
  nickname: "닭싸움왕",
  nicknameKey: "닭싸움왕",
  idempotencyKey: "123e4567-e89b-42d3-a456-426614174000",
  requestHash: "body-hash",
  requestId: "original-request-id",
};

function store(send: ReturnType<typeof vi.fn>) {
  return new DynamoProfileStore(
    { send } as unknown as DynamoDBDocumentClient,
    "profiles",
    () => 1_800_000_000_000,
  );
}

describe("DynamoProfileStore", () => {
  it("reads a profile consistently by verified player ID", async () => {
    const send = vi.fn().mockResolvedValueOnce({ Item: { nickname: "닭싸움왕" } });

    expect(await store(send).get(PLAYER_ID)).toEqual({ playerId: PLAYER_ID, nickname: "닭싸움왕" });
    expect(send.mock.calls[0][0]).toBeInstanceOf(GetCommand);
    expect(send.mock.calls[0][0].input).toEqual({
      TableName: "profiles",
      Key: { pk: `PLAYER#${PLAYER_ID}`, sk: "PROFILE" },
      ConsistentRead: true,
    });
  });

  it("returns identity without a nickname when a profile does not exist", async () => {
    const send = vi.fn().mockResolvedValueOnce({});
    expect(await store(send).get(PLAYER_ID)).toEqual({ playerId: PLAYER_ID });
  });

  it("atomically creates the profile, unique claim, and idempotency record", async () => {
    const send = vi.fn().mockResolvedValueOnce({}).mockResolvedValueOnce({});

    expect(await store(send).registerNickname(INPUT)).toEqual({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
      requestId: "original-request-id",
      created: true,
    });
    const command = send.mock.calls[1][0];
    expect(command).toBeInstanceOf(TransactWriteCommand);
    expect(command.input.TransactItems).toHaveLength(3);
    expect(command.input.TransactItems[0].Update).toMatchObject({
      TableName: "profiles",
      Key: { pk: `PLAYER#${PLAYER_ID}`, sk: "PROFILE" },
      ConditionExpression: "attribute_not_exists(nicknameKey)",
    });
    expect(command.input.TransactItems[1].Put).toMatchObject({
      TableName: "profiles",
      Item: { pk: "NICK#닭싸움왕", sk: "CLAIM", playerId: PLAYER_ID },
      ConditionExpression: "attribute_not_exists(pk)",
    });
    expect(command.input.TransactItems[2].Put).toMatchObject({
      TableName: "profiles",
      Item: {
        pk: `PLAYER#${PLAYER_ID}`,
        sk: `IDEMP#POST /v1/me/nickname#${INPUT.idempotencyKey}`,
        requestHash: "body-hash",
        statusCode: 201,
        response: { playerId: PLAYER_ID, nickname: "닭싸움왕", requestId: "original-request-id" },
        expiresAtSeconds: 1_800_086_400,
      },
      ConditionExpression: "attribute_not_exists(pk)",
    });
  });

  it("returns the original result on a same-key same-body retry", async () => {
    const send = vi.fn().mockResolvedValueOnce({
      Item: { requestHash: "body-hash", response: { playerId: PLAYER_ID, nickname: "닭싸움왕", requestId: "original-request-id" } },
    });
    expect(await store(send).registerNickname(INPUT)).toEqual({
      playerId: PLAYER_ID,
      nickname: "닭싸움왕",
      requestId: "original-request-id",
      created: false,
    });
    expect(send).toHaveBeenCalledTimes(1);
  });

  it("rejects an idempotency key reused for another body", async () => {
    const send = vi.fn().mockResolvedValueOnce({ Item: { requestHash: "other-hash" } });
    await expect(store(send).registerNickname(INPUT)).rejects.toMatchObject({
      code: "IDEMPOTENCY_KEY_REUSED",
    });
    expect(send).toHaveBeenCalledTimes(1);
  });

  it("resolves an ambiguous transaction outcome from its idempotency record", async () => {
    const send = vi.fn()
      .mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error("response lost"))
      .mockResolvedValueOnce({
        Item: { requestHash: "body-hash", response: { playerId: PLAYER_ID, nickname: "닭싸움왕", requestId: "original-request-id" } },
      });
    expect(await store(send).registerNickname(INPUT)).toMatchObject({ created: false });
    expect(send).toHaveBeenCalledTimes(3);
  });

  it("maps another player's existing claim to NICKNAME_TAKEN", async () => {
    const send = vi.fn()
      .mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error("transaction conflict"))
      .mockResolvedValueOnce({})
      .mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { playerId: "another-player" } });
    await expect(store(send).registerNickname(INPUT)).rejects.toMatchObject({
      code: "NICKNAME_TAKEN",
    });
    expect(send.mock.calls[4][0].input).toMatchObject({
      Key: { pk: "NICK#닭싸움왕", sk: "CLAIM" },
      ConsistentRead: true,
    });
  });

  it("maps a profile with a nickname to NICKNAME_ALREADY_SET", async () => {
    const send = vi.fn()
      .mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error("transaction conflict"))
      .mockResolvedValueOnce({})
      .mockResolvedValueOnce({ Item: { nickname: "기존이름" } });
    await expect(store(send).registerNickname(INPUT)).rejects.toMatchObject({
      code: "NICKNAME_ALREADY_SET",
    });
  });
});
