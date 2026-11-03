import { createHash } from "node:crypto";

export type ActiveMatchStatus = "FORMING" | "SESSION_READY" | "READY_TO_START" | "IN_GAME";
export type TerminalMatchStatus = "FINISHED" | "FAILED" | "EXPIRED";
export type MatchStatus = ActiveMatchStatus | TerminalMatchStatus;

export function phaseDurationMs(status: ActiveMatchStatus): number {
  return status === "IN_GAME" ? 1_800_000 : 60_000;
}

export function deadlineIndexKeys(matchId: string, deadlineAtMs: number): { deadlineKey: string; deadlineSortKey: string } {
  const bucket = createHash("sha256").update(matchId).digest()[0]! % 16;
  return {
    deadlineKey: `ACTIVE#${bucket}`,
    deadlineSortKey: `${String(deadlineAtMs).padStart(13, "0")}#${matchId}`,
  };
}
