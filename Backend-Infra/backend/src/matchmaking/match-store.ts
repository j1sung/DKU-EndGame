import type { MatchMutation } from "./dynamo-match-store.js";

export interface MatchStore {
  getForMember(matchId: string, playerId: string): Promise<Record<string, unknown>>;
  registerSession(input: MatchMutation & { sessionId: string }): Promise<Record<string, unknown>>;
  join(input: MatchMutation): Promise<Record<string, unknown>>;
  start(input: MatchMutation): Promise<Record<string, unknown>>;
  finish(input: MatchMutation): Promise<Record<string, unknown>>;
  leave(input: MatchMutation): Promise<Record<string, unknown>>;
}
