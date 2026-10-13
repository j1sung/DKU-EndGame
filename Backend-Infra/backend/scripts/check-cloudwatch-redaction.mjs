import { createHash } from "node:crypto";
import { pathToFileURL } from "node:url";

import {
  CloudWatchLogsClient,
  FilterLogEventsCommand,
} from "@aws-sdk/client-cloudwatch-logs";
import { fromIni } from "@aws-sdk/credential-providers";

import { parseOptions, readHiddenLine } from "./set-client-secret.mjs";

function fingerprints(value) {
  return [
    value,
    createHash("sha256").update(value, "utf8").digest("hex"),
    createHash("sha256").update(value, "utf8").digest("hex").slice(0, 12),
  ];
}

export async function runCloudWatchRedactionCheck({
  argv,
  prompt,
  client,
  output,
}) {
  const options = parseOptions(argv, {
    values: ["--request-id", "--function-name"],
  });
  const requestId = options?.["--request-id"]?.trim();
  const functionName = options?.["--function-name"]?.trim();
  if (!requestId || !functionName) {
    output.write(
      "Usage: npm run logs:check -- --request-id <id> --function-name <name>\n",
    );
    return 1;
  }
  if (
    !/^[A-Za-z0-9-]{1,128}$/.test(requestId) ||
    !/^[A-Za-z0-9-_]{1,64}$/.test(functionName)
  ) {
    output.write("Request ID or function name is invalid.\n");
    return 1;
  }

  const token = await prompt("EOS Connect ID Token used for the request: ");
  if (token === null) {
    output.write("Cancelled; CloudWatch was not queried.\n");
    return 130;
  }
  if (token.trim().length === 0) {
    output.write("Token cannot be empty; CloudWatch was not queried.\n");
    return 1;
  }
  const secret = await prompt("EOS Client Secret: ");
  if (secret === null) {
    output.write("Cancelled; CloudWatch was not queried.\n");
    return 130;
  }
  if (secret.trim().length === 0) {
    output.write("Client Secret cannot be empty; CloudWatch was not queried.\n");
    return 1;
  }

  const needles = [...fingerprints(token), ...fingerprints(secret)];
  let nextToken;
  do {
    const result = await client.send(
      new FilterLogEventsCommand({
        logGroupName: `/aws/lambda/${functionName}`,
        filterPattern: `"${requestId}"`,
        ...(nextToken ? { nextToken } : {}),
      }),
    );
    for (const event of result.events ?? []) {
      const message = event.message ?? "";
      if (needles.some((needle) => message.includes(needle))) {
        output.write("FAIL sensitive material or a sensitive fingerprint was found in logs.\n");
        return 1;
      }
    }
    if (!result.nextToken || result.nextToken === nextToken) {
      break;
    }
    nextToken = result.nextToken;
  } while (true);

  output.write("PASS no supplied token, Client Secret, or fingerprint was found.\n");
  return 0;
}

async function main() {
  const client = new CloudWatchLogsClient({
    credentials: fromIni({ profile: "dku-dev" }),
    region: "ap-northeast-2",
  });
  return runCloudWatchRedactionCheck({
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
    process.stderr.write("CloudWatch redaction check failed. No sensitive value was logged.\n");
    process.exitCode = 1;
  }
}
