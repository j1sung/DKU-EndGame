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

2026-11-05 표현 조정: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋. `VisualBodyTilt`는 서버 균형값과 분리된 로컬 표현값이다. `VisualTiltResponse=6`의 지수 보간과 `MaxVisualTiltSpeed=100`도/초의 변화량 제한을 함께 적용하고 BeginPlay에서 읽은 메시 기본 방향에 더한다. 이 표현값은 균형 실패 판정·발사 방향에 사용하지 않는다. 이후 2026-11-06 조작감 변경으로 입력 기울기 제한과 중심 자동 복귀를 적용했다. 회전 제한으로 실제 판정과 보이는 기울기의 차이가 생길 수 있으므로 플레이에서 조정해야 한다.
> 수치·역입력 회복 조정: 2026-10-30 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
> 최근 기능 변경: 2026-09-29 · @sunsi-game (`87afe94`의 GitHub noreply ID 근거)

`AChickenCharacter`는 MoveForward/MoveRight 입력으로 시점의 수평 방향으로 이동한다. 이동 방향은 `GetControlRotation().Yaw`에서 즉시 계산하며 캐릭터 회전 보간을 기다리지 않는다. 2026-11-06 조작감 변경(Codex 수행, 요청자 GitHub ID 확인 필요, 미커밋)으로 평상시 기울기는 WASD 입력을 계속 적분하는 방식에서 안전한 목표값을 따라가는 임계 감쇠 스프링으로 바뀌었다. 정규화 입력 × MaxBodyTilt × InputTiltLimitRatio(기본 0.35, 최대 0.5)가 목표다. 입력을 놓으면 목표가 0이 되어 자동으로 중심으로 복귀한다. BalanceReturnSpeed 기본 2/s이며 0.1~10 범위로 제한한다. 최대 1/120초 단위로 가속도 `(TargetTilt - BodyTilt) × Response² - 2 × Response × TiltVelocity`를 적분한다. TiltInputAcceleration, CounterTiltInputMultiplier, CounterTiltBraking, TiltGravityStrength, TiltDamping은 아래 불안정 상태에서만 사용한다. 이전 TiltSpeed/TiltRecoverySpeed/MaxTiltTime 필드는 현재 계산에서 사용하지 않는다. 전투 캐릭터 기본 MaxWalkSpeed는 275cm/s다. 충돌은 기존처럼 TiltVelocity에 충격을 더하며 실제 BodyTilt는 입력 목표 범위로 강제 제한하지 않는다. 어느 축이 MaxBodyTilt=60/35(약 1.714)에 도달하면 넘어짐을 요청한다. 기본 입력만 유지할 때 목표 기울기는 0.6(메시 목표 약 21도)이고 넘어짐 한계는 약 60도다. 평상시의 자동 복귀가 충돌 직후까지 적용되지 않도록 아래 불안정 상태를 사용한다. 실제 충돌 강도는 플레이 조정이 필요하다. 30/60/120 FPS의 별도 수치 시뮬레이션에서 10초 입력 후 10초 해제 시 복귀와 한계 근처의 외부 충격에 의한 한계 초과를 확인했다. 게임 빌드·PIE 검증은 수행하지 않았다.

<a id="unstable-balance"></a>
### 강한 점프·충돌 뒤의 불안정 상태

2026-11-06 변경: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋. 서버는 충전량이 `StrongJumpChargeThreshold`(기본 0.5) 이상인 방향 점프 또는 실제 충돌 반응 뒤 `UnstableBalanceDuration`(기본 2초) 동안 기존 누적 기울기 계산을 적용한다. 일반 자동 홉·제자리 점프·약한 차징 점프는 새 불안정 상태를 만들지 않는다. 강한 방향 점프는 발사 방향을 캐릭터 로컬 기울기 축으로 변환해 `StrongJumpTiltImpulse`(기본 1.2) × 정규화 충전량만큼 기울기 속도를 더한다. 충돌은 기존 충격량을 유지하며 양쪽 캐릭터가 불안정 상태에 들어간다. 충돌 쿨다운에서 거절된 접촉은 시간을 갱신하지 않는다.

불안정 상태에서는 입력 가속도(기본 3), 반대 입력 증폭(2배), 바깥쪽 관성 제동(6), 기울기 중력(2), 감쇠(1.5)를 사용하며 중심 자동 복귀는 중단한다. 재발생 시 남은 시간을 기본 지속 시간까지 갱신하고 시간을 누적하지 않는다. 타이머는 서버에서 계산 분할마다 감소하며 충전 시작·착지로 취소하지 않는다. 만료 후에는 현재 기울기·속도를 유지한 채 안정 스프링으로 돌아가므로 순간 자세 초기화나 무적 구간은 없다. 탈락·라운드 초기화·종료 시 타이머를 지운다. 클라이언트는 복제된 `BodyTilt`를 표시하며 타이머를 독립 계산하지 않는다.

수치 계산에서 30/60/120 FPS 모두 평상시 10초 입력은 안전 목표 0.6에 머물렀고, 중립 자세에 1.2의 기울기 충격을 주면 약 1.81초에 한계를 넘었다. 짧은 반대 입력으로 버티는 사례와 약한 충격 뒤 복귀도 확인했다. 이는 별도 수치 시뮬레이션이며 실제 게임 충돌·네트워크·애니메이션 검증은 아니다.

