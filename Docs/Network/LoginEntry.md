# 로그인과 연습 로비 진입

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위: [Network 개요](Overview.md#login-entry) · 화면: [메인 메뉴](../UI/MainMenu.md)

## 현재 상태

`UCFEntrySubsystem`이 기존 `UCFFrontendSubsystem`의 로그인·닉네임 등록·로비 요청을 처리한다. Epic Account Portal 로그인과 EOS Connect 인증, 프로필 HTTP 호출, 실제 맵 이동 코드를 연결했다. **2026-11-05 통합 담당 PC의 로컬 EOS 설정 입력·정적 검사는 완료했다. 해당 PC에는 Unreal이 없어 실계정 로그인은 아직 검증하지 않았다.** 자격 증명 파일은 각 PC에서 별도 생성하며 다른 개발자의 체크아웃에 포함되지 않는다. 설정이 없으면 로그인 화면에서 오류와 재시도를 제공한다.

사용자 확인: Dev Auth Tool과 별도 SDK 도구의 Connect 로그인·백엔드 인증은 검증했지만, Unreal 클라이언트 설정과 권한은 통합 작업에서 확인·구성한다. 해당 도구의 성공을 현재 Unreal 빌드의 실계정 검증으로 간주하지 않는다. Client Secret과 토큰은 공유·저장소 추가하지 않았다.

2026-11-06 추가 사용자 보고: 실제 Epic 로그인부터 연습 로비 진입까지 성공했다. 이 PC에서 직접 실행한 검증은 아니며 계정별 최초 등록·인증 만료·패키징 전체 완료를 뜻하지 않는다. 이후 [로비 매칭 티켓 제공자](MatchmakingTickets.md)를 추가했으며 해당 기능의 Unreal 빌드·실계정 실행은 별도로 확인해야 한다.

## 실행 흐름

1. `Lvl_CF_MainMenu`의 게임 시작 → LOG IN NOW.
2. Entry가 명시적으로 `EOS` OnlineSubsystem을 선택하고 `accountportal`로 로그인한다. `bUseEAS`와 `bUseEOSConnect`가 모두 필요하다. 기본 플랫폼 서비스 `NULL`과 기존 LAN 게임 경로는 유지한다.
3. 인증 완료 후 현재 PUID의 `EOS_Connect_CopyIdToken`으로 Connect ID Token을 복사한다. OSS의 `GetAuthToken`은 Epic Access Token이므로 이 백엔드의 Bearer로 사용하지 않는다.
4. `GET /v1/me` 성공 응답의 `playerId`가 현재 PUID인지 확인한다. 닉네임이 없으면 등록 화면, 있으면 로비 진입으로 이동한다.
5. 등록 시 다시 `GET /v1/me`를 확인한다. 이전 등록이 이미 반영됐다면 서버의 이름으로 진행하고, 미등록이면 `POST /v1/me/nickname`을 호출한다.
6. 서버가 확정한 닉네임으로 `CompleteNickname` 또는 `CompleteLogin`을 완료한다. Entry의 로비 핸들러가 `BP_CF_TitleController.LobbyMap`의 `/Game/Map/L_Lobby_Playground`를 연다.
7. 같은 GameInstance의 해당 맵 로딩이 끝나면 `CompleteLobbyEntry`를 완료한다. 연습 로비의 기존 컨트롤러·캐릭터·UI를 사용한다.

`UCFFrontendSubsystem`은 UI 상태·요청 ID만 관리하고 토큰을 받지 않는다. Entry는 GameInstance 수명이며 의존성을 초기화한 뒤 이벤트를 한 번씩 바인딩하고 종료 때 해제한다. 통합 담당은 기존 Entry를 확장·교체할 경우 동일 이벤트에 중복 제공자를 연결하지 않는다. 자동 매칭용 `UCFLobbySubsystem`에는 아직 실제 티켓·세션 제공자를 연결하지 않았다.

## 실패·취소·재시도

| 상황 | 처리 |
|---|---|
| EOS 설정 없음·로그인 취소/실패 | 로그인 화면에서 오류와 재시도 |
| 프로필 조회 통신 실패·5xx | 실패 안내, 재시도 때 인증이 살아 있으면 프로필부터 다시 조회 |
| HTTP 401·Connect 토큰 확보 실패 | UI의 캐시된 프로필을 비우고 재로그인 요청. 재시도 시 필요하면 기존 EOS 인증을 종료한 뒤 로그인 |
| 닉네임 중복·형식 오류 | 입력 화면 유지, 사용자용 안내 |
| 닉네임 등록 응답 유실 | 다음 시도에서 GET으로 실제 프로필 확인. 동일 사용자·이름의 미확정 POST는 같은 UUID 멱등성 키 유지 |
| 이미 등록됨 응답 | GET으로 확정 이름 조회. 모순된 미등록 응답에서는 POST 반복 없이 오류 처리 |
| 다른 PUID·잘못된 JSON·문자열이 아닌 닉네임 | 응답 거절, 안전한 오류 안내 |
| 뒤로가기·UI 시간 초과 | HTTP 취소, 타이머 정리, 요청 ID와 HTTP 순번으로 늦은 응답 무시 |
| 로비 참조 누락·이동 실패 | 타이틀에서 재시도 안내, 확인된 프로필 유지 |

HTTP 요청별 기한은 12초다. 등록 전 조회와 POST를 합쳐 기존 닉네임 UI의 30초 기한 안에 처리하며, UI의 로그인 120초·로비 60초 기한도 유지한다. Account Portal 창은 SDK 소유이므로 뒤로가기가 외부 인증 창을 강제로 닫지는 않는다. 진행 중 인증이 있으면 새 창을 중복 생성하지 않고 현재 유효한 UI 요청에만 결과를 연결한다.

서버의 원시 오류 본문이나 EOS 오류 문자열을 UI에 표시하지 않는다. 토큰은 HTTP Authorization 헤더에만 사용하고 프로젝트 코드에서 로그·설정·UI 필드에 저장하지 않는다. 토큰 갱신은 OSS EOS의 인증 수명을 사용하며 HTTP마다 Connect ID Token을 새로 복사한다. 실제 만료·재인증 동작은 클라이언트 설정 후 검증해야 한다.

기존 `CFSessionSubsystem`은 Idle일 때 발생한 TravelFailure를 가로채지 않도록 좁혔다. LAN 작업 중 오류 처리는 기존 경로를 유지한다.

## 통합 담당이 구성할 것

- 설치된 UE 5.8.2의 `OnlineSubsystemEOS`·`EOSShared` 플러그인은 활성화했다. 새 SDK나 프레임워크를 설치하지 않았다.
- 프로젝트/샌드박스/배포 ID와 Client ID·정책을 개발 환경과 일치시킨다. 2026-11-05 사용자가 기존 `xyza7891MHpvCZ6hgPhGFHQwn6KZNTp9` Client의 Peer2Peer 정책과 EAS Linked Clients 연결을 확인했다. 백엔드의 허용 Client ID도 동일하다. 아래 도구는 이 Client의 Portal 자격 증명을 사용하며 AWS Secrets Manager의 값을 가져오지 않는다. TrustedServer 정책의 자격 증명은 공개 게임 빌드에 넣지 않는다.
- Epic Account Services 애플리케이션·계정 접근·Account Portal·Connect 권한을 확인한다. `Config/DefaultEngine.ini`에서 `!AuthScopeFlags=ClearArray` 뒤 `+AuthScopeFlags=BasicProfile`을 지정해 상위 친구 목록 등 요청 범위를 제거한다. 현재 로그인·닉네임 흐름에는 친구 권한이 필요하지 않다.
- 민감 설정은 팀의 비공개 로컬/빌드 설정 경로로 제공한다. 추적 중인 DefaultEngine.ini에 실제 Secret이나 토큰을 넣지 않는다. 저장소에는 `bUseEAS`·`bUseEOSConnect` 등 비밀이 아닌 기본 옵션만 있다.
- `Config/DefaultGame.ini`의 `[/Script/DKUEndGame.CFEntrySubsystem] ProfileApiBaseUrl`은 기존 개발 API 주소다. **URL 전체를 큰따옴표로 감싼다** (`ProfileApiBaseUrl="https://..."`). 따옴표가 없는 기존 설정은 실제 로그에서 `https:`로 잘려 읽혔다. Subsystem 초기화 시 `GGameIni`의 최종 설정 캐시에서 명시적으로 읽고, 명령행 설정 덮어쓰기는 계속 존중한다. 누락·잘못된 주소는 대체 주소로 우회하지 않고 거절한다. 2026-11-05 무토큰 GET의 `401 AUTH_MISSING`까지 확인했다. 환경 변경 시 통합 담당이 주소를 바꾼다.
- 준비 후 신규/기존 계정 각각으로 실제 Epic 로그인, GET, 최초 닉네임 POST, 중복 이름, 취소, 인증 만료 후 재로그인을 확인한다.

## EOS 로컬 설정 먼저 준비하기

이 단계에는 Unreal 설치가 필요 없다. 프로젝트 루트에서 PowerShell로 다음을 실행한다.

```powershell
pwsh -NoProfile -File .\Tools\Network\Set-EOSLocal.ps1
```

출력된 Client ID가 Portal의 기존 Client와 같은지 확인하고, 그 Client의 **Client Secret**을 마스킹 프롬프트에 붙여넣는다. Secret은 명령행·채팅·환경 변수로 전달하지 않는다. 이 값은 사용자 계정 비밀번호나 Connect ID Token이 아니다.

도구는 `Backend-Infra/samconfig.toml`의 dev 식별자를 읽고 `Config/Custom/EOSLocal/DefaultEngine.ini`를 만든다. 이 경로는 `.gitignore`로 제외되며, 추적 중인 파일로 바뀌면 입력/검사를 거절한다. 저장한 파일은 해당 PC에서만 사용하고 동료에게 복사하거나 Git에 추가하지 않는다. 다른 PC에서도 도구를 실행해 로컬로 입력한다. 설정을 다시 실행하면 기존 로컬 자격 증명을 갱신한다.

이 파일에는 `ChickenGameDev` Artifact와 현재 dev ID, 해당 Client Secret이 들어간다. `bUseNamedPlatformConfig=false`로 이 Artifact를 선택하고, 현재 사용하지 않는 Player/Title Data Storage의 EncryptionKey는 Epic 문서의 개발 예시값(64개의 `1`)을 쓴다. 파일 저장 기능을 도입할 때에는 이 키 정책을 다시 정한다. 2026-11-08부터 **EOSLocal 실행에서만 기본 OSS=EOS와 UE5.8 EOS P2P NetDriver**를 적용해 게임 세션의 접속 ID와 전송을 일치시킨다. 일반 실행의 NULL/LAN 설정은 유지한다. 이전 도구로 만든 파일은 Secret 재입력 없이 아래 명령으로 갱신한다. [Round1 세션 검증](EOSRound1Session.md) 참고.

```powershell
pwsh -NoProfile -File .\Tools\Network\Set-EOSLocal.ps1 -Upgrade
```

입력 없이 저장 상태만 검사할 때:

```powershell
pwsh -NoProfile -File .\Tools\Network\Set-EOSLocal.ps1 -CheckOnly
```

`READY`는 파일 형식·dev ID 일치·Git 제외 확인이 끝났다는 뜻이다. 실제 Secret의 유효성, Epic 로그인, 인증된 프로필 호출 성공을 의미하지 않는다.

## Unreal 설치 후 실계정 확인

UE 5.8.2에서 프로젝트 Editor 빌드를 완료한 뒤 실행 파일 경로를 지정한다. 아래 경로는 예시이므로 실제 설치 경로로 바꾼다.

```powershell
pwsh -NoProfile -File .\Tools\Network\Start-EOSLoginTest.ps1 -UnrealEditorPath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
```

이 도구는 Secret을 인수로 넘기지 않고 `-game -CustomConfig=EOSLocal`로 실행한다. 일반 실행은 이 개발 설정을 읽지 않으므로 **이 실행 도구로 테스트한다**. UE는 [CustomConfig 방식](https://dev.epicgames.com/documentation/unreal-engine/using-lyra-with-epic-online-services-in-unreal-engine)과 [OSS EOS Artifact 설정](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine)을 지원한다. 설치·Editor 빌드·실제 로그인 검증은 현재 설정 도구의 자동 검증 범위 밖이다.

1. 게임 시작 → LOG IN NOW → 열린 Account Portal에서 테스트 계정으로 로그인한다. EAS 브랜드가 미검증이면 조직에 가입한 계정을 사용한다.
2. 기존 닉네임 계정은 재등록 없이 연습 로비로 이동하는지 확인한다.
3. 미등록 계정은 닉네임 입력 화면에서 한글 이름을 등록한 뒤 연습 로비로 이동하는지 확인한다. 최초 등록 후 이름 변경은 지원하지 않으므로 사용할 이름을 먼저 정한다.
4. 다른 계정의 중복 이름을 입력하면 화면에 오류가 나오고 다른 이름으로 다시 입력할 수 있는지 확인한다.
5. Account Portal 취소와 로그인 중 뒤로가기, 재시도, 인증 만료 후 재로그인을 확인한다. 연습 로비에서 인증이 끊겼을 때의 표시와 복구도 별도 확인한다.

자동 매칭 티켓 API 제공자는 [별도 연결](MatchmakingTickets.md)했고 실제 4인 EOS 세션 제공자는 아직 연결되지 않았다. 이 문서의 로그인 테스트 성공 판정은 실계정 인증·프로필·닉네임·연습 로비 진입까지다. 현재 PC에는 Unreal이 없어 이 절차는 아직 재실행하지 않았다. 설정 생성 도구의 검증과 게임 실행 결과를 구분해 기록한다.

## 로그인 준비 실패 진단

`현재 로그인 서비스를 사용할 수 없습니다`는 API 주소 또는 EOS 준비 검사 실패 시 표시된다. 설정 도구의 `READY`와 `-CustomConfig=EOSLocal` 인식만으로 런타임 EOS 준비 성공을 판단하지 않는다.

진단 로그가 포함된 최신 코드를 **Editor 대상으로 다시 빌드**한 뒤 기존 실행 도구로 실행하고 LOG IN NOW를 누른다. `Saved/Logs/DKUEndGame.log`에서 `LogCFEntry`를 검색한다.

- `ApiBaseValid=0`: 프로필 API 주소 형식 검사 실패.
- `Profile API configuration: Found=... Valid=...`: 초기화 시 최종 Game 설정 캐시에서 주소를 찾았는지와 형식 검사 결과. 둘 다 `1`이어야 한다. 값 원문은 기록하지 않는다.
- `EOS settings`: EAS·EOS Connect·Named Platform 설정의 실제 활성화 여부. EAS와 EOS Connect는 모두 `1`이어야 한다.
- `artifact selection failed`: Artifact 선택 실패. `EOS artifact presence`의 각 항목은 값의 존재 여부이며 `0`은 누락, `1`은 존재다. 존재 여부로 자격 증명의 유효성까지 판단하지 않는다.
- `EOS preparation failed`: 필수 필드 누락, 서브시스템 또는 Identity 생성 실패를 구분한다. Named Platform 설정을 사용하는 경우 Artifact 검사를 생략한 사실도 기록한다.
- `EOS preparation succeeded` → `Starting Account Portal login`: 준비 완료 후 실제 로그인 호출 단계 진입. 로그인 성공을 의미하지 않는다.

로그에는 API 주소·식별자·Secret·토큰 원문을 출력하지 않고 검사 결과만 기록한다. 화면의 오류 문구와 로그인 조건은 유지한다. 진단 로그 추가 후 Unreal 빌드와 실제 계정 재실행은 Unreal이 설치된 PC에서 확인해야 한다.

2026-11-05 팀원 제공 로그에서 일반 실행의 `ApiBaseValid=0`, 명령행 주소 지정 후 `ApiBaseValid=1`·EOS 준비 완료를 확인했다. 이어 Account Portal이 `friends_list` 요청을 거절했다. 명시적 주소 로딩과 요청 범위 초기화 이후에도 `Found=1 Valid=0`이었으며, `GetIni` 조회 로그에서 **`ProfileApiBaseUrl = https:`**를 확인했다. 주소 항목 누락이나 프로퍼티 복사 실패가 아닌 설정 문자열 잘림이 확인되어 URL을 큰따옴표로 감쌌다. 수정 후 실제 로그인 성공은 아직 확인하지 않았다. 최신 코드의 Editor 대상 빌드를 완료한 PC는 이 URL 따옴표 변경만 받는 경우 재빌드 없이 게임을 완전히 종료 후 기존 실행 도구로 다시 실행하고 **API 주소 덮어쓰기 없이** 재확인한다. 기존 EOS 로컬 설정과 Secret은 다시 입력할 필요가 없다.

## 구현 위치와 패키징

| 위치 | 책임 |
|---|---|
| `Source/DKUEndGame/Network/CFEntrySubsystem.h/.cpp` | EOS 인증, Connect 토큰, 프로필 API, 취소·응답 검증, 로비 이동 |
| `Source/DKUEndGame/UI/CFFrontendSubsystem.h/.cpp` | 기존 상태 흐름, `RequireLogin` 오류·프로필 초기화 |
| `Source/DKUEndGame/UI/CFTitleMenuController.h` | Designer에서 지정하는 `LobbyMap` soft 참조 |
| `/Game/UI/MainMenu/BP_CF_TitleController` | 기존 메뉴 위젯 지정 유지, 연습 로비 참조 추가 |
| `Source/DKUEndGame/Tests/CFEntryTests.cpp` | 프로필 응답·재시도·취소 회귀 검사 |

로비 참조는 Blueprint에 저장되어 에셋 레지스트리의 soft package dependency로 확인했다. 메인 맵 → 메뉴 GameMode → 컨트롤러 → 연습 로비의 에셋 참조를 사용하며 전체 MapsToCook 목록을 덮어쓰지 않았다. 실제 Windows 패키징·cook 및 EOS 런타임 배포 검증은 아직 하지 않았다.

## 확인한 범위

- 2026-11-05 설정 도구: PowerShell 7에서 dev ID 일치, 잘못된 입력과 개행 거절, Secret 저장 시 출력 비노출, Git 제외·추적 파일 거절, Secret 없는 실행 인수 검사를 통과했다. 통합 담당이 마스킹 입력한 실제 로컬 설정의 `-CheckOnly` 결과는 `READY`다. 이는 실계정 인증 성공 기록이 아니다.

- UE 5.8.2 Editor C++ 빌드 성공, 메인 컨트롤러 Blueprint 컴파일·저장과 로비 의존성 재조회.
- `ChickenFight.Entry.ProfileResponses` 자동화 검사 통과: 신규/기존 프로필, PUID 불일치, 잘못된 응답, 중복 이름, 등록 응답 유실 복구, 401, 지연·취소 응답, 멱등성 키 보존 등.
- PIE 20개 검사 통과: EOS 설정 누락 시 오류·재시도, 신규/기존 프로필 분기, 실제 로비 이동과 UI·Pawn 생성, 타이틀 복귀, 로비 참조 누락 복구.
- PIE 성공 인증 응답은 **프로젝트 밖 테스트 스크립트에서 주입**했다. 제품에 가짜 로그인이나 우회 버튼을 추가하지 않았다. 실계정 인증·인증된 HTTP·토큰 만료·실제 EOS 세션·Shipping 패키징은 미검증이다.

공식 참고: [OSS EOS 설정·로그인](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine), [Connect API](https://dev.epicgames.com/docs/api-ref/interfaces/connect). 구현은 설치된 UE 5.8.2 공개 헤더와 백엔드 프로필 계약에 맞췄다.
