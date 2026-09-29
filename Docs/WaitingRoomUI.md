# 아레나 대기 UI

`Content/Maps/Lvl_CF_Arena`에서 Play하면 경기장이 보이는 대기 UI가 표시됩니다. 별도 대기 맵은 생성하지 않았습니다.

## 에셋과 편집 위치

| 에셋 | 역할 |
| --- | --- |
| `Content/UI/WaitingRoom/WBP_CF_WaitingRoom` | 제목, 참가자 패널, 상태 문구, 나가기/경기 시작 버튼. Designer에서 배치·색·폰트 수정 |
| `Content/UI/WaitingRoom/WBP_CF_WaitingPlayerRow` | 참가자 한 줄의 이름·상태 점·호스트 배지 디자인 |
| `Content/UI/WaitingRoom/BP_CF_ArenaController` | 기존 BP_ThirdPersonPlayerController 상속. 대기용 IMC_CF_Waiting 사용. 로컬 BeginPlay에서 위젯 생성 및 Game and UI 입력 설정 |
| `Content/UI/WaitingRoom/BP_CF_ArenaGameMode` | 기존 BP_CF_GameMode 상속. PlayerController는 위 클래스, Default Pawn은 BP_CF_WaitingCharacter로 지정 |
| `Source/DKUEndGame/UI/CFWaitingRoomWidget.h/.cpp` | 참가자 목록 갱신, 버튼 표시 조건, 상태 문구와 이벤트 |

`Lvl_CF_Arena`의 World Settings → GameMode Override가 `BP_CF_ArenaGameMode`입니다. 맵 구조와 스폰 위치는 유지하며, Default Pawn은 대기용 `BP_CF_WaitingCharacter`를 사용합니다. 전역 GameDefaultMap/EditorStartupMap과 메인 메뉴의 방 생성 버튼은 이번 변경에서 수정하지 않았습니다.

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

## 대기 중 서기·걷기

`BP_CF_WaitingCharacter`는 C++ `ACFWaitingCharacter`를 상속하고 기존 White Round 메시·재질·스켈레톤을 사용합니다. UI가 표시된 상태로 WASD 및 방향키 이동이 가능합니다. 캐릭터는 이동 방향을 바라보며 걷고, 멈추면 두 발로 섭니다.

- 캐릭터: `Content/Characters/WhiteRound/Blueprints/BP_CF_WaitingCharacter`
- AnimBP: `Content/Characters/WhiteRound/Animations/ABP_CF_Waiting`
- 입력: `Content/Input/IMC_CF_Waiting` → `IA_Move` (WASD, 방향키, 게임패드 왼쪽 스틱 매핑)
- 상태 머신: `WaitingLocomotion`의 Stand(`AN_CF_STAND`) ↔ Walk(`AN_CF_WALK`), 전환 블렌드 0.15초
- 두 전환은 Standard Blend, `Allow Inertialization for Self Transitions` 꺼짐. 빠른 재진입 시 Inertialization 노드 누락 경고를 방지합니다.
- STAND/WALK의 Legacy FBX Import Uniform Scale은 100으로 저장했습니다. 기존 스켈레톤의 루트 배율과 맞추기 위한 값이며, 메시 컴포넌트 Scale은 1입니다.
- 이동 수치: `Source/DKUEndGame/Character/CFWaitingCharacter.cpp`
  - 최대 걷기 속도 110cm/s, 가속도 600cm/s², 제동 감속도 800cm/s²
  - 대기용 캐릭터는 점프하지 않으며, 대기 입력에 차징/기울기를 포함하지 않습니다.
- 애니메이션 값: `Source/DKUEndGame/Character/CFWaitingAnimInstance.h/.cpp`
  - 실제 수평 속도(GroundSpeed)를 읽어 상태 판정
  - 걷기 진입 5cm/s 초과 / 서기 복귀 2cm/s 이하로 경계 떨림 방지
  - 재생 속도 = 실제 속도 ÷ (원본 클립 기준 속도 65.75cm/s × 메시의 균일 스케일)
  - 기본 110cm/s에서는 약 1.673배속, 55cm/s에서는 약 0.837배속
  - Root Motion 없이 CharacterMovement가 실제 이동을 처리합니다.

기존 공용 `IMC_Default`의 WASD는 닭싸움용 `IA_Tilt`를 유지합니다. 대기 컨트롤러에서만 별도의 `IMC_CF_Waiting`을 사용하여 기존 `IA_Move` 입력 불일치를 해결했습니다. `BP_CF_Character`, `ABP_Chicken` 및 닭싸움 물리 코드는 수정하지 않았습니다.

경기 시작 시 닭싸움 캐릭터와 전투용 입력으로 전환하는 작업, 탈락 후 관전용 캐릭터 배치는 게임 매니저 단계에서 연결해야 합니다. 게임패드 매핑은 포함했지만 실제 게임패드 장치 검증은 아직 수행하지 않았습니다.

## 검증

- 프로젝트 C++ 빌드 성공, 새 블루프린트 4개 재컴파일: 오류 0 / 경고 0.
- PIE: 위젯 단일 생성, 실제 1인 목록, 호스트·빈자리 표시, 한글 이름, 호스트/참가자 표시 전환, 시작 버튼 조건, 월드 목록 복구 확인.
- 대기용 이동 PIE 검증 45개 통과: WASD 4방향/대각선 이동, 실제 Walk 상태 평가, 입력 해제 후 Stand 복귀, 110/55cm/s 재생 속도 대응, 점프·차징 미실행, UI 유지 및 메뉴 복귀. 추가로 서기/걷기의 실제 뼈 크기와 12회 빠른 입력 재진입을 확인했으며 Inertialization 누락 경고는 발생하지 않았습니다.
- PIE와 실제 편집기 화면: 나가기 → 메인 메뉴, 이전 위젯·캐릭터 제거 확인.
- 실제 편집기에서 패널·문구·버튼 배치 확인. 2명 데이터 주입은 UI 상태 테스트이며, 별도 호스트/클라이언트 접속 테스트는 아직 수행하지 않았습니다.
- 수정 후 실제 렌더링으로 정상 크기의 서기, 오른쪽으로 걷기, 멈춤, 메인 메뉴 복귀 화면까지 확인했습니다.

확인 방법: `Content/Maps/Lvl_CF_Arena` 열기 → Play → 게임 화면 클릭 → WASD로 이동 → 키를 놓아 서기 확인 → 나가기. 디자인 수정은 `Content/UI/WaitingRoom/WBP_CF_WaitingRoom`의 Designer에서 진행합니다.
