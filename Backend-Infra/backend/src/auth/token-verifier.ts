export interface TokenVerificationInput {
  token: string;
  candidatePlayerId: string;
  clientSecret: string;
}

export interface VerifiedIdentity {
  playerId: string;
  verificationMs: number;
}

export interface TokenVerifier {
  verify(input: TokenVerificationInput): Promise<VerifiedIdentity>;
}

export type TokenVerificationErrorKind =
  | "invalid"
  | "unavailable"
  | "failure";

export class TokenVerificationError extends Error {
  public readonly kind: TokenVerificationErrorKind;

  public constructor(kind: TokenVerificationErrorKind) {
    super("Token verification failed");
    this.name = "TokenVerificationError";
    this.kind = kind;
  }
}
