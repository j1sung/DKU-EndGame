# DynamoDB 프로필 기반 구축 구현 계획

> **구현 담당자 안내:** 이 계획은 작업 단위로 실행한다. `superpowers:subagent-driven-development` 또는 `superpowers:executing-plans` 지침을 적용하고, 각 단계는 체크박스로 추적한다.

**목표:** 설계한 DynamoDB 테이블 3개를 정의하고, 인증된 `GET /v1/me`와 `POST /v1/me/nickname`이 전역 고유 닉네임을 포함한 영속 프로필을 사용하도록 만든다.

**구조:** 기존 EOS 토큰 검증기와 Lambda 이미지를 유지한다. 매칭의 키 구조를 고정하기 위해 테이블 3개를 함께 정의하되, 이번 단계에서는 프로필 테이블에만 접근한다. 닉네임 규칙과 DynamoDB 쓰기를 작은 `ProfileStore` 인터페이스 뒤에 두고, 인증 성공 후 두 프로필 경로를 처리한다. API Gateway → Lambda → DynamoDB 읽기·쓰기 검증으로 Sprint 1의 미완료 통합 경로를 확인한다.

**기술:** Node.js 24, TypeScript, AWS SDK v3 DynamoDB DocumentClient, AWS SAM, DynamoDB, Vitest.

**설계 문서:** `docs/superpowers/specs/2026-10-27-dynamodb-matchmaking-design.md`

## 공통 제약

- PUID는 EOS Connect ID Token 검증 결과에서만 얻는다. 클라이언트가 보낸 플레이어 ID를 신뢰하지 않는다.
- API 경로는 `/v1`로 시작하고 JSON 필드명은 camelCase를 사용한다. 모든 응답에 `requestId`를 넣는다.
- API 시간은 UTC ISO 8601 문자열, DynamoDB 시간 비교는 epoch milliseconds, TTL은 epoch seconds를 사용한다.
- `PlayerProfiles`의 기본키는 `pk`와 `sk`다. 프로필 키는 `PLAYER#<PUID>/PROFILE`, 닉네임 점유 키는 `NICK#<nicknameKey>/CLAIM`, 멱등성 키는 `PLAYER#<PUID>/IDEMP#<route>#<key>`다.
- 닉네임 쓰기에는 UUID 형식의 `Idempotency-Key`가 필요하다. 같은 키·같은 요청의 재시도에는 원래 결과를 반환하고, 같은 키로 다른 본문을 보내면 `409 IDEMPOTENCY_KEY_REUSED`를 반환한다.
- 표시 닉네임은 NFKC 정규화, 앞뒤 공백 제거, 연속 공백 축약 후 2~16자다. 한글 완성형·ASCII 영문·숫자·내부 한 칸 공백만 허용한다. 점유 키에서는 ASCII 영문을 소문자로 바꾼다. 첫 버전에서는 최초 등록만 허용한다.
- Client Secret과 Connect ID Token을 Git, 이미지, Lambda 환경 변수, 셸 인수, 응답, 로그에 남기지 않는다.
- 이번 계획은 티켓·매치·매칭 워커·EOS 세션 상태 API를 구현하지 않는다. 이들은 같은 설계를 바탕으로 별도 구현 계획을 작성한다.

## 집중 검토 항목

1. 두 사용자가 같은 정규화 닉네임을 동시에 등록하면 한 명만 성공하고 다른 한 명은 `NICKNAME_TAKEN`을 받는다(작업 3).
2. 성공 응답을 받지 못한 사용자가 같은 요청을 재시도하면 두 번째 점유 항목을 만들지 않고 원래 응답을 받는다(작업 3).
3. 동일한 멱등성 키를 다른 닉네임에 재사용하면 `IDEMPOTENCY_KEY_REUSED`를 받는다(작업 3).
4. 영문 대소문자·전각 문자·공백 변형은 같은 키로 충돌하고, 잘못된 문자나 길이는 쓰기 전에 거부된다(작업 2).
5. 인증 실패 요청은 DynamoDB에 접근하지 않으며, 토큰과 Secret이 로그·응답에 나타나지 않는다(작업 4).

