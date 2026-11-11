# 플레이어 식별과 네 맵 랜덤 경기

> 최근 기능 변경: 2026-11-10 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위: [경기 상태](Overview.md#match) · [Network](../Network/Overview.md#eos-round1) · 선행: [EOS 연결](../Network/EOSRound1Session.md), [네 맵 준비](RoundGameplaySetup.md)

## 사용자 확인과 이번 변경

사용자는 선행 `codex/eos-session-integration`에서 실계정 4인 접속·플레이·같은 월드의 4라운드와 화면 종료 흐름 성공을 보고했다. 이후 빌드/EOS 주소 수정까지 PR #51로 dev에 병합했고 `3ffa8e6` 기준 `codex/match-presentation-and-round-travel`에서 후속 코드를 작성했다. **이번 새 다중 맵 코드의 Unreal 빌드·실행 결과는 아니다.**

이번 요청은 머리 위 닉네임, 서로 다른 랜덤 색상, 네 실제 맵의 무중복 랜덤 순서, 매칭 후 경기 전 걷기 제거다. 추가 선택은 ‘각자 최종 결과 확인 후 연습 로비 복귀’이며 로그인 후 연습 로비는 유지한다.

## 구성

| 위치 | 책임 |
|---|---|
| `CFMatchRuntimeSubsystem` | 서버 맵/색상 순서, 라운드 이동 전 점수·PlayerId·닉네임 사본, 클라이언트 최종 결과와 독립 화면 |
| `CFRoundGameMode` / `CFWaitingNetwork` | Seamless Travel, 서버 승인 4인 로드 확인, 점수 복구와 다음 라운드 준비, 결과 RPC/ACK, 이동 후 Controller UI 재설정 |
| `CFMatchSessionSubsystem` | 최초 맵 광고·검증, 기존 세션 유지, 첫 joined/started와 마지막 finished, 종료·실패 정리 |
| `CFPlayerPresentationComponent` | 복제된 색상 슬롯으로 MID 갱신, PlayerState의 현재 이름을 화면 공간 UMG 이름표로 표시 |
| `CFMatchStatusWidget` | 걷기 준비 UI를 숨기고 불투명 접속 준비 화면 표시, 기존 카운트다운·경기 HUD 재사용 |
| `CFCompletedMatchWidget` | 세션/월드와 독립적으로 받은 최종 결과 표시, 종료 정리 후 본인 확인 버튼 |
| `configure_player_tint.py` | 기존 머티리얼의 BaseColor 출력에 흰색 기본 Tint를 곱하는 보존형 에디터 적용 도구 |

## 흐름과 경계

2026-11-10 QA 보완: 로컬 이름표는 인증된 게임 프로필 이름, 원격 이름표는 서버 PlayerState 이름을 사용한다. 로비는 PlayerState에도 프로필 닉네임을 적용해 첫 등장·프로필 갱신·R키 부활 후 일관되게 표시한다. EOS 활성 세션 또는 자동 경기 GameState 중 하나라도 확인되면 기존 수동 대기 창을 사용하지 않는다. 복제 지연으로 자동 플래그가 아직 false인 순간과 Blueprint의 대기 위젯 등록도 차단하고, 접속 준비 화면만 유지한다. 신규 `ChickenFight.MatchRounds.NicknameAndWaiting` 검사는 작성했으나 Unreal 미설치로 미실행이다.

재확인: Epic 표시 이름과 다른 게임 닉네임으로 로그인 → 로비 이름표·R키 부활·결과 후 복귀 확인 → 최초 접속과 2~4라운드 이동 때 기존 참가자 목록·시작/방 나가기 대기 창이 한 프레임도 보이지 않는지 확인한다. 정상 4인 준비·카운트다운, 이름/색상/점수 유지, 기존 LAN 수동 대기도 회귀 확인한다. 이번 변경의 빌드·실계정 테스트는 이 PC에서 수행하지 않았다.

서버는 네 맵 번호와 구분 가능한 파랑·주황·자홍·초록 4색을 각각 한 번 섞는다. 맵 순서는 예를 들어 1→4→2→3이 될 수 있고, 첫 라운드가 반드시 L_Round1일 필요가 없다. 라운드 번호는 진행 순서이며 화면의 ROUND 1~4를 따른다. 색상은 서버의 멤버 PUID별 배정으로 정하고 Pawn 재생성·맵 이동에도 유지한다. 이름은 복제 PlayerState에서 읽어 늦은 이름 갱신도 반영한다.

세션 설정 `CF_FIRST_MAP`과 `CF_FLOW_PROTOCOL=2`를 광고한다. 클라이언트는 정확한 세션·호스트·매치·버전 외에도 프로토콜과 맵 whitelist를 확인한다. 이동 승인 URL에도 `CFProtocol=2`를 넣는다. 백엔드 mode/version은 기존 `standard`/`0.1.0`이며 서버 배포 변경은 없다. **구버전 실행 파일과 섞지 말고 4PC 모두 이 브랜치로 빌드한다.**

최초 경기장과 이후 경기장에서는 숨긴 대기 Pawn의 서버 이동을 비활성화한다. 접속 준비 화면 뒤에서 실제 4인 로드·소유 Pawn을 확인하고 3초 카운트다운으로 전환한다. 기본 NULL/LAN 검사는 기존 수동/걷기 대기를 유지한다.

각 라운드 결과 표시가 끝나면 서버가 누적 점수·PlayerId·닉네임을 저장하고 `ServerTravel`로 다음 맵에 이동한다. `bUseSeamlessTravel`로 기존 연결을 유지하며 새 GameState에 점수를 복구한다. 서버 `HandleSeamlessTravelPlayer`와 로컬 `NotifyLoadedWorld(bFinalDest)`에서 신원·화면을 복구한다. 네 명의 다음 맵 준비 최대 시간은 60초다. 새 EOS 세션이나 joined/started 보고를 라운드마다 만들지 않는다. [Epic Seamless Travel 설명](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine) 참고.

4라운드 최종 결과는 reliable RPC로 각 클라이언트 GameInstance에 복사하고 4명 ACK 이후 호스트가 멱등 `/finished`를 보고한다. ACK는 소유 Controller의 실제 EOS PUID로 확인하고 최대 60초를 기다린다. 수신 실패·중간 이탈은 정상 완료로 꾸미지 않고 기존 전체 실패로 처리한다. 정상 종료 확인과 로컬 세션 정리가 끝나면 연습 로비를 로드하되 불투명 결과 화면은 각 PC에 남긴다. 본인이 확인해야 로비 UI를 다시 사용할 수 있다. 따라서 호스트가 먼저 확인해도 다른 사람의 결과는 유지된다.

인증 만료 시 결과 사본을 지우지 않고 덮개를 잠시 내려 재로그인할 수 있게 한다. 같은 계정으로 서버 종료를 확인하고 정리가 끝나면 결과 화면을 다시 표시한다. 결과/세션이 불명확한 상태에서 새 매칭을 허용하지 않는다.

## Unreal PC 적용과 검사

2026-11-09 Unreal 5.8.4 설치 PC에서 Editor 정식 빌드와 색상 머티리얼 적용·저장·새 Editor 재조회까지 완료했다. 복제 매크로가 요구하는 `OutLifetimeProps` 인수 이름을 `CFPlayerPresentationComponent`에 맞춰 빌드 오류를 수정했다. 네 맵 에셋은 변경하지 않았다. Tint 기본값은 흰색이다. 실제 렌더링과 실계정 4PC 검증은 별도다.

1. 동일 브랜치 소스로 Editor를 정식 재빌드한다. 새 UCLASS/컴포넌트/RPC가 있으므로 Live Coding만으로 검증하지 않는다. EOSLocal 준비/실행 명령은 [기존 절차](../Network/EOSRound1Session.md#각-테스트-pc-준비)를 따른다.
2. `-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities`로 두 플러그인을 해당 실행에서 활성화한 Editor에서 PIE와 미저장 머티리얼 편집을 종료하고 Output Log의 Python으로 조회한다.

```python
import runpy, unreal
from pathlib import Path
tool = runpy.run_path(str(Path(unreal.Paths.project_dir()) / "Tools/Editor/configure_player_tint.py"))
tool["run"]()  # 조회
tool["run"](apply=True)  # 확인 후 적용·저장
```

실제 캐릭터 슬롯 0의 `/Game/Characters/WhiteRound/Mesh/M_White_Matte`는 MaterialInstanceConstant이고, 부모는 `/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial`이다. 최초 도구가 일반 Material로 가정해 적용 전 중단된 것을 확인하고 보완했다. 공용 Interchange 부모는 수정하지 않고 프로젝트의 `/Game/Characters/WhiteRound/Mesh/M_CF_PlayerTintBase`로 복제해 기존 BaseColor 출력에 `CF_PlayerTint`(기본 흰색)를 곱한다. 기존 인스턴스는 이 부모로 연결하며 vector/scalar/texture 값과 BasePropertyOverrides는 보존한다. 다른 Material 또는 임의 부모 체인으로 우회하지 않는다.

도구는 `Saved/PlayerTintSetup/`에 원본 인스턴스 uasset을 백업한다. 이번 백업은 `Saved/PlayerTintSetup/20261109_212451_211945/`다. 같은 이름의 외부 매개변수·변형 표식·연결 없는 BaseColor·PIE·미저장 파일·기존의 알 수 없는 복제 경로를 거절한다. 적용 실패 시 이미 생성된 부모 에셋이나 미저장 변경이 남을 수 있으므로 임의 전체 저장·재적용하지 말고 Undo/백업과 로그를 확인한다. 다른 PC에 전달할 에셋은 **수정된 `M_White_Matte.uasset`과 새 `M_CF_PlayerTintBase.uasset` 두 개**다. 브랜치에 반영하기 전에는 다른 PC에 전달되지 않는다.

3. Automation에서 `ChickenFight.MatchRounds`, `ChickenFight.EOS`, 기존 `ChickenFight.Matchmaking`, `ChickenFight.RoundSetup.SpawnReservations`를 확인한다. 이 PC에서는 새 `MatchRounds.Plan`·`MatchRounds.ResultCache` 두 개를 실행해 통과했다. 나머지 검사를 이 브랜치에서 재실행한 것은 아니다.
4. 실제 다른 Epic 계정 4PC로 아래를 확인한다.

| 검사 | 성공 기준 |
|---|---|
| 최초 접속 | 연습 로비 유지, 매칭 후 걷기/UI 노출 없이 준비 화면, 4인 로드 후 카운트다운 |
| 식별 | 한글 이름표가 각자의 프로필과 일치, 네 색 중복 없음, 모든 화면의 색상 일치 |
| 맵 순서 | 4라운드 동안 L_Round1~4 각각 한 번, 네 화면의 같은 맵/라운드 |
| 상태 이전 | 각 맵의 네 스폰, 이름·색·PlayerId·누적 점수 유지, 결과/탈락/관전 정상 |
| 다음 맵 준비 | 느린 클라이언트가 있는 동안 3인 시작 금지, 60초 실패 후 복구 |
| 최종 종료 | 4명 결과 수신 후 finished 200와 FINISHED 조회, 각자 확인해 로비 복귀 |
| 호스트 먼저 확인 | 다른 PC 결과 유지, 남은 사람이 확인 후 로비와 새 빠른 대전 사용 가능 |
| 재경기 | 새 매칭 정상, 이전 결과/점수 없음, 맵/색상 재배정 |
| 실패 | 이동 중 종료·인증 만료·호스트 연결 끊김이 정상 완료로 처리되지 않음 |

Editor 검사가 패키징 검증을 대신하지 않는다. 현재 저장소에는 명시적인 `MapsToCook` 목록이 없으므로 패키징 시 새로 문자열로 선택하는 네 맵의 cook 포함을 반드시 구성·검증해야 한다. 이번 작업에서는 패키징 설정이나 패키지 빌드를 변경/실행하지 않았다.

## 검증 기록과 남은 작업

- 2026-11-09 사용자는 재빌드 후 실계정 4인 경기가 시작됐지만 첫 라운드 종료 후 이동 중 클라이언트가 크래시했다고 보고했다. 로그에서 Seamless Travel 직후 `CreateWidget cannot be used on Player Controller with no attached player`와 `UpdateMatchDisplay` 크래시를 확인했다. HUD/입력 갱신은 유효한 월드와 현재 LocalPlayer 소유 Controller에서만 수행하고, 위젯 생성·화면 등록 실패 시 다음 타이머에서 다시 시도하도록 수정했다. 플레이어 분리·Controller 교체·재연결·월드 종료 회귀 검사는 수정 전 실패, 수정 후 통과했다. Editor 정식 빌드와 `ChickenFight.MatchRounds` 3개 검사 통과. 실제 EOS 4PC 전체 경기 재검증은 남아 있다.
- 추가 로컬 2인 PIE(`-nullrhi`)에서 최초 호스트·클라이언트 HUD의 소유자와 Viewport 등록을 확인했다. 수동 Seamless Travel은 서버 목적지 로드까지 관찰했지만 양쪽 이동 완료 검사는 시간 제한에 걸려 통과로 기록하지 않는다. 실제 매칭 배정·4인 경기·렌더링을 대체하는 검사가 아니다. 자료: `Saved/Logs/MatchDisplay-green.log`, `MatchDisplay-pie.log`, `MatchDisplay-pie.json`.
- 초기 코드 작성 시 Python 보호 검사 25개 통과(기존 스폰/경기 19 + Tint 계획 6). 이번 실제 Material Instance 지원 시 정상 복제·임의 부모/기존 목적지 거절·재실행 보호를 추가해 Tint 검사 9개를 통과했다. 새 지원 검사는 수정 전 실패·수정 후 통과를 확인했다.
- native 순서/색상/결과 사본 검사 2개(`ChickenFight.MatchRounds`)를 UE 5.8.4에서 실행해 통과했다. EOS 주소 회귀 검사의 새 프로토콜 옵션은 이 브랜치에서 재실행 전이다.
- 저장된 에셋을 새 Editor에서 로드해 기존 인스턴스의 직렬화 설정을 원본 백업과 비교했다. 공용 부모의 원본 연결·85개 노드, 프로젝트 부모의 BaseColor 외 출력·BlendMode·TwoSided 보존과 노드 2개 추가(총 87개)를 확인했다. Tint 기본 흰색, Multiply와 원본 BaseColor 연결, 동적 인스턴스의 네 가지 색상 설정·조회, 머티리얼 재컴파일 오류 0개와 도구 재조회 `unchanged`를 확인했다. `-nullrhi` 검사이므로 실제 화면 외형 검증은 아니다. 자료는 `Saved/PlayerTintSetup/verified.json`·`verify.log`·`matchrounds_tests.log`에 남긴다.
- 독립 정적 리뷰의 `TObjectPtr` 형식 문제를 수정하고 클라이언트 로드 완료 UI 경로를 보완했다. 후속 리뷰에서 확정 P0/P1 추가 없음. 실제 UE API/UHT 빌드와 4PC 실행 증거를 대신하지 않는다.
- 캐릭터 원본의 비 UTF-8 주석 인코딩은 변환하지 않고 ASCII include/컴포넌트 코드만 보존 편집했다.
