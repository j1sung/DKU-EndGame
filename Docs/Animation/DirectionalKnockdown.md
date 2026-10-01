# 로컬 4방향 넘어짐 테스트

> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

관련 개요: [카테고리 개요](Overview.md)

## 실행과 조작

1. C++ 변경을 받은 뒤 `DKUEndGameEditor / Development Editor / Win64`를 빌드한다.
2. 콘텐츠 브라우저에서 `Content/Maps/Test_CF_Knockdown`을 연다.
3. Play 설정을 **Number of Players: 1 / Net Mode: Play Standalone**로 두고 Play한다.
4. 화면을 클릭하고 기존 전투 조작으로 색상 구역에 들어간다. 작은 뷰포트는 F11로 넓힐 수 있다.

| 입력 | 동작 |
|---|---|
| W / A / S / D | 기존 캐릭터 기준 이동과 기울기 |
| 왼쪽 마우스 누름 / 놓음 | 기존 차징 점프 충전 / 실행 |
| R | 중앙에서 원본 캐릭터 재생성 |

테스트 전용 WASD 월드 축 이동, Q/E 90도 회전, Space 차징 입력은 제거했다. 플레이어는 **원본 `BP_ChickenCharacter`와 `BP_ThirdPersonPlayerController`를 그대로 사용**한다. 이동 속도, 가속, 공중 제어, 카메라, 입력 매핑, 기울기, 점프 및 균형 실패에 테스트용 보정을 적용하지 않는다. 원본 캐릭터와 컨트롤러의 변경이 이 테스트 맵에도 반영된다.

`dev`의 `BodyTilt` 누적·회복 방식을 사용한다. 입력이 있으면 `TiltSpeed`(기본 0.8)로 기울기가 쌓이고, 입력을 놓으면 `TiltRecoverySpeed`(기본 1.5)로 중심을 회복한다. 어느 한 축이 `MaxBodyTilt`(기본 1.0)에 도달하면 넘어짐을 요청한다. 이전 `MaxTiltTime` 2초 판정은 사용하지 않는다. 테스트 맵에서도 동일한 균형 판정을 사용하므로 구역까지 이동할 때 입력을 조절해야 한다. 원본 기본 이동 속도는 600, 가속은 2048, 공중 제어는 0.8, 중력 배율은 1이다.

## 구역

| 구역 | 지정하는 넘어짐 방향 | 클립 |
|---|---|---|
| 파랑 FORWARD | 캐릭터 앞 | AN_CF_Knockdown_Forward |
| 주황 BACK | 캐릭터 뒤 | AN_CF_Knockdown_Back |
| 보라 LEFT | 캐릭터 왼쪽 | AN_CF_Knockdown_Left |
| 초록 RIGHT | 캐릭터 오른쪽 | AN_CF_Knockdown_Right |

구역은 **맞는 방향이 아니라 넘어질 방향**을 지정한다. 방향은 요청 시점의 캐릭터 몸 기준이다. 회전한 상태에서는 화면의 상하좌우와 다를 수 있다.

플레이어 시작점을 바닥보다 높게 두어 원본 캐릭터가 자연스럽게 착지하고 기존 자동 홉을 시작하게 했다. R 초기화 후에도 같은 시작점에 생성된다. 테스트 코드가 직접 점프시키거나 이동을 덮어쓰지 않는다.

흐름은 **기존 이동·기울기·차징·자동 홉 → 구역 진입 → 공중이면 착지 대기 → 해당 방향으로 넘어짐 → R로 중앙 복귀**이다. 이미 넘어진 상태에서는 자세를 유지하며 다시 뛰지 않는다.

## 연결 구조

