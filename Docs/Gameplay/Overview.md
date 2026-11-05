# Gameplay 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위

대기 캐릭터 이동, 닭싸움 이동·균형·차징·충돌 넘어짐과 서버의 경기 시작·탈락·관전·4라운드 점수·최종 결과·같은 방 대기 복귀를 구현한다. 엔진 템플릿 Variant 코드 전체를 완성 기능으로 간주하지 않는다.

<a id="waiting"></a>
## 대기 캐릭터
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `3b7a7c8`)

`ACFWaitingCharacter`는 기존 `ADKUEndGameCharacter` 계열 이동을 사용한다. 기본 속도 110cm/s, 가속 600, 제동 800cm/s²이며 이동 방향으로 회전하고 점프는 불가. 기존 문서상 `BP_CF_WaitingCharacter`와 `IMC_CF_Waiting`을 대기 경로에서 사용한다.
- 코드: `Source/DKUEndGame/Character/CFWaitingCharacter.h/.cpp`.
- 표현: [서기·걷기 상세](../Animation/StandWalkAnimations.md).

<a id="balance"></a>
## 이동과 균형
> 수치·역입력 회복 조정: 2026-10-30 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
> 최근 기능 변경: 2026-09-29 · @sunsi-game (`87afe94`의 GitHub noreply ID 근거)

`AChickenCharacter`는 MoveForward/MoveRight 축 입력을 캐릭터 방향 이동과 기울기에 사용한다. 현재 구현은 입력 가속도와 기울어진 방향의 중력 효과를 `TiltVelocity`에 누적하고 감쇠한 뒤 `BodyTilt`를 갱신한다. 입력을 놓아도 자동으로 중심에 복귀하지 않으며 반대 방향 입력으로 균형을 잡는다. 각 축에서 입력과 BodyTilt의 부호가 반대일 때만 입력 가속도를 CounterTiltInputMultiplier(기본 2배)로 강화한다. 동시에 기울기 속도가 아직 바깥쪽이면 CounterTiltBraking(기본 6)의 지수 감쇠를 추가해 관성을 줄인다. 중심으로 돌아오는 속도에는 추가 제동을 걸지 않고, 매 계산 분할마다 판정해 중심 통과·입력 해제 시 보정을 해제한다. 대각 입력은 정규화하고 최대 1/120초 간격으로 계산을 나눈다. 전투 캐릭터의 기본 MaxWalkSpeed는 275cm/s다. 어느 축이 MaxBodyTilt=60/35(약 1.714)에 도달하면 균형 실패를 요청한다. 메시 목표 회전의 축별 약 60도에 해당하며 기존 35도보다 완화했다. 실제 메시 회전에는 보간이 적용되므로 화면상 각도와 판정 시점은 다를 수 있다. `TiltSpeed`, `TiltRecoverySpeed`, `MaxTiltTime` 필드는 남아 있지만 현재 계산에는 사용하지 않는다. 이 설명은 이번 충돌 변경과 연결되는 기존 구현을 대조해 정정했다.

<a id="jump"></a>
## 차징 점프와 자동 홉
> 차징 범위 조정: 2026-10-30 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

> 최근 기능 변경: 2026-10-02 · @sunsi-game (`7ed2786`, `2ee4755`), 서버 입력·상태 복제: @KyuminChung (`c8a7452`)

JumpCharge 누름은 충전을 시작하고 해제는 누적 BodyTilt 방향으로 LaunchCharacter를 실행한다. 기본 MinJumpPower 200, MaxJumpPower 700, ChargeRate 700이며 BodyTilt.Y의 입력 부호에 맞춰 -RightDir를 사용한다. 충전 시작 시 파워는 200이고 약 0.714초 후 700에 도달한다. 정규화 게이지는 이 범위를 0~1로 표시한다. 파워는 발사 벡터의 계수이며 기울기에 따라 수평·수직으로 분배된다. 충전 중에도 기울기 입력은 반영하지만 MoveForward/MoveRight의 AddMovementInput은 생략한다. 현재 StartJumpCharge는 상태 검사 전에 StopMovementImmediately와 입력 벡터 소비를 호출하므로 공중 수직 속도도 즉시 정지한다. 공중 재충전·발사 제한은 없다. 착지 후 0.2초 회복을 거쳐 자동 홉을 요청하며 충전·넘어짐·라운드 입력 잠금은 이를 억제한다. 서버가 차징·홉을 처리하고 TakeoffSerial 등 표현 상태를 복제한다.

