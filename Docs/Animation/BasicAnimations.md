# 기본 애니메이션 연결

> 최근 기능 변경: 2026-09-30 · 작성자 GitHub ID 확인 필요 (Git 작성자: 정규민, `4553e07`)

관련 개요: [카테고리 개요](Overview.md)

## 확인 방법

1. C++ 변경을 받은 뒤 `DKUEndGameEditor / Win64 / Development`를 빌드한다.
2. Content Browser에서 `/Game/Maps/Test_CF_Animation`을 연다.
3. Play를 누르고 게임 화면을 클릭한다.
4. 마우스 왼쪽 버튼을 누르고 있으면 충전, 놓으면 점프한다. WASD는 기존 이동·기울기 입력이다.
5. 처음에는 Idle이다. 첫 점프 후에는 착지와 자동 홉이 반복된다. 충전 중에는 다음 자동 홉이 억제된다.
6. 평상시 WASD 유지로는 제한된 기울기에 안정되고 키를 놓으면 중심으로 복귀한다. 강한 방향 점프·충돌 후 2초간은 누적 기울기 상태이므로 균형 한계에서 넘어진다. 다시 시험하려면 Stop → Play한다. 방향별 넘어짐은 [4방향 넘어짐](DirectionalKnockdown.md)을 참고한다.

## 연결 구조

- Pawn: `/Game/Characters/BP_ChickenCharacter`
- AnimBP: `/Game/Characters/Anims/ABP_Chicken`
- AnimBP의 새 C++ 부모: `UChickenAnimInstance`
- AnimGraph의 `BasicLocomotion` 상태 머신: Idle, ChargeStart, ChargeLoop, JumpStart, AirLoop, Land
- 테스트 게임모드: `/Game/Characters/Anims/BP_CF_AnimationTestGameMode`

| 상태 | 애니메이션 | 반복 |
|---|---|---|
| Idle | AN_CF_Idle | 예 |
| ChargeStart | AN_CF_Charge_Start | 아니오 |
| ChargeLoop | AN_CF_Charge_Loop | 예 |
| JumpStart | AN_CF_Jump_Start | 아니오 |
| AirLoop | AN_CF_Air_Loop | 예 |
| Land | AN_CF_Land | 아니오 |

일반 흐름은 `Idle → ChargeStart → ChargeLoop → JumpStart → AirLoop → Land`이다. 짧은 입력은 ChargeLoop를 거치지 않을 수 있다. 공중 충전은 AirLoop를 유지하며, 착지 후에도 버튼을 누르고 있으면 착지 회복 다음에 ChargeStart로 전환한다. 각 상태는 현재 캐릭터 상태에 따라 중간에 끊고 다른 상태로 전환할 수 있다.

기존 `AnimTilt` 이벤트 그래프, spine_01/pelvis 보정, DefaultSlot은 유지했다. 이 변경은 4방향 Lean 클립을 연결한 것이 아니다.

## C++ 변경과 조절값

2026-11-06 상체 입력 완화: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋. 기존 AnimBP가 참조하는 `CurrentTilt` 이름을 유지하면서 로컬 표현값으로 변경했다. 실제 입력·점프 조준·균형 계산은 복제된 `RawTiltInput`을 사용한다. `CurrentTilt`는 `UpperBodyTiltResponse=6`의 지수 보간과 `MaxUpperBodyTiltSpeed=4` 입력 단위/초의 변화량 제한으로 따라가므로 좌우 반전 때 즉시 -1↔1로 바뀌지 않는다. 상체 보간은 게임플레이 입력을 지연시키지 않는다. 전체 메시의 `VisualBodyTilt` 보간은 별도로 유지한다. 탈락·라운드 잠금·넘어짐 대기에서는 표현 기울기를 0으로 정리한다.

에셋의 직렬화된 `ChickenCharacter:CurrentTilt`, `AnimTilt`, `spine_01`, `pelvis` 참조는 확인했지만 에디터에서 AnimGraph 연결을 다시 열거나 재생하지 않았다. AnimBP 바이너리는 수정하지 않았다. 정식 빌드 후 좌우·앞뒤 급반전, 키 해제, 호스트·원격 캐릭터, 넘어짐 포즈를 확인해야 한다.

