export type AuthErrorCode =
  | "AUTH_MISSING"
  | "AUTH_MALFORMED"
  | "AUTH_INVALID";

export class AuthError extends Error {
  public readonly code: AuthErrorCode;
  public readonly statusCode: number;

  public constructor(
    code: AuthErrorCode,
    statusCode: number,
    message: string,
  ) {
    super(message);
    this.name = "AuthError";
    this.code = code;
    this.statusCode = statusCode;
  }
}
