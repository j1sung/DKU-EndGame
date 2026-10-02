# Animation 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 구조
게임플레이가 이동·충전·발사·착지·넘어짐을 결정하고 AnimInstance가 상태를 읽어 표현하는 구조. 대기 애니메이션과 전투 애니메이션은 서로 다른 Pawn/AnimInstance 경로.

<a id="basic"></a>
## 전투 기본 6개 상태
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

`UChickenAnimInstance`는 Idle→ChargeStart→ChargeLoop→JumpStart→AirLoop→Land 상태를 캐릭터에서 계산한다. 공중 충전은 공중 표현을 유지하며 TakeoffSerial로 새 점프를 감지한다. ChargeStartDuration 0.4초, JumpStartDuration 0.15초, LandPlayRate는 0.7/LandingRecoveryTime. 기존 LandMontage를 별도로 재생하지 않는 경로.
- 상세: [기본 애니메이션](BasicAnimations.md).

<a id="waiting"></a>
## 대기 서기·걷기
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `3b7a7c8`)

`UCFWaitingAnimInstance`는 수평 속도 기준으로 Stand/Walk를 선택한다. 걷기 진입 >5cm/s, 서기 복귀 ≤2cm/s로 떨림을 줄이고 실제 속도를 기준 속도 65.75cm/s와 메시 스케일로 나눠 재생 속도를 계산한다. Root Motion 대신 CharacterMovement 이동을 표현하는 구성.
- 상세: [서기·걷기 반입과 연결](StandWalkAnimations.md).

<a id="knockdown"></a>
## 4방향 넘어짐
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

AnimInstance의 bFallen·KnockdownIndex가 방향별 표현을 전달한다. 기존 문서에는 4개 클립 분기·마지막 자세 유지·기울기 보정 우회가 기록되어 있다. 이 AnimGraph 내부 설정은 이번 작업에서 에디터로 재확인하지 않았다.
구역은 캐릭터 로컬 축의 넘어질 방향을 직접 지정한다. dev의 캐릭터 충돌 경로는 상대 위치로 방향을 계산한다. 실제 밀려나는 방향 보정은 후속 검증 범위다.
- 상세: [넘어짐 연결·테스트 맵](DirectionalKnockdown.md).
- 상태 소유자: [Gameplay 넘어짐](../Gameplay/Overview.md#knockdown).

## 관련 구현과 주의
`Source/DKUEndGame/Character/ChickenAnimInstance.h/.cpp`, `CFWaitingAnimInstance.h/.cpp`.
에셋 연결 위치·클립 설정·과거 확인 기록은 상세 문서 참고. 외부 Blender·FBX·검사 JSON 경로는 확인 필요. 전투 표현 상태·넘어짐 방향은 서버에서 복제한다. dev 병합에서 받은 AnimBP 변경은 유지하며 실제 지연 환경의 재생 시점 보정은 별도 검증 대상이다.
