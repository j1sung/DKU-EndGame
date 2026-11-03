import { SecretsManagerClient } from "@aws-sdk/client-secrets-manager";
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";

import { createHandler } from "./app.js";
import { NativeTokenVerifier } from "./auth/native-token-verifier.js";
import { SecretsManagerClientSecretProvider } from "./auth/secrets-manager-secret-provider.js";
import { loadRuntimeConfig } from "./config.js";
import type { SafeLogger } from "./logging.js";
import { DynamoProfileStore } from "./profile/dynamo-profile-store.js";
import { DynamoTicketStore } from "./matchmaking/dynamo-ticket-store.js";
import { DynamoMatchStore } from "./matchmaking/dynamo-match-store.js";
import { SQSClient } from "@aws-sdk/client-sqs";
import { SqsQueueHintPublisher } from "./matchmaking/queue-hint.js";

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
const documentClient = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const profileStore = new DynamoProfileStore(documentClient, config.profileTableName);
const matchStore = new DynamoMatchStore(documentClient, config.profileTableName, config.ticketTableName, config.matchTableName);
const ticketStore = new DynamoTicketStore(documentClient, config.profileTableName, config.ticketTableName, Date.now, undefined, (matchId) => matchStore.expire(matchId));
const queueHintPublisher = new SqsQueueHintPublisher(new SQSClient({}), config.matchmakingQueueUrl);
const logger: SafeLogger = {
  write(event) {
    console.log(JSON.stringify(event));
  },
};

export const handler = createHandler({ verifier, secretProvider, logger, profileStore, ticketStore, matchStore, queueHintPublisher });
