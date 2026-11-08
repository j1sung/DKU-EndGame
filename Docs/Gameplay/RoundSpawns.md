# 라운드 시작 위치를 PlayerStart에 연결하기

> 2026-11-08 · Codex 수행 · 요청자 GitHub ID 확인 필요
>
> 상위: [경기 상태와 라운드](Overview.md#match)

## 현재 상태

작업 브랜치 `codex/eos-session-integration`에 최신 원격 dev `e5401e9`의 `L_Round1`~`L_Round4` 배치 변경을 fast-forward 반영했다. 기존 `ACFWaitingGameMode`는 `ArenaSpawn` 액터 태그가 붙은 `PlayerStart`를 찾아 라운드 캐릭터를 생성하고, 최종 결과 후 대기 복귀 위치로도 사용한다.

`Tools/Editor/link_round_spawns.py`는 **각 맵에 배치된 닭 캐릭터 4개의 위치·회전**을 그대로 스폰 기준으로 연결하는 에디터 도구다. 2026-11-08 이 PC의 Unreal 5.8.4 Editor에서 조회 후 실제 적용했다. `L_Round1`~`L_Round4` 각각 `ArenaSpawn`·`CFLinkedRoundSpawn` 액터 태그가 붙은 PlayerStart 4개를 저장하고 재로딩 검증했다. Round1~3은 새로 생성했고 Round4는 기존 4개를 재사용했다. **스폰 에셋 적용 완료와 실제 경기 연결 완료를 구분한다.**

적용 전 백업은 `Saved/RoundSpawnLink/20261108_210436_365321/`의 네 `.umap` 및 `before.json`이다. 원본 캐릭터의 위치·회전·크기와 GameMode 설정은 보존했다. 조회 당시 Round1·2의 GameMode Override는 없었고, Round3·4는 `/Game/Testing/Knockdown/BP_CF_KnockdownTestGameMode`였다. Python 플러그인은 실행 인자의 `-EnablePlugins=PythonScriptPlugin`으로 해당 실행에만 활성화했으며 프로젝트 플러그인 설정은 수정하지 않았다.

## 변경 범위와 보호

- 기본 `run()`은 네 맵을 열어 GameMode·캐릭터 위치/회전/크기·기존 스폰과 변경 계획을 Output Log에 출력한다. 맵을 변경하거나 저장하지 않는다. 마지막에는 Round4가 열린다.
- 명시적 `run(apply=True)`만 수정·저장한다. 전 맵 조회를 먼저 통과한 뒤 `Saved/RoundSpawnLink/<시각>/`에 네 `.umap`과 `before.json`을 백업한다.
- 기존 `ArenaSpawn` 4개 또는 이전 도구 관리 지점 4개를 우선 재사용한다. 태그가 없고 PlayerStart가 정확히 4개면 재사용하며, 그 외에는 새 네 지점을 만든다. 새 지점에는 `ArenaSpawn`과 `CFLinkedRoundSpawn` 액터 태그를 붙인다. `Player Start Tag`라는 별도 속성과 구분한다.
- 캐릭터가 4개가 아니거나 서로 캡슐이 겹치면 중단한다. 스폰 태그가 일부만 있거나 서로 다른 네 지점 구성이 겹쳐 있어도 임의로 덮어쓰지 않는다. 사용자 정의/부착 부모·자식이 있는 PlayerStart·서브레벨·외부 액터 맵은 자동 적용하지 않는다.
- 미저장 맵/에셋 또는 Play/Simulate 상태에서는 시작하지 않는다. 미저장 작업을 자동 저장·폐기하지 않는다.
- 캐릭터·GameMode·기믹·지형·원본 배치는 변경하지 않는다. 기존 일반 PlayerStart도 삭제하지 않는다. 재실행은 관리 지점을 재사용한다.
- 저장 전후 스폰 4개와 원본 캐릭터 위치·회전·GameMode 보존을 검사한다. 적용 중 오류가 나면 남은 맵을 처리하지 않는다. 이미 저장한 앞 맵까지 자동 원복하지 않으며 백업 경로와 Output Log를 확인한다.

## Unreal 설치 PC에서 실행

이 도구 파일이 있는 작업 브랜치를 받은 뒤 최신 프로젝트 Editor 빌드를 완료하고 **에디터**로 프로젝트를 연다. 기존 게임 실행 도구의 `-game` 창에서는 실행하지 않는다. 내장 Python Editor Script Plugin이 꺼져 있다면 에디터의 Plugins에서 활성화하고 재시작해야 한다. 저장하지 않은 작업은 먼저 직접 정리하고 Play/Simulate를 종료한다.

Output Log 입력 모드를 Python으로 바꾼 뒤 아래를 한 줄씩 실행한다.

```python
import runpy, unreal
spawn_tool = runpy.run_path(unreal.Paths.project_dir() + "Tools/Editor/link_round_spawns.py")
spawn_tool["run"]()
```

`CF_ROUND_SPAWN_AUDIT` 4개 결과에서 맵마다 선택한 캐릭터와 좌표·기존 GameMode를 확인한다. 원본 캐릭터가 아닌 잘못된 대상이 잡히면 적용하지 않고 결과를 공유한다. 조회가 정상이고 현재 배치 위치를 사용할 경우:

```python
spawn_tool["run"](apply=True)
```

네 맵마다 `CF_ROUND_SPAWN_SAVED`가 나오고 최종 재로딩 완료 안내가 나오는지 확인한다. 에디터에서 각 맵의 `ArenaSpawn` 태그 지점 4개를 확인하고 `git status --short`로 변경 범위를 확인한다. 예상 대상은 네 라운드 맵뿐이며 다른 에셋을 임의 커밋하지 않는다.

## 별도 확인이 필요한 부분

캡슐 간 겹침 검사는 네 시작 위치끼리의 거리 검사다. 바닥·벽·기믹과의 실제 충돌, 초기 방향, PlayerStart의 BADsize 표시와 실제 4인 생성은 에디터/플레이로 별도 확인한다. 배치된 테스트 캐릭터는 그대로 남아 있으므로 아직 최종 경기용 맵 설정 완료를 의미하지 않는다. 이후 경기 GameMode 연결과 함께 미리 배치된 캐릭터의 중복 생성·자동 소유 문제를 확인해 처리한다. 라운드별 맵 전환·점수 유지·EOS 입장도 이번 도구의 범위가 아니다.

다음 순서로 진행한다.

1. 일반 에디터에서 네 맵의 `RoundSpawns` 폴더와 태그 지점 4개, 높이·방향·주변 충돌을 확인한다. 새로 만든 Round1~3 지점은 이 폴더에 있고, 재사용한 Round4 지점은 기존 이름·폴더를 유지한다.
2. 후속으로 [경기용 GameMode와 미리보기 정리 도구](RoundGameplaySetup.md)를 준비했다. 적용·실제 생성 검증은 아직 필요하다. 기존 `PrepareRound()`는 참가자가 소유한 Pawn과 `RoundPawns`만 충돌을 끈 뒤 새 캐릭터를 생성한다. 소유되지 않은 배치 캐릭터는 정리 대상에 포함되지 않아 스폰 위치 보정 또는 생성 실패가 생길 수 있다. 배치 캐릭터를 남긴 상태의 BADsize만으로 지형 충돌이라고 단정하지 않는다.
3. 실제 4인 생성·카운트다운·소유·시작 방향을 호스트와 원격 클라이언트에서 확인한다. 테스트 GameMode에서 캐릭터가 움직이는 것만으로 경기 검증을 대신하지 않는다.
4. 별도 구현으로 라운드별 맵 전환과 점수·참가자 상태 유지를 연결한다. 현재 `ACFWaitingGameMode::AdvanceRound()`는 같은 월드에서 `PrepareRound(CurrentRound+1)`만 호출하며 `L_Round1`~`L_Round4`를 전환하지 않는다. EOS 입장 연결도 별도 범위다.

## 확인한 범위

2026-11-08 엔진에 포함된 Python으로 11개 검사를 다시 실행해 통과했다. 위치·회전/입력 보존, 기존 지점 재사용, 다른 PlayerStart 보존, 재실행, 태그 대소문자, 모호한 태그 구성, 캐릭터 수·캡슐 겹침·수직 분리, 잘못된 좌표/클래스, 부착 액터 이동 위험과 미저장 작업 거절을 검사했다. Unreal Editor를 `-nullrhi -unattended`로 실행해 네 맵 조회·적용·저장·재로딩과 캐릭터/GameMode 보존 검사를 통과했고, 네 번의 `CF_ROUND_SPAWN_SAVED` 및 최종 완료를 확인했다. 일반 에디터의 시각 확인·실제 충돌·4인 스폰은 미실행이다.

```powershell
python -B -m unittest discover -s Tools/Editor/tests -p test_round_spawns.py -v
```
