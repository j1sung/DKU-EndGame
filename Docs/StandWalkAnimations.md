# 일반 서기 · 걷기

기존 White Round v2 리그와 웨이트를 사용하는 반복 애니메이션 두 개입니다.

| 파일 | 동작 | 길이 | 샘플 | 반복 |
|---|---|---|---|---|
| fbx/AN_CF_STAND.fbx | 두 발로 서서 작게 호흡 | 2초 | 61프레임 | 예 |
| fbx/AN_CF_WALK.fbx | 양발 교대, 반대쪽 팔 스윙을 갖는 제자리 전진 걷기 | 1초 | 31프레임 | 예 |

- 30fps, 루트 이동/회전 고정. 실제 이동은 게임 코드에서 처리합니다.
- 기존 모델·스켈레톤·웨이트를 유지했습니다. 새 스켈레톤 반입은 필요 없습니다.
- 편집용 `CH_White_Round_Complete19.blend`에는 기존 17개 액션과 새 2개 액션이 함께 들어 있습니다. 기존 액션의 키 데이터 해시가 유지되는 것을 확인했습니다.
- Blender에서 Space로 서기와 걷기 미리보기를 재생합니다. 다른 동작을 볼 때는 NLA 미리보기 트랙을 하나만 켜세요.
- FBX는 변형 뼈 28개를 포함하며 IK 컨트롤과 메시를 포함하지 않습니다. Unreal에는 기존 클립과 동일하게 armature 노드를 포함한 29개 트랙으로 들어갑니다.

## Unreal 반입 완료

두 AnimSequence를 `/Game/Characters/WhiteRound/Animations/`에 추가했습니다.

- 이름: `AN_CF_STAND`, `AN_CF_WALK`
- Skeleton: `SK_White_Round_Skeleton`
- Root Motion: 꺼짐
- 두 클립의 기존 Legacy FBX 재임포트 설정: **Import Uniform Scale = 100**. 기존 스켈레톤의 `rig_white_round` 루트 배율(100)에 맞췄으며 메시 컴포넌트 Scale은 1로 유지합니다. 이 FBX를 Legacy 방식으로 다시 가져올 때도 100을 사용하세요. 다른 모델에 적용하는 일반 배율 규칙은 아닙니다.
- Content Browser에서 각 애니메이션을 더블클릭해 미리볼 수 있습니다.
- 아레나 대기 캐릭터의 `ABP_CF_Waiting`에 Stand/Walk 전환 연결 완료. 관전 상태 배치는 별도 후속 작업입니다.

걷기는 몸이 진행 방향을 바라보는 전진 동작입니다. 후진/옆걸음 전용 클립은 아닙니다. 1배속에서 발이 지면을 미는 구간의 속도는 약 **65.75cm/s**입니다. 게임 이동 속도를 이 기준에 맞추거나 재생 속도를 함께 조절해야 발 미끄러짐을 줄일 수 있습니다. STAND/WALK 블렌딩은 시작값으로 0.15~0.2초를 사용할 수 있습니다.

## 확인 결과

- Blender 저장본 재열기 및 FBX 재반입: 전체 92프레임 확인.
- 기존 17개 액션 보존, 28개 변형 뼈 유지, IK 도달 오차와 지면 관통 검사 통과.
- Unreal에서 저장된 두 AnimSequence 재열기 및 전체 92프레임 검사.
- 기존 Idle과 뼈 트랙 이름 전체 일치, 동일 WhiteRound 메시로 포즈 평가 성공.
- 크기 수정 후 92프레임의 루트 배율 100 및 나머지 모든 뼈의 기존 위치·회전·스케일 보존 확인. PIE에서 서기/걷기 모두 정상 크기 확인.
- 루프 시작/끝 일치, 루트 이동 없음, 양쪽 다리·팔에 걷기 동작 존재.
- `previews/Stand_Walk_Compare.gif`: 실제 모델 렌더 비교 미리보기.

검사 기록: `validation.json`, `export_validation.json`, `unreal_import_validation.json`, `unreal_pose_validation.json`.

FBX 원본은 게임 저장소 외부의 이 폴더에 보관하고, 게임 저장소에는 `.uasset` 두 개를 추가했습니다.

## 아레나 적용

`Lvl_CF_Arena`의 `BP_CF_ArenaGameMode`는 `BP_CF_WaitingCharacter`를 생성합니다. `IMC_CF_Waiting`으로 WASD를 IA_Move에 연결하고, 실제 이동 속도에 맞춰 `CFWaitingAnimInstance`가 걷기 재생 속도를 계산합니다. 기본 속도 110cm/s에서 약 1.673배속이며 Stand/Walk 전환은 0.15초입니다. 상세 설정과 검증은 [WaitingRoomUI.md](WaitingRoomUI.md)를 참고하세요.

전환은 Standard Blend이며 양쪽 전환의 `Allow Inertialization for Self Transitions`를 껐습니다. UE 5.8에서 새 전환 생성 시 이 옵션이 켜지면 빠르게 걷기/서기로 재진입할 때 Inertialization 노드 누락 경고가 발생할 수 있습니다.