## 파일 구성

| 파일 | 역할 |
|---|---|
| `Backend-Infra/template.yaml` | DynamoDB 테이블 3개, 프로필 권한·환경 변수, 닉네임 경로 |
| `Backend-Infra/backend/package.json`, `package-lock.json` | DynamoDB SDK 의존성 버전 고정 |
| `Backend-Infra/backend/src/config.ts` | 프로필 테이블 이름 읽기 |
| `Backend-Infra/backend/src/profile/nickname.ts` | 표시 닉네임·점유 키 정규화와 입력 검증 |
| `Backend-Infra/backend/src/profile/profile-store.ts` | `ProfileStore` 계약과 공개 결과·오류 타입 |
| `Backend-Infra/backend/src/profile/dynamo-profile-store.ts` | 일관 읽기와 프로필·닉네임 점유·멱등성 기록의 원자적 쓰기 |
| `Backend-Infra/backend/src/app.ts` | 인증 후 경로 분기와 공개 응답 |
| `Backend-Infra/backend/src/handler.ts` | DynamoDB 클라이언트와 저장소 생성·주입 |
| `Backend-Infra/backend/test/profile/*.test.ts` | 닉네임·DynamoDB 동작 검증 |
| `Backend-Infra/backend/test/app.test.ts`, `test/infra/template.test.ts` | API·SAM 계약 검증 |
| `Backend-Infra/README.md`, `SPRINT_1_CHECKLIST.md` | 배포 절차와 실제 통합 검증 결과 |

---

### 작업 1: DynamoDB 테이블 3개와 런타임 설정 정의

**파일:** `Backend-Infra/template.yaml`, `Backend-Infra/backend/package.json`, `package-lock.json`, `src/config.ts`, `test/config.test.ts`, `test/infra/template.test.ts` 수정.

**인터페이스:** `RuntimeConfig.profileTableName: string`을 추가한다. 프로필 테이블은 문자열 `pk`/`sk`와 TTL 속성 `expiresAtSeconds`, 티켓 테이블은 문자열 `ticketId`와 `queueKey`/`queueSortKey`를 사용하는 `QueueIndex` 및 TTL, 매치 테이블은 문자열 `pk`/`sk`와 TTL을 갖는다. Lambda에 `PLAYER_PROFILES_TABLE`을 전달하고, 이번 단계의 IAM 권한은 프로필 테이블의 `GetItem`과 `TransactWriteItems`로 제한한다. 기존 EOS 리소스 계약은 유지한다.

- [ ] **1단계: 실패하는 설정·템플릿 테스트 작성.** 세 테이블의 키와 TTL, `QueueIndex`, 프로필 환경 변수, 프로필 전용 IAM 작업, 테이블 이름 누락 시 `loadRuntimeConfig` 오류를 검사한다.
- [ ] **2단계: 실패 확인.** `Backend-Infra/backend`에서 `npm test -- test/config.test.ts test/infra/template.test.ts` 실행. 예상: 테이블·설정이 없어 실패한다.
- [ ] **3단계: 리소스·설정·의존성 구현.** `@aws-sdk/client-dynamodb`와 `@aws-sdk/lib-dynamodb`를 `npm install --save-exact`로 추가하고 lockfile을 포함한다. 테이블은 온디맨드 과금과 Project/Environment 태그를 사용한다.
- [ ] **4단계: 통과 확인.** 집중 테스트와 `npm run typecheck` 실행. 예상: 모두 통과한다.
- [ ] **5단계: 커밋.** 변경한 템플릿, 패키지, 설정, 테스트 파일을 스테이징하고 `infra: define player matchmaking and match tables`로 커밋한다.

### 작업 2: 최초 등록 닉네임 검증과 정규화

**파일:** `Backend-Infra/backend/src/profile/nickname.ts`, `Backend-Infra/backend/test/profile/nickname.test.ts` 생성.

**인터페이스:** `normalizeNickname(input: unknown): { nickname: string; nicknameKey: string }`를 제공한다. 잘못된 입력은 공개 코드 `INVALID_NICKNAME`을 가진 `NicknameValidationError`를 던진다. NFKC를 적용한 뒤 허용 문자 여부를 판단하고, 점유 키에서는 ASCII A–Z만 소문자로 바꾼다.

