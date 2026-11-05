# 로비 4인 매칭 대기·취소

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위: [Network 개요](Overview.md#matchmaking) · 화면: [연습 로비](../UI/Lobby.md) · 서버 계약: [티켓·매치 API](../EOS/unreal-matchmaking-ticket-api.md)

## 현재 구현 범위

`UCFMatchmakingSubsystem`이 연습 로비 게임 시작·매칭 취소·다시 확인을 실제 HTTP 요청 코드에 연결한다. `UCFEntrySubsystem::GetBackendAccess`에서 기존 API 주소와 현재 계정의 Connect ID Token/PUID를 얻으며 인증 코드를 복제하지 않는다. 토큰은 Blueprint·UI·로그에 노출하지 않는다. `DefaultGame.ini`의 기존 `CFEntrySubsystem.ProfileApiBaseUrl`을 함께 사용한다.

**클라이언트 EOS 설정·실계정 로그인 검증은 아직 필요하다.** 이 작업의 자동화·Play 검사는 모의 응답을 사용했다. 인증된 AWS HTTP, 서로 다른 실제 계정 4개의 Unreal 매칭, EOS 세션 생성·참가와 패키징은 미검증이다. EOS 세션 연결 자체는 후속 구현이며, 현재는 배정·역할·매치 상태를 읽는 단계까지다.

## 버튼과 서버 요청

1. 인증된 로비 진입 시 `GET /v1/matchmaking/tickets/current`로 기존 대기/배정을 복구한다. 인증 없이 맵을 직접 Play하면 연습을 허용하고, 게임 시작을 누를 때 로그인 화면으로 이동한다.
2. 게임 시작도 먼저 current를 확인한다. 활성 상태가 없고 시작 의도가 유지될 때만 `POST /v1/matchmaking/tickets`를 보낸다. 본문은 `{"mode":"standard"}`, 버전은 `X-Client-Version: 0.1.0`이다.
3. POST 응답 이후 다시 current를 조회한다. 멱등성 캐시의 이전 WAITING 응답을 현재 상태로 오인하지 않는다. WAITING이면 남은 시간과 취소 버튼을 표시한다.
4. 약 2초마다 current를 조회한다. 오류가 반복되면 2/4/5초로 늘리고, `Retry-After`의 초/HTTP 날짜를 우선한다. 수동 다시 확인도 이 제한을 우회하지 않는다.
5. 취소 의도와 티켓 ID가 확보되면 `DELETE /v1/matchmaking/tickets/current`에 `{"ticketId":"..."}`를 보낸다. current로 종료를 확인한 뒤 빠른 대전 패널을 복원한다. 타이틀 복귀 의도였다면 그때 맵을 이동한다.
6. ASSIGNED면 취소·타이틀 복귀를 차단하고 `GET /v1/matches/{matchId}`로 네 구성원·호스트·상태·기한을 확인한다. 서버의 FINISHED/FAILED/EXPIRED가 확인되어야 새 시작을 허용한다. 자동 재매칭은 없다.

현재 API에는 대기열 인원 수가 없으므로 `2/4` 등의 가짜 진행 인원은 표시하지 않는다. 대기 시간은 서버 expiresAt와 응답 Date를 기준으로 계산하고 로컬 단조 시계로 표시한다. Date가 없으면 UTC를 사용한다. 시간 0은 서버 취소/만료의 확정이 아니다.

## 응답 유실·경합·인증

- FlowId와 HTTP Serial을 모두 검사하며 한 번에 HTTP 요청 하나만 유지한다. 화면 종료 후 늦은 응답은 무시한다.
- POST/DELETE에는 동작별 UUID 멱등성 키를 사용한다. 불명확한 재시도는 **같은 키·같은 본문**을 유지한다.
- POST 응답이 유실된 후 current가 null이어도 즉시 종료하지 않는다. 같은 POST를 재확인한 뒤 current를 다시 읽어 지연 완료와 캐시된 응답을 구분한다. 생성 중 취소를 눌렀다면 티켓을 얻는 대로 취소한다.
- DELETE 응답 유실, TICKET_MISMATCH, TICKET_NOT_FOUND는 current로 재확인한다. TICKET_ALREADY_ASSIGNED 또는 ASSIGNED가 먼저 확정되면 배정 흐름을 따른다.
- ALREADY_ACTIVE는 기존 상태 복구, CLIENT_VERSION_UNSUPPORTED는 업데이트 안내로 처리한다. 알 수 없는/잘못된 응답과 통신 오류는 종료 성공으로 간주하지 않는다.
- 401·토큰 확보 실패는 로그인 화면으로 돌아간다. NICKNAME_REQUIRED도 진입 절차로 보내 프로필 재조회 후 닉네임 등록을 진행한다. 재인증 전 취소·타이틀 복귀 의도는 같은 계정에서 복구한다. 다른 PUID로 로그인하면 이전 계정의 티켓·멱등성 키를 재사용하지 않는다.
- `ResetForEntry()`는 로컬 표시 상태만 해제한다. 인증 소실/맵 종료가 서버 취소 성공을 뜻하지 않으며 다음 인증된 로비 진입에서 다시 조회한다. 프로세스 종료 후에는 서버 current 복구에 의존하고 토큰·멱등성 키를 디스크에 저장하지 않는다.

## 다음 EOS 세션 계층의 연결점

`Assignment`와 `OnAssignmentChanged`에 검증한 `MatchId`, `Status`, 네 `MemberIds`, `HostPlayerId`, `HostAttempt`, `SessionId`, `DeadlineAt`, `bIsHost`를 제공한다. `bIsHost`는 현재 인증 PUID와 서버 HostPlayerId를 비교하며 Unreal의 Actor LocalRole과 별개다. 첫 계약의 hostAttempt는 1이다.

- FORMING: 지정 호스트의 실제 EOS 세션 생성·listen 시작·session 등록을 연결할 위치.
- SESSION_READY: 실제 세션 검색·참가·Travel 및 joined 보고를 연결할 위치.
- READY_TO_START: 실제 경기 시작과 started 보고를 연결할 위치.
- IN_GAME 복구: 첫 버전은 경기 중 재접속을 지원하지 않으므로 이미 진행 중임을 알리고 종료를 확인한다.

현재 `/session`, `/joined`, `/started`, `/finished`, `/left`는 호출하지 않는다. 따라서 실제 배정이 되어도 지금 코드만으로 경기가 시작되지는 않으며 준비 기한 만료가 확인되면 다시 매칭할 수 있다. 세션 실패/이탈 보고와 경기 맵 이동도 다음 통합 범위다. 기존 LAN 리슨 서버 경로는 독립적으로 유지한다.

## EOS 없이 직접 화면 확인하기

1. 최신 Editor 빌드로 프로젝트를 열고 `/Game/Map/L_Lobby_Playground`에서 **Play → Selected Viewport** 또는 **New Editor Window (PIE)**를 실행한다.
2. Play를 유지한 채 Output Log의 명령어 입력칸에 `CF.Matchmaking.Mock`를 입력한다.
3. **`CF_MATCHING_MOCK_ENABLED`** 로그가 나오면 현재 Play에 적용된 것이다. 화면 디버그 메시지가 켜져 있으면 초록색 `Mock matchmaking ON (Editor test only)` 안내도 표시한다. Play 후 얼마가 지나도 실행할 수 있다.
4. 첫 번째 게임 시작은 모의 대기, 취소는 빠른 대전 복귀다. 두 번째 게임 시작은 모의 4인 배정·연결 준비·만료를 확인한다. 실제 EOS/AWS는 호출하지 않는다.
5. Stop으로 Play를 종료하면 모의 설정이 사라진다. 같은 Play에서 명령을 반복해도 진행 중인 모의 매칭을 초기화하지 않는다.

이 명령은 Editor + 개발 자동화 테스트 빌드의 현재 로비 PIE에만 적용된다. 별도 Standalone Game 프로세스·Shipping에서는 제공하지 않는다. 로비 Play가 없거나 대상이 여러 개면 적용하지 않고 이유를 로그에 표시한다. 진행 중이거나 결과가 불명확한 **실제** 매칭을 모의 응답으로 덮어쓰지 않는다.

이전 `Automation RunTests ChickenFight.Matchmaking.InstallPIEFixture`는 자동화 도구용으로 유지한다. Success/ARMED는 준비 성공이며, 30초 내 유효한 다음 로비 PIE에 적용된 경우 `CF_MATCHING_FIXTURE_APPLIED`, 시간이 지났다면 `CF_MATCHING_FIXTURE_EXPIRED`를 기록한다. 종료 중인 월드에는 설치하지 않는다. 수동 확인에는 위 `CF.Matchmaking.Mock`를 사용한다.

## 코드와 검증

- 제공자: `Source/DKUEndGame/Network/CFMatchmakingSubsystem.h/.cpp`.
- 인증 재사용: `CFEntrySubsystem.h/.cpp`의 native-only 접근 함수.
- 화면 수명: `CFLobbyController`의 로비 진입/종료·로그인 이동, `CFLobbySubsystem`의 표시 초기화.
- 자동화: `Source/DKUEndGame/Tests/CFMatchmakingTests.cpp`, 필터 `ChickenFight.Matchmaking.StateMachine`. 기존 `ChickenFight.Entry`는 인증 회귀 검사다.
- 모의 HTTP 설치는 Editor 전용 `CF.Matchmaking.Mock` 명령 또는 자동화 fixture를 사용한다. 기본 실행·제품/Shipping에는 모의 성공을 연결하지 않는다. 자동화 회귀 검사는 `ChickenFight.Matchmaking.StateMachine` 필터를 사용한다.

2026-11-05 UE 5.8.2 Editor 빌드, `ChickenFight.Matchmaking.StateMachine`, 기존 `ChickenFight.Entry.ProfileResponses` 통과. 960×640 PIE에서 서로 다른 20개 항목(화면 캡처 포함)을 확인했다. 실제 버튼으로 시작→대기→취소→재시작, 배정 시 취소/복귀 차단, 서버 매치 만료 후 Idle, 미로그인 시작 시 실제 타이틀 Login 이동, 대기 중 기존 이동을 확인했다. HTTP 응답과 네 구성원 배정은 C++ fixture가 제공했으며 실제 네 계정 통신 성공을 뜻하지 않는다. 수동 활성화 보완 후 Editor 재빌드·StateMachine 회귀 검사와 25개 PIE 확인 항목도 통과했다. Play 후 40초를 기다린 다음 Editor Output Log와 같은 월드 경로로 `CF.Matchmaking.Mock`를 실행하여 현재 Play 유지·대기·취소·배정·반복 명령의 상태 보존·종료 후 해제를 확인했다. Blueprint 에셋·캐릭터 이동·전투/애니메이션 구현은 이 작업에서 수정하지 않았다.
