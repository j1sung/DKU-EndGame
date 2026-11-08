# Unreal 매칭·매치 연동 계약 (개발 초안)

게임 화면·게임플레이 담당자의 작업 순서와 UI 항목은 [언리얼 게임 클라이언트 인계서](unreal-client-handoff.md)를 참고한다.

`Backend-Infra/`의 서버 코드와 2026-11-02 개발 AWS 스택에 배포된 매칭·매치 API 계약이다. 서로 다른 Epic 계정 4개로 인증·배정·취소·상태 보고·기한 복구를 검증했다. 세션·참가·시작·종료 보고는 테스트 도구의 모의 호출이며 실제 EOS 세션과 Unreal UI 연동은 후속 검증 대상이다. `dev`의 기존 LAN 방 생성·수동 시작은 테스트 구현이며 이 API와 아직 연결되지 않았다.

2026-11-06 별도 브랜치에 [Unreal 로비 티켓 제공자](../Network/MatchmakingTickets.md)를 구현했다. 시작·current 조회·배정 전 취소의 코드 연결이며 Unreal 빌드·실계정 UI 실행은 미확인이다. 실제 EOS 세션 API·매치 상태 보고와 LAN 경로는 이번 범위에 포함하지 않았다.

## 공통 요청

- 모든 요청: `Authorization: Bearer <EOS Connect ID Token>`. 토큰은 `EOS_Connect_CopyIdToken`으로 받고 만료 전에 갱신한다. PUID는 서버가 토큰에서 확인한다.
- 쓰기 요청: `Idempotency-Key: <UUID>`. 한 번의 사용자 동작에 키 하나를 생성하고, 네트워크 재시도에는 같은 키와 같은 JSON 본문을 그대로 쓴다.
- 매칭 시작 요청: `X-Client-Version: 0.1.0`. 첫 버전은 `standard` 4인 모드 하나다.
- 모든 응답에 `requestId`가 있다. 오류에는 `error.code`가 있다. 토큰·비밀번호를 로그에 기록하지 않는다.

## 시작

```http
POST /v1/matchmaking/tickets
Authorization: Bearer <token>
X-Client-Version: 0.1.0
Idempotency-Key: 123e4567-e89b-42d3-a456-426614174000
Content-Type: application/json

{"mode":"standard"}
```

성공 `201`: `{"ticketId":"<UUID v7>","status":"WAITING","expiresAt":"<UTC ISO 8601>","requestId":"..."}`. 3분 안에 4명이 모이면 배정된다. 3분이 지나면 대기를 끝내며 자동 재시작하지 않는다. 닉네임이 없다면 `409 NICKNAME_REQUIRED`; 프로필 등록 화면으로 안내한다. 이미 티켓이나 매치가 있다면 `409 ALREADY_ACTIVE`이며 현재 상태를 조회한다. 버전이 다르면 `426 CLIENT_VERSION_UNSUPPORTED`다.

## 현재 상태

```http
GET /v1/matchmaking/tickets/current
Authorization: Bearer <token>
```

- 대기 중 `200`: `{"ticketId":"...","status":"WAITING","expiresAt":"...","requestId":"..."}`
- 4명 배정됨 `200`: `{"ticketId":"...","status":"ASSIGNED","matchId":"...","requestId":"..."}`
- 활성 대기 없음 `200`: `{"ticket":null,"requestId":"..."}`

대기 화면에서는 약 2초 간격으로 조회하고, 서버가 `Retry-After`를 보내면 그 값을 우선한다. 네트워크 오류가 반복되면 최대 5초까지 간격을 늘린다. `ASSIGNED`면 취소 버튼을 비활성화하고 `matchId`를 세션 준비 흐름에 넘긴다.

## 4명 배정 전 취소

```http
DELETE /v1/matchmaking/tickets/current
Authorization: Bearer <token>
Idempotency-Key: 223e4567-e89b-42d3-a456-426614174000
Content-Type: application/json

{"ticketId":"<현재 티켓의 UUID v7>"}
```

