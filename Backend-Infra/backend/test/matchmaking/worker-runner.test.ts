import { expect, it, vi } from "vitest";

import { runWorker } from "../../src/matchmaking/worker-runner.js";

it("recovers due matches and drains available fours on a scheduled invocation", async () => {
  const deps = { matchStore: { recoverDue: vi.fn().mockResolvedValue(undefined), expire: vi.fn().mockResolvedValue("EXPIRED") }, retryPublisher: { publish: vi.fn() }, assignmentWorker: { assignOnce: vi.fn().mockResolvedValueOnce("m3").mockResolvedValueOnce(null) } };
  await runWorker({}, deps);
  expect(deps.matchStore.recoverDue).toHaveBeenCalledTimes(1);
  expect(deps.assignmentWorker.assignOnce).toHaveBeenCalledTimes(2);
});

it("uses an SQS message only as a queue lookup hint", async () => {
  const deps = { matchStore: { recoverDue: vi.fn(), expire: vi.fn() }, retryPublisher: { publish: vi.fn() }, assignmentWorker: { assignOnce: vi.fn().mockResolvedValue(null) } };
  await runWorker({ Records: [{ body: '{"mode":"standard","clientVersion":"0.1.0"}' }] }, deps);
  expect(deps.matchStore.recoverDue).not.toHaveBeenCalled();
  expect(deps.assignmentWorker.assignOnce).toHaveBeenCalledTimes(1);
});

it("stops after a bounded number of assignments and lets the next invocation continue", async () => {
  const deps = { matchStore: { recoverDue: vi.fn().mockResolvedValue(undefined), expire: vi.fn() }, retryPublisher: { publish: vi.fn() }, assignmentWorker: { assignOnce: vi.fn().mockResolvedValue("m1") } };
  await runWorker({}, deps);
  expect(deps.assignmentWorker.assignOnce).toHaveBeenCalledTimes(4);
});

it("processes deadline retries from SQS and lets failures retry through its DLQ policy", async () => {
  const deps = { matchStore: { recoverDue: vi.fn(), expire: vi.fn().mockRejectedValue(new Error("capacity")) }, retryPublisher: { publish: vi.fn() }, assignmentWorker: { assignOnce: vi.fn() } };
  await expect(runWorker({ Records: [{ body: '{"kind":"deadline-retry","matchId":"m1"}' }] }, deps)).rejects.toThrow("capacity");
  expect(deps.matchStore.expire).toHaveBeenCalledWith("m1");
  expect(deps.assignmentWorker.assignOnce).not.toHaveBeenCalled();
});
