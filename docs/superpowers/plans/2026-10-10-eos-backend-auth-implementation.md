# EOS Backend Authentication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unreal 클라이언트의 EOS Connect ID Token을 AWS Lambda에서 네이티브 EOS SDK로 검증하고, 검증된 PUID를 반환하는 `GET /v1/me` 개발 API를 실제 AWS에 배포한다.

**Architecture:** API Gateway HTTP API가 TypeScript/Node.js 24 Lambda 컨테이너를 호출한다. Lambda는 토큰의 기본 형식만 선검사하고 Secrets Manager에서 Client Secret을 가져온 뒤, 같은 이미지의 격리된 C++ 실행 파일에 토큰을 표준 입력으로 전달한다. C++ 실행 파일은 `EOS_Connect_VerifyIdToken`의 PUID와 제품 범위를 검증하며, TypeScript 계층은 결과를 공통 HTTP 응답과 구조화 로그로 변환한다.

**Tech Stack:** AWS SAM, API Gateway HTTP API, AWS Lambda container image, TypeScript 5, Node.js 24, Vitest, AWS SDK for JavaScript v3, C++17, CMake, json-c, EOS SDK 1.19.2.1, Docker, CloudWatch Logs, Secrets Manager

**Spec:** `docs/superpowers/specs/2026-10-10-eos-backend-auth-design.md`

## Global Constraints

- AWS 리전은 `ap-northeast-2`, 로컬 AWS CLI 프로필은 `dku-dev`, 스택 이름은 `chickengame-backend-dev`다.
- Lambda는 Node.js 24, Amazon Linux 2023, `x86_64`, 512MB, 전체 제한시간 15초를 사용한다.
- 네이티브 검증 제한시간은 8초이고 warm 요청 목표는 2초 이내다.
- Connect ID Token 최대 길이는 UTF-8 기준 16KB이며, 검증 전 payload와 PUID 후보는 신뢰하지 않는다.
- 토큰은 자식 프로세스 명령행이나 파일에 넣지 않고 stdin으로만 전달한다.
- Client Secret은 이미지, Git, SAM 템플릿, Lambda 정적 환경 변수, Docker build argument, 로그, 응답에 넣지 않는다.
- EOS SDK 원본·헤더·공유 라이브러리는 `Backend-Infra/vendor/eos-sdk/` 아래에 로컬로만 두고 Git에서 제외한다.
- 인증 성공 시 EOS 콜백의 PUID와 Client ID, Product ID, Sandbox ID, Deployment ID가 설정과 모두 일치해야 한다.
- API 오류 본문은 안전한 공통 형식을 사용하고 `requestId`를 항상 포함한다.
- DynamoDB, 프로필, 닉네임, 매칭, EOS Session, JWT Authorizer, 검증 캐시는 이번 구현에 포함하지 않는다.
- 각 작업은 실패 테스트 확인 → 최소 구현 → 전체 관련 테스트 통과 → 커밋 순서를 지킨다.
- 별도 작업 디렉터리가 적히지 않은 `npm` 명령은 모두 `Backend-Infra/backend`에서 실행한다.

## Review Focus

- 중복·대소문자·공백 변형이 있는 Authorization 헤더는 단 하나의 유효한 Bearer 값만 허용하고, 나머지는 `401 AUTH_MALFORMED`로 거부한다. Task 1의 테이블 테스트로 고정한다.
- 잘못된 base64url, JSON이 아닌 payload, 배열 payload, 16KB 경계값은 네이티브 검증기를 실행하지 않고 `401`로 끝나야 한다. Task 1과 Task 2의 호출 횟수 테스트로 고정한다.
- 네이티브 성공 JSON의 PUID가 후보 PUID와 다르면 성공 종료 코드여도 `401 AUTH_INVALID`여야 한다. Task 4의 위조 출력 테스트로 고정한다.
- 자식 프로세스가 멈추거나 stdout/stderr를 과도하게 쓰면 종료·출력 제한이 적용되고 토큰을 포함하지 않는 `503` 또는 `500`으로 매핑돼야 한다. Task 4의 fixture 프로세스 테스트로 고정한다.
- Secret이 빈 문자열이거나 Secrets Manager가 일시 오류를 반환하면 실패 결과를 캐시하지 않고 다음 호출에서 재시도해야 한다. Task 3의 연속 호출 테스트로 고정한다.

---

## File Map

### TypeScript API

