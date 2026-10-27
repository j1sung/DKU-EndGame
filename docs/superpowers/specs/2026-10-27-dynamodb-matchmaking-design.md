# 프로필·닉네임·4인 매칭 데이터 및 API 설계 초안

상태: 검토용 초안 (2026-10-27). 데이터 보존 시간, 게임 모드와 버전 정책, 닉네임 규칙은 클라이언트팀과 확정 전이다. 이 문서는 설계이며 리소스와 API는 아직 구현되지 않았다.

## 1. 목적과 범위

기존 `GET /v1/me`의 EOS Connect ID Token 검증 결과 PUID를 유일한 요청자 식별자로 사용한다. DynamoDB에 프로필과 전역 고유 닉네임을 저장하고, 한 명이 동시에 하나의 활성 매칭 흐름에만 참여하도록 한다. 4개 티켓의 선점과 매치 생성을 하나의 트랜잭션으로 처리하며, 취소·만료·실패 뒤에는 재매칭 가능 상태로 복구한다.

이 요청의 산출물은 설계 문서다. 설계 범위는 데이터 키·인덱스·조건식·상태 전이·워커·API 계약이다. 이 단계에서는 DynamoDB 리소스, Lambda 코드, EOS 세션 및 Unreal 클라이언트를 구현하지 않는다. 승인 후 별도 구현 계획에서 Sprint 1의 최소 DynamoDB 통합과 후속 API·매칭 배포를 단계로 나눈다. 기존 인증 오류 형식과 `requestId`를 유지한다.

## 2. 접근 방식

| 방식 | 장점 | 비용과 위험 |
|---|---|---|
| 도메인별 3개 테이블 (권장) | 기존 `PlayerProfiles`·`MatchmakingTickets`·`Matches` 구분과 맞고 항목 수명·권한·조회 패턴이 명확함 | 매칭 트랜잭션이 여러 테이블에 걸침 |
| 단일 테이블 | 공통 키와 인덱스를 한곳에서 관리 가능 | 서로 다른 항목의 키 규칙과 sparse GSI, 보존 정책이 복잡해짐 |

초안은 3개 테이블을 전제로 한다. DynamoDB `TransactWriteItems`는 테이블 간 원자적 쓰기를 지원하므로 핵심 동시성 조건을 같은 트랜잭션에서 검사한다. 보조 인덱스 조회 결과는 최종 판정에 사용하지 않고 원본 항목을 다시 읽는다.

## 3. 공통 규칙

- 모든 API 경로는 `/v1`로 시작하고 JSON은 camelCase를 사용한다. 응답의 시간은 UTC ISO 8601 문자열, DynamoDB 비교용 시간은 Unix epoch milliseconds 정수다. TTL에는 epoch seconds를 별도 저장한다.
- `playerId`는 인증 계층의 검증된 PUID다. 본문·쿼리의 사용자 ID는 권한 근거가 아니다.
- `ticketId`와 `matchId`는 서버가 생성한 UUID v7 문자열이다. 클라이언트가 제공하거나 재사용하지 않는다.
- 쓰기 API는 `Idempotency-Key` 헤더에 UUID를 받는다. 같은 사용자·경로·키·요청 본문 조합의 반복은 원 응답을 돌려준다. 같은 키로 다른 본문을 보내면 `409 IDEMPOTENCY_KEY_REUSED`다. 키 기록은 별도 항목으로 24시간 보관하며, 원 쓰기와 동일한 트랜잭션에 포함한다. 멱등성 기록의 최종 저장 형식과 트랜잭션 항목 수는 구현 계획에서 검증한다.
- `X-Client-Version`은 매칭 티켓 생성 시 필수다. 지원 버전은 서버 설정에서 관리하고, 불일치는 `426 CLIENT_VERSION_UNSUPPORTED`로 반환한다. 세션 참가 호환성은 정확한 버전 문자열로 시작하고 버전 범위 정책은 별도 합의한다.
- 공통 오류: `{ "error": { "code": "...", "message": "..." }, "requestId": "..." }`. 공개 응답과 로그에는 토큰·Secret을 넣지 않는다.

