# 매칭 티켓·4인 배정 설계 초안

상태: 검토 초안 (2026-11-01). [기존 매칭 설계](2026-10-27-dynamodb-matchmaking-design.md)의 3개 테이블, EOS PUID 인증, SQS 호출 방식을 구체화한다. 이 문서는 티켓과 최초 매치 배정까지 다룬다. EOS 세션 생성·참가와 경기 상태 전이는 다음 설계 단계다. 매치 기한 복구는 [GSI 후속 설계 초안](2026-11-01-match-deadline-recovery-design.md)에 기록했다.

## 1. 이번 단계의 결정

| 항목 | 첫 버전 |
|---|---|
| 게임 모드 | 단일 4인 모드 `standard` |
| 클라이언트 버전 | `X-Client-Version: 0.1.0` 정확히 일치 |
| 티켓 수명 | 생성부터 3분 (`expiresAtMs = createdAtMs + 180_000`) |
| 대기열 | `mode#clientVersion#bucket`, 버킷 16개 |
| 사용자 제한 | 검증된 EOS PUID당 활성 티켓 또는 매치 하나 |
| 매칭 | 같은 모드·버전의 오래된 유효 티켓 4개를 원자적으로 배정 |
| 닉네임 | 최초 등록한 프로필만 티켓 생성 허용 (`409 NICKNAME_REQUIRED`) |

`standard`와 `0.1.0`은 서버 설정의 허용 목록에 넣고 클라이언트 인계 문서에 그대로 기록한다. 버전이 다른 빌드는 같은 큐에 넣지 않는다. 모드를 늘릴 때에는 API를 바꾸기 전에 서버 허용 목록과 워커의 탐색 대상만 추가한다.

## 2. 사용자 흐름과 API

모든 경로는 `Authorization: Bearer <EOS Connect ID Token>`으로 인증한다. PUID는 검증 결과에서만 얻고 요청 본문의 플레이어 ID는 받지 않는다. 모든 응답에는 `requestId`를 포함한다. 쓰기 요청에는 UUID 형식 `Idempotency-Key`가 필요하다.

| 요청 | 응답 | 주요 오류 |
|---|---|---|
| `POST /v1/matchmaking/tickets` `{ "mode": "standard" }`, `X-Client-Version: 0.1.0` | `201 {ticketId,status:"WAITING",expiresAt,requestId}` | `400 INVALID_MODE/INVALID_IDEMPOTENCY_KEY`, `409 NICKNAME_REQUIRED/ALREADY_ACTIVE/IDEMPOTENCY_KEY_REUSED`, `426 CLIENT_VERSION_UNSUPPORTED` |
| `GET /v1/matchmaking/tickets/current` | 활성 티켓이면 `200 {ticketId,status,matchId?,expiresAt?,requestId}`, 없으면 `200 {ticket:null,requestId}` | 기존 인증 오류 |
| `DELETE /v1/matchmaking/tickets/current` `{ "ticketId": "<UUID v7>" }` | `200 {ticketId,status:"CANCELLED",requestId}` | `400 INVALID_TICKET_ID/INVALID_IDEMPOTENCY_KEY`, `404 TICKET_NOT_FOUND`, `409 TICKET_MISMATCH/TICKET_ALREADY_ASSIGNED/IDEMPOTENCY_KEY_REUSED` |

`expiresAt`은 UTC ISO 8601이다. 3분은 최대 대기 시간이며 4명이 모이면 즉시 배정한다. 3분이 지나면 `EXPIRED` 처리 후 대기 화면에서 나가고, 재시작은 사용자 입력으로만 한다. **매칭 취소는 4명이 모이기 전 `WAITING` 상태에서만 가능하다.** `ASSIGNED` 이후에는 매칭 취소 버튼을 사용하지 않고 세션 준비·경기 흐름으로 진행한다. 네트워크 오류 또는 시간 초과 뒤에는 **같은** 멱등성 키와 본문으로 재시도한다. 동일 키에 다른 본문을 보내면 원 작업을 재해석하지 않고 `IDEMPOTENCY_KEY_REUSED`를 반환한다.

### 대기 화면의 취소 버튼

- `WAITING`: 현재 `ticketId`로 `DELETE /v1/matchmaking/tickets/current`를 호출한다. 성공하면 본인 티켓과 본인 활동 가드만 해제하고 메인 화면으로 돌아간다. 다른 사람의 티켓·가드·대기 상태는 변경하지 않으며 자동으로 새 티켓을 만들지 않는다. 남은 인원이 4명 미만이면 다른 참가자가 올 때까지 계속 대기한다.
- 티켓 생성 응답을 기다리는 중: 취소 의도를 로컬에 보존하고, 생성 응답 또는 `GET current`로 `ticketId`를 확인하자마자 삭제한다. 늦게 도착한 생성 응답이 취소된 화면을 다시 대기 상태로 바꾸면 안 된다.
- 취소와 4인 배정이 경합해 `TICKET_ALREADY_ASSIGNED`가 오면 서버에서 배정이 먼저 확정된 것이다. `GET current`로 `matchId`를 확인하고 세션 준비 화면으로 이동한다. 이 경우 취소 성공으로 표시하거나 메인 화면으로 돌려보내지 않는다.
- `ASSIGNED` 이후에는 매칭 취소 버튼을 숨기거나 비활성화한다. 별도의 게임 이탈·연결 실패 처리는 다음 매치 수명 설계의 범위이며, 대기 화면의 취소 버튼과 연결하지 않는다.