<a id="knockdown"></a>
## 넘어짐과 충돌

> 최근 기능 변경: 2026-10-02 · 충돌: @sunsi-game (`7ed2786`, `2ee4755`), 탈락·관전: @KyuminChung (`60d0b2f`)

`StartKnockdown`은 서버에서 상태 중복·입력 잠금을 검사하고 아레나의 생존자 목록을 갱신한다. 충전·회복·기울기를 정리하며 지상은 즉시 넘어짐, 공중은 수평 속도를 멈추고 중력을 유지하여 착지 후 넘어짐. 방향과 상태를 복제하며 아레나에서는 1.6초 후 캐릭터 제거·고정 관전으로 전환한다. 독립 테스트에서는 자세를 유지한다. R 초기화는 전용 테스트 GameMode/HUD가 필요하며 현재 dev 맵은 해당 설정과 4구역을 제거한 상태다.

충돌 반응·이동·점프·탈락 한계 변경: 2026-10-30 · Codex 수행, 요청자 GitHub ID 확인 필요 · 미커밋.

서버의 MoveBlockedBy에서 이동 속도의 슬라이드·굴절 보정 전 OnChickenHit의 공통 충돌 처리를 호출한다. 기존 캡슐 OnComponentHit도 유지하며 두 경로는 같은 쌍별 쿨다운을 사용해 중복 충격을 막는다. 지연된 캡슐 콜백에서 이미 줄어든 속도 때문에 접근 속도 조건을 통과하지 못하는 경로를 보완했다. OnChickenHit는 두 생존자의 상태·상대 캡슐·쌍별 쿨다운을 검사한다. 중심 간 수평 방향으로 상대속도를 투영한 접근 속도가 30cm/s 이상이면, 접근 속도 × 0.8(최대 600cm/s)의 수평 속도 변화를 양쪽에 반대 방향으로 적용한다. 이탈·스침처럼 접근 속도가 부족한 접촉은 제외한다. 수직 속도 변화는 각 캐릭터의 수평 충격 크기 × 0.35(최대 200cm/s)다. AddImpulse의 질량 무시 모드를 사용하며 착지 회복을 해제해 충돌 직후 자동 홉이 겹치지 않게 한다. 충전 상태는 유지한다. 같은 쌍의 중복 충격은 기본 0.2초 동안 막는다.

충격을 각 캐릭터의 로컬 균형 좌표로 변환해 `TiltVelocity`에 더한다(속도 변화 × 0.004, 충격당 최대 1.2). BodyTilt를 즉시 꺾지 않고 기울기 속도를 누적한 뒤 전체 메시를 보간해 밀린 방향으로 서서히 쏠리게 한다. 바닥 법선을 이용한 기울기 계산은 추가하지 않았다. 충돌 자체는 탈락시키지 않으며, 기존 균형 계산에서 한계를 넘을 때만 `FallOver()`가 `StartKnockdown()`을 호출한다. 반대 방향 입력으로 버틸 수 있고 넘어짐 방향은 현재 `BodyTilt`의 우세 축으로 결정한다. `Chicken|Collision` 기본값은 플레이 조정용 초기값이며 실제 2인 플레이·지연 환경 검증은 아직 수행하지 않았다.
- 코드: `Source/DKUEndGame/Character/ChickenCharacter.h/.cpp`. 이번 변경은 C++ 정적 검토만 수행했다. `Content/Characters/BP_ChickenCharacter.uasset`의 별도 기본값 오버라이드와 실제 2인 플레이는 미확인이다.
- 표현·시험 맵: [4방향 넘어짐 상세](../Animation/DirectionalKnockdown.md).

<a id="match"></a>
## 경기 상태와 라운드
> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `60d0b2f`, `99aeaf1`)

정원 4명·최소 2명, 호스트 시작 → 3초 카운트다운 → 경기 → 결과 6초를 4R까지 진행한다. 서버가 참가자·생존자·탈락 순서·5/3/1/0점·누적 점수를 관리한다. 최후 1인 승리, 동시 마지막 탈락은 무승부·공동 순위다. 최종 누적 순위·MVP를 표시한 뒤 호스트가 같은 연결을 유지하며 전원을 걷기 대기로 돌려보낼 수 있다. 새 경기는 1R·0점으로 초기화한다.
- 상태 소유자: `ACFWaitingGameMode`, 복제: `ACFWaitingGameState`, 소유 플레이어 입력·관전: `ACFWaitingPlayerController`.
- 상세: [경기 시작](../MatchStart.md), [탈락·관전](../RoundEnd.md), [점수·라운드](../RoundProgression.md), [최종 결과·복귀](../FinalResults.md).

