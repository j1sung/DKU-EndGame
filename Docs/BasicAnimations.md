# 기본 애니메이션 연결

## 확인 방법

1. C++ 변경을 받은 뒤 `DKUEndGameEditor / Win64 / Development`를 빌드한다.
2. Content Browser에서 `/Game/Maps/Test_CF_Animation`을 연다.
3. Play를 누르고 게임 화면을 클릭한다.
4. 마우스 왼쪽 버튼을 누르고 있으면 충전, 놓으면 점프한다. WASD는 기존 이동·기울기 입력이다.
5. 처음에는 Idle이다. 첫 점프 후에는 착지와 자동 홉이 반복된다. 충전 중에는 다음 자동 홉이 억제된다.
6. WASD를 오래 유지하면 기존 균형 제한으로 움직임이 멈춘다. 다시 시험하려면 Stop → Play한다. 방향별 넘어짐 표현은 아직 연결하지 않았다.

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

`AChickenCharacter`가 이동, 충전력, 점프와 착지 후 재점프 시점을 결정한다. `UChickenAnimInstance`는 그 결과를 읽어 표현할 상태와 재생 속도만 계산한다. 애니메이션 Notify로 점프하거나 체력을 변경하지 않는다.

- `LandingRecoveryTime`: 기본 **0.2초**. 이전의 즉시 재점프를 짧은 착지 회복 후 재점프로 변경했다. BP_ChickenCharacter의 Class Defaults → Movement → Hopping에서 조절할 수 있다.
- `ChargeStartDuration`: 기본 **0.4초**. 현재 Charge_Start 클립 길이와 일치한다.
- `JumpStartDuration`: 기본 **0.15초**. 0.3초짜리 Jump_Start를 기본 2배속으로 재생한다. 실제 발사 시점은 C++ 입력 처리 시점이다.
- Land 재생 속도는 `0.7 / LandingRecoveryTime`으로 계산한다. 기본값은 3.5배속이다. 짧게 압축한 착지감은 추후 플레이 감각에 맞춰 조절할 수 있다.
- `TakeoffSerial`: 새 점프를 감지하는 로컬 카운터다. 네트워크 복제 이벤트가 아니다.
- 기존 `LandMontage` 필드는 호환을 위해 남았지만 현재 재생하지 않는다. `AM_Land`를 추가로 재생하면 착지가 중복될 수 있으므로 기본 경로에서는 사용하지 않는다.

## 범위와 후속 작업

원본 `Lvl_CF_Arena`, 친구의 `TestMap`, 기존 게임모드, 메인 메뉴는 바꾸지 않았다. 테스트 레벨은 경기장 복사본이며 ChickenCharacter를 사용한다.

이번 범위는 로컬 기본 6개 동작이다. 4방향 기울기 클립, 피격·균형회복·방향별 넘어짐, 대기/관전용 서기·걷기, 리슨 서버 상태 복제는 후속 작업이다. 현재 넘어짐 로직은 입력 중단까지만 되어 있어 실제 넘어지는 애니메이션은 나오지 않는다.

## 검증

- C++ Development Editor 빌드 및 AnimBP 컴파일 성공.
- PIE 자동 검증 **21개 통과**, 상태 머신 불일치 0건. 저장된 테스트 레벨에서 실제 입력 실행: 실제 마우스/키 입력 바인딩, 충전 시작/유지/해제, 점프의 실제 높이 변화, 착지 후 자동 홉, 공중 충전, 짧은 탭, 기존 기울기/균형 제한을 확인.
- NullRHI 자동 검증에서는 메시의 애니메이션 평가를 강제로 켜고 실제 상태 머신이 6개 상태를 재생하는지 확인한다. 테스트 중 변경한 런타임 값은 에셋에 저장하지 않는다.
- 실제 에디터 Play 화면에서 흰색 캐릭터의 기본 닭싸움 자세가 표시되는 것을 확인했다. 전체 동작의 세부 타이밍/외형 조정은 직접 플레이하며 이어갈 수 있다. 호스트/클라이언트 검증은 리슨 서버 연결 단계에서 진행한다.
