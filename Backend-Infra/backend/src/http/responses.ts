import type { APIGatewayProxyStructuredResultV2 } from "aws-lambda";

export interface ErrorBody {
  error: {
    code: string;
    message: string;
  };
  requestId: string;
}

export function jsonResponse(
  statusCode: number,
  body: unknown,
): APIGatewayProxyStructuredResultV2 {
  return {
    statusCode,
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify(body),
  };
}

export function errorResponse(
  statusCode: number,
  code: string,
  message: string,
  requestId: string,
): APIGatewayProxyStructuredResultV2 {
  const body: ErrorBody = {
    error: { code, message },
    requestId,
  };
  return jsonResponse(statusCode, body);
}