<a id="debug-logging"></a>
## 디버깅 로그
> 최근 기능 변경: 2026-10-23 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

충돌·기울기·이동을 LogChickenCollision, LogChickenTilt, LogChickenMovement로 분리했다. Actor·World·프레임·시간·네트워크 역할을 공통 출력한다. 사건 로그는 기본 활성화하고 매 이동 갱신·균형 계산·반복 접촉 로그는 기본 비활성화한다. cf.Debug.Events와 cf.Debug.Frames로 제어한다. 밀치기 요청값과 이후 관측 속도를 구분하며 기존 물리 계산과 게임플레이 상태 전환은 유지한다.
- 설정·필드 해석·PIE 절차: [캐릭터 디버깅 로그](DebugLogging.md).

<a id="ld-moving-obstacles"></a>
## LD_Gym 이동 발판
> 최근 기능 변경: 2026-11-01 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`Content/LD_Gym/Levels/LevelPrototyping/Obstacles/`의 `BP_PhysicsObstacle_MovingPlatform`과 `BP_PhysicsObstacle_TurnTable`은 `BP_KinematicObstacle`을 부모로 사용한다. 새 부모는 Movable Root만 제공하며 Mover 플러그인에 의존하지 않는다. 기존 부모 `BP_PhysicsObstacle`의 저장되지 않은 편집은 보존했고, 그 부모의 Mover 오류 자체를 복구한 것은 아니다.

- 상하 발판: `VerticalMovement` (`InterpToMovementComponent`)의 상대 Control Points `(0,0,0)` → `(0,0,300)`, Duration 3초, Behaviour Type `PingPong`. 기본 자세에서 300cm 위까지 편도 3초·왕복 6초로 반복한다. 이동 중 경로 충돌 Sweep은 끄고, 발판 메시의 충돌은 유지한다.
- 회전 발판: `TurntableMovement` (`RotatingMovementComponent`)의 Rotation Rate `(Pitch=0, Yaw=45, Roll=0)`, 로컬 축 기준 초당 45도·한 바퀴 8초. 공전이 아니라 발판 중심의 자전이다.
- 두 컴포넌트는 Auto Activate를 사용한다. 기존 메시·머티리얼·스케일·에셋 경로를 유지하며, 메시의 Simulate Physics는 꺼진 상태다. 이동 범위·시간·회전 속도는 각 Blueprint의 해당 컴포넌트에서 조절한다.
- 확인 범위: 부모·자식 Blueprint 컴파일과 개별 저장, 설정 재조회, 메시 표시 및 배치된 상하 발판의 Root→Platform 연결 확인. PIE·캐릭터 탑승·멀티플레이 동기화는 미검증이며 서버 권한 기반 이동 복제는 추가하지 않았다.

<a id="ld-window-glass"></a>
## LD_Gym 유리창 에셋
> 최근 기능 변경: 2026-11-01 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`Content/LD_Gym/LDProps/StaticMeshes/SM_WindowGlass.uasset`은 프레임 없는 유리판 메시다. 엔진 Cube를 별도 에셋으로 복제하고 LOD Build Scale `(1.2, 0.04, 1.8)`을 적용했다. 렌더 정점 기준 가로 X 120cm·두께 Y 4cm·높이 Z 180cm이며, 단순 박스 충돌 1개를 추가했다. 같은 폴더의 `M_WindowGlass`는 Translucent·Two Sided·Surface 조명, 연한 청색 Base Color·Opacity 0.22·Roughness 0.1·Specular 0.5를 사용한다. 맵에 드래그해 배치하며, 크기는 액터 스케일로 조절할 수 있다. 머티리얼·메시 개별 저장과 에디터 미리보기의 배경 투과를 확인했으며 맵 배치·PIE는 수행하지 않았다.

<a id="playground-lobby"></a>
## 놀이터 튜토리얼 로비

