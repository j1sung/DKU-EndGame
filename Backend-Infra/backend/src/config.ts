export interface RuntimeConfig {
  productId: string;
  sandboxId: string;
  deploymentId: string;
  clientId: string;
  clientSecretArn: string;
  verifierPath: string;
  verifierTimeoutMs: number;
  verifierMaxOutputBytes: number;
}

export class ConfigurationError extends Error {
  public readonly code = "INTERNAL_CONFIGURATION_ERROR" as const;

  public constructor() {
    super("Internal server error");
    this.name = "ConfigurationError";
  }
}

function required(env: NodeJS.ProcessEnv, name: string): string {
  const value = env[name]?.trim();
  if (value === undefined || value.length === 0) {
    throw new ConfigurationError();
  }
  return value;
}

function positiveInteger(
  env: NodeJS.ProcessEnv,
  name: string,
  fallback: number,
): number {
  const raw = env[name];
  if (raw === undefined) {
    return fallback;
  }
  if (!/^[1-9][0-9]*$/.test(raw)) {
    throw new ConfigurationError();
  }
  const value = Number(raw);
  if (!Number.isSafeInteger(value)) {
    throw new ConfigurationError();
  }
  return value;
}

export function loadRuntimeConfig(env: NodeJS.ProcessEnv): RuntimeConfig {
  return {
    productId: required(env, "EOS_PRODUCT_ID"),
    sandboxId: required(env, "EOS_SANDBOX_ID"),
    deploymentId: required(env, "EOS_DEPLOYMENT_ID"),
    clientId: required(env, "EOS_CLIENT_ID"),
    clientSecretArn: required(env, "EOS_CLIENT_SECRET_ARN"),
    verifierPath:
      env.EOS_VERIFIER_PATH === undefined
        ? "/opt/bin/eos-token-verifier"
        : required(env, "EOS_VERIFIER_PATH"),
    verifierTimeoutMs: positiveInteger(env, "EOS_VERIFIER_TIMEOUT_MS", 8_000),
    verifierMaxOutputBytes: positiveInteger(
      env,
      "EOS_VERIFIER_MAX_OUTPUT_BYTES",
      65_536,
    ),
  };
}
