# 4인 매치 수명·워커 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 네 명의 대기 티켓을 하나의 매치에 원자적으로 배정하고, 세션·참가·시작·종료·기한 복구를 통해 활동 가드를 안전하게 해제한다.

**Architecture:** 기존 EOS 인증 API와 세 테이블을 유지한다. `Matches.DeadlineIndex`는 활성 메타만 포함한다. SQS는 매칭 조회 힌트이고 EventBridge 1분 호출은 누락된 대기 티켓과 기한 지난 매치를 복구한다. 모든 상태 변경은 기본 테이블의 일관 읽기 후 조건부 트랜잭션으로 확정한다.

**Tech Stack:** Node.js 24, TypeScript, AWS SDK v3, DynamoDB, SQS, EventBridge, AWS SAM, Vitest.

**Spec:** `docs/superpowers/specs/2026-11-01-matchmaking-tickets-worker-design.md`, `docs/superpowers/specs/2026-11-01-match-deadline-recovery-design.md`.

## Global Constraints

- `standard` 4인, 정확한 버전 `0.1.0`, 티켓 최대 3분.
- `FORMING`, `SESSION_READY`, `READY_TO_START` 각각 최대 60초; 보고가 완료되면 즉시 전이. `IN_GAME` 최대 30분.
- 이탈 보고는 전체 `FAILED`; 호스트 종료 보고는 `FINISHED`; 강제 종료·무응답은 기한 복구로 `EXPIRED`.
- `WAITING` 취소는 본인 티켓만 변경한다. `ASSIGNED` 이후에는 기존 매칭 취소 경로를 사용하지 않는다.
- PUID는 검증된 EOS 토큰에서만 얻고, 모든 응답에 `requestId`, 모든 쓰기에 UUID `Idempotency-Key`를 둔다.
- 토큰·Secret·PUID 원문·EOS 세션 ID를 로그에 남기지 않는다.
- 워커와 복구가 완성되기 전 티켓 API를 개발 스택에 단독 배포하지 않는다.

## Review Focus

1. 두 워커가 같은 네 티켓을 잡아도 매치 하나만 만들어야 한다.
2. 취소·만료·배정이 동시에 일어나도 다른 사람의 가드를 건드리면 안 된다.
3. 네 번째 참가 보고가 다른 참가 보고와 겹쳐도 정확히 한 번 `READY_TO_START`가 된다.
4. 종료·기한 복구가 경쟁해도 새 매치 가드를 지우면 안 된다.
5. GSI가 늦게 반영되거나 같은 SQS 메시지가 반복되어도 원본 상태가 최종 판정이어야 한다.

---

### Task 1: 데이터 계약과 SAM 인덱스

**Files:** `Backend-Infra/template.yaml`, `Backend-Infra/backend/src/matchmaking/match-contract.ts`, `Backend-Infra/backend/test/infra/template.test.ts`, `Backend-Infra/backend/test/matchmaking/match-contract.test.ts`.

**Interfaces:** `MatchStatus`, `phaseDurationMs(status)`, `deadlineIndexKeys(matchId, deadlineAtMs)`.

- [ ] 실패 테스트: 16개 버킷, 60초·30분, 활성 메타에만 들어갈 인덱스 키, `DeadlineIndex` 키/`KEYS_ONLY` 투영을 검사한다.
- [ ] 집중 테스트에서 의도한 실패를 확인한다.
- [ ] 계약 함수와 SAM GSI를 구현한다.
- [ ] 집중 테스트, 타입 검사, `sam validate --lint`를 통과시킨다.

### Task 2: 매치 종료와 기한 복구 저장소

**Files:** `Backend-Infra/backend/src/matchmaking/match-store.ts`, `.../dynamo-match-store.ts`, `.../match-recovery.ts`, 대응 테스트.

**Interfaces:** `MatchStore.get(matchId)`, `MatchStore.finish({matchId,actorId,kind,idempotencyKey,requestHash,requestId})`, `MatchStore.expire(matchId,now)`, `MatchStore.due(now,cursor)`.

- [ ] 실패 테스트: 메타·4가드·4멤버·4배정 티켓의 13개 원자적 종료, 다른 매치 가드 보존, 중복 종료, GSI stale 후보, 기한 전 후보 무시를 검사한다.
- [ ] 집중 테스트의 의도한 실패를 확인한다.
- [ ] 일관 읽기·조건부 트랜잭션·GSI 조회를 구현한다. 종료 보고의 멱등성 기록을 같은 트랜잭션에 추가할 때 총 14개 작업을 유지한다.
- [ ] 집중 테스트와 타입 검사를 통과시킨다.

### Task 3: 매치 상태 API

**Files:** `Backend-Infra/backend/src/app.ts`, `handler.ts`, `config.ts`, `match-store.ts`, `dynamo-match-store.ts`, `template.yaml`, 대응 테스트.

**Interfaces:** `GET /v1/matches/{matchId}`, `POST /v1/matches/{matchId}/session|joined|started|finished|left`.

- [ ] 실패 테스트: 멤버만 조회, 호스트만 세션·시작·종료 보고, 참가 4명 완료 즉시 상태 전이, 상태별 60초/30분 기한, 이탈 전체 실패, 멱등성 재시도, 인증 실패 시 저장소 호출 0회.
- [ ] 의도한 실패를 확인한다.
- [ ] 인증 후 권한·상태를 검증하고 조건부 트랜잭션으로 전이한다. 호출별 본문 크기 제한과 공개 오류를 적용한다.
- [ ] 집중 테스트, 전체 테스트, 타입 검사, 빌드를 통과시킨다.

### Task 4: 4인 배정 워커와 SQS 힌트

**Files:** `Backend-Infra/backend/src/matchmaking/assignment-worker.ts`, `.../dynamo-assignment-store.ts`, `handler.ts`, `template.yaml`, 대응 테스트.

**Interfaces:** SQS 메시지 `{mode:"standard",clientVersion:"0.1.0"}`와 1분 주기 재조회; 트랜잭션 4 티켓+4 가드+메타+4 멤버.

- [ ] 실패 테스트: 16개 GSI 버킷 병합, 유효한 오래된 네 명 선택, 네 원본 일관 읽기, 13개 작업 원자성, 중복 SQS/워커 경합, 취소·만료 후보 교체.
- [ ] 의도한 실패를 확인한다.
- [ ] SQS 힌트 발행, 워커·주기 복구, DLQ·최소 권한 IAM을 구현한다.
- [ ] 집중 테스트, 전체 테스트, 타입 검사, 빌드, `sam validate --lint`를 통과시킨다.

### Task 5: 제한된 통합 검증과 인계

**Files:** `docs/unreal-matchmaking-ticket-api.md`, `Backend-Infra/README.md`, 통합 스모크 스크립트와 필요한 테스트.

- [ ] Unreal에 매치 API·클라이언트 보고의 신뢰 범위·실패 UI를 기록한다.
- [ ] 전체 테스트, 타입 검사, Node 빌드, 컨테이너 빌드, `sam validate --lint`, `git diff --check`를 실행한다.
- [ ] 서로 다른 4개 EOS 계정으로 개발 스택에서 배정 1회와 종료·복구를 검증한다. 계정 또는 Docker가 준비되지 않았으면 로컬 검증 결과와 배포 보류 이유를 명시한다.
- [ ] EJ 브랜치에 커밋한다. 원격 푸시는 요청받은 동기화 범위에 따라 수행한다.
