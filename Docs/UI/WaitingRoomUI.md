# 아레나 대기 UI

> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `99aeaf1`); 초기 세션·대기 구현: Git 작성자 정규민 (`d13290b`)

관련 개요: [카테고리 개요](Overview.md)

`Content/Maps/Lvl_CF_Arena`에서 Play하면 경기장이 보이는 대기 UI가 표시됩니다. 별도 대기 맵은 생성하지 않았습니다.

## 에셋과 편집 위치

| 에셋 | 역할 |
| --- | --- |
| `Content/UI/WaitingRoom/WBP_CF_WaitingRoom` | 제목, 참가자 패널, 상태 문구, 나가기/경기 시작 버튼. Designer에서 배치·색·폰트 수정 |
| `Content/UI/WaitingRoom/WBP_CF_WaitingPlayerRow` | 참가자 한 줄의 이름·상태 점·호스트 배지 디자인 |
| `Content/UI/WaitingRoom/BP_CF_ArenaController` | C++ CFWaitingPlayerController 상속. 대기용 IMC_CF_Waiting 사용. 로컬 BeginPlay에서 위젯 생성 및 Game and UI 입력 설정 |
| `Content/UI/WaitingRoom/BP_CF_ArenaGameMode` | C++ CFWaitingGameMode 상속. PlayerController는 위 클래스, Default Pawn은 BP_CF_WaitingCharacter, GameState는 CFWaitingGameState |
| `Source/DKUEndGame/UI/CFWaitingRoomWidget.h/.cpp` | 참가자 목록 갱신, 버튼 표시 조건, 상태 문구와 이벤트 |

`Lvl_CF_Arena`의 World Settings → GameMode Override가 `BP_CF_ArenaGameMode`입니다. 맵 구조와 스폰 위치는 유지하며, Default Pawn은 대기용 `BP_CF_WaitingCharacter`를 사용합니다. 메인 메뉴의 방 만들기는 LAN 세션을 생성하고 이 레벨을 리슨 서버로 엽니다.

## 현재 동작

- 현재 GameState의 PlayerArray에서 실제 참가자 이름과 인원을 0.35초 간격으로 읽습니다. CFWaitingGameState가 복제하는 정원과 호스트 PlayerState로 양쪽 화면을 갱신합니다. 정원은 C++ CFSessionSubsystem::RoomCapacity의 4명이며 서버가 초과 참가를 거부합니다.
- 혼자 Play하면 `참가자 1 / 4`, 호스트 배지, `참가 대기 중` 빈자리 표시가 나옵니다. 메인 메뉴 닉네임은 GameInstanceSubsystem에 보관하고 서버 RPC를 통해 PlayerState로 전달합니다.
- 권한이 있는 로컬 컨트롤러에는 시작 버튼이 보입니다. 대기 상태에서 참가자 2명 이상이면 시작할 수 있으며, 서버가 호스트 권한과 인원을 다시 검사합니다.
- 참가자 `나가기`는 로컬 세션을 정리하고 `Lvl_CF_MainMenu`로 돌아갑니다. 호스트 방은 유지됩니다. 호스트가 나가면 연결된 참가자에게 종료를 알리고 모두 메뉴로 복귀합니다. 호스트 강제 종료·연결 손실과 연결 시도 시간 초과도 메뉴 복귀 및 안내를 처리합니다.
- UI를 로컬 플레이어 화면에 한 번만 추가합니다. 커서를 표시하고 Game and UI 입력을 사용합니다.

## 게임 매니저 연결

`WBP_CF_WaitingRoom`을 생성할 때 반환된 위젯 참조를 컨트롤러에 저장한 후 다음 인터페이스를 사용하면 됩니다.

