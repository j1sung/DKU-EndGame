# 로비 자동 매칭 티켓 연결

> 2026-11-06 · Codex 수행 · 요청자 GitHub ID 확인 필요
>
> 상위: [Network 개요](Overview.md#matchmaking-tickets) · 계약: [매칭 API](../EOS/unreal-matchmaking-ticket-api.md) · UI: [연습 로비](../UI/Lobby.md)

## 구현 범위

로그인과 닉네임 확인 후 연습 로비의 빠른 대전 버튼으로 실제 개발 API에 티켓을 생성·조회·취소한다. `standard` 4인 단일 모드, `X-Client-Version: 0.1.0`, 대기 최대 180초다. 대기 인원 수는 API에 없으므로 표시하지 않는다.

`ASSIGNED`이면 취소와 일반 타이틀 복귀를 막고 `matchId`를 보관한다. `UCFMatchmakingSubsystem::OnMatchAssigned`를 GameInstance 초기화에서 바인딩한 `UCFMatchSessionSubsystem`이 `ClaimAssignment`로 소유권을 넘겨받는다. 인수 후 티켓 폴링·티켓 복구 요청은 중단하며 매치 API·실제 EOS 연결은 세션 계층만 담당한다. 서버 종료와 로컬 세션 정리를 확인하면 `CompleteAssignment`로 로비의 빠른 대전을 다시 허용한다. 가짜 참가·시작 보고를 보내지 않는다. [Round1 연결 코드·테스트 절차](EOSRound1Session.md). 새 EOS 경로의 실계정 검증은 아직 수행하지 않았다.

## 소유권과 코드

| 위치 | 책임 |
|---|---|
| `Network/CFMatchmakingFlow.h/.cpp` | 네이티브 티켓 흐름·응답 검증·쓰기 키·취소 의도·재시도 결정 |
| `Network/CFMatchmakingSubsystem.h/.cpp` | GameInstance 제공자, Entry/Lobby 바인딩, HTTP 1개와 폴링 타이머 1개, 계정·FlowId·HTTP 순서 확인 |
| `Network/CFEntrySubsystem.h/.cpp` | 네이티브 `GetMatchmakingContext`, 현재 Connect 토큰 복사와 프로필 확인, 인증 만료 알림 |
| `UI/CFLobbySubsystem.h/.cpp` | 기존 표시 상태, 취소 의도, 인증 만료 안내와 계정 전환 시 표시 흐름 분리 |
| `UI/CFLobbyWidget.cpp` | 기존 Designer 위젯의 인증 만료 문구·로그인 복귀 버튼 표시 |
| `Tests/CFMatchmakingTests.cpp` | `ChickenFight.Matchmaking.TicketFlow`, `ChickenFight.Matchmaking.ResponseGuards` |

기존 HTTP·JSON·EOS 모듈을 사용하며 새 플러그인·프레임워크·Blueprint 에셋 변경은 없다. GameInstance 초기화에서 Entry와 Lobby 의존성을 먼저 만들고 로비 컨트롤러의 `RestoreOnEntry` 전에 제공자를 바인딩한다.

## 요청과 복구

1. 로비 진입 시 `GET /v1/matchmaking/tickets/current`로 이전 대기/배정을 복구한다. 서버가 활성 티켓 없음을 확인하기 전에는 새 시작을 막는다.
2. 시작은 `POST /v1/matchmaking/tickets`, 본문 `{"mode":"standard"}`. 생성 성공 또는 `ALREADY_ACTIVE` 뒤에는 현재 상태를 조회한다. 생성 멱등 응답의 과거 WAITING을 그대로 표시하지 않는다.
3. 정상 조회 간격은 2초다. 남은 시간은 응답의 `Date`를 기준으로 `expiresAt`와 비교하며 Date가 없거나 잘못됐으면 UTC 로컬 시각을 쓴다. 0초가 되어도 서버 종료 확인 전 새 시작을 막는다.
4. 취소는 `DELETE current`에 현재 티켓 ID를 보낸다. 시작 중 누른 취소는 티켓을 확인한 뒤 전달한다. 취소 확인 전에는 화면을 Idle로 바꾸지 않는다. 취소는 해당 계정의 대기에만 적용된다.
5. `TICKET_ALREADY_ASSIGNED`, `TICKET_MISMATCH`, `TICKET_NOT_FOUND`이면 현재 상태를 다시 조회한다. ASSIGNED가 확정되면 취소 의도를 버린다. 취소 성공의 오래된 멱등 응답도 GET current로 재확인한다.
6. HTTP 기한은 12초다. 실패 시 재시도 간격은 3초, 5초로 늘리고 연속 3회 실패하면 `다시 확인`을 기다린다. 숫자/HTTP 날짜 형식의 `Retry-After`가 있으면 우선 적용하고 수동 복구·취소도 그 시각보다 일찍 보내지 않는다.
7. 생성·취소 응답 유실/5xx/잘못된 성공 응답은 같은 UUID 키와 동일 본문으로 복구한다. 미확정 생성 결과를 GET의 `ticket:null`만으로 덮지 않는다. 상태 확인을 못한 채 새 키로 자동 재매칭하지 않는다.
8. `426`은 업데이트 안내, 닉네임 미등록은 프로필 캐시를 무효화하고 재로그인/등록으로 안내한다. 서버 원문 오류는 사용자에게 전달하지 않는다.

## 인증 만료와 계정 전환

각 요청 직전에 현재 계정의 EOS Connect ID Token을 복사한다. 토큰은 네이티브 HTTP 헤더에만 사용하고 UI·로그·파일로 전달하지 않는다. 로그 `LogCFMatchmaking`에는 FlowId·동작 종류·HTTP 코드만 남긴다.

401 또는 Entry의 인증 상실 알림은 HTTP·타이머를 중단하고 로비를 `AuthenticationRequired`로 만든다. 이 상태의 로그인 화면 복귀는 **서버 매칭 취소 완료를 뜻하지 않는다.** 같은 계정으로 재로그인하면 미확정 쓰기 키/본문을 유지해 복구하고, 다른 계정이면 과거 티켓·키를 보내지 않고 새 계정의 current만 조회한다. 이전 계정의 매칭은 서버 정책에 따라 남거나 만료될 수 있다.

키와 취소 의도는 GameInstance 메모리에만 유지한다. 프로세스 종료 때 DELETE나 전체 매치 이탈을 자동 호출하지 않는다. 다시 실행하면 current 조회로 복구하며 백엔드의 활성 작업 제한과 만료를 따른다.

## 테스트 순서

Unreal 설치 PC에서 이 브랜치의 Editor 대상을 재빌드하고 기존 `Start-EOSLoginTest.ps1`로 실행한다. 실제 SDK 자격 증명은 기존 Git 제외 로컬 설정을 사용한다. 먼저 조직 멤버 1계정으로 확인한다.

- 로그인 → 로비 초기 조회 → 빠른 대전 → 대기 시간 표시 → 매칭 취소 → 로비 빠른 대전 복구.
- 시작 직후 취소, 대기 중 타이틀 복귀(서버 취소 확인 후 이동).
- 최대 3분 대기 후 서버 current의 종료 확인 → 자동 재시작 없음.
- 일시 통신 끊김/응답 유실 시 새 매칭 차단과 `다시 확인` 복구.
- 인증 만료 → 로그인 화면 복귀 → 같은 계정 복구, 다른 계정으로 전환 시 과거 DELETE 미전송.

Unreal Editor의 Automation 창에서 `ChickenFight.Matchmaking` 검사를 실행한다. 테스트는 모의 응답을 네이티브 흐름에 전달하며 실계정 HTTP를 실행하지 않는다. 이후 4계정으로 배정·취소 경합·취소 차단을 확인한다. 실제 EOS 세션·자동 시작 검사는 [별도 절차](EOSRound1Session.md)를 따른다. 테스트를 위해 새 프로필 닉네임을 임의 등록하거나 AWS 인프라를 변경하지 않는다.

현재 PC에는 Unreal과 C++ 빌드 도구가 없어 Editor 빌드·위 자동화 검사·실계정 매칭 UI 실행은 미확인이다. 기존 백엔드 4계정 성공 기록과 이번 Unreal 클라이언트 검증을 구분한다. 사용자는 2026-11-06 실제 로그인부터 연습 로비 진입 성공을 보고했으며 이 성공 범위에는 이번 매칭 연결이 포함되지 않는다.
