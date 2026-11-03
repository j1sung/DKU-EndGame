import { createInterface, emitKeypressEvents } from "node:readline";
import { pathToFileURL } from "node:url";

import {
  PutSecretValueCommand,
  SecretsManagerClient,
} from "@aws-sdk/client-secrets-manager";
import { fromIni } from "@aws-sdk/credential-providers";

export function parseOptions(argv, { values, flags = [] }) {
  const parsed = {};
  for (let index = 0; index < argv.length; index += 1) {
    const option = argv[index];
    if (flags.includes(option)) {
      if (parsed[option] !== undefined) return undefined;
      parsed[option] = true;
      continue;
    }
    if (!values.includes(option) || parsed[option] !== undefined) {
      return undefined;
    }
    const value = argv[index + 1];
    if (value === undefined || value.startsWith("--")) {
      return undefined;
    }
    parsed[option] = value;
    index += 1;
  }
  return parsed;
}

export function readHiddenLine(
  label,
  { input = process.stdin, output = process.stdout } = {},
) {
  output.write(label);

  if (!input.isTTY || typeof input.setRawMode !== "function") {
    return new Promise((resolve) => {
      const lines = createInterface({ input, terminal: false, crlfDelay: Infinity });
      let settled = false;
      lines.once("line", (value) => {
        settled = true;
        lines.close();
        output.write("\n");
        resolve(value);
      });
      lines.once("close", () => {
        if (!settled) {
          output.write("\n");
          resolve(null);
        }
      });
    });
  }

  return new Promise((resolve) => {
    let value = "";
    const wasRaw = Boolean(input.isRaw);
    emitKeypressEvents(input);
    input.setRawMode(true);
    input.resume();

    const finish = (result) => {
      input.off("keypress", onKeypress);
      input.setRawMode(wasRaw);
      if (!wasRaw) {
        input.pause();
      }
      output.write("\n");
      resolve(result);
    };
    const onKeypress = (text, key = {}) => {
      if (key.ctrl && key.name === "c") {
        finish(null);
        return;
      }
      if (key.name === "return" || key.name === "enter") {
        finish(value);
        return;
      }
      if (key.name === "backspace") {
        if (value.length > 0) {
          value = value.slice(0, -1);
          output.write("\b \b");
        }
        return;
      }
      if (!key.ctrl && !key.meta && typeof text === "string" && text.length > 0) {
        value += text;
        output.write("*");
      }
    };

    input.on("keypress", onKeypress);
  });
}

export async function runSetClientSecret({ argv, prompt, client, output }) {
  const options = parseOptions(argv, { values: ["--secret-id"] });
  const secretId = options?.["--secret-id"]?.trim();
  if (!secretId) {
    output.write("Usage: npm run secret:set -- --secret-id <arn>\n");
    return 1;
  }

  const secret = await prompt("EOS Client Secret: ");
  if (secret === null) {
    output.write("Cancelled; no AWS changes were made.\n");
    return 130;
  }
  if (secret.trim().length === 0) {
    output.write("Client Secret cannot be empty; no AWS changes were made.\n");
    return 1;
  }

  await client.send(
    new PutSecretValueCommand({ SecretId: secretId, SecretString: secret }),
  );
  output.write("EOS Client Secret version stored successfully.\n");
  return 0;
}

async function main() {
  const client = new SecretsManagerClient({
    credentials: fromIni({ profile: "dku-dev" }),
    region: "ap-northeast-2",
  });
  return runSetClientSecret({
    argv: process.argv.slice(2),
    prompt: (label) => readHiddenLine(label),
    client,
    output: process.stdout,
  });
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  try {
    process.exitCode = await main();
  } catch {
    process.stderr.write("Failed to store the Client Secret. No sensitive value was logged.\n");
    process.exitCode = 1;
  }
}
