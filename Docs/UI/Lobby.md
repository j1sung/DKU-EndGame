# 연습 로비 UI

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위 문서: [UI 개요](Overview.md#practice-lobby) · 통합 계약: [EOS 클라이언트 인계서](../EOS/unreal-client-handoff.md)

## 열어서 확인하기

2026-11-12 커서·배경음 변경(Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋): 로비 입장 시 Game Only 입력과 숨김 커서를 적용한다. 설정을 열 때만 Game and UI·커서 표시를 사용하며 카메라/이동 입력을 막고, 닫으면 복구한다. 기존 BG 슬라이더는 모든 라운드에서도 유지되는 공통 배경음 볼륨에 적용한다. [공통 배경음](../Gameplay/Overview.md#background-music). 빌드·실제 UI 조작/오디오 재생은 미검증이다.

2026-11-10 닉네임 보완: 로비 컨트롤러가 등장·Pawn 소유·프로필 상태 변경 때 인증된 `ProfileNickname`을 로컬 권한 PlayerState에 적용한다. 로컬 이름표도 게임 프로필 이름을 우선 읽으므로 Epic 표시 이름이 노출되지 않는다. R키 부활과 경기 후 로비 복귀에도 같은 경로를 사용한다. 새 Editor 빌드·실계정 재확인은 아직 수행하지 않았다.

1. Content Browser에서 `/Game/Map/L_Lobby_Playground`를 연다. 기존 `L_LobbyJS` 이름의 에셋과 구분한다.
2. World Settings의 GameMode Override가 `BP_CF_LobbyGameMode`인지 확인하고 Play한다.
3. 평소에는 커서 없이 마우스로 시점을 조작한다. Esc로 설정을 열면 커서가 나타나고 빠른 대전·매칭 취소·볼륨 버튼을 사용할 수 있다. Esc 또는 톱니바퀴로 설정을 닫으면 커서가 다시 숨겨진다.
4. 기존 닭싸움 캐릭터의 WASD·마우스 차징 점프로 연습한다. 착지 후 자동 반동도 기존 캐릭터 코드가 처리한다.
5. BG는 배경음, VFX는 게임 소리(자동 홉·점프 차징·발사·착지·충돌) 볼륨이다. 2026-11-12부터 캐릭터 효과음에 실제 배율을 적용하며 재생 중인 소리와 다른 라운드에도 저장값을 반영한다. VFX를 0으로 내려도 BG는 유지된다. 타이틀로 돌아가기는 기존 `Lvl_CF_MainMenu`를 연다.

메인 타이틀의 인증·프로필·실제 로비 이동은 [Entry 연결 코드](../Network/LoginEntry.md)에 구현했다. 2026-11-06 사용자는 실제 로그인부터 로비 입장 성공을 보고했다. [매칭 티켓 제공자](../Network/MatchmakingTickets.md)를 추가해 빠른 대전·조회·취소를 API에 연결했다. 로그인 없이 직접 맵을 열면 로그인 안내를 표시한다. 실제 EOS 세션 생성·참가는 후속 작업이며 가짜 매칭 성공이나 임의의 인원을 만들지 않는다.

## 로비에서 R키 부활

2026-11-10 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋.

`L_Lobby_Playground`에서 캐릭터가 넘어져 사망했거나 공중 넘어짐 대기 상태일 때 R키를 누르면 로비 최초 등장 위치에 같은 캐릭터 클래스로 부활한다. 생존 중에는 무시한다. 새 Pawn을 소유하므로 기울기·점프 충전·넘어짐 상태가 초기화되며, 매칭과 로비 UI는 유지한다. 최초 위치가 다른 장애물로 막혀 새 캐릭터를 만들 수 없으면 기존 사망 캐릭터를 유지하고 다시 요청할 수 있다.

`ACFLobbyController`의 입력과 `UCFLobbyWidget`의 키 미리보기에서 같은 요청을 전달하므로 UI에 포커스가 있어도 R키를 처리한다. 서버 RPC는 현재 소유 Pawn의 사망 상태, 정확한 로비 맵 이름, 경기용 `ACFWaitingGameMode`가 아닌지를 재검사한다. 경기 대기·카운트다운·라운드·결과 맵에서는 사용할 수 없다. 코드 정적 검토만 수행했으며 빌드·PIE 입력 및 부활 검증은 미수행이다.

## 화면과 에셋

Figma `ChickenFight`의 로비 프레임 `100:994`를 기준으로 제작했다. 빠른 대전과 매칭 중 패널은 같은 오른쪽 위 위치에서 교체한다. 디자인 설명용 화살표·주석과 예시 인원 `2/4`는 런타임 화면에 넣지 않았다. 현 API에는 대기열 인원 수가 없으므로 실제 서버 데이터가 추가되기 전에는 인원 진행 막대를 표시하지 않는다.

| 대상 | 경로·역할 |
|---|---|
| 로비 맵 | `/Game/Map/L_Lobby_Playground` — 기존 배치 유지, 로비 게임모드 지정 |
| UMG | `/Game/UI/Lobby/WBP_CF_Lobby` — Designer에서 배치 수정, 부모 `UCFLobbyWidget` |
| 컨트롤러 | `/Game/UI/Lobby/BP_CF_LobbyController` — 부모 `ACFLobbyController`, 평소 Game Only / 설정 중 Game and UI 입력 |
| 게임모드 | `/Game/UI/Lobby/BP_CF_LobbyGameMode` — 기존 `BP_ChickenCharacter` 사용 |
| 원본 디자인 | `Content/UI/Lobby/SourceArt/` — Gear PNG와 패널 SVG 3개 |
| 텍스처 | `/Game/UI/Lobby/Textures/` — 원본 이미지와 SVG를 변환한 UI 텍스처 |
| 폰트 | 기존 `/Game/UI/MainMenu/Fonts/F_CF_Inter` 재사용 |
| 상태 | `Source/DKUEndGame/UI/CFLobbySubsystem.h/.cpp` |
| 표시·입력 | `CFLobbyWidget.h/.cpp`, `CFLobbyController.h/.cpp` |

맵에 배치된 Player0 자동 소유 캐릭터를 유지한다. 컨트롤러는 기존 `ADKUEndGamePlayerController`를 상속하고 기존 ThirdPerson 입력 매핑 목록을 복사했다. 경기용 `CFWaitingGameMode`·기존 LAN 대기 UI와 별도이므로 연습 화면이 경기 시작이나 승패 판정을 실행하지 않는다. 기존 메인 UI 코드와 에셋은 수정하지 않았다.

## 매칭 통합 연결

`UCFLobbySubsystem`은 GameInstance 수명의 표시 상태를 가진다. 요청의 `FlowId`는 시작부터 종료 확정까지 유지하며 이전 흐름의 늦은 응답은 무시한다. 티켓 ID, 토큰, HTTP 주소, 폴링·재시도, 역할·EOS 세션은 통합 계층이 관리한다. 같은 FlowId 안에서도 통합 계층은 오래된 HTTP 응답 순서를 검증해야 한다.

| UI 요청 이벤트 | 제공자의 처리 |
|---|---|
| `OnMatchmakingRequested(FlowId)` | 티켓 생성 또는 이미 활성인 상태 복구 |
| `OnCancelRequested(FlowId)` | 서버에 현재 티켓 취소 요청. 취소 확정 전 성공 처리 금지 |
| `OnStatusRecoveryRequested(FlowId)` | 현재 티켓·매치 상태 재조회 |

| 제공자가 호출할 응답 | 표시와 효과 |
|---|---|
| `ReportWaiting(FlowId, RemainingSeconds)` | 매칭 중·남은 대기 시간. 시작 응답 전에 취소했으면 실제 취소 요청을 이어서 보냄 |
| `ReportAssigned(FlowId, Stage)` | 상대와 연결 중. 취소·타이틀 복귀 차단. 배정이 취소보다 먼저 확정된 경우에도 이 경로 사용 |
| `ReportIdle(FlowId, Message)` | 취소·실패·만료·활성 티켓 없음이 **서버에서 확인된 경우** 새 매칭 허용. 복귀 의도가 있으면 타이틀 이동 |
| `ReportUncertain(FlowId, Message)` | 연결 확인 중·다시 확인. 활성 흐름을 유지하며 새 매칭 차단 |

- `RestoreOnEntry()`는 복구 제공자가 바인딩되어 있고 활성 흐름이 없을 때 호출돼 기존 티켓을 확인한다. 통합 제공자는 로비 컨트롤러 BeginPlay 이전에 바인딩한다.
- 요청 중 중복 시작을 차단한다. 티켓 생성 응답 전에 취소·복귀를 누르면 의도를 저장하고 생성 확인 뒤 취소한다.
- 시작·취소·복구 요청의 로컬 응답 기한은 30초, 연결 단계는 60초다. 시간 초과와 대기 시간 0은 취소 성공을 뜻하지 않는다. 다시 확인을 통해 서버 상태를 확정해야 한다.
- 배정 뒤 도착한 늦은 WAITING 응답은 연결 화면을 되돌리지 않는다. 종료 확정 뒤 이전 FlowId의 응답은 무시한다.
- 타이틀 복귀 중 활성 매칭이 있으면 취소 확인까지 맵에 남는다. 실제 경기 이동·호스트/게스트 세션 동작은 통합 계층의 후속 작업이다.
- UI용 모의 응답은 프로젝트 밖 Play 검증 스크립트에만 있고 런타임에 데모 자동 성공 경로는 없다.
- `UCFMatchmakingSubsystem`이 위 요청·응답을 실제 티켓 HTTP와 연결한다. 로비 진입 복구, 미확정 쓰기 재시도, 2초 조회, 401 복구는 [제공자 문서](../Network/MatchmakingTickets.md)를 따른다.
- 인증 만료 상태 `AuthenticationRequired`에서는 취소 완료를 가장하지 않고 로그인 화면 복귀를 허용한다. 같은 계정의 미확정 쓰기는 유지하고, 다른 계정으로 로그인하면 이전 계정의 키·티켓을 보내지 않고 새 current를 조회한다.

## 음량

BG와 VFX는 각각 0~1의 배경음·효과음 볼륨을 조절한다. `GameUserSettings.ini`의 `[ChickenFight.Audio]`에 저장해 재실행 후 읽고, 잘못된 범위는 제한한다.

로비 컨트롤러는 `/Game/Audio/SC_CF_Music`, `SC_CF_Effects`를 `SMix_CF_UserVolume`으로 개별 제어한다. 두 클래스는 `SC_CF_Master`의 자식이다. 로비 시작 때 믹스를 적용하고 종료 때 제거하며, 슬라이더 변경마다 중복 Push하지 않는다. 음원을 추가할 때 해당 Sound Class를 지정해야 한다. 현재 프로젝트에는 재생할 음원 에셋이 없으며, 다른 맵에 음향을 추가할 때 그 맵에도 저장한 볼륨 적용을 연결한다.

## 검증과 제한

- 1920×1080 요청 창에서 30개, 960×640 요청 창에서 28개 검증 통과. 기어·패널 원본 텍스처와 렌더 치수도 확인했다.
- C++ Editor 빌드와 새 Blueprint 컴파일·저장 통과. 배치 액터의 실제 좌표·회전·스케일이 재저장 전후 일치하고, 런타임 에셋에 외부 편집 도구 의존성이 없는지 확인했다.
- 마우스 클릭, 슬라이더 입력·개별 볼륨·설정 파일 저장, Esc, 기존 이동·차징 점프·착지 반동, 타이틀 맵 복귀를 실제 PIE에서 검증했다. 매칭 상태 응답은 외부 테스트에서 주입했다.
- 참가자 4명의 실제 EOS 통신·서버 호출, 음원 청취, Shipping 패키징은 미검증이다. 2026-11-06 추가한 티켓 제공자의 Editor 빌드·자동화·실계정 UI 실행도 미확인이다.
- 복귀 대상 타이틀은 기존 GameDefaultMap을 사용한다. 타이틀 컨트롤러의 `LobbyMap`에 `/Game/Map/L_Lobby_Playground`를 저장하고 soft package dependency를 확인했다. 실제 패키징에서 cook 포함은 추가 검증해야 하며, 기존 패키징 목록을 로비 맵 하나로 대체하지 않았다.