- `Backend-Infra/backend/package.json`: 빌드·형검사·Vitest 스크립트와 런타임 의존성.
- `Backend-Infra/backend/tsconfig.json`: Node.js 24용 strict TypeScript 설정.
- `Backend-Infra/backend/vitest.config.ts`: 단위 테스트 검색 및 커버리지 제외 설정.
- `Backend-Infra/backend/src/auth/auth-errors.ts`: 인증·검증 오류 종류와 HTTP 매핑 전 내부 오류 타입.
- `Backend-Infra/backend/src/auth/parse-authorization.ts`: Bearer 토큰, JWT 구조, PUID 후보 선검사.
- `Backend-Infra/backend/src/auth/token-verifier.ts`: 네이티브 구현과 테스트 대역이 공유하는 검증 인터페이스.
- `Backend-Infra/backend/src/auth/secret-provider.ts`: Client Secret 공급자 인터페이스.
- `Backend-Infra/backend/src/auth/secrets-manager-secret-provider.ts`: Secrets Manager 조회와 성공값 메모리 캐시.
- `Backend-Infra/backend/src/auth/native-token-verifier.ts`: C++ 프로세스 실행, stdin 전달, 타임아웃·출력·종료 코드 처리.
- `Backend-Infra/backend/src/config.ts`: 필수 EOS/AWS 런타임 설정 파싱.
- `Backend-Infra/backend/src/http/request-context.ts`: API Gateway 요청 ID 선택.
- `Backend-Infra/backend/src/http/responses.ts`: JSON 성공·오류 응답 생성.
- `Backend-Infra/backend/src/logging.ts`: 토큰·비밀정보를 받지 않는 구조화 로그 인터페이스.
- `Backend-Infra/backend/src/app.ts`: 의존성 주입 가능한 `GET /v1/me` 처리기.
- `Backend-Infra/backend/src/handler.ts`: 실제 설정, Secret 공급자, 네이티브 검증기를 조립하는 Lambda 진입점.
- `Backend-Infra/backend/test/**/*.test.ts`: TypeScript 단위·통합 경계 테스트.
- `Backend-Infra/backend/test/fixtures/*.mjs`: 네이티브 자식 프로세스 성공·오류·정지·출력 과다 fixture.

### Native verifier

- `Backend-Infra/backend/native/CMakeLists.txt`: C++17 검증기와 테스트 빌드.
- `Backend-Infra/backend/native/src/protocol.hpp`: stdin/stdout JSON 프로토콜 타입과 함수 선언.
- `Backend-Infra/backend/native/src/protocol.cpp`: json-c 기반 제한된 JSON 파싱·출력.
- `Backend-Infra/backend/native/src/eos-verifier.hpp`: EOS 설정과 검증 결과 타입.
- `Backend-Infra/backend/native/src/eos-verifier.cpp`: EOS 초기화, Tick, 콜백 결과·범위 검증.
- `Backend-Infra/backend/native/src/main.cpp`: 환경·stdin 검사와 종료 코드 변환.
- `Backend-Infra/backend/native/test/protocol-test.cpp`: SDK 네트워크 없이 실행되는 프로토콜 테스트.
- `Backend-Infra/backend/native/test/run-negative-integration.ps1`: 실제 SDK로 비밀을 출력하지 않고 실패 토큰을 확인하는 테스트.

### Container, SAM, operations

- `Backend-Infra/backend/Dockerfile`: C++와 TypeScript 멀티스테이지 Lambda 이미지.
- `Backend-Infra/.dockerignore`: `Backend-Infra` Docker 빌드 컨텍스트의 불필요 파일과 로컬 결과 제외.
- `Backend-Infra/template.yaml`: API, Lambda Image 함수, Secret, IAM, 로그 그룹, 출력.
- `Backend-Infra/samconfig.toml`: `dev` 환경의 스택·리전·프로필·이미지 배포 설정.
- `Backend-Infra/backend/scripts/set-client-secret.mjs`: 마스킹 입력 후 AWS SDK로 Secret 등록.
- `Backend-Infra/backend/scripts/invoke-auth-smoke.mjs`: 마스킹 토큰 입력 후 실제 API 정상·조작 테스트.
- `Backend-Infra/backend/scripts/check-cloudwatch-redaction.mjs`: requestId 기준 로그 조회와 민감값 부재 검사.
- `Backend-Infra/README.md`: SDK 준비, 테스트, 빌드, 배포, 롤백, 제거 절차.
- `.gitignore`: EOS SDK, Node/CMake/SAM 빌드 결과, 로컬 환경 파일 제외.
- `Backend-Infra/SPRINT_1_CHECKLIST.md`: 결정과 실제 검증 증거 반영.

### Task 1: TypeScript project and authorization parser