성공 `200`: `{"ticketId":"...","status":"CANCELLED","requestId":"..."}`. 본인만 대기열에서 빠지고 메인 화면으로 돌아간다. 다른 참가자의 대기는 계속된다. `409 TICKET_ALREADY_ASSIGNED`는 배정이 먼저 확정된 경우다. `GET current`로 `matchId`를 확인하고 세션 준비 화면으로 이동한다. `409 TICKET_MISMATCH`는 오래된 티켓 ID로 취소를 시도한 경우이므로 현재 상태를 다시 조회한다. `404 TICKET_NOT_FOUND`는 취소 가능한 현재 티켓이 없다는 뜻이다.

시작 응답이 아직 오지 않았을 때 취소를 누르면, 취소 의도를 로컬에 저장한다. 시작 응답이나 `GET current`로 티켓 ID를 얻는 즉시 취소 요청을 보낸다. 늦게 도착한 시작 응답이 취소된 화면을 다시 대기 화면으로 바꾸지 않게 한다.

## 배정 뒤 매치 진행

`ASSIGNED` 응답의 `matchId`로 `GET /v1/matches/{matchId}`를 조회한다. 멤버에게만 매치 상태, 네 구성원 ID, 호스트 ID, `hostAttempt`, 세션 ID(등록 후), 현재 단계 기한이 반환된다. 호스트만 세션 등록·시작·완료를 보고할 수 있고, 네 구성원은 각각 참가 또는 이탈을 보고할 수 있다.

| 현재 상태 | 클라이언트 동작 | 서버에 보내는 보고 | 다음 상태 |
|---|---|---|---|
| `FORMING` | 지정 호스트가 EOS 세션 생성 | `POST /v1/matches/{matchId}/session` 본문 `{"sessionId":"...","hostAttempt":1}` | `SESSION_READY` |
| `SESSION_READY` | 네 명이 EOS 세션 참가 | 각자 `POST /v1/matches/{matchId}/joined` 본문 `{"hostAttempt":1}` | 네 번째 보고 즉시 `READY_TO_START` |
| `READY_TO_START` | 호스트가 게임 시작 | `POST /v1/matches/{matchId}/started` 본문 `{"hostAttempt":1}` | `IN_GAME` |
| `IN_GAME` | 호스트가 경기 완료 | `POST /v1/matches/{matchId}/finished` 본문 `{"hostAttempt":1}` | `FINISHED` |
| 준비 또는 경기 중 | 구성원 한 명이 이탈 | `POST /v1/matches/{matchId}/left` 본문 `{"hostAttempt":1}` | 전체 `FAILED` |

각 POST에는 공통 인증 헤더와 동작별 새 UUID `Idempotency-Key`가 필요하다. `FORMING`, `SESSION_READY`, `READY_TO_START`는 각각 최대 60초이며 필요한 보고가 끝나면 즉시 다음 단계로 넘어간다. `IN_GAME`은 시작 뒤 최대 30분이다. 기한이 지나면 서버가 `EXPIRED`로 복구하고 네 명이 다시 매칭할 수 있게 한다. 기한은 정리 자격이 생기는 시각이며 주기 워커의 실제 정리는 지연될 수 있다. 상태 조회에서 기한이 지났다면 서버가 복구를 시도한다.

서버는 인증된 클라이언트의 EOS 세션 생성·참가·시작·완료 보고를 첫 버전의 상태 근거로 사용한다. 실제 EOS 세션 존재와 게임 결과를 서버가 독립적으로 증명하지는 않는다. `FAILED`·`EXPIRED` 후 자동 재매칭은 하지 않는다. UI에서 결과를 안내하고 사용자가 다시 시작하도록 한다.

## 연동 전 확인

- [ ] 게임 시작 버튼은 `standard`와 `0.1.0`을 전송한다.
- [ ] 재시도에서 멱등성 키와 JSON 본문을 유지한다.
- [ ] 취소 버튼은 `WAITING`에서만 표시하고 성공 시 메인 화면으로 돌아간다.
- [ ] `ASSIGNED` 또는 취소와 배정의 경합은 `matchId` 흐름으로 이동한다.
- [ ] EOS ID Token을 채팅·로그·화면에 표시하지 않는다.
- [ ] 호스트·멤버별 보고 권한과 단계별 60초 제한을 UI에 반영한다.
- [x] 개발 스택에서 실제 계정 4개로 인증·배정·취소·모의 상태 보고·이탈·기한 복구를 검증한다.
- [ ] Unreal 빌드에서 실제 EOS 세션 생성·4명 참가·게임 시작·종료를 검증한다.
