# UI 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위와 흐름

현재 진입 화면은 Figma 기반 타이틀·로그인·닉네임·접속 중 UI다. 인증·프로필·로비 이동 제공자는 [Entry](../Network/LoginEntry.md)에 연결했으며 2026-11-06 사용자가 실계정 로그인부터 로비 입장 성공을 보고했다. 로비의 [자동 매칭 티켓 제공자](../Network/MatchmakingTickets.md)는 코드 연결 후 빌드·실행 검증 전이다. 제품 목표인 자동 4인 매칭은 [클라이언트 인계서](../EOS/unreal-client-handoff.md)를 따른다.
기존 LAN 방 생성·검색·직접 접속 메뉴는 F9 또는 Development 실행 인자 `-CFLanMenu`로 이용한다. 아레나 대기 UI는 실제 참가자 정보를 표시한다. 로컬 UI 요청 이벤트와 서버의 경기 시작 승인은 별개다.

<a id="menu"></a>
## 메인 메뉴
> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`WBP_CF_TitleMenu`의 타이틀 → Epic 로그인 → 최초 닉네임 등록 → 접속 중 화면을 `CFTitleMenuWidget`이 제어한다. `CFFrontendSubsystem`은 화면 상태와 통합 요청/응답·취소·기한을 관리한다. `CFEntrySubsystem`은 EOS 인증·프로필 API·실제 로비 이동을 담당하며, 클라이언트 EOS 설정 없이는 실패 안내를 표시한다. 인증·등록 성공 응답 없이 다음 단계로 넘기지 않으며 중복 요청과 취소 후 늦은 응답을 차단한다.
`BP_CF_MenuGameMode`는 새 `BP_CF_TitleController`를 사용한다. 기존 `WBP_CF_MainMenu`/`CFMainMenuWidget`과 LAN 세션 기능은 개발용 진입 경로로 유지한다. 새 타이틀의 게임 시작은 인증 진입이고, 로비에서의 자동 매칭 UI는 별도 `WBP_CF_Lobby`에 구현되어 있고, 실제 EOS 제공자는 미연결이다.
- 상세: [메인 메뉴](MainMenu.md).

<a id="practice-lobby"></a>
## 연습 로비
> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`L_Lobby_Playground`는 기존 연습 캐릭터·놀이기구를 유지하며 `BP_CF_LobbyGameMode`와 `WBP_CF_Lobby`를 사용한다. 오른쪽 위 빠른 대전/매칭 중 패널, Esc·톱니바퀴 설정, 배경음·효과음 볼륨 저장, 타이틀 복귀를 제공한다. `CFLobbySubsystem`의 비동기 요청/응답 인터페이스는 중복 시작·취소와 배정 경합·상태 불명·복귀 대기를 처리한다. `UCFMatchmakingSubsystem`이 티켓 제공자를 연결하고 인증 만료 시 로그인 복귀를 안내한다. 실제 EOS 세션 제공자는 후속 작업이다.
- 상세: [연습 로비 UI](Lobby.md).

<a id="waiting"></a>
## 대기방

> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `99aeaf1`)

`CFWaitingRoomWidget`은 0.35초 타이머로 PlayerArray와 복제된 GameState를 읽어 이름·정원 4명·호스트·빈자리를 표시한다. 대기 중 최소 2명이면 호스트가 시작을 요청하며 서버에서 재검증한다. 외부 `SetRoomDisplay`는 자동 조회를 끄고 `UseWorldRoster`는 복구한다.
나가기는 기본 세션 퇴장 경로로 전달하며, `OnLeaveRequested`를 바인딩하면 외부 처리 뒤 연결 정리가 필요하다. 최종 결과에서 대기로 복귀하면 같은 대기 UI와 걷기 입력을 복원한다.
- 상세: [대기 UI](WaitingRoomUI.md).

## 수정 위치와 연결
- 코드: `Source/DKUEndGame/UI/CFTitleMenuWidget.h/.cpp`, `CFTitleMenuController.h/.cpp`, `CFFrontendSubsystem.h/.cpp`. 기존 LAN·대기는 `CFMainMenuWidget.h/.cpp`, `CFWaitingRoomWidget.h/.cpp`.
- 에셋: `/Game/UI/MainMenu/`, `/Game/UI/Lobby/`, `/Game/UI/WaitingRoom/`. Designer·부모 클래스·맵 설정은 상세 문서의 기존 기록 참고.
- `BindWidget` 이름은 C++ 연결에 사용하므로 단순 디자인 수정에서 임의 변경 금지.
- 연결 처리: [Network 개요](../Network/Overview.md).
- 대기 Pawn: [Gameplay](../Gameplay/Overview.md#waiting), [대기 애니메이션](../Animation/StandWalkAnimations.md).

## 경기·결과 HUD

`CFMatchStatusWidget`은 복제된 경기 상태에 따라 카운트다운, 라운드·생존 인원, 탈락·관전, 라운드 순위·점수, 최종 누적 순위·MVP를 표시한다. 호스트에게만 방 대기 복귀 버튼을 보여주며 서버도 권한을 검사한다. 상세는 [라운드 진행](../RoundProgression.md), [최종 결과](../FinalResults.md). 승자 클로즈업 연출은 후속 범위다.

## 튜토리얼 팝업

`/Game/UI/GameLevel/BP_TutorialPopup`의 Widget Component는 `WBP_TutorialPopup`을 월드에 표시한다. 제목과 그림을 묶은 Vertical Box, 그림의 Size Box(1600 × 702.58), Down Only Scale Box 구성을 사용한다. 2026-10-06에는 컴포넌트의 Draw Size를 기본 500 × 500에서 1920 × 1080으로 맞춰 제목과 축소된 그림 사이의 빈 공간을 해소했다. 에디터 액터 미리보기와 Blueprint 컴파일·저장으로 확인했으며 게임 실행 검증은 하지 않았다. WBP의 별도 미저장 디자인 변경은 이번 수정에서 저장하지 않았다.