**Files:**
- Create: `Backend-Infra/backend/package.json`
- Create: `Backend-Infra/backend/tsconfig.json`
- Create: `Backend-Infra/backend/vitest.config.ts`
- Create: `Backend-Infra/backend/src/auth/auth-errors.ts`
- Create: `Backend-Infra/backend/src/auth/parse-authorization.ts`
- Test: `Backend-Infra/backend/test/auth/parse-authorization.test.ts`

**Interfaces:**
- Produces: `parseAuthorization(headers: Record<string, string | undefined> | undefined): { token: string; candidatePlayerId: string }`.
- Produces: `AuthError` with readonly `code`, `statusCode`, and public-safe `message` properties.
- Produces: npm scripts `test`, `test:watch`, `typecheck`, and `build`.

- [ ] **Step 1: Create the TypeScript package configuration**

  Configure ESM, strict TypeScript, Vitest, esbuild, `@types/aws-lambda`, `@aws-sdk/client-secrets-manager`, and Node.js 24 engines, including the four npm scripts declared in this task's Interfaces block.

- [ ] **Step 2: Write the failing authorization parser tests**

  Test missing header → `AUTH_MISSING`; wrong scheme, duplicate comma-separated values, empty token, extra fields → `AUTH_MALFORMED`; malformed JWT/base64url/JSON, non-object payload, missing or non-32-hex `sub`, and 16KB 초과 → `AUTH_INVALID`; exactly 16KB is accepted and the `Bearer` scheme is case-insensitive.

- [ ] **Step 3: Run the parser test to confirm the red state**

  Run from `Backend-Infra/backend`: `npm install && npm test -- test/auth/parse-authorization.test.ts`

  Expected: FAIL because `parse-authorization.ts` and `AuthError` do not exist.

- [ ] **Step 4: Implement the minimal parser and error types**

  Decode only the JWT payload with base64url for candidate extraction; do not verify or trust claims. Count token bytes with `Buffer.byteLength(token, 'utf8')`, allow at most 16,384 bytes, require exactly three non-empty JWT segments, require payload to be a JSON object, and require `sub` to match `/^[0-9a-f]{32}$/i`.

- [ ] **Step 5: Verify tests, typecheck, and build**

  Run: `npm test -- test/auth/parse-authorization.test.ts && npm run typecheck && npm run build`

  Expected: all parser tests PASS, TypeScript reports no errors, and `dist/` is produced.

- [ ] **Step 6: Commit Task 1**

  Run: `git add Backend-Infra/backend && git commit -m "feat: validate EOS authorization headers"`

### Task 2: Injectable `/v1/me` application handler

**Files:**
- Create: `Backend-Infra/backend/src/auth/token-verifier.ts`
- Create: `Backend-Infra/backend/src/auth/secret-provider.ts`
- Create: `Backend-Infra/backend/src/http/request-context.ts`
- Create: `Backend-Infra/backend/src/http/responses.ts`
- Create: `Backend-Infra/backend/src/logging.ts`
- Create: `Backend-Infra/backend/src/app.ts`
- Test: `Backend-Infra/backend/test/app.test.ts`

**Interfaces:**
- Consumes: `parseAuthorization(...)` and `AuthError` from Task 1.
- Produces: `TokenVerifier.verify(input: { token: string; candidatePlayerId: string; clientSecret: string }): Promise<{ playerId: string; verificationMs: number }>`.
- Produces: `ClientSecretProvider.getClientSecret(): Promise<string>`.
- Produces: `createHandler(deps: { verifier: TokenVerifier; secretProvider: ClientSecretProvider; logger: SafeLogger }): APIGatewayProxyHandlerV2`.

- [ ] **Step 1: Write failing handler contract tests with fakes**

  Assert valid token + fake verified PUID returns `200`, JSON content type, `{playerId, requestId}`; API Gateway request ID is preserved; a generated UUID is used when absent; body/query `playerId` is ignored. Assert parser failures never call Secret provider or verifier. Assert verifier invalid/unavailable/failure errors map to `401 AUTH_INVALID`, `503 AUTH_VERIFIER_UNAVAILABLE`, `500 AUTH_VERIFIER_FAILURE`, and every response includes `requestId`.

- [ ] **Step 2: Run the handler tests to confirm the red state**

  Run: `npm test -- test/app.test.ts`

  Expected: FAIL because `createHandler` and the dependency interfaces do not exist.

- [ ] **Step 3: Implement interfaces, response helpers, safe logger, and handler factory**

  `SafeLogger` accepts only an allow-listed event object containing request ID, route, status, auth outcome, verified player ID on success, and duration; its method signatures must not accept token or Client Secret fields. Call the Secret provider only after local token parsing succeeds.

