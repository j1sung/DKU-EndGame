# 캐릭터 디버깅 로그와 플레이 테스트

관련 개요: [Gameplay](Overview.md#debug-logging)

> 2026-10-23 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

## 범위와 설정

2026-10-23에는 ChickenCharacter의 관측용 UE_LOG를 추가했다. 2026-10-30에는 접근 속도 기반 수평·수직 충격, 이동 275, 차징 200~700, 축별 목표 기울기 60도 한계로 변경했다. 기울기 적분·감쇠, 차징 시작의 즉시 정지와 공중 발사 허용은 유지한다. Blueprint 내부와 실제 플레이 결과는 이 문서 작성에서 검증하지 않았다.

콘솔 또는 Output Log 명령 입력에서 설정한다. 설정은 실행 프로세스 단위이며 별도 프로세스의 서버·클라이언트에는 각각 적용한다.

| 명령 | 효과 |
|---|---|
| `cf.Debug.Events 1` | 사건 로그 켜기. 기본값 1 |
| `cf.Debug.Events 0` | 사건 로그 끄기 |
| `cf.Debug.Frames 0` | 반복 로그 끄기. 기본값 0 |
| `cf.Debug.Frames 1` | 충돌 콜백·거절·쿨다운 반복 로그 |
| `cf.Debug.Frames 2` | 기울기 계산 전후와 한계 판정 결과 |
| `cf.Debug.Frames 4` | 이동 갱신마다 입력·가속·실제 속도 |
| `cf.Debug.Frames 7` | 위 세 종류 모두. 비트 합으로 조합 가능 |

Events와 Frames는 독립 설정이다. 완전히 끄려면 둘 다 0으로 설정한다. Output Log에서 LogChickenCollision, LogChickenTilt, LogChickenMovement로 필터링한다. 파일은 Saved/Logs 아래 실행 로그에 남는다.

모든 출력은 Actor, PIE 월드를 구분할 World, Frame, 월드 Time, Role, Local을 포함한다. Role은 ENetRole 숫자이며 Authority=3, AutonomousProxy=2, SimulatedProxy=1이다. 다른 프로세스의 Frame/Time은 동일한 전역 시계로 간주하지 않는다.

## 로그 읽기

### 충돌

- MovementBlocked: 서버의 MoveBlockedBy 진입 시 상대 이름·속도 차이·접촉 법선. 이동 속도 보정 전의 즉시 경로이며 Frames의 충돌 비트가 필요하다. 뒤이어 공통 Hit/PushDispatch 또는 HitSkipped가 기록된다.
- Hit: 두 Actor, 상대 컴포넌트, 상대속도, 접촉 법선. 공통 처리 진입 로그이며 MoveBlockedBy와 캡슐 OnComponentHit 양쪽에서 호출된다. 서버 권한 검사 전 관측하므로 클라이언트 캡슐 콜백도 확인 가능하다. Frames의 충돌 비트가 필요하다.
- HitSkipped: State, NotCapsule, Cooldown, ZeroDirection, ClosingSpeed. ClosingSpeed 거절은 접근 속도가 최소값보다 작은 경우다. Cooldown은 상대 이름, 경과 시간, 제한 시간을 출력한다.
- PushDispatch: 상대속도, 중심 간 수평 방향, 해당 방향에 투영한 ClosingSpeed, 접근 속도 × CollisionPushScale을 MaxCollisionPushSpeed로 제한한 수평 속도 변화와 양쪽 요청 벡터. Mode=ClosingSpeed로 계산 경로를 표시한다. Cooldown=Passed는 이 쌍이 차단되지 않았다는 뜻이다.
- PushApplied: AddImpulse에 전달한 수평·수직 합산 벡터와 TiltKick·TiltVelocity·Fallen. AppliedDeltaV는 물리 힘(N)이 아니라 질량을 무시하는 속도 변화 요청(cm/s)이다. VelocityNow는 같은 콜백 시점의 값이므로 실제 충격 반영 완료를 의미하지 않는다.
- PostPush: 충격 요청 프레임보다 뒤의 이동 갱신에서 관측한 속도와 Fallen·Pending·이동 모드. 여러 충격은 Count와 요청 합으로 묶는다. 충격만의 순수 결과가 아니라 마찰·제동·입력·다른 접촉까지 포함한 관측값이다. 이동이 비활성화된 경우 Tick에서 Observation=MovementDisabled로 기록하며, 충격 처리 완료의 증거로 해석하지 않는다. 관측 전에 Actor가 제거되면 후속 로그가 없을 수 있다.

### 기울기

- Input: W/S 또는 A/D 원시 축값, 기울기 축 부호, 키 해제, 현재 Body·TiltVelocity. Body.X는 전후, Body.Y는 좌우이며 오른쪽 입력 D는 음수다.
- ServerInput: 서버에 수신된 기울기 입력 변화. 서버는 일반 이동 입력 대신 복제된 기울기 입력을 관측한다.
- BalanceBegin/BalanceEnd: 프레임 계산 전후 Body·TiltVelocity, 입력 정규화, 계산 분할 수, 기본 입력 가속도(BaseInputAcceleration), 첫 분할 시작 시 중력 가속도, 기본 분할당 감쇠 계수(BaseDampingFactor). 역입력 중에는 축별 입력 힘 2배와 바깥 방향 관성에만 추가 제동 6이 적용되므로 기본값 로그는 실제 보정 후 가속도·감쇠가 아니다. 중력 항은 각 분할에서 다시 계산되므로 첫 값이 프레임 전체에 일정하다는 뜻은 아니다.
- RestoringForce=0: 현재 구현에는 중심 복원력이 없다. TiltDamping은 속도 감쇠이고 BodyTilt×TiltGravityStrength는 기울어진 방향으로 작용한다. InputReleased=1 이후 값을 비교해 입력 해제 후 변화를 확인한다.
- Threshold: 축별 한계 도달. 한계에 머무르는 동안 기본 로그를 반복하지 않는다. BalanceFailure는 넘어짐 요청의 Accepted와 방향을 반복 설정이 켜졌을 때 출력한다.
- State: Fallen·Pending·Round 전환. PreviousMask=255는 최초 관측이다. 상태 비트는 Fallen=1, Pending=2, RoundInputEnabled=4다. 서버의 넘어짐 전환은 즉시, 복제 수신 상태는 Tick에서 관측한다.

### 이동·점프

- Input: 입력 변화 또는 입력을 유지한 채 차단 상태가 변할 때 기록한다. BlockMask는 라운드 잠금=1, Fallen=2, Pending=4, Charging=8의 합이다. Charging은 일반 이동만 막고 기울기 입력은 허용한다.
- MovementUpdate: 실제 이동 갱신 후 속도·이동량·모드. Input은 마지막 이동 입력이고 Acceleration은 실제 MovementComponent의 가속도이므로 서버/클라이언트 비교 시 함께 본다.
- ChargePressed/ChargeReleased: 입력 처리 진입 시점과 주요 상태. 서버 요청 수신과 소유 클라이언트 입력은 Role·World로 구분한다. 진입 로그만으로 충전/발사 성공을 판정하지 않는다.
- JumpLaunch: Charged 또는 AutoHop 발사 요청, 발사 속도와 다음 TakeoffSerial. 실제 이동 시작은 Mode 및 MovementUpdate와 함께 확인한다. 이미 Falling이면 공중 재발사 때 새 Mode 전환이 없을 수 있다.
- Landed: 착지 콜백 진입. 이 시점 Mode가 Falling일 수 있으며 이후 Mode 전환과 함께 본다.
- Mode: 이전/현재 이동 모드와 CustomMode, 속도, Fallen.

## Unreal Editor 플레이 테스트

1. Development Editor / Win64로 빌드하고 프로젝트를 연다. Output Log에서 위 카테고리를 검색한다.
2. 실제 전투 Pawn이 BP_ChickenCharacter인 경기 경로를 사용한다. 메인 메뉴에서 호스트/참가 후 경기를 시작하거나, 준비된 테스트 맵의 GameMode·빙의 설정을 먼저 확인한다. Test_CF_Knockdown의 R 재생성·4구역이 현재 연결되어 있다고 가정하지 않는다.
3. 2인 충돌은 Listen Server와 클라이언트로 실행한다. Events=1, Frames=1로 설정하고 정면·측면·스침·벽 근처·공중 충돌을 재현한다. MovementBlocked → Hit → PushDispatch → 양쪽 PushApplied → PostPush를 비교한다. 이후 지연된 캡슐 Hit가 Cooldown으로 거절되어 한 쌍에 충격이 한 번만 적용되는지 확인한다. 정면 접촉에서 HitSkipped Reason=ClosingSpeed가 나오는지와 지상·공중 경로의 차이도 확인한다. 접근 속도 30 미만인 접촉은 거절되고, 더 빠른 정면 충돌일수록 밀림·수직 충격이 증가하는지 확인한다. 바닥에서 살짝 뜨는지, 몸이 밀린 방향으로 서서히 기우는지도 확인한다.
4. Frames=2로 변경하고 W/S/A/D를 각각 유지·해제·반대로 입력한다. InputReleased 이후 BalanceBegin/End를 비교하고 Threshold → State 전환을 확인한다. 대각선과 캐릭터 회전 상태도 확인한다. 한쪽으로 기운 뒤 반대 입력 시 관성이 빨리 멈추는지, 중심 통과 후 보정이 해제되는지, 입력을 놓으면 자동 복원되지 않는지 확인한다.
5. Frames=4로 변경하고 지상 및 공중에서 왼쪽 마우스를 누르고 해제한다. ChargePressed → JumpLaunch → MovementUpdate → Landed → AutoHop을 확인한다. 기존 공중 속도 정지와 반복 공중 발사도 관측 대상이다.
6. 넘어진 상태와 공중 Pending 상태에서 WASD를 누른다. Input의 BlockMask와 Fallen/Pending, MOVE_None 및 실제 속도를 확인한다. 아레나에서는 Pawn 제거 전에 관측한다.
7. 호스트/원격 클라이언트, 30/60/120 FPS 및 필요 시 네트워크 지연 조건을 비교한다. 각 실행의 시간·Actor·World·Role을 함께 보존한다.
8. 종료 전에 Frames=0으로 돌린다. 기본 설정에서 키 유지·지속 접촉으로 매 프레임 상세 로그가 발생하지 않는지도 확인한다.

컴파일 결과는 작업 보고에서 별도로 제시한다. 위 PIE·다중 사용자·지연·FPS 검증은 미실행이며, 로그가 추가됐다는 사실만으로 게임플레이 정상 동작을 보증하지 않는다.