- `AChickenCharacter::StartKnockdown(ECFKnockdownDirection)`이 C++와 Blueprint의 공통 진입점이다.
- `Forward / Back / Left / Right`는 캐릭터 로컬 축 `+X / -X / -Y / +Y`에 대응한다.
- 요청 시 충전, 착지 회복, 기울기 입력을 취소한다. 이미 넘어졌거나 착지를 기다리는 중이면 추가 요청을 무시하고 false를 반환한다.
- 지상에서는 즉시 넘어지고, 공중에서는 수평 이동을 멈춘 채 중력을 유지해 착지한 뒤 지정 클립을 재생한다.
- 넘어지는 동안 이동·점프·몸 회전을 막는다. 1.4초 클립을 한 번 재생한 뒤 마지막 자세를 유지한다.
- 넘어짐 요청 시 누적 `BodyTilt`를 초기화하고, 넘어짐 재생 직전에 Mesh를 BeginPlay에서 기록한 기본 회전으로 복구한다. 평상시의 Mesh 기울기가 넘어짐 클립을 비틀지 않도록 한다. 균형 실패가 발생한 프레임에는 일반 기울기·자동 홉 처리를 더 진행하지 않는다.
- `UChickenAnimInstance`의 `bFallen`과 `KnockdownIndex`로 AnimGraph를 선택한다.
- 평상시에는 기존 BasicLocomotion → DefaultSlot → 기울기 보정을 사용한다. 넘어짐 분기는 기존 기울기 보정을 거치지 않고 4개 클립 중 하나를 출력한다. `dev`의 기울기 Pitch 부호 보정과 기존 상태·이벤트 그래프도 유지한다.
- 넘어짐 전환은 0.12초 블렌딩, 방향 선택 내부는 0초 블렌딩이다. 활성화 시 클립을 초기화하며 Root Motion을 사용하지 않는다.
- 기존 `FallOver()` 균형 실패도 이 진입점을 사용한다. 임시 방향은 Forward이고 실제 피격 방향 계산은 TODO다.

## 테스트 구성

- 맵: `/Game/Maps/Test_CF_Knockdown`
- GameMode: `/Game/Testing/Knockdown/BP_CF_KnockdownTestGameMode`
- Pawn: `/Game/Characters/BP_ChickenCharacter`
- Controller: `/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController`
- `ACFKnockdownTestHUD`: 조작 안내, 상태 표시, R 초기화 입력만 추가한다.
- `ACFKnockdownZone`: 캡슐 진입 시 Details의 Direction으로 넘어짐을 요청한다.

이전의 테스트 전용 캐릭터 BP/C++ 클래스와 컨트롤러 C++ 클래스는 제거했다. 원본 캐릭터의 `SetupPlayerInputComponent`와 기존 컨트롤러의 입력 매핑을 사용한다.

R은 테스트용 재생성이다. 경기 중 부활이나 관전 전환은 별도 작업이다. 누운 캐릭터의 캡슐은 기존 충돌 형태를 유지한다. 사망 후 상대와의 충돌 정책은 경기 로직 연결 시 결정한다.

## 기존 검증 기록

아래는 기존 문서에 남아 있던 결과이며 이번 문서 정리에서 재실행한 결과가 아니다.

- C++ Development Editor 빌드와 테스트 GameMode BP 컴파일 성공.
- `dev` 병합 후 저장된 맵에서 실제 입력을 사용한 PIE 검증 73개 통과. 원본 Pawn/Controller 클래스와 이동 기본값, 네 방향 WASD 및 기울기 전달, 왼쪽 마우스 충전 유지·해제, 자동 홉, R 재생성, 공중 진입 후 착지·넘어짐, `BodyTilt` 균형 제한, 캐릭터가 향하는 방향 기준 이동을 확인했다.
- 입력 해제 후 누적 기울기의 점진적 회복, 방향 키를 놓은 상태에서도 누적 `BodyTilt`를 사용하는 차징 점프, 네 방향 넘어짐 시 Mesh 기본 회전 복구를 확인했다.
- `dev` AnimBP의 기존 노드 128개와 기울기 Pitch 부호 보정을 보존하고 4방향 넘어짐 분기를 추가했다.
- 기본 애니메이션 6개 상태와 넘어짐 후 자세 유지·입력 중단을 확인했다.
- 원본 카메라를 사용하는 실제 Play 화면에서 맵과 방향별 넘어짐을 확인했다.

## 후속 TODO

1. 친구의 충돌·피격 코드에서 공격/충격 벡터를 받아 캐릭터 기준 4방향을 결정한다.
2. 임시 체력 1 및 HP 0 → 넘어짐 연결.
3. 애니메이션 이후 제거, 링 밖 일반 걷기 관전자 생성, 생존자 수와 승리 판정.
4. 리슨 서버 권한 판정과 상태·방향·재생 시점 복제. 새 넘어짐 로직은 현재 로컬 구현이다.
5. 필요하면 공중 전용 피격, 래그돌 또는 바닥·벽에 맞춘 자세 보정.
