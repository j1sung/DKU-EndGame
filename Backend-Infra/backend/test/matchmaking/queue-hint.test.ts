import { SendMessageCommand, type SQSClient } from "@aws-sdk/client-sqs";
import { expect, it, vi } from "vitest";

import { SqsDeadlineRetryPublisher, SqsQueueHintPublisher } from "../../src/matchmaking/queue-hint.js";

it("sends only the allowed mode and version as a queue lookup hint", async () => {
  const send = vi.fn().mockResolvedValue({});
  await new SqsQueueHintPublisher({ send } as unknown as SQSClient, "https://sqs.example/queue").publish();
  expect(send).toHaveBeenCalledTimes(1);
  const command = send.mock.calls[0]![0];
  expect(command).toBeInstanceOf(SendMessageCommand);
  expect(command.input).toEqual({ QueueUrl: "https://sqs.example/queue", MessageBody: '{"mode":"standard","clientVersion":"0.1.0"}' });
});

it("queues a failed deadline candidate for SQS retry", async () => {
  const send = vi.fn().mockResolvedValue({});
  await new SqsDeadlineRetryPublisher({ send } as unknown as SQSClient, "https://sqs.example/queue").publish("match-1");
  expect(send.mock.calls[0]![0].input).toEqual({ QueueUrl: "https://sqs.example/queue", MessageBody: '{"kind":"deadline-retry","matchId":"match-1"}' });
});