## 3. 저장 항목

### MatchmakingTickets

기본키 `ticketId`는 서버가 만든 UUID v7이다. `WAITING` 티켓에는 `playerId`, `mode`, `clientVersion`, `createdAtMs`, `expiresAtMs`, `updatedAtMs`, `revision=1`, `queueKey`, `queueSortKey`를 저장한다. `bucket = hash(ticketId) mod 16`, `queueKey = standard#0.1.0#<bucket>`, `queueSortKey = <createdAtMs의 13자리 0 패딩>#<ticketId>`다. `QueueIndex`에 나타나는 항목은 대기 중인 티켓뿐이다. `ASSIGNED`·`CANCELLED`·`EXPIRED`로 바꿀 때 두 인덱스 속성을 제거한다. 종료 상태에만 7일 뒤 삭제를 위한 `expiresAtSeconds`를 부여한다.

### PlayerProfiles

`PLAYER#<PUID>/ACTIVITY`에 `kind=WAITING`, `ticketId`, `expiresAtMs`, `revision`을 저장한다. 활동 가드에는 DynamoDB TTL을 사용하지 않는다. 만료된 가드는 원본 티켓 상태를 확인한 뒤 조건부 트랜잭션으로 정리한다. 멱등성 기록은 `PLAYER#<PUID>/IDEMP#<route>#<key>`에 요청 해시, 상태 코드, 원 응답, 24시간 TTL을 저장한다.

### Matches

4인 배정 시 `MATCH#<matchId>/META`에 `FORMING`, 모드·버전, 정렬된 네 PUID, 결정적 호스트(정렬된 PUID의 첫 번째), `hostAttempt=1`, 생성 시각, 60초 준비 기한, `revision=1`을 저장한다. `MATCH#<matchId>/MEMBER#<PUID>` 네 항목도 동시에 만든다. 실제 EOS `sessionId`는 다음 단계에서 호스트가 등록한다.

## 4. 티켓 API의 원자적 처리

### 생성

1. 토큰 검증 후 닉네임·모드·버전·멱등성 키를 검사한다. 기존 멱등성 기록을 일관 읽기로 조회한다.
2. 기존 활성 가드가 있으면 `ALREADY_ACTIVE`를 반환한다. `WAITING` 가드가 만료됐으면 연결된 원본 티켓의 ID·상태·revision을 확인하고 티켓을 `EXPIRED`로 바꾸면서 가드를 지우는 트랜잭션을 먼저 수행한다. `MATCH` 가드는 다음 단계의 매치 복구 규칙으로 처리한다.
3. 한 `TransactWriteItems`에서 새 티켓 `Put`(ID 미존재), 활동 가드 `Put`(미존재), 멱등성 기록 `Put`(미존재)을 수행한다. 불명확한 결과나 경합에서는 같은 기록과 가드를 일관 읽기로 재확인한다.
4. 커밋 후 SQS에 `mode`, `clientVersion`만 담은 대기열 조회 힌트를 보낸다. 전송 실패를 기록하고 짧게 재시도한다. 티켓 생성은 이미 확정됐으므로 SQS 실패만으로 새 티켓을 만들지 않으며 1분 주기 복구 호출이 놓친 티켓을 발견한다.

### 조회

활동 가드를 일관 읽기로 조회하고 연결된 티켓 원본도 일관 읽기로 확인한다. `WAITING`이고 기한이 지났으면 조건부 만료 정리 후 `ticket:null`을 반환한다. `ASSIGNED`면 `matchId`를 반환한다. 가드와 티켓이 어긋나면 무조건 새 티켓을 만들지 않고 제한된 재조회 뒤 서버 오류로 남긴다.

### 취소

본문 `ticketId`와 가드의 `ticketId`가 일치할 때만 진행한다. 한 트랜잭션에서 **요청자 자신의** `WAITING`·미만료 원본 티켓을 `CANCELLED`로 갱신하고 인덱스 속성을 제거하며, 같은 티켓을 가리키는 **요청자 자신의** 활동 가드를 삭제하고 멱등성 기록을 생성한다. 다른 플레이어의 항목은 이 트랜잭션에 포함하지 않는다. 배정 워커와 경합하면 한쪽 트랜잭션만 성공한다. 취소 실패 후에는 가드와 티켓을 재조회해 `TICKET_ALREADY_ASSIGNED`, `TICKET_MISMATCH`, `TICKET_NOT_FOUND`를 구분한다. 이전 티켓의 늦은 취소가 새 가드를 삭제할 수 없도록 조건에 티켓 ID와 revision을 포함한다.

