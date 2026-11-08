# Animation 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 구조
게임플레이가 이동·충전·발사·착지·넘어짐을 결정하고 AnimInstance가 상태를 읽어 표현하는 구조. 대기 애니메이션과 전투 애니메이션은 서로 다른 Pawn/AnimInstance 경로.

<a id="basic"></a>
## 전투 기본 6개 상태
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

`UChickenAnimInstance`는 Idle→ChargeStart→ChargeLoop→JumpStart→AirLoop→Land 상태를 캐릭터에서 계산한다. 공중 충전은 공중 표현을 유지하며 TakeoffSerial로 새 점프를 감지한다. ChargeStartDuration 0.4초, 점프 시작의 실효 기본 길이 0.22초, LandPlayRate는 0.7/GetLandingRecoveryTime()이다. 기존 LandMontage를 별도로 재생하지 않는 경로.
2026-11-05 보완(Codex 수행, 요청자 GitHub ID 확인 필요, 미커밋): 점프 시작의 실효 길이는 `max(JumpStartDuration, MinimumJumpStartDuration)`이며 새 최소값은 0.22초다. 발사 직후 이동 모드가 아직 지상이어도 JumpStart를 유지하되 실제 착지 회복이 우선한다. 착지 모션은 `GetLandingRecoveryTime()`의 실효 회복 시간(기본 최소 0.3초)을 사용한다. AnimBP 전환 블렌드 에셋 자체는 변경하지 않았으며 에디터에서 내부 연결과 재생을 재확인하지 않았다.
- 상세: [기본 애니메이션](BasicAnimations.md).

2026-11-06 상체 조정(Codex 수행, 요청자 GitHub ID 확인 필요, 미커밋): 기존 AnimBP 참조 `CurrentTilt`를 보간된 표현값으로 분리했다. `RawTiltInput`은 즉시 게임플레이에 반영·복제하며, 상체 입력은 응답률 6과 최대 4 입력 단위/초로 보간한다. 에셋 참조 문자열은 확인했으나 내부 그래프·실제 재생은 미확인이며 AnimBP 바이너리는 변경하지 않았다. 강한 점프·충돌 후의 균형은 [일시적 불안정 상태](../Gameplay/Overview.md#unstable-balance)를 따른다.

<a id="waiting"></a>
## 대기 서기·걷기
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `3b7a7c8`)

`UCFWaitingAnimInstance`는 수평 속도 기준으로 Stand/Walk를 선택한다. 걷기 진입 >5cm/s, 서기 복귀 ≤2cm/s로 떨림을 줄이고 실제 속도를 기준 속도 65.75cm/s와 메시 스케일로 나눠 재생 속도를 계산한다. Root Motion 대신 CharacterMovement 이동을 표현하는 구성.
- 상세: [서기·걷기 반입과 연결](StandWalkAnimations.md).

<a id="knockdown"></a>
## 4방향 넘어짐
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

AnimInstance의 bFallen·KnockdownIndex가 방향별 표현을 전달한다. 기존 문서에는 4개 클립 분기·마지막 자세 유지·기울기 보정 우회가 기록되어 있다. 이 AnimGraph 내부 설정은 이번 작업에서 에디터로 재확인하지 않았다.
구역은 캐릭터 로컬 축의 넘어질 방향을 직접 지정한다. 2026-10-14 충돌 반응 변경(Codex 수행, 사용자 GitHub ID 확인 필요, 미커밋)으로 플레이어 충돌은 밀림과 균형 충격을 주고, 균형 한계 초과 시 현재 기울기의 우세 축으로 넘어짐 방향을 결정한다. 실제 애니메이션 방향과 네트워크 재생은 플레이 검증이 필요하다.
- 상세: [넘어짐 연결·테스트 맵](DirectionalKnockdown.md).
- 상태 소유자: [Gameplay 넘어짐](../Gameplay/Overview.md#knockdown).

## 관련 구현과 주의
`Source/DKUEndGame/Character/ChickenAnimInstance.h/.cpp`, `CFWaitingAnimInstance.h/.cpp`.
에셋 연결 위치·클립 설정·과거 확인 기록은 상세 문서 참고. 외부 Blender·FBX·검사 JSON 경로는 확인 필요. 전투 표현 상태·넘어짐 방향은 서버에서 복제한다. dev 병합에서 받은 AnimBP 변경은 유지하며 실제 지연 환경의 재생 시점 보정은 별도 검증 대상이다.