- [ ] **Step 4: Run handler and complete TypeScript tests**

  Run: `npm test && npm run typecheck`

  Expected: all Task 1–2 tests PASS with no type errors.

- [ ] **Step 5: Commit Task 2**

  Run: `git add Backend-Infra/backend && git commit -m "feat: add authenticated me handler"`

### Task 3: Runtime configuration and Secrets Manager provider

**Files:**
- Create: `Backend-Infra/backend/src/config.ts`
- Create: `Backend-Infra/backend/src/auth/secrets-manager-secret-provider.ts`
- Test: `Backend-Infra/backend/test/config.test.ts`
- Test: `Backend-Infra/backend/test/auth/secrets-manager-secret-provider.test.ts`

**Interfaces:**
- Consumes: `ClientSecretProvider` from Task 2.
- Produces: `loadRuntimeConfig(env: NodeJS.ProcessEnv): RuntimeConfig` with `productId`, `sandboxId`, `deploymentId`, `clientId`, `clientSecretArn`, `verifierPath`, `verifierTimeoutMs`, and `verifierMaxOutputBytes`.
- Produces: `SecretsManagerClientSecretProvider(client: Pick<SecretsManagerClient, 'send'>, secretId: string)`.

- [ ] **Step 1: Write failing config and Secret provider tests**

  Require all four EOS IDs and Secret ARN; default verifier path to `/opt/bin/eos-token-verifier`, timeout to `8000`, and combined stdout/stderr limit to `65536` bytes. Test SecretString success is cached once; empty/missing SecretString throws `ConfigurationError`; rejected AWS calls and empty results are not cached and a second call invokes AWS again.

- [ ] **Step 2: Run focused tests to confirm the red state**

  Run: `npm test -- test/config.test.ts test/auth/secrets-manager-secret-provider.test.ts`

  Expected: FAIL because configuration and provider implementations do not exist.

- [ ] **Step 3: Implement strict configuration and success-only promise caching**

  Use `GetSecretValueCommand`; cache only the resolved non-empty string. Clear an in-flight cache on rejection. Convert all missing/empty server configuration to `INTERNAL_CONFIGURATION_ERROR` without including AWS messages that could contain secret metadata in public responses.

- [ ] **Step 4: Verify focused and full TypeScript suites**

  Run: `npm test && npm run typecheck`

  Expected: all tests PASS and transient retrieval retry is observed as two `send` calls.

- [ ] **Step 5: Commit Task 3**

  Run: `git add Backend-Infra/backend && git commit -m "feat: load EOS credentials from Secrets Manager"`

### Task 4: Native verifier process boundary and production composition

**Files:**
- Create: `Backend-Infra/backend/src/auth/native-token-verifier.ts`
- Create: `Backend-Infra/backend/src/handler.ts`
- Create: `Backend-Infra/backend/test/auth/native-token-verifier.test.ts`
- Create: `Backend-Infra/backend/test/handler.test.ts`
- Create: `Backend-Infra/backend/test/fixtures/verifier-success.mjs`
- Create: `Backend-Infra/backend/test/fixtures/verifier-invalid.mjs`
- Create: `Backend-Infra/backend/test/fixtures/verifier-hang.mjs`
- Create: `Backend-Infra/backend/test/fixtures/verifier-flood.mjs`

**Interfaces:**
- Consumes: `TokenVerifier`, `RuntimeConfig`, `SecretsManagerClientSecretProvider`, and `createHandler` from Tasks 2–3.
- Produces: `NativeTokenVerifier(options: NativeTokenVerifierOptions)` implementing `TokenVerifier`; options contain `executablePath`, test-only fixed `executableArguments`, EOS scope, timeout, output limit, and an injectable spawn function.
- Produces: Lambda export `handler` from `src/handler.ts`.
- Native stdin contract: one JSON object `{ "token": string, "candidatePlayerId": string }` followed by EOF.
- Native stdout success contract: `{ "ok": true, "playerId": string }`; failure contracts use `{ "ok": false, "kind": "invalid_token" | "unavailable" | "internal" }`.

- [ ] **Step 1: Write failing child-process boundary tests**

  Assert `shell: false`, no token in argv, token appears only in stdin, and child env is an allow-list containing the four EOS IDs plus `EOS_CLIENT_SECRET`. Test exit `0` + matching PUID succeeds; exit `0` + different PUID becomes invalid; exit `2` invalid, exit `3` unavailable, other exit malformed JSON, spawn error, or signal becomes failure; 8-second timeout kills the child and becomes unavailable; output beyond 65,536 bytes kills the child and becomes failure. Inspect captured errors/log events to ensure the token and secret never appear.

