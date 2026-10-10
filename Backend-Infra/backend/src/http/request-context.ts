import { randomUUID } from "node:crypto";

import type { APIGatewayProxyEventV2 } from "aws-lambda";

export function getRequestId(event: APIGatewayProxyEventV2): string {
  const requestId = event.requestContext.requestId.trim();
  return requestId.length > 0 ? requestId : randomUUID();
}