<a id="jump"></a>
## 차징 점프와 자동 홉
> 차징 범위 조정: 2026-10-30 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

> 최근 기능 변경: 2026-10-02 · @sunsi-game (`7ed2786`, `2ee4755`), 서버 입력·상태 복제: @KyuminChung (`c8a7452`)

> 점프 조준 분리: 2026-11-06 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

JumpCharge(기본 SpaceBar 또는 LeftMouseButton)를 누르면 충전하고 놓으면 점프한다. 서버는 충전 시작·충전 중 Tick·발사 시 `RawTiltInput`의 WASD 입력을 정규화해 `JumpAim`에 저장한다. 방향키를 놓으면 마지막 조준을 유지하고, 새 충전에서는 초기화하므로 방향 입력 없이 충전하면 제자리 점프한다. 조준은 WASD 축 기준이며 발사 시 시점의 수평 방향(`GetControlRotation().Yaw`)으로 변환한다. 일반 이동과 같은 기준을 사용한다. 입력 Y의 부호에 맞춰 `-RightDir`를 사용한다. 대각선도 정규화해 직선보다 강해지지 않는다. 발사·탈락·라운드 상태 초기화 때 조준을 지운다.

현재 작업 트리의 MinJumpPower 100, MaxJumpPower 400, ChargeRate 700이다. 충전 시작 시 파워는 100이고 약 0.429초 후 400에 도달한다. 이 파워 범위는 2026-11-05 작업 시작 전에 존재하던 미커밋 수정(작성자 확인 필요)을 보존한 값이며 BP 오버라이드는 미확인이다. 정규화 게이지는 이 범위를 0~1로 표시한다. 발사 속도는 수평 조준 방향 × 파워 × 0.95와 위쪽 방향 × 파워의 합이다. 방향 입력이 없으면 수평 성분은 0이다. `BodyTilt`는 점프 방향·높이에 관여하지 않는다. 충전 중에도 기울기 입력은 반영하지만 MoveForward/MoveRight의 AddMovementInput은 생략한다. 현재 StartJumpCharge는 상태 검사 전에 StopMovementImmediately와 입력 벡터 소비를 호출하므로 공중 수직 속도도 즉시 정지한다. 공중 재충전·발사 제한은 없다. 착지 후 `max(LandingRecoveryTime, MinimumLandingRecoveryTime)`만큼 회복한 뒤 자동 홉을 요청한다. 최소 회복의 새 기본값은 0.3초이며 충전·넘어짐·라운드 입력 잠금은 자동 홉을 억제한다. 서버가 차징·홉을 처리하고 TakeoffSerial 등 표현 상태를 복제한다. 조준은 기존 Reliable 입력 RPC를 통해 서버에서 계산하며 별도 복제하지 않는다. 변경 후 빌드·PIE·멀티플레이 조작감은 미검증이다.

<a id="combat-audio"></a>
## 닭싸움 효과음

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

`AChickenCharacter`의 기본 서브오브젝트 `ChickenAudio`(`UChickenAudioComponent`)가 일반 홉, 충전 펄스, 차징 점프, 착지, 플레이어 간 충돌을 구분한다. 기본음은 외부 에셋 없이 24kHz 모노 PCM으로 생성하는 짧은 전자 효과음이다. 충전은 복제된 충전 상태에서 0.24초마다 낮은 음량의 펄스를 재생하고 충전량에 따라 음높이가 올라간다. 해제·탈락·라운드 잠금에서는 페이드아웃한다.

홉·발사·착지·충돌은 서버가 확정한 사건을 Unreliable NetMulticast로 전달하므로 패킷 유실 시 소리는 생략될 수 있으며 게임 판정에 영향은 없다. 충돌은 기존 쌍별 쿨다운을 통과한 한쪽에서 한 번만 재생한다. 충돌은 접근 속도, 착지는 착지 시 수직 속도에 따라 음량이 달라진다. 전용 서버는 소리를 생성하지 않는다.

BP 캐릭터의 `ChickenAudio` 컴포넌트에서 `SoundOverrides`에 이벤트별 짧은 단발 `SoundBase` 에셋을 넣어 교체할 수 있다. Charge도 반복 루프 에셋이 아닌 짧은 펄스용 단발음을 사용한다. 기본 `MasterVolume=0.65`, `bEnableSynthesizedSounds=true`이며 합성음을 끄면 지정한 에셋만 재생한다. 공간 감쇠는 반경 250cm, 감쇠 거리 1800cm다. 이벤트 종류별 하나의 음성만 유지하고 재발생 시 재시작하며, 사용자 음원은 최대 5초로 제한한다. 절차적 음원의 버퍼 소진 후 명시적으로 정지·참조 해제하고 EndPlay에서 오디오 컴포넌트를 제거한다.

코드와 로컬 UE 5.8.2 선언을 확인했다. 에디터 청음·멀티플레이 검증은 미수행이다.

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

## 후속 범위와 제약

HP·피해량·인터넷 서비스 연결은 후속 범위다. 로컬 리슨 서버 검증을 외부 인터넷·패키징 검증으로 해석하지 않는다. 병합 시 dev의 `ABP_Chicken`·`Test_CF_Knockdown` 변경을 그대로 유지한다.
