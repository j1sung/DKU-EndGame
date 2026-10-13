import { readFileSync } from "node:fs";

import { describe, expect, it } from "vitest";
import { parse } from "yaml";

const templatePath = new URL("../../../template.yaml", import.meta.url);
const samConfigPath = new URL("../../../samconfig.toml", import.meta.url);

function loadTemplate(): any {
  return parse(readFileSync(templatePath, "utf8"));
}

describe("SAM authentication stack", () => {
  it("stores only the approved non-secret EOS IDs in both deployment environments", () => {
    const config = readFileSync(samConfigPath, "utf8");
    const expectedIds = [
      "EosProductId=16646f3bec4d46d1a546820856e47486",
      "EosSandboxId=f4a9863af26845ac90dbcaae53c6375f",
      "EosDeploymentId=f9dbca2f19ea4980b305355ff3bdbcea",
      "EosClientId=xyza7891MHpvCZ6hgPhGFHQwn6KZNTp9",
    ];

    expect(config).toContain("[dev.deploy.parameters]");
    expect(config).toContain("[dev-timeout.deploy.parameters]");
    for (const id of expectedIds) {
      expect(config.split(id)).toHaveLength(3);
    }
    expect(config).toMatch(/\[dev\.deploy\.parameters\][\s\S]*VerifierTimeoutMs=8000/);
    expect(config).toMatch(/\[dev-timeout\.deploy\.parameters\][\s\S]*VerifierTimeoutMs=1/);
    expect(config).not.toMatch(/client[_-]?secret/i);
  });

  it("defines the image Lambda and unauthenticated HTTP route", () => {
    const template = loadTemplate();
    const authFunction = template.Resources.AuthFunction;

    expect(authFunction.Type).toBe("AWS::Serverless::Function");
    expect(authFunction.Metadata).toEqual({
      DockerContext: ".",
      Dockerfile: "backend/Dockerfile",
      DockerTag: "nodejs24-v1",
    });
    expect(authFunction.Properties).toMatchObject({
      PackageType: "Image",
      Architectures: ["x86_64"],
      MemorySize: 512,
      Timeout: 15,
      FunctionName: "chickengame-dev-api",
      Events: {
        Me: {
          Type: "HttpApi",
          Properties: {
            ApiId: { Ref: "HttpApi" },
            Path: "/v1/me",
            Method: "GET",
          },
        },
      },
    });
    expect(JSON.stringify(template.Resources.HttpApi)).not.toMatch(
      /Authorizer|JwtConfiguration|CorsConfiguration/i,
    );
  });

  it("passes only non-secret IDs and a Secret ARN to the function", () => {
    const template = loadTemplate();
    const parameters = template.Parameters;
    const variables = template.Resources.AuthFunction.Properties.Environment.Variables;

    expect(Object.keys(parameters)).toEqual(
      expect.arrayContaining([
        "EosProductId",
        "EosSandboxId",
        "EosDeploymentId",
        "EosClientId",
        "VerifierTimeoutMs",
        "VerifierMaxOutputBytes",
      ]),
    );
    expect(Object.keys(parameters).join(" ")).not.toMatch(/secret/i);
    expect(variables).toEqual({
      EOS_PRODUCT_ID: { Ref: "EosProductId" },
      EOS_SANDBOX_ID: { Ref: "EosSandboxId" },
      EOS_DEPLOYMENT_ID: { Ref: "EosDeploymentId" },
      EOS_CLIENT_ID: { Ref: "EosClientId" },
      EOS_CLIENT_SECRET_ARN: { Ref: "EosClientSecret" },
      EOS_VERIFIER_TIMEOUT_MS: { Ref: "VerifierTimeoutMs" },
      EOS_VERIFIER_MAX_OUTPUT_BYTES: { Ref: "VerifierMaxOutputBytes" },
    });
    expect(parameters.VerifierTimeoutMs.Default).toBe(8000);
    expect(parameters.VerifierMaxOutputBytes.Default).toBe(65536);
  });

  it("grants only GetSecretValue for the retained secret", () => {
    const template = loadTemplate();
    const secret = template.Resources.EosClientSecret;
    const statements = template.Resources.AuthFunction.Properties.Policies.flatMap(
      (policy: any) => policy.Statement ?? [],
    );

    expect(secret).toMatchObject({
      Type: "AWS::SecretsManager::Secret",
      DeletionPolicy: "Retain",
      UpdateReplacePolicy: "Retain",
    });
    expect(statements).toContainEqual({
      Effect: "Allow",
      Action: "secretsmanager:GetSecretValue",
      Resource: { Ref: "EosClientSecret" },
    });
  });

  it("retains logs for 14 days and applies project tags", () => {
    const template = loadTemplate();
    expect(template.Resources.AuthFunctionLogGroup).toMatchObject({
      Type: "AWS::Logs::LogGroup",
      Properties: {
        LogGroupName: { "Fn::Sub": "/aws/lambda/${AuthFunction}" },
        RetentionInDays: 14,
      },
    });
    expect(template.Resources.AuthFunction.Properties.Tags).toEqual({
      Project: "chickengame",
      Environment: "dev",
    });
    expect(template.Resources.EosClientSecret.Properties.Tags).toEqual(
      expect.arrayContaining([
        { Key: "Project", Value: "chickengame" },
        { Key: "Environment", Value: "dev" },
      ]),
    );
  });

  it("exports the API URL, Secret ARN, and function name", () => {
    const template = loadTemplate();
    expect(template.Outputs).toEqual({
      ApiUrl: {
        Description: expect.any(String),
        Value: {
          "Fn::Sub": "https://${HttpApi}.execute-api.${AWS::Region}.${AWS::URLSuffix}",
        },
      },
      ClientSecretArn: {
        Description: expect.any(String),
        Value: { Ref: "EosClientSecret" },
      },
      FunctionName: {
        Description: expect.any(String),
        Value: { Ref: "AuthFunction" },
      },
    });
  });
});
