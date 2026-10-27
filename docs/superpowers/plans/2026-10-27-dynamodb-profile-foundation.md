# DynamoDB Profile Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the three designed DynamoDB tables and make authenticated `GET /v1/me` and `POST /v1/me/nickname` use a durable, globally unique player profile.

**Architecture:** Keep the existing EOS verifier and Lambda image. Add three SAM tables now so subsequent matchmaking work uses fixed keys; only the profile table is accessed in this slice. Place nickname rules and DynamoDB writes behind a small `ProfileStore` interface, then dispatch the two profile routes after authentication. The DynamoDB read/write smoke test closes Sprint 1's missing API Gateway → Lambda → DynamoDB path.

**Tech Stack:** Node.js 24, TypeScript, AWS SDK v3 DynamoDB DocumentClient, AWS SAM, DynamoDB, Vitest.

**Spec:** `docs/superpowers/specs/2026-10-27-dynamodb-matchmaking-design.md`

## Global Constraints

- PUID comes only from the successful EOS Connect ID Token verifier; never trust a caller supplied player ID.
- All API paths start `/v1`, JSON uses camelCase, and every response carries `requestId`.
- UTC ISO 8601 strings are the API time format; DynamoDB comparisons use epoch milliseconds and TTL uses epoch seconds.
- `PlayerProfiles` uses `pk` and `sk`; profile key is `PLAYER#<PUID>/PROFILE`, nickname claim key is `NICK#<nicknameKey>/CLAIM`, and the idempotency key is `PLAYER#<PUID>/IDEMP#<route>#<key>`.
- New nickname writes require `Idempotency-Key` as a UUID. A repeated key and identical request returns the original result; reuse with a different body returns `409 IDEMPOTENCY_KEY_REUSED`.
- Nickname display form is NFKC normalized, trimmed, consecutive spaces collapsed, 2–16 characters, and limited to Hangul syllables, ASCII letters, digits, and internal single spaces. The claim key lowercases ASCII letters. Nicknames are set once in the first release.
- Keep Client Secret and Connect ID Token out of Git, images, Lambda environment variables, shell arguments, responses, and logs.
- This plan does not implement tickets, matches, the matchmaking worker, or EOS session state endpoints. Those require separate plans against the same spec.

## Review Focus

1. Two players concurrently claim the same normalized nickname: one succeeds and the other gets `NICKNAME_TAKEN` (Task 3 test).
2. A player retries a successful nickname request after a lost response: the stored response is returned without a second claim (Task 3 test).
3. An idempotency key is reused with a different nickname: return `IDEMPOTENCY_KEY_REUSED` (Task 3 test).
4. NFKC/case and whitespace variants collide, while illegal or overlength names fail before any write (Task 2 test).
5. An unauthenticated caller cannot reach DynamoDB and neither token nor Secret appears in logs or responses (Task 4 test).

## File map

| File | Responsibility |
|---|---|
| `Backend-Infra/template.yaml` | Three DynamoDB tables, profile permissions, environment variable, and nickname route |
| `Backend-Infra/backend/package.json`, `package-lock.json` | Pin DynamoDB SDK packages |
| `Backend-Infra/backend/src/config.ts` | Parse profile table name |
| `Backend-Infra/backend/src/profile/nickname.ts` | Display normalization, nickname key, input rejection |
| `Backend-Infra/backend/src/profile/profile-store.ts` | `ProfileStore` contract and public result/error types |
| `Backend-Infra/backend/src/profile/dynamo-profile-store.ts` | Consistent profile read and atomic nickname+claim+idempotency write |
| `Backend-Infra/backend/src/app.ts` | Authenticated route dispatch and public responses |
| `Backend-Infra/backend/src/handler.ts` | Construct DynamoDB client/store and inject it |
| `Backend-Infra/backend/test/profile/*.test.ts` | Nickname and DynamoDB behavior |
| `Backend-Infra/backend/test/app.test.ts`, `test/infra/template.test.ts` | API and SAM contract |
| `Backend-Infra/README.md`, `SPRINT_1_CHECKLIST.md` | Deployment checks and observed integration results |

---

### Task 1: Define the three DynamoDB tables and runtime configuration

**Files:** Modify `Backend-Infra/template.yaml`, `Backend-Infra/backend/package.json`, `Backend-Infra/backend/package-lock.json`, `Backend-Infra/backend/src/config.ts`, `Backend-Infra/backend/test/config.test.ts`, `Backend-Infra/backend/test/infra/template.test.ts`.

