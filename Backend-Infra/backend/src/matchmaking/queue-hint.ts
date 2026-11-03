import { SendMessageCommand, type SQSClient } from "@aws-sdk/client-sqs";

import { MATCHMAKING_CLIENT_VERSION, MATCHMAKING_MODE } from "./ticket-contract.js";

export interface QueueHintPublisher {
  publish(): Promise<void>;
}

export class SqsQueueHintPublisher implements QueueHintPublisher {
  constructor(private readonly client: SQSClient, private readonly queueUrl: string) {}

  async publish(): Promise<void> {
    await this.client.send(new SendMessageCommand({
      QueueUrl: this.queueUrl,
      MessageBody: JSON.stringify({ mode: MATCHMAKING_MODE, clientVersion: MATCHMAKING_CLIENT_VERSION }),
    }));
  }
}

export class SqsDeadlineRetryPublisher {
  constructor(private readonly client: SQSClient, private readonly queueUrl: string) {}

  async publish(matchId: string): Promise<void> {
    await this.client.send(new SendMessageCommand({
      QueueUrl: this.queueUrl,
      MessageBody: JSON.stringify({ kind: "deadline-retry", matchId }),
    }));
  }
}
