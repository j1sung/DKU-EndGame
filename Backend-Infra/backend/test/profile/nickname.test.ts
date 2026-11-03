import { describe, expect, it } from "vitest";

import {
  NicknameValidationError,
  normalizeNickname,
} from "../../src/profile/nickname.js";

describe("normalizeNickname", () => {
  it("normalizes width, case for the key, and repeated spaces", () => {
    expect(normalizeNickname("  ＣＨＩＣＫＥＮ   1  ")).toEqual({
      nickname: "CHICKEN 1",
      nicknameKey: "chicken 1",
    });
    expect(normalizeNickname("chicken 1").nicknameKey).toBe("chicken 1");
  });

  it("allows completed Hangul syllables", () => {
    expect(normalizeNickname("닭싸움왕")).toEqual({
      nickname: "닭싸움왕",
      nicknameKey: "닭싸움왕",
    });
  });

  it.each(["A", "A".repeat(17), "A!", "A\nB", "ㅋㅋㅋ", "A😀", 123, null])(
    "rejects invalid nickname %s",
    (input) => {
      expect(() => normalizeNickname(input)).toThrow(NicknameValidationError);
      try {
        normalizeNickname(input);
      } catch (error) {
        expect(error).toMatchObject({ code: "INVALID_NICKNAME" });
      }
    },
  );
});