- [ ] **Step 2: Run the boundary tests to confirm the red state**

  Run: `npm test -- test/auth/native-token-verifier.test.ts test/handler.test.ts`

  Expected: FAIL because `NativeTokenVerifier` and production `handler` do not exist.

- [ ] **Step 3: Implement bounded spawn handling and the Lambda composition root**

  Spawn the configured executable directly with only the preconfigured fixed arguments, close stdin after writing one JSON object, collect bounded stdout/stderr separately, terminate on timeout/overflow, parse exactly one stdout JSON value, and compare returned PUID with the candidate using normalized lowercase. Production supplies no fixed arguments; fixture paths are the only test arguments. Instantiate one AWS SDK client and one Secret provider outside the exported handler for warm reuse.

- [ ] **Step 4: Verify all TypeScript tests and bundled handler**

  Run: `npm test && npm run typecheck && npm run build`

  Expected: all tests PASS and `dist/handler.js` exports `handler`.

- [ ] **Step 5: Commit Task 4**

  Run: `git add Backend-Infra/backend && git commit -m "feat: invoke isolated EOS token verifier"`

### Task 5: C++ EOS Connect token verifier

**Files:**
- Create: `Backend-Infra/backend/native/CMakeLists.txt`
- Create: `Backend-Infra/backend/native/src/protocol.hpp`
- Create: `Backend-Infra/backend/native/src/protocol.cpp`
- Create: `Backend-Infra/backend/native/src/eos-verifier.hpp`
- Create: `Backend-Infra/backend/native/src/eos-verifier.cpp`
- Create: `Backend-Infra/backend/native/src/main.cpp`
- Create: `Backend-Infra/backend/native/test/protocol-test.cpp`
- Create: `Backend-Infra/backend/native/test/run-negative-integration.ps1`

**Interfaces:**
- Consumes: stdin/stdout and environment contract from Task 4.
- Produces: executable `eos-token-verifier` with exit `0` success, `1` input/configuration error, `2` invalid token/scope/PUID, `3` EOS unavailable/timeout, `4` internal failure.
- Produces: `parseRequest(std::string_view): ParseResult`, `serializeResult(const VerifierResult&): std::string`, and `verifyConnectIdToken(const VerifyRequest&, const EosConfig&): VerifierResult`.

- [ ] **Step 1: Write failing protocol unit tests**

  Test empty input, invalid JSON, wrong JSON types, unknown top-level keys, invalid PUID, oversized token, escaped JSON content, and valid request round-trip. Test result serialization emits one compact JSON object and never includes token or Client Secret.

- [ ] **Step 2: Configure CMake and run the protocol test to confirm the red state**

  Run from the repository root:

  `docker run --rm --platform linux/amd64 -v "${PWD}/Backend-Infra:/workspace" -w /workspace public.ecr.aws/amazonlinux/amazonlinux:2023 sh -lc "dnf -y install gcc-c++ cmake make pkgconf-pkg-config json-c-devel && cmake -S backend/native -B /tmp/eos-build -DBUILD_TESTING=ON -DEOS_SDK_ROOT=/workspace/vendor/eos-sdk && cmake --build /tmp/eos-build && ctest --test-dir /tmp/eos-build --output-on-failure"`

  Expected: FAIL until protocol sources and json-c linkage are implemented.

- [ ] **Step 3: Implement the json-c protocol layer and exit-code main**

  Enforce the same 16,384-byte token and 32-hex PUID constraints as TypeScript. Read required EOS settings with null/empty checks, direct all EOS diagnostics to stderr, and emit exactly one protocol JSON object to stdout.

- [ ] **Step 4: Write the EOS verification path and negative integration assertions**

  Initialize EOS once per process with product name `chickengame-backend`, product version `1.0.0`, server mode enabled, and no cache/encryption key because this verifier uses no EOS storage interface. Build `EOS_Connect_IdToken`, Tick at 10ms intervals until callback or a 7-second native deadline, and compare callback PUID plus non-null Client ID/Product ID/Sandbox ID/Deployment ID with configuration before success. Map `EOS_NoConnection`, `EOS_ServiceFailure`, `EOS_TimedOut`, and `EOS_TooManyRequests` to exit `3`; token, PUID, and scope rejection to exit `2`; always release Platform and call `EOS_Shutdown`. The PowerShell integration script must assert malformed input → exit `1`, tampered token/PUID/scope mismatch → exit `2`, and must accept secrets only through inherited environment or masked prompt.

