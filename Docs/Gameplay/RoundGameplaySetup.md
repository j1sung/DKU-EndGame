# 네 라운드 경기 GameMode 준비

> 2026-11-08 · Codex 수행 · 요청자 GitHub ID 확인 필요
>
> 상위: [경기 상태와 라운드](Overview.md#match) · 선행: [스폰 위치 적용](RoundSpawns.md)

## 받은 작업과 현재 상태

### Round2 임시 단독 테스트 (2026-11-10)

같은 날 후속 요청으로 `ACFKnockdownTestHUD::DrawHUD`의 테스트 제목·조작키·넘어짐 상태 안내와 배경 패널을 제거했다(Codex 수행 · 미커밋). HUD의 `R` 캐릭터 리셋 입력은 유지한다. 이 HUD를 사용하는 다른 테스트 맵에도 안내 제거가 적용되며, 정식 경기 UI와 `stat`/VSM 진단 출력은 변경하지 않는다.

Codex 수행 · 미커밋. 사용자 요청으로 `L_Round2`의 World Settings → GameMode Override만 `/Script/DKUEndGame.CFRoundGameMode`에서 `/Script/DKUEndGame.CFKnockdownTestGameMode`로 임시 변경했다. 기존 닭 캐릭터·ThirdPerson 컨트롤러와 테스트 HUD를 사용하여 대기창·4인 시작 조건 없이 바로 플레이한다. 저장 후 PIE에서 닭 캐릭터와 테스트 HUD가 나타나고 대기창을 우회하는 것을 확인했다. 다른 라운드의 모드는 변경하지 않았다.

원상복구는 **현재 맵의 GameMode Override만 `CFRoundGameMode`로 되돌리고 저장**한다. 이후의 배치 수정을 보존하기 위해 맵 전체를 백업으로 교체하지 않는다. 변경 전 디스크 사본은 `Saved/Round2QuickTest/L_Round2_before_disk.umap`이며, 당시 에디터에 있던 미저장 배치 수정까지 포함한 백업은 아니다. 임시 모드에서는 정식 경기·점수·라운드 진행을 검증하지 않는다. 아래의 네 맵 경기 모드 설명은 이 임시 예외를 제외한 정식 설정이다.

선행 커밋 `8c50f59`에서 Unreal 5.8.4의 네 맵 ArenaSpawn 4개 적용·저장·재로딩과 재조회를 완료했다. 해당 시점 Round1·2의 GameMode Override는 없었고 Round3·4는 테스트용 GameMode였다.

원격 커밋 `59c9f30`의 `ACFRoundGameMode`와 설정 도구를 받은 뒤, 2026-11-08 이 PC의 Unreal 5.8.4에서 정식 Editor 빌드와 네 맵 적용·저장·재로딩을 완료했다. 빌드에서 발견한 지역 변수 `Children`과 `AActor::Children`의 이름 충돌은 `AttachedActors`로 수정했다. **각 맵의 로컬 4인 생성과 첫 카운트다운까지 검증했으며, 전체 경기·맵 전환·EOS 연결 완료는 아니다.**

적용 전 네 맵과 상태 JSON은 `Saved/RoundGameplaySetup/20261108_212804_165375/`에 백업했다. 네 맵의 GameMode Override는 `/Script/DKUEndGame.CFRoundGameMode`이며, 각 맵의 편집용 닭 캐릭터 4개에 `CFRoundSpawnPreview`와 `net_load_on_client=false`를 적용했다. 저장·재로딩 검사에서 모든 액터의 위치·회전·크기, 스폰 및 기존 캐릭터 태그 보존을 확인했다.

## 책임과 범위

2026-11-09 후속 [다중 맵 경기 코드](MatchPresentationAndTravel.md)는 EOS 배정 문맥에서 네 맵을 무중복 랜덤 순서로 이동하고 걷기 준비를 숨기며 각자 결과 확인 후 연습 로비로 복귀하도록 확장했다. 아래 초기 스폰·같은 프로세스 검증 기록은 선행 결과다. 새 코드의 Editor 빌드·Seamless Travel·색상 에셋 적용은 아직 확인하지 않았다.

| 위치 | 역할 |
|---|---|
| `Network/CFRoundGameMode.h/.cpp` | 기존 경기 코드 상속, 정원/최소 인원 4, 네 스폰 예약·퇴장 시 반환, 서버의 명시적 미리보기 정리 |
| `Tools/Editor/configure_round_gameplay.py` | 조회와 명시적 적용, 네 맵 백업·저장·재로딩, GameMode·표식·클라이언트 로드 정책 연결 |
| `Tools/Editor/tests/test_round_gameplay.py` | 모호한 모드/표식/소유/부착/스폰 거절과 네트워크 로드 정책 계획 검사 |
| `Tests/CFRoundSetupTests.cpp` | `ChickenFight.RoundSetup.SpawnReservations` — 슬롯 선택 정책 검사. 실제 네트워크 예약 생명주기 검사를 대신하지 않음 |

기존 `BP_CF_WaitingCharacter`·`BP_CF_ArenaController`를 참조해 입력·Designer UI를 재사용한다. 기존 LAN 아레나 GameMode는 수정하지 않는다. 초기 Pawn은 ArenaSpawn 네 곳만 선택하고 Controller마다 예약한다. 전투 Pawn 교체·점수·탈락은 기존 코드다.

일반 맵/PIE 검사는 **호스트 수동 시작·같은 월드에서 4라운드 진행**이다. 후속 [EOS Round1 연결 코드](../Network/EOSRound1Session.md)를 추가해 배정 문맥으로 열린 Round1에서는 PUID 명단 검사와 4인 자동 시작을 적용한다. 새 EOS 코드의 Editor 빌드·실계정 실행은 아직 검증하지 않았다. 아래 선행 검증은 각 맵의 초기 생성과 첫 카운트다운까지다. 최종 종료 보고·라운드별 맵 전환·점수의 월드 간 보존은 후속 작업이다.

## 배치 캐릭터 보존과 런타임 정리

편집용 캐릭터 4개는 에셋에 남긴다. 도구는 기존 태그를 보존해 `CFRoundSpawnPreview`를 추가하고 `net_load_on_client=false`를 설정한다. 이는 원격 클라이언트가 맵에서 미리보기를 로드하지 않도록 하는 정책이다. 서버 초기 Destroy의 전파에만 의존하지 않는다. [Epic Actor API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor) 참고.

GameMode는 서버 `InitGame`에서 태그된 닭 캐릭터만 제거한다. 태그 없는 캐릭터, Controller/Owner가 있는 캐릭터, 부착 부모/자식이 있는 캐릭터, 불완전한 표식은 임의로 지우지 않고 시작을 거절한다. 클라이언트 로드가 켜진 미리보기도 거절한다. 정상 미리보기 수는 0 또는 4다. 에디터 원본의 위치·크기·회전·자동 소유 설정과 네 스폰은 변경하지 않는다.

## Unreal PC 적용

1. 이 코드가 있는 브랜치를 받고 에디터를 종료한 상태에서 최신 Editor 대상을 빌드한다. 새 클래스는 Live Coding만으로 검증하지 않는다.
2. 에디터로 프로젝트를 열고 Play/Simulate를 종료한다. 미저장 작업은 직접 정리한다. Python 플러그인은 기존처럼 `-EnablePlugins=PythonScriptPlugin`으로 해당 실행에서만 활성화할 수 있다.
3. Output Log의 Python 입력에서 한 줄씩 조회한다.

```python
import runpy, unreal
round_setup = runpy.run_path(unreal.Paths.project_dir() + "Tools/Editor/configure_round_gameplay.py")
round_setup["run"]()
```

네 `CF_ROUND_GAMEPLAY_AUDIT`에서 기존 모드와 네 캐릭터 대상을 확인한다. 기본 조회는 저장하지 않는다. 알려진 없음/테스트용/기존 아레나/이미 적용된 RoundGameMode만 허용한다. 소유·부착·불완전 표식·스폰 위치 불일치는 중단한다.

```python
round_setup["run"](apply=True)
```

모든 사전 검사 후 `Saved/RoundGameplaySetup/<시각>/`에 원본 네 맵과 상태 JSON을 백업한다. GameMode Override를 `/Script/DKUEndGame.CFRoundGameMode`로 바꾸고 네 캐릭터에 표식·클라이언트 로드 제외만 지정한다. 모든 액터의 위치·회전·크기와 원본 캐릭터/스폰 및 기존 태그 보존을 확인한다.

네 `CF_ROUND_GAMEPLAY_SAVED`와 최종 완료를 확인한다. 중간 오류는 남은 적용을 중단하며 앞서 저장한 맵을 자동 원복하지 않는다. 실패한 맵에 미저장 변경이 남을 수 있으므로 백업·로그를 확인하고 임의 전체 저장하지 않는다.

## 스폰 검증

- 각 맵에서 편집용 캐릭터 4개·ArenaSpawn 네 곳·GameMode Override를 확인한다.
- 로컬 Play를 플레이어 4명/Listen Server로 실행한다. EOS 로그인·매칭 검증과 별도인 로컬 네트워크 검사다.
- 서버의 `LogCFRoundSetup`에 `ArenaSpawns=4 PreviewsRemoved=4`가 나오는지 확인한다. **호스트와 원격 클라이언트 양쪽**에서 표식 미리보기가 0개이고, 각 참가자가 대기 Pawn 하나씩 서로 다른 위치에 생성·소유하는지 확인한다. 서버 로그만으로 원격 정리 성공을 판정하지 않는다.
- 실제 바닥·벽·기믹 겹침과 시작 방향을 확인한다. 3명 이하에서는 시작을 거절하고 4명 접속 후 호스트 수동 시작 시 기존 3초 카운트다운·전투 Pawn 교체가 작동해야 한다.
- 첫 카운트다운까지만 확인한 뒤 종료한다. 현재 코드를 맵별 전체 경기 완료로 판정하지 않는다.
- `ChickenFight.RoundSetup.SpawnReservations` 검사도 실행한다. 이 PC에서는 작성만 했다.

## 이 PC에서 확인한 결과

- Unreal 5.8.4 Editor 정식 빌드 성공, Python 보호 검사 19개 및 C++ `ChickenFight.RoundSetup.SpawnReservations` 통과.
- 일반 Editor를 `-nullrhi -unattended`로 실행해 네 맵 적용·저장·재로딩 완료. 이후 다인 검사는 실제 PIE 월드와 네트워크를 사용했다.
- 같은 프로세스의 Listen Server와 PIE 클라이언트로 맵마다 3인/4인, 총 8개 시나리오 통과. 서버 로그에서 `ArenaSpawns=4 PreviewsRemoved=4`를 확인했고, 서버와 모든 클라이언트 월드에서 미리보기 0개와 참가 인원만큼의 대기 Pawn을 확인했다. 3인은 호스트 시작 요청 후 Waiting 유지, 4인은 전투 Pawn 4개로 전환되고 전 월드가 Countdown으로 복제됐다. Playing 진입 전 종료했다.
- 추가 4인 검사에서 모든 월드의 로컬 Controller가 Pawn 하나씩 소유하고 서버의 참가자별 Pawn이 각각 다른 스폰에 배정됨을 확인했다. 대기/전투 Pawn의 XY 오차 1cm 미만, 서버 ControlRotation의 Yaw 오차 1도 미만을 검사했다. 바닥 정착으로 Z는 달라지며, Round2 한 지점은 대기 약 -14.5cm·전투 약 -16.5cm였다. 수평 이탈은 없고 각 캐릭터의 MaxStepHeight 이내였다. 클라이언트 화면의 카메라 방향을 시각 확인한 것은 아니다.
- 검사 자료: `Saved/RoundGameplaySetup/pie_3_and_4_results.json`, `pie_results.json`, `pie.log`, `pie_poses.log`, `automation.log`. Python/EditorScriptingUtilities 플러그인은 해당 검사 실행에서만 활성화했으며 프로젝트 플러그인 설정은 변경하지 않았다.

실제 렌더링·입력 조작감·경기 끝까지의 진행·퇴장/재입장에 따른 예약 반환·별도 프로세스/다른 PC·EOS·패키징은 미검증이다. 기존 공통 시작 거절 안내에는 여전히 “2명 이상”이라는 문구가 있어, 라운드 GameMode의 실제 최소 4인 조건과 문구를 맞추는 보완이 남아 있다.
