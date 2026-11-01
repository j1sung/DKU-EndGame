# 4인 매치 기한 조회와 복구 설계 초안

상태: 검토 초안 (2026-11-01). [티켓·4인 배정 설계](2026-11-01-matchmaking-tickets-worker-design.md)에서 미정이던 `Matches` 기한 복구 방식에 대한 후속 설계다. 사용자 선택에 따라 `Matches`의 sparse GSI를 사용한다. 이 문서는 구현·배포 승인을 뜻하지 않는다.

## 1. 목적과 범위

4명의 티켓이 `ASSIGNED`가 된 뒤 호스트가 EOS 세션을 만들지 않거나, 참가·시작·종료 보고가 끊겨도 네 명의 `ACTIVITY` 가드가 영원히 남지 않게 한다. 활동 중인 매치만 기한 순으로 찾아 `EXPIRED`로 종료하고 네 가드를 안전하게 풀어 재매칭할 수 있게 한다. 완료된 매치는 인덱스에서 제거한다.

EOS 세션의 실제 존재나 경기 결과를 서버가 검증하는 기능은 별도 문제다. 첫 버전은 인증된 클라이언트의 세션·참가·시작·종료 보고를 상태 전이의 근거로 삼는다. 대기 화면의 `WAITING` 취소는 이 복구 흐름과 별개이며, `ASSIGNED` 이후에는 매칭 취소 API로 매치를 종료하지 않는다.

## 2. 접근 방식

| 방식 | 장점 | 비용과 제약 |
|---|---|---|
| `Matches`에 기한 GSI 추가 **(사용자 선택)** | 활성 매치만 기한 순으로 조회; 주기 워커로 누락 복구; 기존 테이블·트랜잭션 구조 유지 | 쓰기·인덱스 비용; GSI는 최종 일관성이므로 원본 재검증 필요 |
| 매치별 EventBridge Scheduler 예약 | 개별 기한에 가까운 호출 | 매치마다 예약 생성·변경·삭제와 실패 복구가 필요 |
| `Matches` 전체 Scan | 인덱스 변경 없음 | 매치 기록이 쌓일수록 읽기 비용과 지연 증가 |

GSI를 채택하고 전체 Scan을 정기 복구 경로에 사용하지 않는다. AWS 문서에 따르면 sparse GSI는 인덱스 키가 있는 항목만 포함하고, GSI 조회는 최종 일관성이다. 따라서 GSI 응답은 후보 목록이며 최종 판정은 기본 테이블의 일관 읽기와 조건부 트랜잭션에서 한다. [Sparse GSI](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/bp-indexes-general-sparse-indexes.html) · [읽기 일관성](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/HowItWorks.ReadConsistency.html)

## 3. 저장 구조

기존 `Matches` 기본키 `pk`/`sk`를 유지한다. `DeadlineIndex`의 PK는 `deadlineKey`(String), SK는 `deadlineSortKey`(String)이다. 초기 버킷 수는 16개이며 `bucket = hash(matchId) mod 16`이다. `META` 항목 중 활성 상태에만 다음 두 필드를 둔다.

```text
pk              = MATCH#<matchId>
sk              = META
status          = FORMING | SESSION_READY | READY_TO_START | IN_GAME
deadlineAtMs    = <기한 epoch milliseconds>
deadlineKey     = ACTIVE#<bucket>
deadlineSortKey = <13자리 0 패딩 deadlineAtMs>#<matchId>
```

`DeadlineIndex`는 `KEYS_ONLY`로 시작한다. 워커는 인덱스에서 기본키를 얻어 `MATCH#<matchId>/META`를 일관 읽기로 재확인한다. `deadlineAtMs`와 인덱스 SK를 **같은 메타 항목의 한 번의 쓰기**로 갱신한다. 이 `deadlineAtMs`를 기존 초안의 `readyDeadlineMs` 대신 전 상태의 공통 기한 필드로 사용한다. `FINISHED`·`FAILED`·`EXPIRED`로 전이할 때 `deadlineKey`, `deadlineSortKey`, `deadlineAtMs`를 제거하고 메타·멤버·배정 티켓에 종료 시점부터 7일 뒤 지연 삭제용 `expiresAtSeconds`를 설정한다. `MEMBER#<PUID>` 항목은 기한 인덱스에 넣지 않는다. DynamoDB TTL은 기한 판정이나 가드 해제를 대신하지 않는다.

