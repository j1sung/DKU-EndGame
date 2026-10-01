# Gameplay 개요

> 전체 문서 갱신: 2026-10-01 (KST) · @j1sung
> 갱신 기준: 커밋 f7132fa9402b9acd6625877e55b917fc03bdeef7
> 기준 보완 필요: C++와 기존 문서를 대조한 초기 정리. Blueprint 내부·맵 설정은 기존 문서 기반이며 이번 작업에서 에디터로 재확인하지 않음. 관련 에셋 확인 후 완전한 갱신 기준 확정 필요.

## 범위
현재 커스텀 구현은 대기 캐릭터 이동과 닭싸움 캐릭터의 이동·균형·차징 점프·로컬 넘어짐이다. 엔진 템플릿 Variant 코드 전체를 게임 완성 기능으로 간주하지 않는다.

<a id="waiting"></a>
## 대기 캐릭터
> 최근 기능 변경: 2026-09-29 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `3b7a7c8`)

`ACFWaitingCharacter`는 기존 `ADKUEndGameCharacter` 계열 이동을 사용한다. 기본 속도 110cm/s, 가속 600, 제동 800cm/s²이며 이동 방향으로 회전하고 점프는 불가. 기존 문서상 `BP_CF_WaitingCharacter`와 `IMC_CF_Waiting`을 대기 경로에서 사용한다.
- 코드: `Source/DKUEndGame/Character/CFWaitingCharacter.h/.cpp`.
- 표현: [서기·걷기 상세](../Animation/StandWalkAnimations.md).

<a id="balance"></a>
## 이동과 균형
> 최근 기능 변경: 2026-09-29 · @sunsi-game (`87afe94`의 GitHub noreply ID 근거)

`AChickenCharacter`는 MoveForward/MoveRight 축 입력을 캐릭터 방향 이동과 기울기에 사용한다. 입력이 있으면 `BodyTilt`를 TiltSpeed 0.8로 누적하고 입력이 없으면 TiltRecoverySpeed 1.5로 회복한다. 어느 축이 MaxBodyTilt 1에 도달하면 균형 실패를 요청한다. `MaxTiltTime` 필드는 남아 있지만 현재 판정은 2초 유지 방식이 아니다.

<a id="jump"></a>
## 차징 점프와 자동 홉
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

JumpCharge 누름은 충전을 시작하고 해제는 누적 BodyTilt 방향과 점프력으로 LaunchCharacter를 실행한다. 기본 ChargeRate 1000, MaxJumpPower 1500. 착지 후 0.2초 회복을 거쳐 자동 홉을 요청하며 충전·넘어짐 상태는 이를 억제한다. TakeoffSerial은 로컬 발사 감지용 카운터.

<a id="knockdown"></a>
## 로컬 넘어짐
> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

`StartKnockdown`은 상태 중복을 거절하고 충전·회복·기울기를 정리한다. 지상은 즉시 넘어짐, 공중은 수평 속도를 멈추고 중력을 유지하여 착지 후 넘어짐. 메시 기본 회전을 복구하고 이동을 중지한다. 균형 실패의 임시 방향은 Forward이며 실제 피격 벡터 계산은 미구현.
- 코드: `Source/DKUEndGame/Character/ChickenCharacter.h/.cpp`.
- 표현·시험 맵: [4방향 넘어짐 상세](../Animation/DirectionalKnockdown.md).

## 후속 범위와 제약
전투용 충전·균형·넘어짐 상태에 전용 RPC·복제 연결이 없다. 체력·실제 피격 방향·탈락·관전·승리 판정과 경기 시작 연결은 현재 문서화한 커스텀 경로에서 미구현. 대기 네트워크 기능이 있다는 이유로 전투 멀티플레이가 완료되었다고 판단하지 않기.
