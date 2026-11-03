import {
  GetSecretValueCommand,
  type GetSecretValueCommandOutput,
  type SecretsManagerClient,
} from "@aws-sdk/client-secrets-manager";
import { describe, expect, it, vi } from "vitest";

import { SecretsManagerClientSecretProvider } from "../../src/auth/secrets-manager-secret-provider.js";
import { ConfigurationError } from "../../src/config.js";

const SECRET_ID = "arn:aws:secretsmanager:ap-northeast-2:123456789012:secret:eos";

function output(secretString?: string): GetSecretValueCommandOutput {
  return {
    ...(secretString === undefined ? {} : { SecretString: secretString }),
    $metadata: { httpStatusCode: 200 },
  };
}

function makeClient(send: ReturnType<typeof vi.fn>) {
  return { send } as unknown as Pick<SecretsManagerClient, "send">;
}

describe("SecretsManagerClientSecretProvider", () => {
  it("loads the requested SecretString once and caches only the success", async () => {
    const send = vi.fn().mockResolvedValue(output("client-secret"));
    const provider = new SecretsManagerClientSecretProvider(
      makeClient(send),
      SECRET_ID,
    );

    await expect(provider.getClientSecret()).resolves.toBe("client-secret");
    await expect(provider.getClientSecret()).resolves.toBe("client-secret");
    expect(send).toHaveBeenCalledTimes(1);
    expect(send.mock.calls[0]?.[0]).toBeInstanceOf(GetSecretValueCommand);
    expect((send.mock.calls[0]?.[0] as GetSecretValueCommand).input).toEqual({
      SecretId: SECRET_ID,
    });
  });

  it.each([output(), output(""), output("   ")])(
    "rejects and does not cache an empty SecretString",
    async (emptyOutput) => {
      const send = vi
        .fn()
        .mockResolvedValueOnce(emptyOutput)
        .mockResolvedValueOnce(output("client-secret"));
      const provider = new SecretsManagerClientSecretProvider(
        makeClient(send),
        SECRET_ID,
      );

      await expect(provider.getClientSecret()).rejects.toBeInstanceOf(
        ConfigurationError,
      );
      await expect(provider.getClientSecret()).resolves.toBe("client-secret");
      expect(send).toHaveBeenCalledTimes(2);
    },
  );

  it("does not cache a rejected AWS request", async () => {
    const send = vi
      .fn()
      .mockRejectedValueOnce(new Error("AWS response containing sensitive metadata"))
      .mockResolvedValueOnce(output("client-secret"));
    const provider = new SecretsManagerClientSecretProvider(
      makeClient(send),
      SECRET_ID,
    );

    await expect(provider.getClientSecret()).rejects.toMatchObject({
      code: "INTERNAL_CONFIGURATION_ERROR",
      message: "Internal server error",
    });
    await expect(provider.getClientSecret()).resolves.toBe("client-secret");
    expect(send).toHaveBeenCalledTimes(2);
  });
});