- [ ] **1단계: 실패하는 테스트 작성.** `normalizeNickname("  ＣＨＩＣＫＥＮ   1  ")`의 표시값은 `"CHICKEN 1"`, 키는 `"chicken 1"`이어야 한다. 대소문자·전각 변형은 같은 키이고 한글은 허용한다. 한 글자, 17자, 문장부호, 줄바꿈, 문자열이 아닌 값은 `INVALID_NICKNAME`이어야 한다.
- [ ] **2단계: 실패 확인.** `npm test -- test/profile/nickname.test.ts` 실행. 예상: 모듈이 없어 실패한다.
- [ ] **3단계: `normalizeNickname` 구현.** 정규화 후 Unicode 코드 포인트 수를 세고 정해진 문자·공백 규칙을 적용한다.
- [ ] **4단계: 통과 확인.** 집중 테스트와 `npm run typecheck` 실행. 예상: 모두 통과한다.
- [ ] **5단계: 커밋.** 두 파일을 `feat: normalize game nicknames`로 커밋한다.

### 작업 3: 프로필과 고유 닉네임을 원자적으로 저장

**파일:** `Backend-Infra/backend/src/profile/profile-store.ts`, `src/profile/dynamo-profile-store.ts`, `test/profile/dynamo-profile-store.test.ts` 생성.

**인터페이스:** `ProfileStore.get(playerId: string): Promise<{ playerId: string; nickname?: string }>`와 `ProfileStore.registerNickname(input: { playerId: string; nickname: string; nicknameKey: string; idempotencyKey: string; requestHash: string }): Promise<{ playerId: string; nickname: string; created: boolean }>`를 제공한다. `DynamoProfileStore`는 `DynamoDBDocumentClient`, 테이블 이름, 주입 가능한 시계를 받는다. 저장소 오류는 `NICKNAME_TAKEN`, `NICKNAME_ALREADY_SET`, `IDEMPOTENCY_KEY_REUSED`를 구분한다.

- [ ] **1단계: 명령 인식형 가짜 DocumentClient로 실패하는 테스트 작성.** `PLAYER#p/PROFILE`을 일관 읽기로 조회하는지, 한 `TransactWriteCommand`가 프로필·`NICK#key/CLAIM`·24시간 멱등성 기록을 조건부로 쓰는지 검사한다. 서로 다른 사용자의 동시 점유는 한 명만 성공해야 한다. 같은 키·같은 해시 재시도는 원 응답, 같은 키·다른 해시는 재사용 오류다. 트랜잭션 결과가 불명확하면 새 쓰기 전에 멱등성 기록을 일관 읽기로 조회한다.
- [ ] **2단계: 실패 확인.** `npm test -- test/profile/dynamo-profile-store.test.ts` 실행. 예상: 저장소가 없어 실패한다.
- [ ] **3단계: 두 인터페이스 구현.** `ConsistentRead: true`인 `GetCommand`, `TransactWriteCommand`, DynamoDB 조건식, 취소 이유와 후속 일관 읽기로 충돌을 분류한다. 원시 DynamoDB 오류를 공개하지 않는다.
- [ ] **4단계: 통과 확인.** 집중 테스트와 `npm run typecheck` 실행. 예상: 모두 통과한다.
- [ ] **5단계: 커밋.** 프로필 구현·테스트를 `feat: store unique player nicknames`로 커밋한다.

### 작업 4: 인증된 프로필 API 연결

**파일:** `Backend-Infra/backend/src/app.ts`, `src/handler.ts`, `test/app.test.ts`, `test/handler.test.ts`, `Backend-Infra/template.yaml`, `test/infra/template.test.ts` 수정.

**인터페이스:** `HandlerDependencies`에 `profileStore: ProfileStore`를 추가한다. 기존 `createHandler(dependencies): APIGatewayProxyHandlerV2`는 유지한다. `GET /v1/me`는 `profileStore.get(verified.playerId)`를 호출한다. `POST /v1/me/nickname`은 `normalizeNickname` 후 `registerNickname`을 호출한다. UUID 형식 `Idempotency-Key`와 경로·본문의 정규 해시가 필요하다. 기존 `errorResponse`로 설계된 오류를 반환한다. 저장소 접근 전에 토큰 검증이 완료되어야 한다.

