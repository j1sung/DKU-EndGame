# EOS Round1 4인 접속과 자동 시작

> 2026-11-08 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위: [Network 개요](Overview.md#eos-round1) · [매치 API 계약](../EOS/unreal-matchmaking-ticket-api.md) · [맵 준비 결과](../Gameplay/RoundGameplaySetup.md)

## 현재 범위

2026-11-09 후속 구현의 현재 구조·제한은 [플레이어 식별과 네 맵 랜덤 경기](../Gameplay/MatchPresentationAndTravel.md)를 따른다. 아래 Round1 고정 흐름은 선행 연결 도입 기준이다. 선행 실계정 4인 접속·같은 월드 4라운드·화면 종료 성공을 사용자가 확인했으며, 새 코드의 다중 맵 이동·개인 결과 복귀를 검증한 것은 아니다.

최신 원격 `codex/eos-session-integration`의 `8578b23`에서 시작했다. 기존 네 맵의 스폰·GameMode 에셋은 그대로 사용한다. 이번 코드는 배정 → EOS 호스트 생성 → Round1 리슨 서버 → 세 명 참가 → 실제 4인 준비 확인 → 3초 카운트다운 → Playing 및 `/started` 보고를 연결한다. **2026-11-08 UE 5.8.4 Editor 정식 빌드와 EOS 자동 검사 2개는 통과했으며, 실제 EOS 4인 접속은 아직 검증하지 않았다.** 선행 커밋의 같은 프로세스 PIE 검증과 구분한다.

다음 맵 이동, 월드 간 점수 보존, 최종 `/finished`, 호스트 이전·재접속은 이번 완료 범위가 아니다. 기존 같은 월드 4라운드 로직은 남아 있으므로 이 테스트는 첫 Playing까지 확인하고 종료한다.

## 소유권과 순서

| 코드 | 책임 |
|---|---|
| `UCFMatchmakingSubsystem` | 배정 전 티켓 생성·조회·취소. 세션 계층이 배정을 인수하면 티켓 폴링을 중지 |
| `UCFMatchSessionSubsystem` | 배정 후 매치 GET/멱등 POST, EOS 세션/검색/참가, 이동과 실패 정리. GameInstance ticker로 맵 이동 중에도 진행 |
| `FCFSessionMatch` | 현재 계정·매치 ID·4명 명단·호스트·단계·시도·세션 ID 응답 검사 |
| `ACFRoundGameMode` | 접속 PUID 명단·중복 검사, 기존 스폰 예약, 자동 시작 승인. 수동 시작 RPC의 우회 차단 |
| `ACFWaitingPlayerController` / 기존 UI | 프로필 닉네임 전달, 참가자·카운트다운·Playing 표시. EOS 자동 경기에서 수동 시작 버튼 숨김 |

1. 배정 후 GET `/v1/matches/{matchId}`로 `memberIds`·`hostPlayerId`·`hostAttempt=1`을 확인한다. 4명 명단에 현재 계정과 호스트가 있어야 한다.
2. 지정 호스트만 EOS `NAME_GameSession`을 생성한다. LAN/presence/lobby/friend 초대를 사용하지 않고 정원 4·광고 활성화·진행 중 참가 금지로 설정한다. 검색 속성은 `CF_MATCH_ID`, `CF_CLIENT_VERSION=0.1.0`이다.
3. 호스트가 `/Game/Map/L_Round1?listen`을 열고 실제 EOS NetDriver·자기 소유 Pawn·GameState를 확인한 뒤 `/session`에 실제 세션 ID를 등록한다. 이동 옵션에는 매치 ID·시도 번호만 넣는다.
4. 나머지는 매치 속성으로 EOS 세션을 검색한다. 검색 결과의 **정확한 세션 ID·소유자 PUID·매치 ID·버전**을 검증한 뒤 JoinSession과 resolved connect string으로 접속한다. 친구 목록 권한은 필요 없다.
5. 각 클라이언트는 Round1의 실제 네트워크 월드·소유 Pawn·자기 EOS PlayerState를 확인한 후 `/joined`를 보고한다. 서버는 접속의 EOS UniqueNetId에서 PUID를 추출해 명단과 중복을 검사한다. 클라이언트가 URL로 자기 PUID를 주장하는 방식은 사용하지 않는다.
6. 백엔드가 `READY_TO_START`가 되고 서버가 서로 다른 승인 계정 4개의 Controller·Pawn·맵 로드 완료를 확인하면 EOS StartSession 및 기존 Round1 준비/카운트다운을 진행한다. 실제 Playing 이벤트 이후 호스트가 `/started`를 보고한다. 최대 준비 기한은 기존 서버 단계별 60초다.

쓰기 요청은 동작별 멱등 키와 본문을 유지해 재시도한다. 쓰기 응답만으로 단계를 확정하지 않고 최신 GET으로 다시 확인한다. HTTP는 하나씩 처리하며 세대·요청 순서와 계정 일치를 검사한다. Retry-After를 지키고 원문 응답·PUID·토큰·Secret은 새 애플리케이션 로그에 넣지 않는다. 엔진 자체 연결 로그에는 식별자/주소가 포함될 수 있으므로 로그 공유 전 확인한다.

## 각 테스트 PC 준비

2026-11-12 접속 시간 비교 검사(Codex 수행 · 요청자 GitHub ID 확인 필요): `Config/DefaultEngine.ini`의 `IpNetDriver.InitialConnectTimeout`을 15초에서 60초로 늘렸다. EOS의 `NetDriverEOSBase → NetDriverEOS → IpNetDriver` 상속에도 적용되며 EOSLocal 생성 도구에는 별도 timeout 덮어쓰기가 없다. 연결 완료 시 즉시 다음 단계로 진행한다. 연결 후 `ConnectionTimeout=10.0`, 매칭 서버 단계별 준비 기한 60초와 클라이언트의 서버 기한 추적은 유지하므로 매칭 전체에 60초를 추가하는 변경은 아니다. 실제 4PC 재시험은 남아 있으며, Unreal/테스트 게임을 재실행해야 새 설정을 읽는다.

Unreal 5.8 Editor와 동일한 브랜치의 소스가 필요하다. 계정은 서로 다른 네 개이며 개발 조직/테스트 접근 권한과 기존 프로필이 있어야 한다. Dev Auth Tool은 이 Account Portal 게임 테스트의 필수 도구가 아니다.

1. 프로젝트와 Unreal을 종료한다. 이 변경을 받은 뒤 프로젝트 루트 PowerShell에서 기존 설정을 갱신한다. **Secret을 다시 출력하거나 팀원에게 설정 파일을 복사하지 않는다.**

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Network\Set-EOSLocal.ps1 -Upgrade
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Network\Set-EOSLocal.ps1 -CheckOnly
```

기존 생성 도구의 정상 형식·동일 dev ID인 파일만 갱신한다. 파일이 없는 새 PC는 `-Upgrade` 없이 `Set-EOSLocal.ps1`을 실행해 Client Secret을 마스킹 입력한다. 임의 편집된 파일은 자동 변환하지 않는다.

`EOSLocal`은 기본 OSS를 EOS로 바꾸고 UE5.8의 `/Script/SocketSubsystemEOS.NetDriverEOSBase` 및 `bIsUsingP2PSockets=true`를 적용한다. 일반 실행의 기본 NULL/LAN 설정은 유지한다. **EOSLocal 실행 안에서 기존 LAN 기능의 동작을 보장하지 않는다.** [Epic UE5.8 공식 OSS EOS 설정](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine)을 따른다.

2. 설치 위치를 실제 경로에 맞춘 뒤 Editor 대상을 재빌드한다. Hot Reload/Live Coding만으로 새 Subsystem/UHT 변경을 검증하지 않는다.

```powershell
$ueRoot = 'C:\Program Files\Epic Games\UE_5.8'
$project = (Resolve-Path .\DKUEndGame.uproject).Path
& "$ueRoot\Engine\Build\BatchFiles\Build.bat" DKUEndGameEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor 빌드 실패: 먼저 빌드 로그를 확인하세요.' }
```

3. 선택적으로 Editor의 Automation에서 `ChickenFight.EOS`(계약·응답 순서 검사), `ChickenFight.Matchmaking`, `ChickenFight.RoundSetup.SpawnReservations`를 실행한다. 실제 계정·네트워크 성공을 대신하지 않는다.
4. 네 PC 각각 아래 도구로 게임을 실행하고 서로 다른 Epic 계정으로 로그인해 연습 로비에서 빠른 대전을 누른다. PIE 4창은 서로 다른 실계정 4PC 검증을 대신하지 않는다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\Network\Start-EOSLoginTest.ps1 -UnrealEditorPath "$ueRoot\Engine\Binaries\Win64\UnrealEditor.exe"
```

## 성공 기준과 실패 확인

- 네 명 모두 같은 Round1에 도착하고 참가자 4/4를 본다. 편집용 미리보기 4개는 나타나지 않으며 각자 다른 플레이어 Pawn 하나를 소유한다.
- 수동 시작을 누르지 않고 3초 카운트다운이 복제된다. 네 화면 모두 Round1 Playing으로 전환되고 각자 캐릭터를 조작한다.
- 호스트 로그: `Match phase: READY_TO_START Joined=4 Host=1`, `EOS four-player readiness accepted`, `CF_MATCH playing round=1 players=4`, `Action=started HTTP=200`, 이후 `Match phase: IN_GAME`을 확인한다. 호스트는 임의 첫 PC가 아니라 서버 배정으로 정해진다.
- 다른 클라이언트도 `Round1 local connection ready: Host=0`, `Action=joined HTTP=200` 및 IN_GAME 조회를 확인한다. `매칭이 완료되었습니다`만으로 성공 처리하지 않는다.
- 한 명이 접속 전에 종료/접속 실패하면 나머지가 임의 3인 시작하지 않아야 한다. 실패/기한 만료 확정 후 세션을 정리하고 연습 로비로 돌아가 다시 매칭할 수 있어야 한다. 성공 흐름을 먼저 확인한 뒤 별도 테스트로 진행한다.

실패 시 `LogCFMatchSession`, `LogCFRoundSetup`, `CF_MATCH`, `LogEOS`, `LogNet`, Travel/Network Failure 주변 로그를 확인한다. 새 설정 오류가 있으면 기존 EOSLocal 갱신과 NetDriver 로드를 먼저 확인한다. Portal의 기존 Peer2Peer 정책이 세션/P2P 동작을 허용하는지도 확인한다. 실제 실패 로그 없이 권한 문제로 단정하지 않는다.

배정 전 매칭 취소는 기존처럼 본인만 큐에서 빠진다. 배정 후 실제 접속 실패·퇴장은 `/left`를 통해 전체 경기를 실패 처리한다. 응답이 불명확하면 새 매칭을 차단하고 상태를 다시 확인한다. 인증이 만료되면 같은 계정으로 재로그인해 확인하며 다른 계정의 토큰으로 이전 매치 보고를 보내지 않는다. EOS 작업이 정리되지 않은 상태를 정리 완료로 표시하지 않는다.

## 검증 기록

- 2026-11-08 실계정 4인 첫 시도의 이 PC 로그는 `Host=0`, `SESSION_READY`, EOS 검색 1건 이후 `InvalidURL`을 기록했다. 따라서 이 로그에서 확인한 실패는 호스트 세션 생성이 아니라 참가자의 접속 주소 처리다. `[EOS:PUID]`를 `FURL`로 파싱·재직렬화하면서 대괄호가 사라지고 기본 맵이 붙어 다음 파싱에서 잘못된 맵 이름으로 해석됐다. OSS 주소 원문에 승인 옵션만 추가하도록 수정했다. `MatchSessionTravelURL`에서 기존 동작의 실패를 재현하고 수정 후 성공을 확인했으며, Editor 빌드와 EOS 검사 3개가 모두 통과했다. 검사 로그는 `Saved/Logs/EOSTravelURL-red.log`·`EOSTravelURL-green.log`이며 수정 후 실제 네 PC 재시도는 아직 필요하다.
- 로컬 PowerShell 가짜 자격 증명 테스트: 기존 로그인 전용 설정 거절, Secret 보존 무출력 업그레이드, 반복 실행, 잘못된 dev ID 거절, Git 제외/추적 방지 및 기존 검사 통과.
- native `ChickenFight.EOS.MatchSessionContract`와 `MatchSessionResponseGuards` 추가: 정상 명단, 다른 계정/매치, 중복 명단, 세션 누락/길이/제어 문자, 알 수 없는 상태, 잘못된 시도와 준비 인원 거절, 지연/중복/종료 콜백 거절, 멱등 키 보존, 쓰기 응답 후 GET 확인을 검사한다. 2026-11-08 UE 5.8.4 Editor에서 두 검사 모두 통과했다.
- 빌드 실패 원인인 `OnlineReplStructs.h` include를 실제 경로 `GameFramework/OnlineReplStructs.h`로 수정하고, 계약 검사 두 곳의 `FString::Replace` 치환 인수를 `const TCHAR*`로 전달하도록 수정했다. Editor 정식 빌드 성공 후 `Saved/Logs/EOSBuildFixTests.log`에서 EOS 검사 두 개의 Success를 확인했다.
- 실제 EOS 세션 등록·검색·Join 완료 콜백까지 진행한 첫 시도에서 위 주소 오류를 발견했다. 수정 후 다른 PC 4인 접속·자동 시작과 인증 만료·연결 실패 복구의 실환경 검증은 **미완료**다. 패키징의 Round1 cook 포함과 EOS 재배포 설정도 별도 확인이 필요하다.
