export class NicknameValidationError extends Error {
  public readonly code = "INVALID_NICKNAME" as const;

  public constructor() {
    super("Invalid nickname");
    this.name = "NicknameValidationError";
  }
}

export function normalizeNickname(input: unknown): {
  nickname: string;
  nicknameKey: string;
} {
  if (typeof input !== "string" || /[\r\n\t]/u.test(input)) {
    throw new NicknameValidationError();
  }

  const nickname = input.normalize("NFKC").trim().replace(/ +/gu, " ");
  const length = [...nickname].length;
  if (length < 2 || length > 16 || !/^[가-힣A-Za-z0-9 ]+$/u.test(nickname)) {
    throw new NicknameValidationError();
  }

  return {
    nickname,
    nicknameKey: nickname.replace(/[A-Z]/gu, (letter) => letter.toLowerCase()),
  };
}
