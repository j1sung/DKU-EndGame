# 아레나 대기 UI

`Content/Maps/Lvl_CF_Arena`에서 Play하면 경기장이 보이는 대기 UI가 표시됩니다. 별도 대기 맵은 생성하지 않았습니다.

## 에셋과 편집 위치

| 에셋 | 역할 |
| --- | --- |
| `Content/UI/WaitingRoom/WBP_CF_WaitingRoom` | 제목, 참가자 패널, 상태 문구, 나가기/경기 시작 버튼. Designer에서 배치·색·폰트 수정 |
| `Content/UI/WaitingRoom/WBP_CF_WaitingPlayerRow` | 참가자 한 줄의 이름·상태 점·호스트 배지 디자인 |
| `Content/UI/WaitingRoom/BP_CF_ArenaController` | 기존 BP_ThirdPersonPlayerController 상속. 로컬 컨트롤러의 BeginPlay에서 위젯 생성 및 Game and UI 입력 설정 |
| `Content/UI/WaitingRoom/BP_CF_ArenaGameMode` | 기존 BP_CF_GameMode 상속. PlayerController만 위 클래스로 변경 |
| `Source/DKUEndGame/UI/CFWaitingRoomWidget.h/.cpp` | 참가자 목록 갱신, 버튼 표시 조건, 상태 문구와 이벤트 |

`Lvl_CF_Arena`의 World Settings → GameMode Override가 `BP_CF_ArenaGameMode`입니다. 기존 맵 구조·스폰 위치·Default Pawn(`BP_CF_Character`)은 유지합니다. 전역 GameDefaultMap/EditorStartupMap과 메인 메뉴의 방 생성 버튼은 이번 변경에서 수정하지 않았습니다.

## 현재 동작

- 현재 GameState의 PlayerArray에서 실제 참가자 이름과 인원을 표시합니다. 자동 미리보기 정원은 2명이며, 위젯 Class Defaults의 MaxPlayers에서 변경할 수 있습니다.
- 혼자 Play하면 `참가자 1 / 2`, 호스트 배지, `참가 대기 중` 빈자리 표시가 나옵니다. 메인 메뉴 닉네임의 저장·전달은 아직 연결하지 않았습니다.
- 권한이 있는 로컬 컨트롤러에는 시작 버튼이 보입니다. 현재 자동 표시 모드에서는 경기 시작 처리가 연결되지 않았으므로 비활성 상태입니다.
- `나가기`는 기존 `Lvl_CF_MainMenu`로 돌아갑니다. 세션 생성/제거, 호스트 종료 통지와 연결 오류 처리는 추후 리슨 서버 작업 범위입니다.
- UI를 로컬 플레이어 화면에 한 번만 추가합니다. 커서를 표시하고 Game and UI 입력을 사용합니다.

## 이후 게임 매니저 연결

`WBP_CF_WaitingRoom`을 생성할 때 반환된 위젯 참조를 컨트롤러에 저장한 후 다음 인터페이스를 사용하면 됩니다.

- `SetRoomDisplay(PlayerNames, HostIndex, Capacity, bLocalHost, bCanStart)`: 복제된 방 상태로 UI 갱신. 호출하면 자동 PlayerArray 조회는 중단됩니다. HostIndex는 0부터 시작하며, 모르면 -1입니다. 클라이언트에서 정확한 호스트 배지를 표시하려면 이 값을 전달해야 합니다.
- `UseWorldRoster()`: 현재 월드 참가자를 읽는 미리보기 방식으로 복귀합니다.
- `OnStartRequested`: 호스트 표시·시작 가능 상태·2명 이상일 때 버튼 클릭 이벤트를 전달합니다. 실제 시작 승인/경기 상태 변경은 서버가 검증해야 합니다.
- `OnLeaveRequested`: 바인딩되어 있으면 메뉴 이동 대신 이 이벤트를 호출합니다. 이후 세션 정리와 메뉴 복귀를 여기서 연결하면 됩니다.
- 위젯의 BindWidget 이름은 C++와 연결되어 있으므로 해당 이름은 유지합니다.

경기 시작 시 이 위젯을 제거하고 경기 HUD로 교체하는 단계는 아직 구현하지 않았습니다. 현재 레벨을 그대로 사용해 Waiting → Playing 상태를 전환할 수도 있습니다.

## 서기·걷기 연결 전 확인 사항

이번 변경은 대기 UI 단계입니다. `AN_CF_STAND` / `AN_CF_WALK` 에셋은 있지만 대기 상태의 애니메이션 전환은 아직 연결하지 않았습니다.

검증 중 기존 입력 불일치를 확인했습니다. `BP_CF_Character`는 `IA_Move`를 사용하지만, 공유 `IMC_Default`의 WASD/방향키는 `IA_Tilt`에 매핑되어 있습니다. UI 추가 전의 `BP_CF_GameMode`로도 이동 거리가 0인 것을 비교 확인했습니다. 다음 대기용 캐릭터/이동 상태 작업에서 걷기용 입력과 Stand/Walk를 함께 연결해야 합니다. 친구의 공용 입력 매핑은 이번에 변경하지 않았습니다.

## 검증

- 프로젝트 C++ 빌드 성공, 새 블루프린트 4개 재컴파일: 오류 0 / 경고 0.
- PIE: 위젯 단일 생성, 실제 1인 목록, 호스트·빈자리 표시, 한글 이름, 호스트/참가자 표시 전환, 시작 버튼 조건, 월드 목록 복구 확인.
- PIE: UI가 표시된 상태에서 W 키가 컨트롤러에 도달하고 이동 입력 차단 플래그가 꺼져 있음 확인. 캐릭터의 실제 걷기는 위의 기존 입력 불일치로 미검증.
- PIE와 실제 편집기 화면: 나가기 → 메인 메뉴, 이전 위젯·캐릭터 제거 확인.
- 실제 편집기에서 패널·문구·버튼 배치 확인. 2명 데이터 주입은 UI 상태 테스트이며, 별도 호스트/클라이언트 접속 테스트는 아직 수행하지 않았습니다.

확인 방법: `Content/Maps/Lvl_CF_Arena` 열기 → Play. 디자인 수정은 `Content/UI/WaitingRoom/WBP_CF_WaitingRoom`의 Designer에서 진행합니다.