**Interfaces:** Produce `RuntimeConfig.profileTableName: string`. The profile table has String `pk`/`sk` and TTL attribute `expiresAtSeconds`. The ticket table has String `ticketId`, `QueueIndex` with String `queueKey`/`queueSortKey`, and TTL. The match table has String `pk`/`sk` and TTL. The Lambda receives `PLAYER_PROFILES_TABLE`; IAM grants only profile table `GetItem` and `TransactWriteItems` in this slice. Preserve the existing EOS resource contract.

- [ ] **Step 1: Write failing config and template tests.** Assert all three resource key schemas and TTL names; assert `QueueIndex` keys, the profile environment variable, exact profile-only IAM actions, and `loadRuntimeConfig` rejects a missing table name.
- [ ] **Step 2: Run `npm test -- test/config.test.ts test/infra/template.test.ts` in `Backend-Infra/backend`.** Expected: tests fail on absent table/config fields.
- [ ] **Step 3: Add resources, config, and `@aws-sdk/client-dynamodb` plus `@aws-sdk/lib-dynamodb` with pinned versions.** Install with `npm install --save-exact`, record generated lockfile, and keep DynamoDB tables in on-demand billing mode with Project/Environment tags.
- [ ] **Step 4: Run `npm test -- test/config.test.ts test/infra/template.test.ts` and `npm run typecheck`.** Expected: PASS.
- [ ] **Step 5: Commit.** `git add Backend-Infra/template.yaml Backend-Infra/backend/package.json Backend-Infra/backend/package-lock.json Backend-Infra/backend/src/config.ts Backend-Infra/backend/test/config.test.ts Backend-Infra/backend/test/infra/template.test.ts && git commit -m "infra: define player matchmaking and match tables"`.

### Task 2: Validate and normalize a one-time nickname

**Files:** Create `Backend-Infra/backend/src/profile/nickname.ts`, `Backend-Infra/backend/test/profile/nickname.test.ts`.

**Interfaces:** Produce `normalizeNickname(input: unknown): { nickname: string; nicknameKey: string }`. Throw `NicknameValidationError` with public code `INVALID_NICKNAME` for invalid input. NFKC normalization happens before the allowlist check; lowercase only ASCII A–Z in the key.

- [ ] **Step 1: Write failing tests.** Assert `normalizeNickname("  ＣＨＩＣＫＥＮ   1  ")` returns display `"CHICKEN 1"` and key `"chicken 1"`; case/width variants share a key; Hangul is allowed; one character, 17 characters, punctuation, line breaks, and non-string input throw `INVALID_NICKNAME`.
- [ ] **Step 2: Run `npm test -- test/profile/nickname.test.ts`.** Expected: FAIL because the module is absent.
- [ ] **Step 3: Implement the exact `normalizeNickname` interface.** Count Unicode code points after normalization, then apply the specified character and whitespace rules.
- [ ] **Step 4: Run the focused test and `npm run typecheck`.** Expected: PASS.
- [ ] **Step 5: Commit.** `git add Backend-Infra/backend/src/profile/nickname.ts Backend-Infra/backend/test/profile/nickname.test.ts && git commit -m "feat: normalize game nicknames"`.

### Task 3: Store profile and unique nickname atomically

**Files:** Create `Backend-Infra/backend/src/profile/profile-store.ts`, `Backend-Infra/backend/src/profile/dynamo-profile-store.ts`, `Backend-Infra/backend/test/profile/dynamo-profile-store.test.ts`.

**Interfaces:** Produce `ProfileStore.get(playerId: string): Promise<{ playerId: string; nickname?: string }>` and `ProfileStore.registerNickname(input: { playerId: string; nickname: string; nicknameKey: string; idempotencyKey: string; requestHash: string }): Promise<{ playerId: string; nickname: string; created: boolean }>`; `DynamoProfileStore` takes a `DynamoDBDocumentClient`, table name, and injectable clock. Typed store errors distinguish `NICKNAME_TAKEN`, `NICKNAME_ALREADY_SET`, and `IDEMPOTENCY_KEY_REUSED`.

- [ ] **Step 1: Write failing tests with a command-aware fake DocumentClient.** Assert consistent `GetCommand` on `PLAYER#p/PROFILE`; one `TransactWriteCommand` conditionally creates/updates profile, creates `NICK#key/CLAIM`, and creates the 24-hour idempotency record; two concurrent different-player claims admit one winner; same-key same-hash replay returns original response; same-key different-hash returns reuse conflict; ambiguous transaction outcome is resolved by reading the idempotency record before any new write.
- [ ] **Step 2: Run `npm test -- test/profile/dynamo-profile-store.test.ts`.** Expected: FAIL because the store is absent.
- [ ] **Step 3: Implement the `ProfileStore` and `DynamoProfileStore` interfaces.** Use `GetCommand` with `ConsistentRead: true`, `TransactWriteCommand`, DynamoDB conditional expressions, and `CancellationReasons`/follow-up consistent reads to map conflicts. Never expose raw DynamoDB errors or PUID from unverified input.
- [ ] **Step 4: Run the focused test and `npm run typecheck`.** Expected: PASS.
- [ ] **Step 5: Commit.** `git add Backend-Infra/backend/src/profile Backend-Infra/backend/test/profile && git commit -m "feat: store unique player nicknames"`.