- [ ] **Step 5: Build and run native tests with the local EOS SDK**

  Run: the Docker CMake/CTest command from Step 2, then `pwsh Backend-Infra/backend/native/test/run-negative-integration.ps1`; the script itself builds/runs the same `linux/amd64` AL2023 image and prompts for any real token or Client Secret it needs.

  Expected: protocol CTest PASS; malformed and tampered inputs return their documented exit codes; stdout contains no token or secret.

- [ ] **Step 6: Commit Task 5**

  Run: `git add Backend-Infra/backend/native && git commit -m "feat: verify Connect ID tokens with EOS SDK"`

### Task 6: Reproducible Lambda container image

**Files:**
- Create: `Backend-Infra/backend/Dockerfile`
- Create: `Backend-Infra/.dockerignore`
- Create: `Backend-Infra/vendor/eos-sdk/README.md`
- Modify: `.gitignore`
- Test: `Backend-Infra/backend/test/container-smoke.ps1`

**Interfaces:**
- Consumes: TypeScript bundle from Task 4 and native CMake target from Task 5.
- Produces: Linux `x86_64` Lambda image containing `/var/task/dist/handler.js`, `/opt/bin/eos-token-verifier`, and `/opt/lib/libEOSSDK-Linux-Shipping.so`.
- Local SDK layout: `Backend-Infra/vendor/eos-sdk/Include/` and `Backend-Infra/vendor/eos-sdk/lib/libEOSSDK-Linux-Shipping.so`.

- [ ] **Step 1: Add ignored SDK and build paths**

  Ignore `Backend-Infra/vendor/eos-sdk/**` except its README, `node_modules`, `dist`, `.aws-sam`, CMake outputs, coverage, `.env*`, and SAM local files. Document the exact local SDK layout in the retained vendor README.

- [ ] **Step 2: Write the failing container smoke script**

  The script must fail if the handler, executable, EOS `.so`, or its dynamic dependencies are missing, and must run malformed stdin to assert verifier exit `1`.

- [ ] **Step 3: Run the smoke script to confirm the red state**

  Run: `pwsh Backend-Infra/backend/test/container-smoke.ps1`

  Expected: FAIL because no Dockerfile/image exists.

- [ ] **Step 4: Implement the multi-stage Dockerfile**

  Use an AL2023 build stage for `gcc-c++`, CMake, and json-c; a Node.js 24 build stage for `npm ci`, tests, typecheck, and bundle; and the AWS Lambda Node.js 24 runtime stage for production JS plus native artifacts. Set library lookup without embedding SDK credentials and run as the Lambda image's non-root runtime user where supported.

- [ ] **Step 5: Build and verify the image**

  Run: `pwsh Backend-Infra/backend/test/container-smoke.ps1`

  Expected: image builds for `linux/amd64`, files and shared libraries resolve, malformed input returns exit `1`, and image history/environment contain no Client Secret.

- [ ] **Step 6: Commit Task 6**

  Run: `git add .gitignore Backend-Infra/backend Backend-Infra/vendor/eos-sdk/README.md && git commit -m "build: package EOS verifier Lambda image"`

### Task 7: SAM infrastructure for the development stack

**Files:**
- Create: `Backend-Infra/template.yaml`
- Create: `Backend-Infra/samconfig.toml`
- Modify: `Backend-Infra/backend/package.json`
- Test: `Backend-Infra/backend/test/infra/template.test.ts`

**Interfaces:**
- Consumes: Docker image build context from Task 6.
- Produces: CloudFormation outputs `ApiUrl`, `ClientSecretArn`, and `FunctionName`.
- Produces: HTTP API route `GET /v1/me`; image Lambda logical ID `AuthFunction`; retained Secret logical ID `EosClientSecret`; explicit log group with 14-day retention.

- [ ] **Step 1: Write failing SAM template assertions**

  Add the `yaml` test dependency. Parse the template and assert `PackageType: Image`, `Architectures: [x86_64]`, memory `512`, timeout `15`, route `/v1/me`, no JWT Authorizer/CORS, four non-secret EOS environment values, Secret ARN environment value, verifier timeout/output-limit environment values, no secret value parameter, exact `GetSecretValue` resource, retained Secret, 14-day logs, standard project/environment tags, and all three outputs.

- [ ] **Step 2: Run template tests and SAM validation to confirm the red state**

  Run from `Backend-Infra/backend`: `npm test -- test/infra/template.test.ts`; then run from the repository root: `sam validate --template-file Backend-Infra/template.yaml --lint --profile dku-dev --region ap-northeast-2`.

  Expected: FAIL because the template does not exist.

