import { MATCHMAKING_CLIENT_VERSION, MATCHMAKING_MODE } from "./ticket-contract.js";

export interface WorkerDependencies {
  matchStore: {
    recoverDue(publishRetry: (matchId: string) => Promise<void>): Promise<void>;
    expire(matchId: string): Promise<"EXPIRED" | "ALREADY_TERMINAL" | "NOT_DUE">;
  };
  retryPublisher: { publish(matchId: string): Promise<void> };
  assignmentWorker: {
    assignOnce(): Promise<string | null>;
  };
}

export async function runWorker(event: { Records?: { body: string }[] }, deps: WorkerDependencies): Promise<void> {
  if (event.Records) {
    for (const record of event.Records) {
      const hint = JSON.parse(record.body) as { kind?: unknown; matchId?: unknown; mode?: unknown; clientVersion?: unknown };
      if (hint.kind === "deadline-retry" && typeof hint.matchId === "string") {
        await deps.matchStore.expire(hint.matchId);
      } else if (hint.mode !== MATCHMAKING_MODE || hint.clientVersion !== MATCHMAKING_CLIENT_VERSION) {
        throw new Error("Invalid queue hint");
      }
    }
  } else {
    await deps.matchStore.recoverDue((matchId) => deps.retryPublisher.publish(matchId));
  }
  for (let count = 0; count < 4; count++) {
    if (await deps.assignmentWorker.assignOnce() === null) break;
  }
}
