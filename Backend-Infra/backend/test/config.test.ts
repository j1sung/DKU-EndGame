import { describe, expect, it } from "vitest";

import { ConfigurationError, loadRuntimeConfig } from "../src/config.js";

const REQUIRED_ENV = {
  EOS_PRODUCT_ID: "product-id",
  EOS_SANDBOX_ID: "sandbox-id",
  EOS_DEPLOYMENT_ID: "deployment-id",
  EOS_CLIENT_ID: "client-id",
  EOS_CLIENT_SECRET_ARN: "arn:aws:secretsmanager:ap-northeast-2:123456789012:secret:eos",
};

describe("loadRuntimeConfig", () => {
  it("loads required values and applies verifier defaults", () => {
    expect(loadRuntimeConfig({ ...REQUIRED_ENV })).toEqual({
      productId: "product-id",
      sandboxId: "sandbox-id",
      deploymentId: "deployment-id",
      clientId: "client-id",
      clientSecretArn: REQUIRED_ENV.EOS_CLIENT_SECRET_ARN,
      verifierPath: "/opt/bin/eos-token-verifier",
      verifierTimeoutMs: 8_000,
      verifierMaxOutputBytes: 65_536,
    });
  });

  it.each(Object.keys(REQUIRED_ENV))("rejects missing or blank %s", (name) => {
    const missing = { ...REQUIRED_ENV } as NodeJS.ProcessEnv;
    delete missing[name];
    const blank = { ...REQUIRED_ENV, [name]: "   " };

    for (const env of [missing, blank]) {
      expect(() => loadRuntimeConfig(env)).toThrowError(ConfigurationError);
      try {
        loadRuntimeConfig(env);
      } catch (error) {
        expect(error).toMatchObject({
          code: "INTERNAL_CONFIGURATION_ERROR",
          message: "Internal server error",
        });
      }
    }
  });

  it.each([
    ["EOS_VERIFIER_TIMEOUT_MS", "0"],
    ["EOS_VERIFIER_TIMEOUT_MS", "8.5"],
    ["EOS_VERIFIER_MAX_OUTPUT_BYTES", "not-a-number"],
    ["EOS_VERIFIER_MAX_OUTPUT_BYTES", "-1"],
  ])("rejects invalid positive integer %s=%s", (name, value) => {
    expect(() =>
      loadRuntimeConfig({ ...REQUIRED_ENV, [name]: value }),
    ).toThrowError(ConfigurationError);
  });

  it("accepts explicit verifier settings", () => {
    expect(
      loadRuntimeConfig({
        ...REQUIRED_ENV,
        EOS_VERIFIER_PATH: "/custom/verifier",
        EOS_VERIFIER_TIMEOUT_MS: "1234",
        EOS_VERIFIER_MAX_OUTPUT_BYTES: "4096",
      }),
    ).toMatchObject({
      verifierPath: "/custom/verifier",
      verifierTimeoutMs: 1_234,
      verifierMaxOutputBytes: 4_096,
    });
  });
});
