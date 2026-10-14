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
> 최근 기능 변경: 2026-09-29 · @sunsi-game (`87afe94`의 GitHub noreply ID 근거)

`AChickenCharacter`는 MoveForward/MoveRight 축 입력을 캐릭터 방향 이동과 기울기에 사용한다. 현재 구현은 입력 가속도와 기울어진 방향의 중력 효과를 `TiltVelocity`에 누적하고 감쇠한 뒤 `BodyTilt`를 갱신한다. 입력을 놓아도 자동으로 중심에 복귀하지 않으며 반대 방향 입력으로 균형을 잡는다. 대각 입력은 정규화하고 최대 1/120초 간격으로 계산을 나눈다. 어느 축이 MaxBodyTilt 1에 도달하면 균형 실패를 요청한다. `TiltSpeed`, `TiltRecoverySpeed`, `MaxTiltTime` 필드는 남아 있지만 현재 계산에는 사용하지 않는다. 이 설명은 이번 충돌 변경과 연결되는 기존 구현을 대조해 정정했다.

<a id="jump"></a>
## 차징 점프와 자동 홉

> 최근 기능 변경: 2026-10-02 · @sunsi-game (`7ed2786`, `2ee4755`), 서버 입력·상태 복제: @KyuminChung (`c8a7452`)

JumpCharge 누름은 충전을 시작하고 해제는 누적 BodyTilt 방향으로 LaunchCharacter를 실행한다. 기본 ChargeRate 700, MaxJumpPower 500이며 BodyTilt.Y의 입력 부호에 맞춰 -RightDir를 사용한다. 충전 중에도 기울기 입력은 반영하지만 MoveForward/MoveRight의 AddMovementInput은 생략한다. 이는 속도 즉시 정지를 뜻하지 않는다. 착지 후 0.2초 회복을 거쳐 자동 홉을 요청하며 충전·넘어짐·라운드 입력 잠금은 이를 억제한다. 서버가 차징·홉을 처리하고 TakeoffSerial 등 표현 상태를 복제한다.

<a id="knockdown"></a>
## 넘어짐과 충돌

> 최근 기능 변경: 2026-10-02 · 충돌: @sunsi-game (`7ed2786`, `2ee4755`), 탈락·관전: @KyuminChung (`60d0b2f`)

`StartKnockdown`은 서버에서 상태 중복·입력 잠금을 검사하고 아레나의 생존자 목록을 갱신한다. 충전·회복·기울기를 정리하며 지상은 즉시 넘어짐, 공중은 수평 속도를 멈추고 중력을 유지하여 착지 후 넘어짐. 방향과 상태를 복제하며 아레나에서는 1.6초 후 캐릭터 제거·고정 관전으로 전환한다. 독립 테스트에서는 자세를 유지한다. R 초기화는 전용 테스트 GameMode/HUD가 필요하며 현재 dev 맵은 해당 설정과 4구역을 제거한 상태다.

충돌 반응 변경: 2026-10-14 · Codex 수행, 사용자 GitHub ID 확인 필요 · 미커밋.

캡슐 `OnChickenHit`는 서버에서 두 생존자의 수평 상대속도를 충돌 방향에 투영해 접근 속도를 계산한다. 지상·공중 모두 접근 속도 30cm/s 이상이면 양쪽에 반대 방향의 `CharacterMovement::AddImpulse`를 적용한다. 밀림 속도 변화는 접근 속도의 0.65배이며 최대 500cm/s다. 수직 속도와 충전 상태는 유지한다. 같은 쌍은 0.2초 동안 중복 충격을 막고, 스치거나 멀어지는 접촉은 무시한다.

충격을 각 캐릭터의 로컬 균형 좌표로 변환해 `TiltVelocity`에 더한다(속도 변화 × 0.004, 충격당 최대 1.2). 충돌 자체는 탈락시키지 않으며, 기존 균형 계산에서 한계를 넘을 때만 `FallOver()`가 `StartKnockdown()`을 호출한다. 반대 방향 입력으로 버틸 수 있고 넘어짐 방향은 현재 `BodyTilt`의 우세 축으로 결정한다. `Chicken|Collision` 기본값은 플레이 조정용 초기값이며 실제 2인 플레이·지연 환경 검증은 아직 수행하지 않았다.
- 코드: `Source/DKUEndGame/Character/ChickenCharacter.h/.cpp`.
- 표현·시험 맵: [4방향 넘어짐 상세](../Animation/DirectionalKnockdown.md).

<a id="match"></a>
## 경기 상태와 라운드
> 최근 기능 변경: 2026-10-02 · @KyuminChung (`c8a7452`, `60d0b2f`, `99aeaf1`)

정원 4명·최소 2명, 호스트 시작 → 3초 카운트다운 → 경기 → 결과 6초를 4R까지 진행한다. 서버가 참가자·생존자·탈락 순서·5/3/1/0점·누적 점수를 관리한다. 최후 1인 승리, 동시 마지막 탈락은 무승부·공동 순위다. 최종 누적 순위·MVP를 표시한 뒤 호스트가 같은 연결을 유지하며 전원을 걷기 대기로 돌려보낼 수 있다. 새 경기는 1R·0점으로 초기화한다.
- 상태 소유자: `ACFWaitingGameMode`, 복제: `ACFWaitingGameState`, 소유 플레이어 입력·관전: `ACFWaitingPlayerController`.
- 상세: [경기 시작](../MatchStart.md), [탈락·관전](../RoundEnd.md), [점수·라운드](../RoundProgression.md), [최종 결과·복귀](../FinalResults.md).

## 후속 범위와 제약

HP·피해량·인터넷 서비스 연결은 후속 범위다. 로컬 리슨 서버 검증을 외부 인터넷·패키징 검증으로 해석하지 않는다. 병합 시 dev의 `ABP_Chicken`·`Test_CF_Knockdown` 변경을 그대로 유지한다.
