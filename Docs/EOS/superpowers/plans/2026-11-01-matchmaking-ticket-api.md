# 매칭 티켓 API 구현 계획

상태 (2026-11-01): 티켓 API 코드·SAM 경로·Unreal 인계 문서 구현 완료. 전체 테스트 125개, 타입 검사, Node 빌드, `sam validate --lint` 통과. Docker 엔진 연결이 없어 `sam build --use-container`는 이번 실행에서 완료하지 못했다. 개발 AWS 스택에는 배포하지 않았다.

> **구현 담당자 안내:** `superpowers:executing-plans` 지침으로 작업을 순서대로 실행한다. 각 작업은 실패 테스트 확인 후 구현하고 체크박스로 추적한다.

**목표:** EOS 인증된 사용자가 단일 `standard` 모드의 3분 매칭 대기를 시작·조회·취소하게 한다. 취소는 본인의 `WAITING` 티켓만 변경한다.

**구조:** 기존 인증 핸들러에 티켓 경로를 연결한다. DynamoDB의 티켓·활동 가드·멱등성 기록을 트랜잭션으로 처리한다. 4인 배정 워커와 SQS 발행은 매치 복구 경로 설계 뒤 별도 계획으로 구현하므로, 이 단계의 API는 개발 스택에 단독 배포하지 않는다.

**기술:** Node.js 24, TypeScript, AWS SDK v3, DynamoDB DocumentClient, Vitest, AWS SAM.

**설계:** `Docs/EOS/superpowers/specs/2026-11-01-matchmaking-tickets-worker-design.md`.

## 공통 제약

- PUID는 EOS Connect ID Token 검증 결과에서만 얻는다. 토큰·Secret·PUID 원문을 공개 오류나 비밀 로그에 넣지 않는다.
- 모드는 `standard`, `X-Client-Version`은 `0.1.0`, 대기 기한은 생성 후 180,000ms다.
- 모든 응답에 `requestId`, 쓰기 요청에 UUID `Idempotency-Key`를 사용한다.
- `WAITING` 취소에 성공하면 본인 티켓·가드만 변경한다. 배정이 먼저 확정됐으면 `409 TICKET_ALREADY_ASSIGNED`다.
- DynamoDB TTL은 지연 삭제용이며 대기 기한 판정에는 사용하지 않는다.
- 티켓 API만 배포하면 매칭이 끝나지 않으므로 워커·매치 복구 경로가 준비될 때까지 개발 스택 배포를 보류한다.

## 검토 초점

1. 동일 사용자의 동시 티켓 생성은 하나만 성공한다.
2. 동일 멱등성 키 재시도는 원래 티켓·응답을 재현하고, 다른 본문은 거부한다.
3. 취소와 배정이 경합해도 취소가 다른 사람의 티켓·가드를 변경하지 않는다.
4. 만료된 가드를 조건부 정리하고 새 티켓을 만들되, 뒤늦은 취소가 새 가드를 건드리지 않는다.
5. DynamoDB 결과가 불명확할 때 새 ID로 무조건 재시도하지 않고 기록과 상태를 일관 읽기로 판정한다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `backend/src/matchmaking/ticket-contract.ts` | 모드·버전·UUID·입력 검증과 공개 타입 |
| `backend/src/matchmaking/ticket-store.ts` | 티켓 저장소 인터페이스와 오류 |
| `backend/src/matchmaking/dynamo-ticket-store.ts` | 일관 읽기, 트랜잭션 생성·취소·만료 처리 |
| `backend/src/app.ts`, `handler.ts`, `config.ts` | 인증 후 경로 연결과 저장소 주입 |
| `Backend-Infra/template.yaml` | 티켓 경로와 두 테이블 IAM 권한 |
| `backend/test/matchmaking/*.test.ts`, `test/app.test.ts`, `test/infra/template.test.ts` | 계약·경합·인프라 검증 |

### 작업 1: 티켓 계약과 설정

- [ ] 실패 테스트: `standard`/`0.1.0`만 허용하고 잘못된 모드·버전·UUID를 거부한다. UTC 만료 시각과 3분 상수를 검사한다.
- [ ] 집중 테스트의 의도한 실패를 확인한다.
- [ ] `ticket-contract.ts`와 설정을 구현한다. UUID v7 생성은 검증된 패키지 또는 RFC 9562 호환 유틸리티를 사용한다.
- [ ] 집중 테스트와 타입 검사를 통과시킨다.

### 작업 2: DynamoDB 티켓 저장소

- [ ] 실패 테스트: 본인 티켓·가드·24시간 멱등성 기록의 원자적 생성, 같은 키 재시도, 다른 본문 거부, 동시 생성 한 명 승리, 일관 조회, 3분 만료 가드 정리, 본인 티켓만 취소, 취소·배정 경합, 늦은 취소를 검사한다.
- [ ] 집중 테스트의 의도한 실패를 확인한다.
- [ ] `DynamoTicketStore`를 구현하고 트랜잭션 취소 후 상태 재조회로 공개 오류를 구분한다.
- [ ] 집중 테스트와 타입 검사를 통과시킨다.

### 작업 3: 인증된 HTTP API와 SAM 연결

- [ ] 실패 테스트: `POST /v1/matchmaking/tickets`, `GET/DELETE /v1/matchmaking/tickets/current`의 응답·오류, 닉네임 선행 요구, 무인증 저장소 호출 0회, 다른 사용자의 티켓 취소 불가, 토큰 비노출, SAM 경로·IAM을 검사한다.
- [ ] 집중 테스트의 의도한 실패를 확인한다.
- [ ] 기존 `app.ts`·`handler.ts`·`config.ts`와 SAM을 수정한다. 요청 본문은 크기 제한과 JSON 객체 검증을 적용한다.
- [ ] 집중 테스트, 전체 테스트, 타입 검사, 빌드, `sam validate --lint`를 통과시킨다.

### 작업 4: 검토와 기록

- [ ] API 계약·실패 흐름을 Unreal 인계 문서에 기록하고, 개발 스택 미배포 이유를 체크리스트에 명시한다.
- [ ] `git diff --check`, 전체 테스트·타입 검사·빌드 결과를 확인한다.
- [ ] 변경을 EJ 브랜치에 커밋한다. 원격 푸시는 별도 사용자 요청 또는 명시적 동기화 단계에서 수행한다.
