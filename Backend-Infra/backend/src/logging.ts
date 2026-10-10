export type AuthOutcome =
  | "success"
  | "missing_header"
  | "malformed_header"
  | "invalid_token"
  | "verifier_unavailable"
  | "verifier_failure"
  | "configuration_error";

interface SafeLogBase {
  requestId: string;
  route: string;
  statusCode: number;
  authOutcome: AuthOutcome;
}

export interface SafeSuccessLog extends SafeLogBase {
  authOutcome: "success";
  playerId: string;
  verificationMs: number;
}

export interface SafeFailureLog extends SafeLogBase {
  authOutcome: Exclude<AuthOutcome, "success">;
  verificationMs?: number;
}

export type SafeLogEvent = SafeSuccessLog | SafeFailureLog;

export interface SafeLogger {
  write(event: SafeLogEvent): void;
}
