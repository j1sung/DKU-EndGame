import { SecretsManagerClient } from "@aws-sdk/client-secrets-manager";
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

import { createHandler } from "./app.js";
import { NativeTokenVerifier } from "./auth/native-token-verifier.js";
import { SecretsManagerClientSecretProvider } from "./auth/secrets-manager-secret-provider.js";
import { loadRuntimeConfig } from "./config.js";
import type { SafeLogger } from "./logging.js";
import { DynamoProfileStore } from "./profile/dynamo-profile-store.js";

const config = loadRuntimeConfig(process.env);
const secretProvider = new SecretsManagerClientSecretProvider(
  new SecretsManagerClient({}),
  config.clientSecretArn,
);
const verifier = new NativeTokenVerifier({
  executablePath: config.verifierPath,
  eos: config,
  timeoutMs: config.verifierTimeoutMs,
  maxOutputBytes: config.verifierMaxOutputBytes,
});
const profileStore = new DynamoProfileStore(
  DynamoDBDocumentClient.from(new DynamoDBClient({})),
  config.profileTableName,
);
const logger: SafeLogger = {
  write(event) {
    console.log(JSON.stringify(event));
  },
};

export const handler = createHandler({ verifier, secretProvider, logger, profileStore });
