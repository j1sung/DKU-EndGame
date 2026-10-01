# UI 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위와 흐름
메인 메뉴는 로컬 위젯 입력을 세션 관리자에 전달하고, 아레나 대기 UI는 실제 참가자 정보를 표시한다. UI 요청 이벤트와 서버의 경기 시작 승인은 별개.

<a id="menu"></a>
## 메인 메뉴
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `d13290b`)

닉네임·방 만들기·검색 목록·직접 IP 참가·검색 취소·진행 및 오류 표시를 담당한다. 처리 중 중복 입력과 모달 뒤쪽 입력을 제한한다. `CFMainMenuWidget`은 C++ 이벤트를 연결하고 `WBP_CF_MainMenu`는 화면 배치를 제공하는 구조.
- 상세: [메인 메뉴](MainMenu.md).

<a id="waiting"></a>
## 대기방
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `d13290b`)

`CFWaitingRoomWidget`은 0.35초 월드 타이머로 PlayerArray와 복제된 GameState를 읽어 이름·정원·호스트·빈자리를 표시한다. 자동 조회 경로는 경기 시작을 허용하지 않아 시작 버튼이 비활성이다. 외부 `SetRoomDisplay`는 자동 조회를 끄고, `UseWorldRoster`는 복구한다. 시작 이벤트는 별도 서버 승인 구현이 필요하다.
나가기는 기본 세션 퇴장 경로로 전달하며, `OnLeaveRequested`를 바인딩하면 외부 처리 뒤 연결 정리가 필요하다.
- 상세: [대기 UI](WaitingRoomUI.md).

## 수정 위치와 연결
- 코드: `Source/DKUEndGame/UI/CFMainMenuWidget.h/.cpp`, `CFWaitingRoomWidget.h/.cpp`.
- 에셋: `/Game/UI/MainMenu/`, `/Game/UI/WaitingRoom/`. Designer·부모 클래스·맵 설정은 상세 문서의 기존 기록 참고.
- `BindWidget` 이름은 C++ 연결에 사용하므로 단순 디자인 수정에서 임의 변경 금지.
- 연결 처리: [Network 개요](../Network/Overview.md).
- 대기 Pawn: [Gameplay](../Gameplay/Overview.md#waiting), [대기 애니메이션](../Animation/StandWalkAnimations.md).

## 미구현
경기 시작 승인·Waiting→Playing·전투 HUD 교체는 후속 구현. UI의 버튼과 이벤트가 존재한다는 이유로 경기 시작 기능 완료로 판단하지 않기.
