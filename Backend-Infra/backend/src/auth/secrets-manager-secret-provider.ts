import {
  GetSecretValueCommand,
  type SecretsManagerClient,
} from "@aws-sdk/client-secrets-manager";

import { ConfigurationError } from "../config.js";
import type { ClientSecretProvider } from "./secret-provider.js";

type SecretClient = Pick<SecretsManagerClient, "send">;

export class SecretsManagerClientSecretProvider
  implements ClientSecretProvider
{
  private cachedSecret: Promise<string> | undefined;

  public constructor(
    private readonly client: SecretClient,
    private readonly secretId: string,
  ) {}

  public async getClientSecret(): Promise<string> {
    if (this.cachedSecret !== undefined) {
      return this.cachedSecret;
    }

    const pending = this.loadClientSecret();
    this.cachedSecret = pending;

    try {
      return await pending;
    } catch {
      if (this.cachedSecret === pending) {
        this.cachedSecret = undefined;
      }
      throw new ConfigurationError();
    }
  }

  private async loadClientSecret(): Promise<string> {
    const output = await this.client.send(
      new GetSecretValueCommand({ SecretId: this.secretId }),
    );
    const secret = output.SecretString?.trim();
    if (secret === undefined || secret.length === 0) {
      throw new ConfigurationError();
    }
    return secret;
  }
}