- [ ] **Step 3: Implement `template.yaml` and non-secret `samconfig.toml`**

  Set stack `chickengame-backend-dev`, function name `chickengame-dev-api`, Docker context `Backend-Infra`, Dockerfile `backend/Dockerfile`, and `--resolve-image-repos`. Define required parameters `EosProductId`, `EosSandboxId`, `EosDeploymentId`, and `EosClientId`, plus `VerifierTimeoutMs` default `8000` and `VerifierMaxOutputBytes` default `65536`. Store only the four non-secret IDs in `dev` parameter overrides. Add a `dev-timeout` config environment with identical non-secret IDs and `VerifierTimeoutMs=1` for the controlled 503 test. Keep Client Secret contents out of both files.

- [ ] **Step 4: Validate and build SAM locally**

  Run the Vitest command from Step 2, then from the repository root run `sam validate --template-file Backend-Infra/template.yaml --lint --profile dku-dev --region ap-northeast-2` and `sam build --template-file Backend-Infra/template.yaml --use-container`.

  Expected: assertions PASS, SAM template valid, and image build completes.

- [ ] **Step 5: Exercise the local API error contract**

  Run `sam local start-api` from `Backend-Infra`, call `/v1/me` without Authorization, then stop it.

  Expected: `401`, `AUTH_MISSING`, and a non-empty `requestId`; no token or Secret appears in console output.

- [ ] **Step 6: Commit Task 7**

  Run: `git add Backend-Infra/template.yaml Backend-Infra/samconfig.toml Backend-Infra/backend/package.json Backend-Infra/backend/test/infra && git commit -m "infra: define EOS authentication SAM stack"`

### Task 8: Secret bootstrap, smoke tests, and operations guide

**Files:**
- Create: `Backend-Infra/backend/scripts/set-client-secret.mjs`
- Create: `Backend-Infra/backend/scripts/invoke-auth-smoke.mjs`
- Create: `Backend-Infra/backend/scripts/check-cloudwatch-redaction.mjs`
- Create: `Backend-Infra/README.md`
- Modify: `Backend-Infra/backend/package.json`
- Test: `Backend-Infra/backend/test/scripts.test.ts`

**Interfaces:**
- Consumes: stack outputs from Task 7 and AWS profile `dku-dev`.
- Produces: `npm run secret:set -- --secret-id <arn>`, `npm run smoke:auth -- --url <api-url>`, and `npm run logs:check -- --request-id <id> --function-name <name>`.

- [ ] **Step 1: Write failing script tests with mocked stdin and AWS/fetch clients**

  Assert Client Secret and token input are masked, values are never placed in child argv or logs, Secret update uses AWS SDK directly, smoke test sends Authorization only in the HTTP header, mutates one character in the JWT signature segment for the tampered-token case, and log checker fails if the supplied token/secret fingerprints appear. Test Ctrl+C and empty input exit safely without AWS writes.

- [ ] **Step 2: Run script tests to confirm the red state**

  Run: `npm test -- test/scripts.test.ts`

  Expected: FAIL because operations scripts do not exist.

- [ ] **Step 3: Implement scripts and npm commands**

  Add the required AWS SDK clients, credential provider, and YAML parser to `package.json`. Use `@aws-sdk/credential-providers` `fromIni({profile: 'dku-dev'})`, Secrets Manager, and CloudWatch Logs clients. Accept only non-secret identifiers as command arguments; collect secret/token via a no-echo prompt and retain them only in process memory.

- [ ] **Step 4: Write the end-to-end operations guide**

  Document exact SDK directory layout, `aws sso login`, identity check, install/test/build, SAM local error test, deploy, Secret registration, fresh token smoke test, expired-token procedure, CloudWatch redaction check, redeploy rollback, `sam delete`, and retained Secret cleanup warning.

- [ ] **Step 5: Verify tests and all documented commands that do not mutate AWS**

  Run: `npm test && npm run typecheck`, `sam validate --template-file Backend-Infra/template.yaml --lint`, and compare README commands against package scripts and SAM outputs.

  Expected: tests and validation PASS; every documented command resolves to an existing script/output.

- [ ] **Step 6: Commit Task 8**

  Run: `git add Backend-Infra && git commit -m "docs: add EOS auth deployment runbook"`

### Task 9: Deploy, verify, and record evidence

**Files:**
- Modify: `Backend-Infra/SPRINT_1_CHECKLIST.md`
- Modify: `Backend-Infra/README.md` only if observed deployment behavior requires a correction.