### Task 4: Connect authenticated profile routes

**Files:** Modify `Backend-Infra/backend/src/app.ts`, `Backend-Infra/backend/src/handler.ts`, `Backend-Infra/backend/test/app.test.ts`, `Backend-Infra/backend/test/handler.test.ts`, `Backend-Infra/template.yaml`, `Backend-Infra/backend/test/infra/template.test.ts`.

**Interfaces:** Add `profileStore: ProfileStore` to `HandlerDependencies`. Preserve `createHandler(dependencies): APIGatewayProxyHandlerV2`. Route `GET /v1/me` to `profileStore.get(verified.playerId)` and `POST /v1/me/nickname` to `normalizeNickname` then `registerNickname`. Require `Idempotency-Key` UUID and hash of canonical route/body. Return spec errors via existing `errorResponse`; keep token verification before storage access.

- [ ] **Step 1: Write failing API/infra tests.** Assert no nickname GET returns `{playerId,requestId}`; existing nickname GET includes it; POST returns 201; invalid nickname returns 400; collision and reused key return 409; missing/malformed key returns 400; unauthenticated requests make zero store calls; success/failure logs and bodies contain no token or Secret; SAM exposes `POST /v1/me/nickname`.
- [ ] **Step 2: Run `npm test -- test/app.test.ts test/handler.test.ts test/infra/template.test.ts`.** Expected: FAIL on absent store injection/route.
- [ ] **Step 3: Implement route dispatch and handler injection.** Instantiate the AWS DynamoDB client and `DynamoDBDocumentClient` in `handler.ts`; share one store between warm requests. Return `404` for unknown route without invoking a write.
- [ ] **Step 4: Run focused tests, `npm test`, `npm run typecheck`, and `npm run build`.** Expected: PASS.
- [ ] **Step 5: Commit.** `git add Backend-Infra/backend/src/app.ts Backend-Infra/backend/src/handler.ts Backend-Infra/backend/test/app.test.ts Backend-Infra/backend/test/handler.test.ts Backend-Infra/template.yaml Backend-Infra/backend/test/infra/template.test.ts && git commit -m "feat: expose authenticated player profile API"`.

### Task 5: Validate the deployed DynamoDB path and document results

**Files:** Modify `Backend-Infra/README.md`, `Backend-Infra/SPRINT_1_CHECKLIST.md`; add a safe smoke script only if the existing scripts cannot exercise profile responses without placing credentials in shell arguments or files.

**Interfaces:** Deployment keeps the existing stack `chickengame-backend-dev` in `ap-northeast-2` and the retained EOS Secret. Do not delete the stack. Record observed values only, never the Connect ID Token or Client Secret.

- [ ] **Step 1: Write/update the runbook verification sequence.** Include `npm ci`, tests, typecheck, build, container smoke, `sam validate --lint`, `sam build --use-container`, stack changeset review, deploy, authenticated `GET /v1/me`, nickname registration/retry/collision checks with two test accounts, DynamoDB item check, and CloudWatch redaction check.
- [ ] **Step 2: Run all local checks.** Expected: each exit code 0; document any unavailable tool or SDK as an unverified gate, not a pass.
- [ ] **Step 3: Review and deploy the SAM changeset to the development stack.** Expected: `UPDATE_COMPLETE` and only the intended tables, route, permissions, and Lambda image changed. Do not proceed if the changeset replaces the existing Secret or deletes another resource.
- [ ] **Step 4: Run the safe API and CloudWatch checks.** Expected: new profile read succeeds, registration persists and is unique across users, retry returns original response, missing token is rejected before DB access, and logs omit token/Secret. Record real request IDs and outcomes in the checklist.
- [ ] **Step 5: Commit verified runbook/checklist updates.** `git add Backend-Infra/README.md Backend-Infra/SPRINT_1_CHECKLIST.md && git commit -m "docs: record DynamoDB profile integration validation"`.

## Follow-on plans

After this slice, create separate implementation plans for (1) ticket creation/cancellation and the 4-player worker, and (2) match/session lifecycle and Unreal integration. The present document fixes their shared table and profile contracts but does not claim either is implemented.