2026-11-05 UI 연결 대상은 현재 플레이용 맵 `/Game/Map/L_Lobby_Playground`다. 이 맵의 GameMode Override에 `BP_CF_LobbyGameMode`를 지정하고, 기존 연습 캐릭터와 배치 오브젝트를 유지했다. 빠른 대전·설정·타이틀 복귀는 [연습 로비 UI](../UI/Lobby.md)를 참고한다. 아래 놀이터 제작 기록의 맵 이름과 구분한다.

> 최근 기능 변경: 2026-11-04 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`Content/Map/L_LobbyJS_Playground.umap`은 `L_LobbyJS`의 별도 복제본이다. 원본 맵과 공유 Blueprint·메시를 수정하지 않고 기존 지형, 점프대, 상하 발판, 회전판, 오뚝이 타깃 2개를 유지했다. 새 맵에 민트 탄성포장 표현, 놀이대 난간, 회전판에 부착된 손잡이, 점프대 패딩, 5개 경사 구간으로 만든 지붕 없는 미끄럼틀, 벤치와 물리 블록 3개를 추가했다. 탄성포장은 시각 표현이며 실제 바닥 탄성 물리는 추가하지 않았다.

- 머티리얼: `Content/Map/Playground/Materials/`의 `M_Playground_*` 8개. 절차적 Noise와 높은 Roughness로 입자감을 표현한다. 기존 지형의 높낮이는 보존한다.
- 외벽은 원본 `L_LobbyJS`의 높은 구성을 유지하면서 놀이터 테마로 꾸몄다. 하단은 Mint, 상단은 Cream, 2층 창틀 3개는 Teal 머티리얼을 사용하며 원본 그리드 재질은 제거했다. 기존 유리창 3개의 배치·회전·메시·유리 재질을 유지하고 노란 창턱을 추가했다. 노란 상부 마감·청록색 모서리 기둥과 둥근 캡·코랄 띠·원형 벽 장식으로 놀이기구와 색상을 통일했다.
- 높이·창문 복원은 `Tools/Editor/restore_playground_enclosure.py`, 테마 마감은 `Tools/Editor/theme_playground_enclosure.py`로 적용했다. 테마 스크립트는 장식 중복 생성을 차단한다. 저장 후 벽·창문 액터 15개의 스케일·표시·충돌, 벽·창틀 12개의 테마 재질과 장식 29개 및 에디터 외형을 확인했다. 벽의 기존 충돌은 유지하며 추가 마감·장식·창턱은 비충돌이다. 놀이대 난간·손잡이·미끄럼틀 가장자리는 장식용 비충돌, 미끄럼틀 바닥·벤치·신규 물리 블록은 충돌을 사용한다.
- 생성 코드: `Tools/Editor/build_playground_lobby.py`. 원본을 연 Unreal Editor Python에서 실행하며 대상 맵이나 생성 머티리얼이 있으면 덮어쓰지 않는다. 현재 생성본에는 재실행하지 않는다.
- 회전판 위 `BP_WobbleTarget2`의 Root를 Movable로 설정하고 회전판의 `Cylinder_0`에 월드 변환을 유지해 부착했다. 물리 제약은 타깃 내부 `BasePlate`–`Dummy` 연결을 사용한다. 회전판을 따라가는 받침대를 기준으로 몸체의 기존 물리 흔들림·각도 제한·복원 드라이브를 유지한다. 바닥의 다른 타깃은 변경하지 않았다.
- 아치 옆 `SM_Dummy_Drum_01`·`SM_Dummy_Drum_2`는 별도 메시 `Content/Map/Playground/Meshes/SM_Playground_PushableDrum.uasset`을 사용한다. 원본 드럼 메시를 복제하고 26-DOP 볼록 충돌을 추가했다. 새 맵의 두 인스턴스만 Movable·PhysicsActor·Simulate Physics·중력·CCD, 질량 12kg, Linear Damping 0.4·Angular Damping 0.8로 설정했다. 기존 색상·배치를 유지한다.
- 상호작용 적용 코드: `Tools/Editor/configure_playground_interactions.py`. 놀이터 맵만 대상으로 하며 공유 Blueprint·원본 메시·원본 맵은 저장하지 않는다. 저장 후 재조회로 타깃 부착·로컬 물리 제약·몸체 시뮬레이션, 전용 메시 볼록 충돌 1개, 항아리 2개의 질량·PhysicsActor·시뮬레이션을 확인했다. 실제 회전 추종·복원·캐릭터 밀침 강도는 PIE 미검증이다.
- 확인: 맵 저장, 에디터 외형, 액터 188개, 점프대·상하 발판·회전판 각 1개, 오뚝이 2개, 미끄럼틀 바닥 5개를 조회했다. 오뚝이는 기존 연습 타깃이며 적 AI 구현을 의미하지 않는다. PIE·캐릭터 탑승·미끄럼틀 접속부 충돌·밀침·네트워크·패키징은 미검증이다. 기본 시작 맵과 매칭 흐름은 바꾸지 않았다.

