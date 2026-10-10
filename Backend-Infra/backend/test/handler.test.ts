import { describe, expect, it, vi } from "vitest";

const REQUIRED_ENV = {
  EOS_PRODUCT_ID: "product-id",
  EOS_SANDBOX_ID: "sandbox-id",
  EOS_DEPLOYMENT_ID: "deployment-id",
  EOS_CLIENT_ID: "client-id",
  EOS_CLIENT_SECRET_ARN: "arn:aws:secretsmanager:ap-northeast-2:123456789012:secret:eos",
};

describe("production handler", () => {
  it("exports a Lambda handler without reading the Secret during module load", async () => {
    vi.stubEnv("EOS_PRODUCT_ID", REQUIRED_ENV.EOS_PRODUCT_ID);
    vi.stubEnv("EOS_SANDBOX_ID", REQUIRED_ENV.EOS_SANDBOX_ID);
    vi.stubEnv("EOS_DEPLOYMENT_ID", REQUIRED_ENV.EOS_DEPLOYMENT_ID);
    vi.stubEnv("EOS_CLIENT_ID", REQUIRED_ENV.EOS_CLIENT_ID);
    vi.stubEnv("EOS_CLIENT_SECRET_ARN", REQUIRED_ENV.EOS_CLIENT_SECRET_ARN);

    const module = await import("../src/handler.js");

    expect(module.handler).toBeTypeOf("function");
  });
});
