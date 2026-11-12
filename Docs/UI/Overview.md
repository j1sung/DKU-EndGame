# UI 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위와 흐름

현재 진입 화면은 Figma 기반 타이틀·로그인·닉네임·접속 중 UI다. 인증·프로필·로비 이동 제공자는 [Entry](../Network/LoginEntry.md)에 연결했으며 2026-11-06 사용자가 실계정 로그인부터 로비 입장 성공을 보고했다. 로비의 [자동 매칭 티켓 제공자](../Network/MatchmakingTickets.md)는 코드 연결 후 빌드·실행 검증 전이다. 제품 목표인 자동 4인 매칭은 [클라이언트 인계서](../EOS/unreal-client-handoff.md)를 따른다.
기존 LAN 방 생성·검색·직접 접속 메뉴는 F9 또는 Development 실행 인자 `-CFLanMenu`로 이용한다. 아레나 대기 UI는 실제 참가자 정보를 표시한다. 로컬 UI 요청 이벤트와 서버의 경기 시작 승인은 별개다.

## 경기 중 식별·준비·결과 화면

> 최근 기능 변경: 2026-11-10 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

EOS 경기 캐릭터는 화면 공간 UMG 이름표와 서버 배정 색상으로 구분한다. 로컬은 인증된 게임 프로필 이름을 우선 사용하고 원격은 서버가 복제한 PlayerState 이름을 읽는다. 한글은 문자열 치환 없이 전달한다. 매칭 후 경기장 준비는 걷기 UI 대신 불투명 접속 준비 화면을 표시하고 서버의 대기 Pawn 이동을 차단한다. 4인 준비 후 기존 3초 카운트다운을 사용한다. 로그인 후 연습 로비는 유지한다.

2026-11-10 보완: 로비 PlayerState에도 게임 닉네임을 적용하고 이름표가 Epic 이름으로 대체하지 않게 했다. EOS에서는 GameState 복제가 늦더라도 활성 세션을 기준으로 기존 참가자 목록·수동 시작 대기 창의 생성/등록/갱신을 차단한다. Blueprint에서 등록한 위젯도 먼저 숨긴 뒤 다음 안전한 HUD 갱신에서 제거한다. 별도의 접속 준비 표시와 실제 4인 확인은 유지한다. NativeConstruct 이후 Blueprint의 별도 재표시 동작까지 에디터로 검사한 것은 아니며 새 Editor 빌드·실제 전환 확인이 남아 있다.

최종 결과는 각 PC의 GameInstance 사본을 독립 화면에 표시한다. 정상 종료 보고와 로컬 세션 정리가 끝나야 ‘확인 · 연습 로비로’ 버튼을 사용할 수 있으며 타인의 확인/퇴장은 자기 결과를 지우지 않는다. 인증 만료 중에는 사본을 보존한 채 덮개를 내려 재로그인을 허용한다. 새 코드의 Editor 빌드·UI/한글 렌더링·색상 적용·4PC 검증은 미완료다. [구조·적용·QA](../Gameplay/MatchPresentationAndTravel.md).

<a id="final-result-table"></a>
### 최종 결과 표 정렬

2026-11-11 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋.

`CFCompletedMatchWidget`은 라운드 결과와 같은 고정 폭 열로 `순위 / 플레이어 / 총점 / MVP`를 표시한다. 닉네임 길이와 MVP 유무에 관계없이 헤더와 네 행의 위치가 맞으며, 긴 이름은 말줄임하고 툴팁에 원문을 남긴다. 서버가 전달한 순위·점수·행 순서는 그대로 사용하고 같은 순위만 `공동 3`처럼 표시한다. 기존 결과 사본, 종료 확인 후 버튼 활성화, 각자 연습 로비로 복귀하는 처리는 유지한다.

UE 5.8.2 Editor 빌드와 실제 위젯의 로컬 PIE 렌더링을 확인했다. 1280×720 및 960×640에서 한글·영문 이름, 공동 순위·공동 MVP, 열 정렬, 확인 버튼 클릭 후 결과 화면 제거를 검사했다. 예시 결과를 로컬 사본에 넣은 UI 검증이며 실제 EOS 종료 보고·4PC 매치는 재시험하지 않았다.

