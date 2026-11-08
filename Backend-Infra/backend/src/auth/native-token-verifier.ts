import {
  spawn,
  type ChildProcessWithoutNullStreams,
  type SpawnOptionsWithoutStdio,
} from "node:child_process";

import type { RuntimeConfig } from "../config.js";
import {
  TokenVerificationError,
  type TokenVerificationInput,
  type TokenVerifier,
  type VerifiedIdentity,
} from "./token-verifier.js";

type SpawnProcess = (
  command: string,
  args: readonly string[],
  options: SpawnOptionsWithoutStdio,
) => ChildProcessWithoutNullStreams;

export interface NativeTokenVerifierOptions {
  executablePath: string;
  executableArguments?: readonly string[];
  eos: Pick<RuntimeConfig, "productId" | "sandboxId" | "deploymentId" | "clientId">;
  timeoutMs: number;
  maxOutputBytes: number;
  spawnProcess?: SpawnProcess;
}

export class NativeTokenVerifier implements TokenVerifier {
  public constructor(private readonly options: NativeTokenVerifierOptions) {}

  public verify(input: TokenVerificationInput): Promise<VerifiedIdentity> {
    const startedAt = Date.now();
    return new Promise((resolve, reject) => {
      let child: ChildProcessWithoutNullStreams;
      try {
        const spawnProcess = this.options.spawnProcess ?? (spawn as SpawnProcess);
        child = spawnProcess(
          this.options.executablePath,
          [...(this.options.executableArguments ?? [])],
          {
            shell: false,
            stdio: "pipe",
            env: {
              EOS_PRODUCT_ID: this.options.eos.productId,
              EOS_SANDBOX_ID: this.options.eos.sandboxId,
              EOS_DEPLOYMENT_ID: this.options.eos.deploymentId,
              EOS_CLIENT_ID: this.options.eos.clientId,
              EOS_CLIENT_SECRET: input.clientSecret,
            },
          },
        );
      } catch {
        reject(new TokenVerificationError("failure"));
        return;
      }

      let stdout = "";
      let outputBytes = 0;
      let forcedKind: "failure" | "unavailable" | undefined;
      let settled = false;
      let timer: NodeJS.Timeout;
      const finish = (kind: "invalid" | "failure" | "unavailable") => {
        if (settled) return;
        settled = true;
        clearTimeout(timer);
        reject(new TokenVerificationError(kind));
      };
      const consume = (chunk: Buffer, capture: boolean) => {
        outputBytes += chunk.byteLength;
        if (outputBytes > this.options.maxOutputBytes && forcedKind === undefined) {
          forcedKind = "failure";
          child.kill();
          return;
        }
        if (capture) stdout += chunk.toString("utf8");
      };

      child.stdout.on("data", (chunk: Buffer) => consume(chunk, true));
      child.stderr.on("data", (chunk: Buffer) => consume(chunk, false));
      child.on("error", () => finish("failure"));
      child.on("close", (code, signal) => {
        if (settled) return;
        clearTimeout(timer);
        if (forcedKind !== undefined) return finish(forcedKind);
        if (signal !== null) return finish("failure");
        if (code === 2) return finish("invalid");
        if (code === 3) return finish("unavailable");
        if (code !== 0) return finish("failure");

        try {
          const result = JSON.parse(stdout) as { ok?: unknown; playerId?: unknown };
          if (
            result.ok !== true ||
            typeof result.playerId !== "string" ||
            result.playerId.toLowerCase() !== input.candidatePlayerId.toLowerCase()
          ) {
            return finish("invalid");
          }
          settled = true;
          resolve({
            playerId: result.playerId,
            verificationMs: Date.now() - startedAt,
          });
        } catch {
          finish("failure");
        }
      });

      timer = setTimeout(() => {
        forcedKind = "unavailable";
        child.kill();
      }, this.options.timeoutMs);
      child.stdin.end(
        JSON.stringify({
          token: input.token,
          candidatePlayerId: input.candidatePlayerId,
        }),
      );
    });
  }
}