- `SetRoomDisplay(PlayerNames, HostIndex, Capacity, bLocalHost, bCanStart)`: 외부에서 방 상태를 지정합니다. 호출하면 자동 PlayerArray 조회는 중단됩니다. HostIndex는 0부터 시작하며, 모르면 -1입니다. 기본 네트워크 경로는 CFWaitingGameState에서 호스트를 자동으로 찾으므로 별도 호출이 필요 없습니다.
- `UseWorldRoster()`: 현재 월드와 복제된 GameState에서 참가자를 읽는 방식으로 복귀합니다.
- `OnStartRequested`: 호스트 표시·시작 가능 상태·2명 이상일 때 버튼 클릭 이벤트를 전달합니다. CFWaitingPlayerController의 시작 RPC와 CFWaitingGameMode의 검증으로 연결했습니다.
- `OnLeaveRequested`: 바인딩되어 있으면 기본 나가기 대신 이 이벤트를 호출합니다. 기본 동작은 CFSessionSubsystem::LeaveRoom이며, 향후 확인창 등으로 재정의할 때도 마지막에 이 함수를 호출해야 연결이 정리됩니다.
- 위젯의 BindWidget 이름은 C++와 연결되어 있으므로 해당 이름은 유지합니다.

경기 시작 시 이 위젯을 제거하고 경기 HUD로 교체합니다. 같은 레벨에서 Waiting → Countdown → Playing으로 전환하며, 상세 내용은 [경기 시작 문서](../MatchStart.md)를 참고하세요.

## 대기 중 서기·걷기

대기 Pawn은 `BP_CF_WaitingCharacter`, 입력은 `IMC_CF_Waiting`을 사용한다. 이동 속도는 기본 110cm/s이며 점프·차징은 대기 경로에 포함하지 않는다. 실제 이동에 맞춰 Stand/Walk를 표현한다.

입력·캐릭터 규칙은 [Gameplay 개요](../Gameplay/Overview.md#waiting), 반입 설정·재생 속도·전환 상세는 [서기·걷기](../Animation/StandWalkAnimations.md)를 참고한다.
경기 시작 시 전투 Pawn·입력으로 전환한다. 탈락하면 착지 후 넘어짐을 재생하고 제거한 뒤 고정 카메라로 관전한다. 최종 결과에서 호스트가 복귀하면 같은 방에서 전원 걷기 대기로 돌아온다. [탈락·관전](../RoundEnd.md), [최종 결과·대기 복귀](../FinalResults.md) 참고. 게임패드 매핑은 포함하지만 실제 장치 검증은 아직 수행하지 않았다.

## 기존 검증 기록

아래는 기존 문서에 남아 있던 결과이며 이번 문서 정리에서 재실행한 결과가 아니다.

- 프로젝트 C++ 빌드 성공, 새 블루프린트 4개 재컴파일: 오류 0 / 경고 0.
- PIE: 위젯 단일 생성, 실제 1인 목록, 호스트·빈자리 표시, 한글 이름, 호스트/참가자 표시 전환, 시작 버튼 조건, 월드 목록 복구 확인.
- 대기용 이동 PIE 검증 45개 통과: WASD 4방향/대각선 이동, 실제 Walk 상태 평가, 입력 해제 후 Stand 복귀, 110/55cm/s 재생 속도 대응, 점프·차징 미실행, UI 유지 및 메뉴 복귀. 추가로 서기/걷기의 실제 뼈 크기와 12회 빠른 입력 재진입을 확인했으며 Inertialization 누락 경고는 발생하지 않았습니다.
- PIE와 실제 편집기 화면: 나가기 → 메인 메뉴, 이전 위젯·캐릭터 제거 확인.
- 실제 편집기에서 패널·문구·버튼 배치 확인. 이후 별도 프로세스의 실제 LAN 접속으로 두 참가자, 호스트 배지, 이름, 이동 동기화와 퇴장·재접속도 검증했습니다.
- 수정 후 실제 렌더링으로 정상 크기의 서기, 오른쪽으로 걷기, 멈춤, 메인 메뉴 복귀 화면까지 확인했습니다.

확인 방법: `Content/Maps/Lvl_CF_Arena` 열기 → Play → 게임 화면 클릭 → WASD로 이동 → 키를 놓아 서기 확인 → 나가기. 디자인 수정은 `Content/UI/WaitingRoom/WBP_CF_WaitingRoom`의 Designer에서 진행합니다.

방 생성부터 연결 종료까지의 흐름과 멀티플레이 테스트 방법은 [ListenServerSessions.md](../Network/ListenServerSessions.md)를 참고하세요. 경기 시작·닭싸움 자세 전환은 [경기 시작](../MatchStart.md), 4라운드 점수 진행과 최종 결과·대기 복귀는 [라운드 진행](../RoundProgression.md), [최종 결과](../FinalResults.md)를 참고하세요.