<a id="menu"></a>
## 메인 메뉴
> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`WBP_CF_TitleMenu`의 타이틀 → Epic 로그인 → 최초 닉네임 등록 → 접속 중 화면을 `CFTitleMenuWidget`이 제어한다. `CFFrontendSubsystem`은 화면 상태와 통합 요청/응답·취소·기한을 관리한다. `CFEntrySubsystem`은 EOS 인증·프로필 API·실제 로비 이동을 담당하며, 클라이언트 EOS 설정 없이는 실패 안내를 표시한다. 인증·등록 성공 응답 없이 다음 단계로 넘기지 않으며 중복 요청과 취소 후 늦은 응답을 차단한다.
`BP_CF_MenuGameMode`는 새 `BP_CF_TitleController`를 사용한다. 기존 `WBP_CF_MainMenu`/`CFMainMenuWidget`과 LAN 세션 기능은 개발용 진입 경로로 유지한다. 새 타이틀의 게임 시작은 인증 진입이고, 로비에서의 자동 매칭 UI는 별도 `WBP_CF_Lobby`에 구현되어 있고, 실제 EOS 제공자는 미연결이다.
- 상세: [메인 메뉴](MainMenu.md).

<a id="practice-lobby"></a>
## 연습 로비
2026-11-11 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋: 로비 안내를 카드 밖에서 제목·버튼 사이로 옮겼다. 기본 두 줄 공간과 긴 문구에 따른 높이 확장을 적용하고, 원본 매칭 상태·요청/취소/재확인 경로를 유지한다. Editor 빌드와 모의 제공자를 사용한 두 해상도의 실제 PIE 표시·버튼 검증을 통과했다. [배치와 검증 범위](Lobby.md#notice-card).

2026-11-10 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋: 사망 후 R키로 최초 등장 위치에 부활한다. 서버가 로비 맵·사망 상태를 검사해 경기 중 사용을 차단한다. [부활 조건과 제한](Lobby.md#로비에서-r키-부활).

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`L_Lobby_Playground`는 기존 연습 캐릭터·놀이기구를 유지하며 `BP_CF_LobbyGameMode`와 `WBP_CF_Lobby`를 사용한다. 오른쪽 위 빠른 대전/매칭 중 패널, 평소 커서 숨김·Esc로 설정을 열 때 커서 표시, 배경음·효과음 볼륨 저장, 타이틀 복귀를 제공한다. `CFLobbySubsystem`의 비동기 요청/응답 인터페이스는 중복 시작·취소와 배정 경합·상태 불명·복귀 대기를 처리한다. `UCFMatchmakingSubsystem`이 티켓 제공자를 연결하고 인증 만료 시 로그인 복귀를 안내한다. 실제 EOS 세션 제공자는 후속 작업이다.
- 상세: [연습 로비 UI](Lobby.md).

<a id="waiting"></a>
## 대기방

> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `99aeaf1`)

`CFWaitingRoomWidget`은 0.35초 타이머로 PlayerArray와 복제된 GameState를 읽어 이름·정원 4명·호스트·빈자리를 표시한다. 대기 중 최소 2명이면 호스트가 시작을 요청하며 서버에서 재검증한다. 외부 `SetRoomDisplay`는 자동 조회를 끄고 `UseWorldRoster`는 복구한다.
나가기는 기본 세션 퇴장 경로로 전달하며, `OnLeaveRequested`를 바인딩하면 외부 처리 뒤 연결 정리가 필요하다. 최종 결과에서 대기로 복귀하면 같은 대기 UI와 걷기 입력을 복원한다.
- 상세: [대기 UI](WaitingRoomUI.md).

## 수정 위치와 연결
<a id="jump-charge-gauge"></a>
### 캐릭터 옆 점프 게이지

2026-11-10 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋.

`UCFPlayerPresentationComponent`가 `UCFJumpChargeWidget`을 생성한다. 내 캐릭터의 게이지는 기존 Screen 공간 18×144 세로 막대를 유지한다. 2026-11-10 후속 변경(Codex 수행 · 미커밋)으로 상대 캐릭터에는 별도 `OpponentJumpCharge` World 공간 위젯을 추가했다. 상대 막대는 같은 160×160 위젯을 0.65배 월드 크기로 사용해 멀어지면 작아지고 카메라를 향한다. 밝은 테두리·어두운 빈 영역 안에서 아래→위로 채워지며, 충전량에 따라 노랑에서 빨강으로 변한다. 입력을 가로채거나 별도 Blueprint 지정을 요구하지 않는다.

게이지는 `IsChargingJump()`와 서버에서 복제되는 `GetNormalizedJumpCharge()`를 읽는다. 발사·충전 취소 시 비워지고 공중 충전 대기는 빈 상태로 표시한다. 내 게이지는 로컬 소유 캐릭터에만 표시한다. 상대는 PlayerState가 있는 다른 캐릭터가 로컬 카메라 앞·화면 안에 있을 때 표시하며, Masked 월드 위젯의 깊이 테스트로 벽에 가려지는 부분을 숨긴다. 벽의 Visibility 충돌 응답이나 라인트레이스에 의존하지 않는다. 두 게이지 모두 라운드 입력 잠금·넘어짐·공중 넘어짐 대기에서는 숨기고, 전용 서버에서는 생성하지 않는다. 기존 이름표 표시는 변경하지 않는다. 로비 R 부활로 생성된 캐릭터에도 자동 연결한다.

위젯은 매 프레임 충전량을 읽는다. 표현 컴포넌트도 매 프레임 상대 게이지의 화면 범위와 방향을 갱신하되, 이름표 문자열·색 갱신은 기존 10Hz로 제한한다. 현재 로컬 첫 PlayerController의 카메라를 기준으로 하며 분할 화면의 복수 로컬 카메라 대응은 검증하지 않았다. 코드: `Source/DKUEndGame/UI/CFJumpChargeWidget.h/.cpp`, `Source/DKUEndGame/Character/CFPlayerPresentationComponent.h/.cpp`.

초기 구현 후 `Slot`이 `UWidget::Slot`을 가리는 C4458 오류를 `FrameSlot`으로 수정했다. 2026-11-10 상대 가림 처리 변경 후 Unreal 5.8.2 Win64 Development Editor 빌드는 통과했다. 후속 Round2 테스트 로그에서는 `UnrealEditor-Win64-DebugGame.exe`가 수정 전 `UnrealEditor-DKUEndGame-Win64-DebugGame.dll`을 불러온 것을 확인했다. Development와 DebugGame은 별도 DLL이므로 실제 실행 구성도 같은 소스로 빌드해야 한다.

후속 확인: 저장하지 않은 임시 PIE 월드에서 PlayerState를 부여한 비소유 닭 캐릭터를 사용했다. 카메라 앞의 World/Masked 게이지 생성, 카메라 뒤·화면 밖 숨김을 확인했고, 렌더 캡처로 충돌 없는 불투명 벽 앞에서는 막대가 보이고 벽 뒤에서는 가려짐을 확인했다. 자료는 `Saved/GaugeVisibility/verified.json`·`clear.png`·`wall.png`다. 검사 전용 카메라·캐릭터·벽은 맵에 저장하지 않았다. 실제 별도 PC 멀티, 분할 화면, 패키징과 플레이 중 크기·밝기 조정은 미검증이다.

<a id="balance-danger-overlay"></a>
### 균형 위험 비네트 HUD

2026-11-12 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋.

`UCFPlayerPresentationComponent`가 로컬 소유 닭 캐릭터에 `UCFDangerOverlayWidget`을 부모로 쓰는 `WBP_CFDangerOverlay`를 하나 생성해 전체 플레이어 화면에 붙인다. 특정 GameMode나 경기 결과 HUD에 의존하지 않는다. ZOrder -10의 입력 비활성 오버레이이며 점수/안내 HUD 아래에서 `/Game/UI/HUD/M_CFDangerVignette`의 붉은 외곽과 투명한 중앙을 표시한다. 체력 시스템이나 탈락 판정은 변경하지 않는다.

`PlayerPresentation` 컴포넌트 Details `Chicken > Danger HUD > Danger Warning Angle Margin`(기본 10도)으로 탈락 한계보다 몇 도 전부터 경고할지 조절한다. 캐릭터 `MaxBodyTilt × 35`가 실제 축별 한계이고 현재 `BP_ChickenCharacter`와 Round4 배치 네 캐릭터는 60도다. 기본은 50도 이하 0, 55도 0.5, 60도 1이며 복제된 `BodyTilt`의 절댓값이 큰 축을 기준으로 선형 계산한다. 대각선 벡터 길이로 한계보다 일찍 경고하지 않는다. 한계를 바꾸면 경고 시작도 따라 바뀐다. 공중에도 균형 위험은 표시하되 기존 탈락 판정은 지상에서만 이뤄진다.

넘어짐/공중 탈락 대기, 라운드 입력 잠금, 균형 탈락 비활성에서는 즉시 강도 0이다. 소유권을 잃거나 캐릭터가 제거되면 위젯을 제거하므로 관전 대상의 경고를 대신 표시하지 않는다. 전용 서버에서는 생성하지 않는다. 클래스 코드: `UI/CFDangerOverlayWidget.h/.cpp`, 연결/수명: `Character/CFPlayerPresentationComponent.h/.cpp`, 판정 수치 조회: `AChickenCharacter::GetBalanceDangerIntensity`와 `GetBalanceFailureAngleDegrees`. 후속 WBP 전환(2026-11-12 · Codex 수행 · 미커밋): `/Game/UI/HUD/WBP_CFDangerOverlay`가 `UCFDangerOverlayWidget`을 상속한다. Designer의 `DangerCanvas` 아래 `DangerVignette` Image를 전체 화면 앵커·여백 0으로 배치하고 기존 재질을 지정한다. C++의 `BindWidget` 연결 이름 `DangerVignette`는 유지한다. C++은 계층/재질을 생성하거나 덮어쓰지 않고 초기 숨김과 전체 위젯 투명도만 처리한다. `PlayerPresentation`의 `Danger Overlay Class` 소프트 클래스 기본값으로 이 WBP를 로컬 최초 표시 때 로드하며, 같은 속성에서 교체할 수 있다. 부모의 `CreateDesignerContent`는 최초 에디터 생성 도구용으로, 이미 계층이 있으면 변경하지 않고 패키지 빌드에서는 실행하지 않는다.

확인: UE 5.8.2 Win64 Development Editor 빌드 통과. 실제 Play 모드에서 0/49/50/55/59/60도·45/45도 대각선·경고 폭 20도 설정의 계산/실제 위젯 투명도와 로컬 위젯 한 개 생성, 탈락 직후 강도 0을 확인했다. 실제 렌더에서도 붉은 외곽과 투명한 중앙을 확인했다. 검사 캐릭터 상태는 PIE 복사본에만 적용했고 맵에는 저장하지 않았다. 원격 멀티·분할 화면·패키징은 미검증이다. 검사 도구는 `Tools/Editor/verify_danger_hud.py`, 최초 생성 도구는 `create_danger_hud_material.py`와 `create_danger_hud_wbp.py`다. 사용자 Blueprint의 경고 폭 15도는 WBP 전환에서도 보존한다. WBP 전환 후 Development Editor 빌드, WBP 컴파일/저장, 실제 WBP 클래스 생성과 초기 경고 폭 15도 보존, 같은 9개 계산/표시·탈락 숨김 검사를 다시 통과했다.

방향 화살표 추가(2026-11-12 · Codex 수행 · 미커밋): 같은 WBP의 `DangerCanvas`에 `DangerArrowUp/Right/Down/Left` Image를 추가했다. `/Game/UI/HUD/M_CFDangerArrow`는 흰 화살표와 짙은 테두리를 그리며 Image의 색상으로 틴트를 조절할 수 있다. C++ 위젯은 기존 강도 값을 그대로 받고, 소유 캐릭터 메시의 실제 기울어진 위쪽 축을 수평 투영해 카메라 yaw 기준 앞/뒤/좌/우 중 우세한 한 방향만 표시한다. 대각선 동률은 위/아래 축을 우선한다. 이동·균형·탈락 코드와 표현 컴포넌트는 수정하지 않는다. 0도에 가까운 직립 상태, 경고 강도 0, 탈락에서는 화살표를 모두 숨긴다.

화살표는 선택 방향을 향해 0~8px 왕복하며 기본 주기는 0.7초다. WBP Class Defaults의 `Danger Arrow > Arrow Travel (Pixels)`와 `Arrow Cycle Seconds`로 거리/주기를 수정한다. 각 Image의 배치·크기·회전·색상/재질은 Designer에서 수정하며, C++은 Designer의 원래 Render Translation에 왕복량만 더한다. `DangerArrowUp/Right/Down/Left` 이름은 바인딩용으로 유지한다. Development Editor 빌드와 실제 Play에서 네 방향 및 카메라 yaw 90/180/270도 변경의 단일 표시·약 8px 왕복 검사 7건, 탈락 시 전체 숨김, 실제 흰 화살표 렌더를 확인했다. 플레이어/표현 컴포넌트와 다른 UI 코드, 캐릭터 BP·맵·기존 비네트 재질의 작업 전후 SHA-256도 동일했다. 원격 멀티·패키징은 미검증이다. 에디터용 `AddDesignerArrows`/`Tools/Editor/add_danger_hud_arrows.py`는 이미 화살표가 있으면 중단하고 기존 `DangerVignette`를 바꾸지 않는다.

후속 요청 기록: 내 충전 게이지의 Screen 공간 표시를 World 공간 UI로 바꾸는 작업은 요청에 따라 보류한다. 이번 위험 HUD 작업에서는 충전 게이지의 생성/표시/충전 판정을 변경하지 않았다.

### 기존 UI 연결

- 코드: `Source/DKUEndGame/UI/CFTitleMenuWidget.h/.cpp`, `CFTitleMenuController.h/.cpp`, `CFFrontendSubsystem.h/.cpp`. 기존 LAN·대기는 `CFMainMenuWidget.h/.cpp`, `CFWaitingRoomWidget.h/.cpp`.
- 에셋: `/Game/UI/MainMenu/`, `/Game/UI/Lobby/`, `/Game/UI/WaitingRoom/`. Designer·부모 클래스·맵 설정은 상세 문서의 기존 기록 참고.
- `BindWidget` 이름은 C++ 연결에 사용하므로 단순 디자인 수정에서 임의 변경 금지.
- 연결 처리: [Network 개요](../Network/Overview.md).
- 대기 Pawn: [Gameplay](../Gameplay/Overview.md#waiting), [대기 애니메이션](../Animation/StandWalkAnimations.md).

## 경기·결과 HUD

`CFMatchStatusWidget`은 복제된 경기 상태에 따라 카운트다운, 라운드·생존 인원, 탈락·관전, 라운드 결과 점수표, 최종 누적 순위·MVP를 표시한다. 2026-11-10(Codex 수행 · 미커밋)부터 매 라운드 결과도 누적 점수 내림차순·누적 공동 순위로 표시한다. 이번 라운드 승자와 획득 점수, 점수 계산용 라운드 순위는 보존한다. 호스트에게만 방 대기 복귀 버튼을 보여주며 서버도 권한을 검사한다. 상세는 [라운드 진행](../RoundProgression.md), [최종 결과](../FinalResults.md). 승자 클로즈업 연출은 후속 범위다.

<a id="match-esc-menu"></a>
### 경기 Esc 메뉴

2026-11-11 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋.

`CFMatchStatusWidget`의 `방 나가기`를 반투명 짙은 둥근 패널과 주황색 둥근 버튼으로 표시한다. 버튼은 UMG 기준 220×56, 글자 크기 20이며 패널 안쪽 여백은 16이다. 기존 `Esc : 메뉴` 아래 우측에 고정하고 메뉴를 닫으면 패널과 버튼을 함께 숨긴다. `LeaveRoom`의 `CFSessionSubsystem::LeaveRoom()` 호출과 Controller의 Esc 입력·경기 상태 처리는 유지한다.

UE 5.8.2 Editor 빌드와 `L_Round1`의 실제 위젯 로컬 PIE 표시를 확인했다. UI 검사용으로 경기 상태만 `Playing`으로 지정했으며 Esc 열기·닫기·재열기, 비일시정지 상태, 1280×720 및 960×640 캡처를 검사했다. 실제 EOS 매치·방 나가기 통신은 재시험하지 않았다.

## 튜토리얼 팝업

`/Game/UI/GameLevel/BP_TutorialPopup`의 Widget Component는 `WBP_TutorialPopup`을 월드에 표시한다. 제목과 그림을 묶은 Vertical Box, 그림의 Size Box(1600 × 702.58), Down Only Scale Box 구성을 사용한다. 2026-10-06에는 컴포넌트의 Draw Size를 기본 500 × 500에서 1920 × 1080으로 맞춰 제목과 축소된 그림 사이의 빈 공간을 해소했다. 에디터 액터 미리보기와 Blueprint 컴파일·저장으로 확인했으며 게임 실행 검증은 하지 않았다. WBP의 별도 미저장 디자인 변경은 이번 수정에서 저장하지 않았다.
