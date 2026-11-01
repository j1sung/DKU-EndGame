# Unreal 매칭 대기 연동 계약 (개발 초안)

현재 서버 코드에 구현된 티켓 API 계약이다. 개발 AWS 스택에는 아직 배포하지 않았다. 4인 배정 워커와 매치 복구 경로가 연결되면 이 문서의 예제로 연동 검증한다.

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

대기 화면에서는 약 2초 간격으로 조회하고, 서버가 `Retry-After`를 보내면 그 값을 우선한다. 네트워크 오류가 반복되면 최대 5초까지 간격을 늘린다. `ASSIGNED`면 취소 버튼을 비활성화하고 `matchId`를 세션 준비 흐름에 넘긴다. 세션 준비 API는 후속 단계에서 확정한다.

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

## 연동 전 확인

- [ ] 게임 시작 버튼은 `standard`와 `0.1.0`을 전송한다.
- [ ] 재시도에서 멱등성 키와 JSON 본문을 유지한다.
- [ ] 취소 버튼은 `WAITING`에서만 표시하고 성공 시 메인 화면으로 돌아간다.
- [ ] `ASSIGNED` 또는 취소와 배정의 경합은 `matchId` 흐름으로 이동한다.
- [ ] EOS ID Token을 채팅·로그·화면에 표시하지 않는다.
- [ ] 4인 워커와 매치 종료·복구 경로가 준비된 뒤 개발 스택에서 실제 계정 4개로 검증한다.