`AChickenCharacter`가 이동, 충전력, 점프와 착지 후 재점프 시점을 결정한다. `UChickenAnimInstance`는 그 결과를 읽어 표현할 상태와 재생 속도만 계산한다. 애니메이션 Notify로 점프하거나 체력을 변경하지 않는다.

- `LandingRecoveryTime`: 기존 값 **0.2초**와 `MinimumLandingRecoveryTime` 기본 **0.3초** 중 큰 값을 실제 회복 시간으로 사용한다. 기존 BP의 0.2초 오버라이드에도 최소값이 적용되며 자동 홉 간격도 늘어난다. BP_ChickenCharacter의 Class Defaults → Movement → Hopping에서 조절할 수 있다.
- `ChargeStartDuration`: 기본 **0.4초**. 현재 Charge_Start 클립 길이와 일치한다.
- `JumpStartDuration`: 기존 값 **0.15초**와 `MinimumJumpStartDuration` 기본 **0.22초** 중 큰 값을 사용한다. 0.3초짜리 Jump_Start는 기본 약 1.36배속이다. 실제 발사 시점은 C++ 입력 처리 시점이며, 이동 모드 변경 전 한 프레임 때문에 Idle로 끊기지 않도록 시작 상태를 유지한다.
- Land 재생 속도는 `0.7 / GetLandingRecoveryTime()`으로 계산한다. 기본 실효 시간 0.3초에서 약 2.33배속이다. 실제 착지하면 JumpStart 유지보다 Land가 우선한다.
- `TakeoffSerial`: 새 점프를 감지하는 로컬 카운터다. 네트워크 복제 이벤트가 아니다.
- 기존 `LandMontage` 필드는 호환을 위해 남았지만 현재 재생하지 않는다. `AM_Land`를 추가로 재생하면 착지가 중복될 수 있으므로 기본 경로에서는 사용하지 않는다.

## 범위와 후속 작업

2026-11-05 조정: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋. 전체 메시 기울기는 서버의 균형값을 별도 시각값으로 따라가며 응답률 6, 최대 변화량 100도/초를 사용한다. 자동 중심 복원과 균형 판정은 변경하지 않았다. 점프·충전·착지·충돌 소리는 [Gameplay 효과음](../Gameplay/Overview.md#combat-audio)을 참고한다. 이번 변경에서 AnimBP 바이너리와 전환 블렌드는 편집하지 않았고 플레이 검증은 별도다.

원본 `Lvl_CF_Arena`, 친구의 `TestMap`, 기존 게임모드, 메인 메뉴는 바꾸지 않았다. 테스트 레벨은 경기장 복사본이며 ChickenCharacter를 사용한다.

이 문서는 기본 6개 동작 연결을 설명한다. 이후 로컬 4방향 넘어짐과 전용 테스트 맵이 추가되었다. 연결 구조와 실행 방법은 [DirectionalKnockdown.md](DirectionalKnockdown.md)를 참고한다. 대기용 서기·걷기는 [StandWalkAnimations.md](StandWalkAnimations.md)를 참고한다. 4방향 기울기 클립, 피격·균형회복, 전투 상태의 네트워크 복제는 별도 작업이다.

## 기존 검증 기록

아래는 기존 문서에 남아 있던 결과이며 이번 문서 정리에서 재실행한 결과가 아니다.

- C++ Development Editor 빌드 및 AnimBP 컴파일 성공.
- PIE 자동 검증 **21개 통과**, 상태 머신 불일치 0건. 저장된 테스트 레벨에서 실제 입력 실행: 실제 마우스/키 입력 바인딩, 충전 시작/유지/해제, 점프의 실제 높이 변화, 착지 후 자동 홉, 공중 충전, 짧은 탭, 기존 기울기/균형 제한을 확인.
- NullRHI 자동 검증에서는 메시의 애니메이션 평가를 강제로 켜고 실제 상태 머신이 6개 상태를 재생하는지 확인한다. 테스트 중 변경한 런타임 값은 에셋에 저장하지 않는다.
- 실제 에디터 Play 화면에서 흰색 캐릭터의 기본 닭싸움 자세가 표시되는 것을 확인했다. 전체 동작의 세부 타이밍/외형 조정은 직접 플레이하며 이어갈 수 있다. 호스트/클라이언트 검증은 리슨 서버 연결 단계에서 진행한다.