## 4. 테이블과 항목

### 4.1 PlayerProfiles

기본키 `pk`(String), `sk`(String). 기본키 외 GSI는 없다.

| 항목 | PK / SK | 핵심 속성 | 용도 |
|---|---|---|---|
| 프로필 | `PLAYER#<PUID>` / `PROFILE` | `nickname`, `nicknameKey`, `createdAt`, `updatedAt`, `revision` | PUID로 조회 |
| 닉네임 점유 | `NICK#<nicknameKey>` / `CLAIM` | `playerId`, `createdAt` | 정규화 닉네임 고유성 |
| 활성 활동 가드 | `PLAYER#<PUID>` / `ACTIVITY` | `kind`, `ticketId`, `matchId`, `expiresAtMs`, `revision` | 활성 티켓·매치 단일성 및 현재 상태 조회 |
| 멱등성 기록 | `PLAYER#<PUID>` / `IDEMP#<route>#<key>` | `requestHash`, `statusCode`, `response`, `expiresAtSeconds` | 중복 요청 응답 재현 |

`nicknameKey`는 Unicode NFKC 정규화 후 Unicode case folding을 적용한 값이다. 앞뒤 공백을 제거하고 내부 연속 공백은 한 칸으로 줄인다. 표시 닉네임은 정규화 후 2~16자, 한글·영문·숫자·한 칸 공백만 허용한다. 이 규칙은 다국어·금칙어 요구가 확인되면 수정한다. 닉네임 최초 등록은 `PROFILE` 조건부 생성, `NICK#.../CLAIM` 조건부 생성으로 원자화한다. 변경 허용 여부는 제품 결정 전까지 **최초 등록 1회만 허용**한다.

활동 가드는 TTL로 지우지 않는다. 기한이 지난 가드는 후속 요청 또는 복구 워커가 `ticketId`/`matchId`/`revision` 조건을 확인하고 정리한다. 이를 통해 TTL 삭제 지연에 영향을 받지 않는다.

### 4.2 MatchmakingTickets

기본키 `ticketId`(String), 정렬키 없음. 대기열용 sparse GSI `QueueIndex`: PK `queueKey`, SK `queueSortKey`; `queueKey = <mode>#<clientVersion>#<bucket>`, `queueSortKey = <13자리 0 패딩 createdAtMs>#<ticketId>`. `bucket = hash(ticketId) mod 16`으로 시작한다. 대기 티켓에만 두 속성을 기록하고, 대기열에서 나갈 때 제거한다. 같은 버전·모드의 16개 버킷을 병합해 오래된 순서로 후보를 고른다. 버킷 수는 부하 측정 후 조정한다.

티켓 속성: `ticketId`, `playerId`, `status`, `mode`, `clientVersion`, `createdAtMs`, `expiresAtMs`, `assignedMatchId?`, `updatedAtMs`, `revision`, `expiresAtSeconds`(종료 후 보존용 TTL). 기본 대기 수명은 5분이다. `QueueIndex`는 eventual consistency이므로 후보의 원본 티켓을 일관 읽기로 다시 확인하고, 트랜잭션 조건식으로 상태·기한을 최종 검증한다.

### 4.3 Matches

기본키 `pk`(String), `sk`(String). `MATCH#<matchId>/META`에는 `status`, `mode`, `clientVersion`, `memberIds`(정렬된 PUID 4개), `hostPlayerId`, `hostAttempt`, `sessionId?`, `createdAtMs`, `readyDeadlineMs`, `updatedAtMs`, `revision`, `expiresAtSeconds`를 둔다. `MATCH#<matchId>/MEMBER#<PUID>`에는 `joinedAtMs?`, `joinStatus`를 둔다. 매치 ID로 `Query`해 메타와 구성원을 읽고, 구성원 확인은 해당 멤버 키의 `GetItem`으로 수행한다. 호스트 권한은 메타의 `hostPlayerId`와 `hostAttempt`를 조건식으로 검사한다. 별도 GSI는 초기 범위에 없다.

