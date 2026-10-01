# Network 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위와 흐름
메인 메뉴 → LAN 방 생성 또는 검색·참가 → 아레나 대기 → 퇴장·연결 오류 시 메뉴 복귀.
`OnlineSubsystemNull`을 사용하는 호스트 포함 2인 리슨 서버 구조. Steam/EOS·인터넷 검색·NAT·호스트 이전은 포함하지 않음.

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
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `d13290b`)

`CFWaitingGameMode::PreLogin`이 정원 초과·종료 중 참가를 거절한다. `CFWaitingGameState`는 호스트 PlayerState·정원·방 종료 상태를 복제한다. 소유 컨트롤러는 닉네임을 서버로 전달하고, 서버가 PlayerState에 적용한다.

## 구현과 상세 문서
- 코드: `Source/DKUEndGame/Network/CFSessionSubsystem.h/.cpp`, `CFWaitingNetwork.h/.cpp`.
- UI 연결: [UI 개요](../UI/Overview.md).
- 설정·전체 흐름·과거 검증·직접 확인 방법: [LAN 리슨 서버](ListenServerSessions.md).

## 제약
경기 시작·전투 Pawn 전환·체력·탈락·관전은 이 세션 기능만으로 구현되지 않는다. 대기 이동 복제와 전투 상태 복제는 별개이며 전투 상태는 현재 로컬 구현. 두 물리 PC·인터넷·패키징 테스트를 완료한 것으로 해석하지 않기.