## 5. SQS 워커와 4인 배정

SQS Standard Queue는 호출 신호이며 메시지 자체가 배정 명령은 아니다. 중복·순서 변경을 허용한다. 티켓 생성마다 힌트를 보내고 EventBridge가 1분마다 같은 워커에 전체 대기열 확인 힌트를 보낸다. 메시지에는 토큰, PUID, 닉네임을 넣지 않는다. 실패 메시지는 재시도 후 DLQ로 보내고 알람 대상으로 삼는다.

1. 워커가 허용된 모드·버전의 16개 `QueueIndex` 버킷을 조회한다. 각 버킷의 앞 4개 이상을 병합해 전역에서 가장 오래된 후보 4개를 고른다. 4명 미만이면 종료한다.
2. GSI 결과는 최종 판정에 사용하지 않는다. 네 티켓 원본과 네 활동 가드를 일관 읽기로 다시 확인한다. 만료 티켓은 조건부로 `EXPIRED` 처리하고, 무효 후보를 버린 뒤 후보를 다시 고른다.
3. 한 트랜잭션에 티켓 `ASSIGNED` 갱신 4개, 가드 `MATCH` 갱신 4개, 매치 메타 `Put` 1개, 멤버 `Put` 4개를 넣는다(총 13개). 각 티켓의 `WAITING`·동일 모드/버전·미만료·revision, 각 가드의 `WAITING`·해당 티켓 ID·revision을 조건으로 건다. 두 워커가 같은 후보를 잡아도 한 트랜잭션만 성공한다.
4. 조건 충돌이면 원본을 다시 읽고 다른 후보를 최대 3회 선택한다. 전송 결과가 불명확하면 동일한 배정 시도를 재확인하며 새 매치 ID로 맹목 재시도하지 않는다. 최종 실패는 SQS 재시도 대상으로 둔다.

AWS 문서상 `TransactWriteItems`는 최대 100개 작업과 총 4MB를 허용하고, 같은 항목에 두 작업을 넣을 수 없다. 이 13개 작업은 개수 제한 이내이며 실제 요청 크기와 각 항목 조건식은 구현 테스트에서 확인한다. [DynamoDB 트랜잭션](https://docs.aws.amazon.com/amazondynamodb/latest/APIReference/API_TransactWriteItems.html) · [GSI 읽기 일관성](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/HowItWorks.ReadConsistency.html) · [SQS 중복 전달](https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/standard-queues-at-least-once-delivery.html)

## 6. 복구와 단계 경계

- 주기 호출은 16개 대기 버킷을 다시 조회해 SQS 전송 실패·일시적 GSI 지연으로 놓친 티켓을 처리하고, 만료된 `WAITING` 티켓을 조건부 정리한다.
- `FORMING` 이후 매치 준비 기한, 경기 중 이탈, 종료 후 네 활동 가드 해제는 다음 매치·세션 수명 단계에서 설계·구현한다. 워커를 실제 사용자에게 활성화하기 전 해당 복구 경로까지 배포한다. 그 전에는 티켓 API와 워커를 개발 환경의 제한된 테스트 흐름에서만 검증한다.
- DynamoDB TTL은 지연 삭제용이며 3분 대기 기한 판정이나 가드 해제를 대신하지 않는다.

## 7. 검증과 Unreal 인계

저장소 단위 테스트에서는 동일 사용자 중복 생성, 동일 키 재시도, 만료 가드 정리, **한 명의 취소 후 다른 사람의 티켓·가드 유지**, 취소·배정 경합, 늦은 취소, GSI 지연·중복 SQS 메시지, 네 명 중 한 명의 만료, 13개 작업의 원자성을 검사한다. 개발 스택에서는 서로 다른 테스트 계정 4개로 한 매치만 생성되는지 확인한다. 현재 확보된 계정은 2개이므로 4인 실환경 검증에는 두 계정을 더 준비해야 한다.

Unreal 인계 문서는 `standard`/`0.1.0` 상수, 토큰 전달·갱신, UUID 멱등성 키 보존, 티켓 생성/조회/취소 JSON 예시, `WAITING`·`ASSIGNED`·만료·오류별 UI 동작, `WAITING` 전용 취소 버튼과 성공 시 메인 화면 복귀, 2초부터 최대 5초 폴링과 `Retry-After` 우선, 다음 매치 조회로 넘길 `matchId`를 담는다. 게임 담당자는 이 인터페이스를 시작 버튼·대기 화면·취소 버튼에 연결하고 게임 로직을 구현한다.

## 8. 구현 전 남은 결정

`standard`, `0.1.0`, 3분 대기, 4인 배정 전 `WAITING`에서만 매칭 취소와 성공 시 메인 화면 복귀는 확정했다. 닉네임 선행 요구는 티켓 API에 구현했다. 매치 기한 복구 조회는 사용자가 `Matches`의 sparse GSI를 선택했고, [후속 설계](2026-11-01-match-deadline-recovery-design.md)에 인덱스·복구 트랜잭션·미확정 게임 정책을 정리했다.