예시 키: `PLAYER#puid/ACTIVITY`, `NICK#chicken/CLAIM`, 티켓 ID `018...`, `MATCH#018.../MEMBER#puid`. 실제 PUID와 토큰은 예시 문서에 넣지 않는다.

## 5. 원자적 변경과 충돌 처리

1. **닉네임 등록:** 프로필이 없거나 닉네임 미설정임을 확인한 뒤 프로필 생성/갱신과 닉네임 점유 `attribute_not_exists(pk)`를 한 트랜잭션에 묶는다. 점유 실패는 `409 NICKNAME_TAKEN`; 같은 사용자의 동일 요청 재시도는 멱등성 기록을 통해 원 응답을 준다.
2. **티켓 생성:** 활동 가드가 없거나 이미 만료된 같은 요청의 복구가 끝난 상태에서만 `WAITING` 티켓과 `ACTIVITY(kind=WAITING,ticketId=...)`를 생성한다. 기존 활성 가드가 있으면 `409 ALREADY_ACTIVE`, 동일 키 재시도는 기존 결과다. 만료 가드가 있으면 먼저 연결된 티켓/매치를 조건부 정리하고 재시도한다.
3. **티켓 취소:** 요청의 `ticketId`가 활동 가드와 원본 티켓의 ID에 일치하고 티켓이 `WAITING`일 때만 티켓을 `CANCELLED`로 바꾸고 가드를 삭제한다. 매치 배정과 경합하면 한 트랜잭션만 성공한다. 이미 `ASSIGNED`라면 `409 TICKET_ALREADY_ASSIGNED`; 새 티켓을 취소하지 않는다.
4. **4인 선점:** 네 원본 티켓이 모두 `WAITING`, 같은 큐, 만료 전이며 네 활동 가드가 각각 해당 티켓을 가리키는지 조건부 확인한다. 한 `TransactWriteItems`에서 네 티켓을 `ASSIGNED`로 변경하고 큐 인덱스 속성을 제거하고, 네 가드를 `MATCH`로 바꾸고, 매치 메타 1개와 멤버 4개를 생성한다. 최다 13개 핵심 항목에 멱등성/작업 기록이 추가돼도 DynamoDB 트랜잭션 한도 안인지 구현 시 확인한다. 트랜잭션 충돌 시 동일 4인 조합을 무한 재시도하지 않고 최대 3회 지수 백오프 후 후보를 다시 조회한다.
5. **상태 변경:** 세션 등록, 참가, 시작은 `matchId`, 현재 `status`, `revision`, 멤버·호스트 자격과 `hostAttempt`를 조건식으로 검사한다. 세션 등록과 시작처럼 기한을 바꾸는 전이는 매치 메타와 네 활동 가드의 `expiresAtMs`를 함께 갱신한다. 참가 보고는 해당 멤버 항목을 조건부 갱신한다. 이미 참가 처리된 멤버가 재시도해도 인원 수가 늘지 않으며, 네 멤버 항목이 모두 참가 상태일 때만 메타를 `READY_TO_START`로 전이한다. 동일 요청 재시도는 멱등성 키로 처리한다.
6. **종료·복구:** 매치를 `FINISHED`/`FAILED`/`EXPIRED`로 변경하고 네 활동 가드를 해당 `matchId` 조건으로 삭제한다. 조건 충돌은 다른 복구자가 먼저 처리했는지 재조회한다. 네 가드와 매치 상태는 한 트랜잭션에서 갱신하며 실패하면 전체가 재시도 가능하게 남는다.

트랜잭션 취소 이유는 내부 로그에 제한된 코드로 남기고 공개 응답은 상태 재조회 후 결정한다. DynamoDB 조건 실패, 처리량 제한, 네트워크 오류를 구분한다. 최종 응답 불명 시 새 ID로 재시도하지 않고 같은 멱등성 키의 결과를 조회한다.