기존 `MatchmakingTickets`의 `ASSIGNED` 티켓은 `matchId`를 가리키고, 각 `PLAYER#<PUID>/ACTIVITY` 가드는 `kind=MATCH`, `ticketId`, `matchId`, `expiresAtMs`, `revision`을 가진다. 모든 매치 상태 전이에서 메타의 기한과 네 가드의 `expiresAtMs`를 같은 트랜잭션으로 갱신한다. 가드의 `expiresAtMs`는 조회·표시와 장애 진단용이며, 매치를 종료할지의 기준은 메타의 `deadlineAtMs`다.

## 4. 상태와 기한 제안

| 상태 | 들어오는 사건 | 다음 기한 제안 | 정상 전이 |
|---|---|---:|---|
| `FORMING` | 4인 배정 | 60초 | 호스트 세션 등록 → `SESSION_READY` |
| `SESSION_READY` | 호스트의 EOS 세션 ID 등록 | 60초 | 네 명의 참가 보고 → `READY_TO_START` |
| `READY_TO_START` | 네 명 참가 보고 완료 | 60초 | 호스트의 시작 보고 → `IN_GAME` |
| `IN_GAME` | 호스트의 시작 보고 | 시작 후 30분 | 호스트의 완료 보고 → `FINISHED` |

표의 60초·30분은 기존 설계 초안을 구체화한 **제안값**이다. 경기 규칙과 실제 시연 길이에 따라 조정할 수 있다. 정상적인 `left` 보고가 오면 준비 중 또는 경기 중 매치를 `FAILED`로 종료하고 네 가드를 해제한다. 호스트 재선정은 첫 버전에서 하지 않고 실패 매치를 종료한 뒤 사용자가 다시 매칭한다. 클라이언트가 강제 종료되어 보고하지 못한 경우에는 기한 워커가 `EXPIRED`로 처리한다.

## 5. 기한 워커