- [ ] **1단계: 실패하는 API·인프라 테스트 작성.** 닉네임이 없는 GET은 `{playerId,requestId}`, 등록된 GET은 닉네임을 포함해야 한다. POST 201, 잘못된 닉네임 400, 점유 충돌·키 재사용 409, 키 누락·형식 오류 400을 검사한다. 미인증 요청은 저장소 호출 0회다. 로그·응답에 토큰과 Secret이 없고 SAM에 `POST /v1/me/nickname` 경로가 있어야 한다.
- [ ] **2단계: 실패 확인.** `npm test -- test/app.test.ts test/handler.test.ts test/infra/template.test.ts` 실행. 예상: 저장소 주입·경로가 없어 실패한다.
- [ ] **3단계: 경로 분기와 의존성 주입 구현.** `handler.ts`에서 AWS DynamoDB 클라이언트와 `DynamoDBDocumentClient`를 만들어 저장소를 주입한다. 웜 요청 사이에 저장소를 재사용한다. 알 수 없는 경로는 쓰기 없이 404를 반환한다.
- [ ] **4단계: 통과 확인.** 집중 테스트, `npm test`, `npm run typecheck`, `npm run build` 실행. 예상: 모두 통과한다.
- [ ] **5단계: 커밋.** API·핸들러·템플릿·테스트를 `feat: expose authenticated player profile API`로 커밋한다.

### 작업 5: 배포된 DynamoDB 경로 검증과 결과 기록

**파일:** `Backend-Infra/README.md`, `Backend-Infra/SPRINT_1_CHECKLIST.md` 수정. 기존 스크립트만으로 비밀을 안전하게 다룰 수 없을 때에만 스모크 스크립트를 추가한다.

**인터페이스:** 기존 `ap-northeast-2`의 `chickengame-backend-dev` 스택과 유지되는 EOS Secret을 사용한다. 스택을 삭제하지 않는다. 실제 관측값만 기록하고 Connect ID Token과 Client Secret은 기록하지 않는다.

- [ ] **1단계: 실행 절차 문서화.** `npm ci`, 테스트, 타입 검사, 빌드, 컨테이너 스모크, `sam validate --lint`, `sam build --use-container`, 변경 세트 검토, 배포, 인증된 `/v1/me`, 테스트 계정 2개의 닉네임 등록·재시도·충돌, DynamoDB 항목 확인, CloudWatch 민감정보 검사를 순서대로 적는다.
- [ ] **2단계: 로컬 검증 실행.** 각 명령의 종료 코드 0을 확인한다. 도구나 SDK가 없어 실행하지 못한 단계는 통과로 표시하지 않고 미검증으로 기록한다.
- [ ] **3단계: SAM 변경 세트 검토와 개발 스택 배포.** 예상: 의도한 테이블·경로·권한·Lambda 이미지만 변경되고 스택이 `UPDATE_COMPLETE`가 된다. 기존 Secret 교체나 다른 리소스 삭제가 포함되면 배포를 중단한다.
- [ ] **4단계: API·CloudWatch 확인.** 프로필 조회, 닉네임 영속 저장과 전역 고유성, 재시도 원 응답, 무토큰 요청의 DB 접근 차단, 로그의 토큰·Secret 비노출을 확인한다. 실제 요청 ID와 결과를 체크리스트에 남긴다.
- [ ] **5단계: 검증 기록 커밋.** 가이드와 체크리스트를 `docs: record DynamoDB profile integration validation`로 커밋한다.

## 후속 계획

이 단계 후에는 (1) 티켓 생성·취소와 4인 매칭 워커, (2) 매치·세션 수명과 Unreal 연동을 각각 별도 구현 계획으로 작성한다. 이 문서는 공통 테이블·프로필 계약을 고정하지만 후속 기능의 구현 완료를 뜻하지 않는다.