## 6. 상태와 기한

```text
Ticket: WAITING -> ASSIGNED | CANCELLED | EXPIRED
Match: FORMING -> SESSION_READY -> READY_TO_START -> IN_GAME -> FINISHED
                  \-> FAILED/EXPIRED (IN_GAME에서도 FAILED 가능)
```

매치 생성 시 호스트는 정렬된 네 PUID 중 첫 번째로 정하는 결정적 초기 정책을 쓴다. `hostAttempt=1`이다. `FORMING`에서 60초 내 세션을 등록해야 하고, `SESSION_READY`에서 60초 내 네 명이 참가를 보고해야 한다. `READY_TO_START`에서 호스트가 시작을 보고한다. 인증된 호스트의 `sessionId` 등록과 인증된 멤버의 `joined` 보고를 첫 구현의 상태 근거로 사용한다. 서버가 EOS 세션 존재나 실제 네트워크 참가를 별도로 증명하지 않으므로 `READY_TO_START`는 **클라이언트 보고 기준**이다. 이 신뢰 경계는 클라이언트팀이 승인해야 한다.

호스트의 `finished` 보고는 `IN_GAME -> FINISHED`, 멤버 누구의 `left` 보고는 준비 또는 경기 중 `-> FAILED`로 전이한다. 두 전이 모두 네 활동 가드를 같은 트랜잭션에서 해제한다. 호스트 재선정은 첫 구현에서 하지 않고 준비 실패 시 매치를 종료·가드 해제해 다시 매칭한다. 보고 없이 클라이언트가 종료되면 복구 워커가 준비 제한시간 또는 경기 시작 후 30분 상한에 `EXPIRED`로 처리한다. 이 시간값은 시연 동선 확인 후 확정한다.

애플리케이션은 매 요청과 워커 실행에서 `expiresAtMs <= now`를 검사한다. DynamoDB TTL `expiresAtSeconds`는 완료/취소 항목의 지연 삭제에만 사용하며 기한 판정이나 가드 해제의 타이머가 아니다. 티켓과 매치 기록은 종료 후 7일 보관하는 초안이다.

## 7. 매칭 워커

티켓 생성 시 SQS에 큐 키를 보내 워커를 깨우고, 주기적 EventBridge 작업으로 놓친 대기열과 만료 상태를 복구한다. SQS 메시지는 중복될 수 있으므로 워커는 메시지를 명령이 아닌 조회 힌트로 취급한다. 동일 큐의 동시 워커는 조건부 트랜잭션으로 승자를 결정한다. 4명 미만이면 종료하고 다음 티켓 또는 주기 작업을 기다린다. 일시 오류는 SQS 재시도와 DLQ로 추적하며, 조건 충돌은 최신 원본을 다시 읽은 뒤 후보를 재선정한다. 로그에는 큐 키, 후보 수, 성공/충돌/재시도 수, `matchId`와 `requestId`를 남기되 토큰은 남기지 않는다.

초기 규모에서는 16개 버킷을 모두 조회한다. 후보 수가 늘면 읽기 비용과 공정성을 측정해 버킷 수와 조회 창을 조정한다. 특정 버킷의 첫 4명만 고르는 방식은 전역 대기 순서를 보장하지 않으므로 사용하지 않는다.

## 8. API 계약 초안

모든 경로는 인증된 PUID를 사용하고 응답에 `requestId`를 포함한다. 모든 쓰기 요청은 `Idempotency-Key`가 필요하다. 인증 오류는 현재 `/v1/me`와 동일하다.