EventBridge의 1분 주기 호출이 16개 `deadlineKey`를 각각 `deadlineSortKey <= <현재 시각 13자리>#~` 조건으로 페이지 끝까지 조회한다. 한 번의 실행에서 처리량·시간 상한에 이르면 다음 호출이 이어서 처리하고 남은 건수를 지표에 남긴다. EventBridge 일정은 초 단위 정밀도를 보장하지 않고 GSI 전파·워커 재시도도 지연될 수 있으므로 상태 기한은 **정리 자격이 생기는 시각**이다. 보통 다음 주기 이후에 정리하지만 1분 상한은 보장하지 않는다. API 요청도 해당 매치의 메타 기한을 확인해 만료 상태 전이를 시도한다. [EventBridge 일정 정밀도](https://docs.aws.amazon.com/eventbridge/latest/userguide/eb-create-rule-schedule.html)

각 후보 처리 순서:

1. 기본 테이블에서 매치 `META`와 네 `MEMBER`를 일관 읽기로 확인한다. 인덱스에서 읽은 오래된 기한이나 이미 종료된 상태는 건너뛴다. 실제 `deadlineAtMs <= now`를 다시 검사한다.
2. 네 PUID의 `ACTIVITY` 가드를 일관 읽기로 확인한다. 모든 가드가 같은 `matchId`를 가리키는지 검사한다. 불일치는 새 사용자의 가드를 삭제하지 않고 별도 경보·조사 대상으로 남긴다.
3. 한 `TransactWriteItems`에서 메타를 `EXPIRED`로 갱신하고 GSI 키를 제거하며 네 가드를 삭제한다. 네 `MEMBER` 항목과 네 `ASSIGNED` 티켓에도 종료 시각 기준 7일 TTL을 설정한다. 메타 조건은 `status`, `revision`, `deadlineAtMs <= now`다. 각 가드 삭제 조건은 `kind=MATCH`, `matchId`, `revision`이다. 멤버·티켓 갱신도 해당 매치 ID를 조건으로 한다. 총 13개 항목이며, 중복 작업 없이 한 트랜잭션으로 처리한다.
4. 다른 API나 워커가 먼저 상태를 바꿔 조건이 실패하면 메타·가드를 재조회한다. 이미 종료됐으면 성공한 중복 처리로 간주한다. 여전히 기한이 지났으면 제한된 횟수로 재시도하고 실패를 알람·DLQ 대상으로 남긴다. 응답이 불명확할 때 다른 종료 ID를 만들거나 가드를 단독 삭제하지 않는다.

이 트랜잭션에서 다른 사람의 `WAITING` 티켓은 다루지 않는다. 각 `MATCH` 가드는 해당 매치 ID와 revision을 조건으로 삭제하므로, 오래된 복구 호출이 새로운 매치 가드를 지울 수 없다. DynamoDB 트랜잭션은 최대 100개 고유 항목, 총 4MB이고 한 항목에 두 작업을 넣을 수 없으므로 13개 작업은 개수 기준을 충족한다. [트랜잭션 제약](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/Constraints.html)

## 6. 정상 종료와 API 간 경쟁

호스트의 `finished`, 멤버의 `left`, 기한 워커의 `EXPIRED`는 모두 **동일한 종료 함수**를 사용하고 메타 상태·revision을 조건으로 경쟁한다. 승자 하나가 메타를 종료 상태로 만들고 네 가드를 동시에 해제한다. 패자는 메타를 다시 읽어 이미 끝난 결과를 반환한다. 종료 후에는 `GET /v1/matchmaking/tickets/current`가 `ticket:null`을 반환하고, 새 티켓 생성이 가능하다.

`FORMING`·`SESSION_READY`·`READY_TO_START`·`IN_GAME`으로 기한을 연장하는 전이도 메타 revision과 네 가드의 매치 ID·revision을 조건으로 원자적으로 갱신한다. 워커가 먼저 종료한 뒤에 늦은 세션·시작 보고가 매치를 되살릴 수 없다. `joined`는 멤버 항목의 한 번만 기록되는 참가 상태와 메타의 참가 수를 같은 트랜잭션에서 갱신하고, 4번째 보고가 `READY_TO_START` 및 새 기한을 설정한다. 동시 보고가 메타 revision에 충돌하면 최신 수를 다시 읽고 재시도한다.

## 7. 관측·검증·배포 경계

- 지표: 기한 후보 수, 실제 만료 수, 이미 처리된 후보 수, 조건 충돌 수, 복구 실패 수, 가장 오래 기한을 넘긴 활성 매치의 지연. 로그에는 토큰·Secret·PUID 원문·EOS 세션 ID를 남기지 않는다.
- 테스트: 인덱스 지연·중복 주기 호출, 종료 보고와 워커 경합, 4개 중 한 가드 불일치, 새 매치의 가드 보존, 네 가드 원자 해제, `READY_TO_START` 기한 갱신, 종료 상태의 인덱스 제거를 검증한다.
- 순서: `DeadlineIndex` 추가 및 활성 상태 전이 코드 → 복구 워커 → 4인 배정 워커 → 제한된 테스트 환경에서 네 계정 검증 → API·워커 동시 배포. 기존 티켓 API를 워커 없이 단독 배포하지 않는다. 현재 티켓 생성 코드는 `MATCH` 가드가 기한을 지나도 `ALREADY_ACTIVE`로 거절하므로, 워커 장애 시 재매칭은 복구가 완료될 때까지 막힌다. 매치 조회·티켓 생성 경로에도 해당 매치의 만료 확인 및 제한된 복구 호출을 추가한다.
- 이전 자료는 현재 초안이며 배포 계약이 아니다. 실제 배포 전에 `Matches` 인덱스 생성 완료, 스택 변경 세트, Docker/SAM 빌드, 4계정 실제 흐름을 확인한다.

## 8. 승인 전 확인할 제품 결정

1. 첫 버전에서 `FORMING`·`SESSION_READY`·`READY_TO_START` 각각 60초, `IN_GAME` 최대 30분을 적용할지.
2. 경기 중 `left`를 매치 전체 `FAILED`로 처리할지. 기존 설계 초안은 이 정책이나 실제 게임의 재접속 허용 여부가 확인되지 않았다.
3. 경기 종료를 호스트의 보고로 확정할지. 현재 서버는 실제 EOS 세션·경기 결과를 별도로 검증하지 못한다.
