# Network 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위와 흐름

이 문서는 LAN 기반 테스트 세션과 인증·연습 로비 진입·매칭 티켓·EOS Round1 연결을 설명한다. 제품 목표인 EOS 자동 4인 배정·세션 연결은 [클라이언트 인계서](../EOS/unreal-client-handoff.md)와 [API 계약](../EOS/unreal-matchmaking-ticket-api.md)을 따른다. EOS Round1 연결 코드의 UE 5.8.4 Editor 빌드와 EOS 자동 검사 2개는 통과했으며 실계정 4인 검증은 아직 수행하지 않았다.

LAN 개발 메뉴 → 방 생성/참가 → 걷기 대기 → 경기·라운드 결과 → 최종 결과 → 같은 방 걷기 대기. 퇴장·연결 오류는 메뉴로 복귀한다. 2026-11-04부터 기본 진입은 새 타이틀이며, LAN 검증에는 F9 또는 `-CFLanMenu`를 사용한다. `CFFrontendSubsystem`은 별도 UI 상태 관리자이며 현재 LAN 세션을 EOS로 바꾸지 않는다. [진입 방법](../UI/MainMenu.md#기존-lan-개발-메뉴) 참고.
`OnlineSubsystemNull`을 사용하는 호스트 포함 최대 4인, 최소 시작 인원 2명의 리슨 서버 구조. Steam/EOS·인터넷 검색·NAT·호스트 이전은 포함하지 않는다.

<a id="login-entry"></a>
## 로그인·프로필·연습 로비 진입
> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`UCFEntrySubsystem`이 Epic Account Portal·EOS Connect 인증, Connect ID Token을 이용한 프로필 조회·최초 닉네임 등록, `L_Lobby_Playground` 이동을 기존 Frontend UI에 연결한다. HTTP 취소·지연 응답·계정 일치·등록 재시도와 401 재로그인을 처리한다. 기본 플랫폼 NULL과 LAN 세션 경로는 유지한다. 2026-11-06 사용자가 실제 Epic 로그인부터 연습 로비 진입 성공을 보고했다. 이 PC에서 직접 재실행한 검증과는 구분한다.
- 상세: [로그인과 연습 로비 진입](LoginEntry.md).
- 개발 EOS 접속 정보는 [로컬 설정 입력 도구](LoginEntry.md#eos-로컬-설정-먼저-준비하기)로 별도 생성한다. Unreal 없이 설정을 준비할 수 있고 실행 시 `-CustomConfig=EOSLocal`로 읽는다. 설정 준비와 실계정 인증 성공은 별도로 검증한다.
- 로그인 준비 실패는 `LogCFEntry`의 검사 결과로 구분한다. 자격 증명 원문은 기록하지 않는다. [진단 절차](LoginEntry.md#로그인-준비-실패-진단) 참고.
- 초기화 시 최종 Game 설정에서 API 주소를 명시적으로 읽고, EOS 요청 범위는 상위 목록을 비운 뒤 `BasicProfile`만 지정한다.
- API URL은 `DefaultGame.ini`에서 큰따옴표로 감싼다. 기존 따옴표 없는 값은 팀원 런타임 조회에서 `https:`로 잘려 읽히는 것이 확인됐다.

<a id="matchmaking-tickets"></a>
## 로비 자동 매칭 티켓

> 최근 기능 변경: 2026-11-06 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`UCFMatchmakingSubsystem`이 로비의 시작·취소·복구 이벤트를 실제 티켓 API에 연결하고 `FCFMatchmakingFlow`가 응답 검증·멱등 키·취소 의도를 관리한다. `standard`/`0.1.0`, 최대 180초, 정상 2초 조회를 사용한다. 배정 전 본인 취소만 가능하며 서버 종료 확정 전 새 매칭을 차단한다. 배정 후 세션 계층이 `ClaimAssignment`로 소유권을 넘겨받으면 티켓 폴링을 멈추고 매치 API로 진행한다.

인증 만료는 취소 완료로 처리하지 않고 로그인 복귀와 계정별 상태 복구로 안내한다. 코드 검토와 Unreal 실실행을 구분한다. 상세: [티켓 연결과 검증 순서](MatchmakingTickets.md).

<a id="eos-round1"></a>

## EOS 4인 접속·자동 시작과 다중 맵 경기

> 최근 기능 변경: 2026-11-08 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

2026-11-12(Codex 수행 · 미커밋): 세션 완료 처리를 멤버 함수에 바인딩해, 검색 완료 델리게이트를 해제한 뒤에도 안전하게 참가 요청을 이어간다. [세션 처리 순서](EOSRound1Session.md#소유권과-순서).

2026-11-12(Codex 수행 · 미커밋): EOS 접속이 ICE 연결 중 15초 제한에 걸린 로그를 바탕으로 초기 NetDriver 접속 제한을 60초로 늘렸다. 연결 후 끊김 제한 10초와 서버의 매칭 단계 기한은 유지한다. 설정·상속 경로를 정적으로 확인했으며 실제 4PC 비교 검사는 남아 있다. [적용과 검사](EOSRound1Session.md#각-테스트-pc-준비).

실계정 첫 시도에서 참가자의 EOS 접속 주소 재직렬화에 따른 `InvalidURL`을 발견해 주소 원문 보존으로 수정했다. 다른 PC의 Editor 빌드와 EOS 검사 3개 통과 기록 및 이후 사용자의 실계정 4인 접속·같은 월드 4라운드 성공 보고를 확인했다. [선행 검증 기록](EOSRound1Session.md#검증-기록).

`UCFMatchSessionSubsystem`이 배정 명단과 호스트를 조회하고 실제 EOS 세션 생성·검색·참가와 Round1 이동을 연결한다. `CFRoundGameMode`는 EOS PUID 명단·중복 접속을 검사하고 실제 4명의 맵 로드·Pawn 준비와 백엔드 READY_TO_START를 확인한 후 자동 카운트다운한다. Playing 이후에만 `/started`를 보고한다. 실패는 `/left`·서버 종료 확인과 로컬 세션 정리 후 복구하며 가짜 참가/시작 보고를 하지 않는다.

EOSLocal 설정에서만 기본 OSS=EOS 및 UE5.8 EOS P2P NetDriver를 사용한다. 기존 파일은 `Set-EOSLocal.ps1 -Upgrade`로 Secret 출력 없이 갱신한다. 일반 실행의 NULL/LAN 경로는 유지한다. [선행 연결 절차](EOSRound1Session.md).

2026-11-09 후속 코드: `CFMatchRuntimeSubsystem`이 네 실제 맵과 색상을 무중복으로 섞고 서버 Seamless Travel 시 점수·신원을 보존한다. EOS 첫 맵/프로토콜 2를 검증하며 최초 joined/started와 최종 4인 결과 ACK 후 finished를 보고한다. 로컬 결과 화면은 세션 정리 후에도 남고 본인 확인으로 연습 로비를 사용한다. 새 코드의 Unreal 빌드·4PC 검증 및 색상 에셋 적용은 미완료다. [구현·적용·QA 절차](../Gameplay/MatchPresentationAndTravel.md).

<a id="sessions"></a>
## 세션 생성·검색·참가
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `d13290b`)

`UCFSessionSubsystem`이 GameInstance 수명 동안 로컬 닉네임과 세션 상태를 관리한다. Idle에서 생성·검색·참가를 시작하며, 콜백·타이머를 정리해 중복 작업을 제한한다. 호스트는 `/Game/Maps/Lvl_CF_Arena`를 listen으로 열고 클라이언트는 검색 결과 또는 IPv4 주소로 연결한다. 닉네임은 최대 24자로 정리하고 실행 중 유지하며 디스크에는 저장하지 않는다.

<a id="closing"></a>
## 퇴장과 오류 복구
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `d13290b`)

참가자 퇴장은 로컬 연결을 정리하고 호스트 방은 유지한다. 호스트 퇴장은 신규 참가를 차단하고 클라이언트에 종료 RPC를 전달한 뒤 방을 정리한다. Network/Travel Failure와 작업 시간 초과는 세션·대기 연결을 정리하고 메뉴로 복귀하는 경로를 사용한다.

<a id="roster"></a>
## 서버 권한과 참가자 정보

> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `60d0b2f`, `99aeaf1`)

`CFWaitingGameMode::PreLogin`이 정원 초과·종료 중·경기 진행 중 참가를 거절한다. 호스트 시작 승인, Pawn 교체, 카운트다운, 탈락·승리·점수·다음 라운드·대기 복귀를 서버에서 결정한다. `CFWaitingGameState`는 호스트·정원·경기 상태·서버 시각·참가자·생존자·결과를 복제한다. 소유 컨트롤러가 닉네임과 시작/복귀 요청을 서버로 전달한다. 전투 캐릭터의 기울기·차징 입력 RPC와 상태·방향 복제도 연결되어 있다.

대기 복귀는 레벨 이동 없이 연결·닉네임을 유지하고 다시 참가를 허용한다. [경기 상태](../Gameplay/Overview.md#match), [최종 결과·복귀](../FinalResults.md) 참고.

## 구현과 상세 문서
- 코드: `Source/DKUEndGame/Network/CFSessionSubsystem.h/.cpp`, `CFWaitingNetwork.h/.cpp`.
- UI 연결: [UI 개요](../UI/Overview.md).
- 설정·전체 흐름·과거 검증·직접 확인 방법: [LAN 리슨 서버](ListenServerSessions.md).

## 제약

HP·피해량과 실제 충격 방향의 최종 판정은 후속 범위다. 한 PC의 독립 프로세스 리슨 서버 검증과 두 물리 PC·인터넷·패키징 검증은 구분한다. 후자의 완료를 의미하지 않는다.