| 요청 | 성공 | 주요 실패 |
|---|---|---|
| `GET /v1/me` | `200 {playerId,nickname?,requestId}`; 프로필 미생성도 200 | 기존 인증 오류 |
| `POST /v1/me/nickname` `{nickname}` | `201 {playerId,nickname,requestId}` | `400 INVALID_NICKNAME`, `409 NICKNAME_TAKEN/NICKNAME_ALREADY_SET` |
| `POST /v1/matchmaking/tickets` `{mode}` + `X-Client-Version` | `201 {ticketId,status:"WAITING",expiresAt,requestId}` | `409 ALREADY_ACTIVE`, `426 CLIENT_VERSION_UNSUPPORTED` |
| `GET /v1/matchmaking/tickets/current` | `200 {ticketId,status,matchId?,expiresAt?,requestId}` 또는 `200 {ticket:null,requestId}` | 인증 오류 |
| `DELETE /v1/matchmaking/tickets/current` `{ticketId}` | `200 {ticketId,status:"CANCELLED",requestId}` | `404 TICKET_NOT_FOUND`, `409 TICKET_ALREADY_ASSIGNED/TICKET_MISMATCH` |
| `GET /v1/matches/{matchId}` | `200 {matchId,status,memberIds,hostPlayerId,hostAttempt,sessionId?,readyDeadline,requestId}` | `403 NOT_MATCH_MEMBER`, `404 MATCH_NOT_FOUND` |
| `POST /v1/matches/{matchId}/session` `{sessionId,hostAttempt}` | `200 {matchId,status:"SESSION_READY",requestId}` | `403 NOT_MATCH_HOST`, `409 MATCH_STATE_CONFLICT/HOST_ATTEMPT_MISMATCH` |
| `POST /v1/matches/{matchId}/joined` `{hostAttempt}` | `200 {matchId,status,joinedCount,requestId}` | `403 NOT_MATCH_MEMBER`, `409 MATCH_STATE_CONFLICT` |
| `POST /v1/matches/{matchId}/started` `{hostAttempt}` | `200 {matchId,status:"IN_GAME",requestId}` | `403 NOT_MATCH_HOST`, `409 MATCH_STATE_CONFLICT` |
| `POST /v1/matches/{matchId}/finished` `{hostAttempt}` | `200 {matchId,status:"FINISHED",requestId}` | `403 NOT_MATCH_HOST`, `409 MATCH_STATE_CONFLICT` |
| `POST /v1/matches/{matchId}/left` `{hostAttempt}` | `200 {matchId,status:"FAILED",requestId}` | `403 NOT_MATCH_MEMBER`, `409 MATCH_STATE_CONFLICT` |

`GET /v1/matches/{matchId}`는 멤버에게만 세션 ID를 반환한다. `sessionId`는 EOS 세션의 실제 식별자 형식 확인 후 길이·문자 검증을 확정한다. `joinedCount=4`가 되면 `READY_TO_START`로 전이한다. 폴링은 대기 티켓 2초부터 시작해 오류 또는 장기 대기 시 최대 5초로 늘리고, `Retry-After`가 있으면 따른다.

## 9. 검증 기준과 미확정 사항

설계 검증에서는 (1) 각 조회가 키/GSI로 가능한지, (2) 같은 사용자의 중복 티켓과 두 매치 배정을 막는지, (3) 취소와 선점의 경합이 한쪽만 성공하는지, (4) 지연된 취소가 새 티켓을 건드리지 않는지, (5) 만료 후 가드가 확실히 풀리는지, (6) 각 API의 권한·오류가 정의됐는지 확인한다. 구현 전에 DynamoDB 트랜잭션 항목 수와 크기 제한을 실제 SDK 테스트로 확인한다.

클라이언트팀과 확정할 항목: 지원 게임 모드와 버전 호환 범위, 닉네임 문자·길이와 변경 허용 여부, 대기·준비·경기 제한시간, 세션 ID 형식, 경기 종료/이탈 보고 API, **클라이언트의 EOS 세션·참가 보고를 서버가 신뢰해도 되는지**, 4인 테스트 계정 및 폴링 주기. 미확정 항목이 결정되기 전에는 이 문서의 기본값을 배포 계약으로 취급하지 않는다.
