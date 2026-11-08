export interface PlayerProfile {
  playerId: string;
  nickname?: string;
}

export interface NicknameRegistration {
  playerId: string;
  nickname: string;
  nicknameKey: string;
  idempotencyKey: string;
  requestHash: string;
  requestId: string;
}

export interface NicknameRegistrationResult {
  playerId: string;
  nickname: string;
  requestId: string;
  created: boolean;
}

export interface ProfileStore {
  get(playerId: string): Promise<PlayerProfile>;
  registerNickname(input: NicknameRegistration): Promise<NicknameRegistrationResult>;
}