**Interfaces:**
- Consumes: all build artifacts and scripts from Tasks 1–8 plus a fresh Unreal/EOS Connect ID Token supplied interactively.
- Produces: deployed stack `chickengame-backend-dev`, verified API URL, CloudWatch evidence, measured latency/memory/image size, and updated checklist.

- [ ] **Step 1: Verify local prerequisites and clean test state**

  Run: `aws sso login --profile dku-dev`, `aws sts get-caller-identity --profile dku-dev`, `docker version`, `sam --version`, `npm ci`, `npm test`, `npm run typecheck`, and `sam build --template-file Backend-Infra/template.yaml --use-container`.

  Expected: identity is the intended SSO role, all tests PASS, and SAM image build succeeds with no Git-tracked SDK binaries or secrets.

- [ ] **Step 2: Deploy the development stack**

  Run from `Backend-Infra`: `sam deploy --config-env dev --resolve-image-repos --profile dku-dev --region ap-northeast-2`.

  Expected: CloudFormation reaches `UPDATE_COMPLETE` or `CREATE_COMPLETE`; outputs include API URL, Secret ARN, and function name.

- [ ] **Step 3: Register the Client Secret through the masked script**

  Run: `npm run secret:set -- --secret-id <ClientSecretArn>` from `Backend-Infra/backend` and enter the value only at the masked prompt.

  Expected: script reports the Secret version update without echoing the value.

- [ ] **Step 4: Verify public error cases**

  Call the API with no Authorization and with the smoke script's tampered token.

  Expected: `401 AUTH_MISSING` and `401 AUTH_INVALID`, each with `requestId`; no stack trace, EOS detail, token, PUID candidate, or secret in the response.

- [ ] **Step 5: Verify a fresh real Connect ID Token**

  Run: `npm run smoke:auth -- --url <ApiUrl>` and enter a freshly copied Connect ID Token at the masked prompt.

  Expected: `200`, returned `playerId` equals the EOS-verified PUID, request ID is present, and warm latency is recorded; repeat once to distinguish cold and warm measurements.

- [ ] **Step 6: Verify expiry, unavailability, and log redaction**

  Reuse a real token only after expiry to confirm `401 AUTH_INVALID`; deploy once with `sam deploy --config-env dev-timeout --resolve-image-repos --profile dku-dev --region ap-northeast-2` to confirm `503 AUTH_VERIFIER_UNAVAILABLE`, then immediately redeploy with `--config-env dev` and recheck a valid token. Run `npm run logs:check` for all request IDs and record Lambda duration, max memory, image size, and absence of token/Client Secret fingerprints.

  Expected: the three failure modes match the API contract and normal configuration is restored before proceeding.

- [ ] **Step 7: Update the checklist with exact evidence and remaining risks**

  Mark SAM selection, Node.js/TypeScript stack, naming, Secret management, API contract, API Gateway/Lambda deployment, request ID/logging, EOS native verification, and deployment verification items complete only where command output proves them. Record cold/warm latency, memory, image size, stack/API identifiers, date, and any unverified item without storing secrets or tokens.

- [ ] **Step 8: Run final verification and commit evidence**

  Run: `npm test && npm run typecheck`, `sam validate --template-file Backend-Infra/template.yaml --lint --profile dku-dev --region ap-northeast-2`, `git diff --check`, and `git status --short`.

  Expected: tests and validation PASS; only intended checklist/runbook edits remain.

- [ ] **Step 9: Commit Task 9**

  Run: `git add Backend-Infra/SPRINT_1_CHECKLIST.md Backend-Infra/README.md && git commit -m "docs: record deployed EOS authentication validation"`

## Final Acceptance Gate

- [ ] Run `npm ci`, `npm test`, `npm run typecheck`, and `npm run build` in `Backend-Infra/backend` from a clean checkout with the local EOS SDK restored only under the ignored vendor path.
- [ ] Run native CTest and negative integration tests in Amazon Linux 2023.
- [ ] Run the container smoke test, SAM template assertions, `sam validate --lint`, and `sam build --use-container`.
- [ ] Confirm the deployed `/v1/me` returns `200` only for a valid current Connect ID Token and uses the verified PUID.
- [ ] Confirm missing, malformed, tampered, expired, scope-mismatched, and verifier-unavailable cases return the specified safe errors.
- [ ] Confirm CloudWatch, image history, Git history, responses, and scripts contain no Connect ID Token or Client Secret.
- [ ] Confirm `git status --short` is empty and review the complete branch diff before requesting push or integration.
