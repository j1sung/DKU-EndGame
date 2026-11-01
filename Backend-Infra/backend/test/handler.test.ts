import { describe, expect, it, vi } from "vitest";

const REQUIRED_ENV = {
  EOS_PRODUCT_ID: "product-id",
  EOS_SANDBOX_ID: "sandbox-id",
  EOS_DEPLOYMENT_ID: "deployment-id",
  EOS_CLIENT_ID: "client-id",
  EOS_CLIENT_SECRET_ARN: "arn:aws:secretsmanager:ap-northeast-2:123456789012:secret:eos",
  PLAYER_PROFILES_TABLE: "chickengame-dev-player-profiles",
  MATCHMAKING_TICKETS_TABLE: "chickengame-dev-matchmaking-tickets",
  MATCHES_TABLE: "chickengame-dev-matches",
  MATCHMAKING_QUEUE_URL: "https://sqs.example/queue",
};

describe("production handler", () => {
  it("exports a Lambda handler without reading the Secret during module load", async () => {
    vi.stubEnv("EOS_PRODUCT_ID", REQUIRED_ENV.EOS_PRODUCT_ID);
    vi.stubEnv("EOS_SANDBOX_ID", REQUIRED_ENV.EOS_SANDBOX_ID);
    vi.stubEnv("EOS_DEPLOYMENT_ID", REQUIRED_ENV.EOS_DEPLOYMENT_ID);
    vi.stubEnv("EOS_CLIENT_ID", REQUIRED_ENV.EOS_CLIENT_ID);
    vi.stubEnv("EOS_CLIENT_SECRET_ARN", REQUIRED_ENV.EOS_CLIENT_SECRET_ARN);
    vi.stubEnv("PLAYER_PROFILES_TABLE", REQUIRED_ENV.PLAYER_PROFILES_TABLE);
    vi.stubEnv("MATCHMAKING_TICKETS_TABLE", REQUIRED_ENV.MATCHMAKING_TICKETS_TABLE);
    vi.stubEnv("MATCHES_TABLE", REQUIRED_ENV.MATCHES_TABLE);
    vi.stubEnv("MATCHMAKING_QUEUE_URL", REQUIRED_ENV.MATCHMAKING_QUEUE_URL);

    const module = await import("../src/handler.js");

    expect(module.handler).toBeTypeOf("function");
  });
});
