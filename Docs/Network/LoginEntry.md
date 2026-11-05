# 로그인과 연습 로비 진입

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
>
> 상위: [Network 개요](Overview.md#login-entry) · 화면: [메인 메뉴](../UI/MainMenu.md)

## 현재 상태

`UCFEntrySubsystem`이 기존 `UCFFrontendSubsystem`의 로그인·닉네임 등록·로비 요청을 처리한다. Epic Account Portal 로그인과 EOS Connect 인증, 프로필 HTTP 호출, 실제 맵 이동 코드를 연결했다. **클라이언트용 EOS 설정·권한은 아직 구성하지 않았으므로 실계정 로그인이 완료된 상태는 아니다.** 설정이 없으면 로그인 화면에서 오류와 재시도를 제공한다.

사용자 확인: Dev Auth Tool과 별도 SDK 도구의 Connect 로그인·백엔드 인증은 검증했지만, Unreal 클라이언트 설정과 권한은 통합 작업에서 확인·구성한다. 해당 도구의 성공을 현재 Unreal 빌드의 실계정 검증으로 간주하지 않는다. Client Secret과 토큰은 공유·저장소 추가하지 않았다.

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
- 프로젝트/샌드박스/배포 ID, **게임 클라이언트용** Client ID·정책·Artifact 또는 Named Platform 설정을 개발 환경과 일치시킨다. 백엔드 검증용 Client Secret을 클라이언트에 복사하지 않는다.
- Epic Account Services 애플리케이션·계정 접근·Account Portal·Connect 권한을 확인한다. 현재 코드의 요청 범위는 `BasicProfile`이다.
- 민감 설정은 팀의 비공개 로컬/빌드 설정 경로로 제공한다. 추적 중인 DefaultEngine.ini에 실제 Secret이나 토큰을 넣지 않는다. 저장소에는 `bUseEAS`·`bUseEOSConnect` 등 비밀이 아닌 기본 옵션만 있다.
- `Config/DefaultGame.ini`의 `[/Script/DKUEndGame.CFEntrySubsystem] ProfileApiBaseUrl`은 기존 개발 API 주소다. 2026-11-05 무토큰 GET의 `401 AUTH_MISSING`까지 확인했다. 환경 변경 시 통합 담당이 주소를 바꾼다.
- 준비 후 신규/기존 계정 각각으로 실제 Epic 로그인, GET, 최초 닉네임 POST, 중복 이름, 취소, 인증 만료 후 재로그인을 확인한다.

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

- UE 5.8.2 Editor C++ 빌드 성공, 메인 컨트롤러 Blueprint 컴파일·저장과 로비 의존성 재조회.
- `ChickenFight.Entry.ProfileResponses` 자동화 검사 통과: 신규/기존 프로필, PUID 불일치, 잘못된 응답, 중복 이름, 등록 응답 유실 복구, 401, 지연·취소 응답, 멱등성 키 보존 등.
- PIE 20개 검사 통과: EOS 설정 누락 시 오류·재시도, 신규/기존 프로필 분기, 실제 로비 이동과 UI·Pawn 생성, 타이틀 복귀, 로비 참조 누락 복구.
- PIE 성공 인증 응답은 **프로젝트 밖 테스트 스크립트에서 주입**했다. 제품에 가짜 로그인이나 우회 버튼을 추가하지 않았다. 실계정 인증·인증된 HTTP·토큰 만료·실제 EOS 세션·Shipping 패키징은 미검증이다.

공식 참고: [OSS EOS 설정·로그인](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine), [Connect API](https://dev.epicgames.com/docs/api-ref/interfaces/connect). 구현은 설치된 UE 5.8.2 공개 헤더와 백엔드 프로필 계약에 맞췄다.
