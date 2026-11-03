import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient } from "@aws-sdk/lib-dynamodb";
import { SQSClient } from "@aws-sdk/client-sqs";

import { DynamoAssignmentWorker } from "./matchmaking/assignment-worker.js";
import { DynamoMatchStore } from "./matchmaking/dynamo-match-store.js";
import { runWorker } from "./matchmaking/worker-runner.js";
import { SqsDeadlineRetryPublisher } from "./matchmaking/queue-hint.js";

function required(name: string): string {
  const value = process.env[name];
  if (!value) throw new Error(`Missing worker configuration: ${name}`);
  return value;
}

const profiles = required("PLAYER_PROFILES_TABLE");
const tickets = required("MATCHMAKING_TICKETS_TABLE");
const matches = required("MATCHES_TABLE");
const queueUrl = required("MATCHMAKING_QUEUE_URL");
const client = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const deps = {
  matchStore: new DynamoMatchStore(client, profiles, tickets, matches),
  assignmentWorker: new DynamoAssignmentWorker(client, profiles, tickets, matches),
  retryPublisher: new SqsDeadlineRetryPublisher(new SQSClient({}), queueUrl),
};

export const handler = async (event: { Records?: { body: string }[] }): Promise<void> => {
  await runWorker(event, deps);
};