<a id="urban-sparring-plaza"></a>
## L_TestJS 도심 야외 스파링 광장
> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`Content/Map/L_TestJS.umap`은 기존 그레이박스 배치를 기준으로 도심 광장 테마를 적용한 테스트 레벨이다. 중앙 12m 링, 로프·기둥, 네 방향 진입 계단, 배치된 캐릭터 4개의 변환과 기존 액터를 유지한다. 주변 건물 11동의 본체 크기·위치를 기준으로 메시를 교체하고, 기존 관중석 자리 4곳에 5열 철골·목재 좌석, 중앙 통로와 난간을 배치했다. 추가 차양·창턱·옥상 설비는 본체 외곽보다 돌출된다.

- 전용 에셋: `Content/Map/UrbanArena/`에 Static Mesh 18개, 머티리얼 25개, 한글 상호 텍스처 6개를 저장했다. 건물 11종과 관중석, 포장석 타일·광장 포장, 링 측면 마감, 가로등, 이동식 펜스, 휴지통을 재사용할 수 있다. 외벽에는 창틀·불투명 유리 표현, 층간 띠·배수관·실외기·차양·상가 간판·옥상 난간과 설비를 포함한다. 재질 Noise로 표면 얼룩을 표현하며 내부 공간이나 투명 창문을 구현하지 않는다.
- 기존 건물 뒤에 동일 메시를 재사용한 배경 건물 18동과 배경 포장면을 추가했다. 배경과 거리 소품·링 측면 마감·추가 포장면은 비충돌 장식이다. 기존 바닥·링·계단 충돌은 유지하고, 교체 건물은 단순 박스 충돌, 관중석은 `Use Complex Collision As Simple`을 사용한다. 관중 캐릭터·관중 AI는 추가하지 않았다.
- 액터 총 95개, 기존 액터 54개를 보존한다. 에디터에서는 `UrbanArena/Buildings`, `Grandstands`, `StreetFurniture`, `Ring`, `Ground`, `Backdrop` 폴더로 정리한다. 게임 모드·캐릭터 Blueprint·기본 시작 맵은 변경하지 않는다.
- 생성 원본: `Tools/Editor/generate_urban_arena_sources.py`를 일반 Python과 Pillow에서 실행하면 `Saved/UrbanArenaWork/Source`에 OBJ·MTL·간판 PNG·manifest를 만든다. 간판 글꼴은 Windows `malgunbd.ttf`를 사용한다. 에디터에서 `apply_urban_arena.py`, 이어서 `finish_urban_arena.py`를 실행하는 최초 생성용 흐름이다. 현재 생성본에는 재실행하지 않는다. 각 스크립트는 기존 생성 에셋 또는 완료 표시가 있으면 중단한다.
- 렌더링 정리: OBJ 면을 재질별로 묶고, 건물 11종·광장 포장 메시의 Nanite를 활성화했다. `Tools/Editor/optimize_urban_arena_meshes.py`는 생성 원본에서 이 전용 메시만 재임포트하고 재질·충돌·Nanite 설정을 다시 적용한다.
- 확인: 에디터 외형·개별 에셋 저장·맵 저장, 기존 액터 생존과 링·캐릭터·계단 변환, 전용 메시 재질 연결을 조회했다. PIE 이동·관중석 충돌·네트워크·패키징 및 목표 기기의 성능은 미검증이다.

## 후속 범위와 제약

HP·피해량·인터넷 서비스 연결은 후속 범위다. 로컬 리슨 서버 검증을 외부 인터넷·패키징 검증으로 해석하지 않는다. 병합 시 dev의 `ABP_Chicken`·`Test_CF_Knockdown` 변경을 그대로 유지한다.
