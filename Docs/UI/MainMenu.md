# 메인 메뉴

> 최근 기능 변경: 2026-11-05 · Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋

관련 개요: [UI 개요](Overview.md#menu)

`Content/Maps/Lvl_CF_MainMenu`를 열고 Play하면 Figma 기반 타이틀이 표시된다. 배경 액터가 없는 메뉴 전용 맵이므로 편집 뷰포트가 검은 것은 정상이다. Game Default Map도 이 레벨이다.

## 타이틀과 진입 흐름

[Figma 타이틀](https://www.figma.com/design/Ppv7el0cMlmQmhkwUZITut/ChickenFight?node-id=67-374)의 크림색 배경, 빨간 `CHICKEN FIGHT`, 빨간 시작 버튼, 노란 종료 버튼을 UMG로 구성했다. 로그인(`71:385`), 닉네임(`71:463`), 접속 중(`71:451`) 화면도 같은 위젯 안에 있다. 설명용 화살표·메모·중복 시안은 런타임 UI에 넣지 않았다.

1. **게임 시작** → Epic 로그인 팝업. 여기의 시작은 인증·프로필 확인 진입이며 자동 매칭 요청이 아니다.
2. **LOG IN NOW** → 통합 계층에 로그인 요청 → 로그인 중 표시. 로그인 성공 응답은 Epic 인증과 프로필 조회까지 끝났다는 의미다.
3. 닉네임이 없는 계정은 **닉네임 설정**. 이미 등록된 계정은 건너뛴다.
4. 등록된 닉네임이 있으면 **접속 중** → 통합 계층이 실제 연습 로비 이동을 처리한다. 로비 UI는 [연습 로비](Lobby.md)에 구현했다. `UCFEntrySubsystem`이 실제 맵 이동을 처리하고 로딩 완료 뒤 진입을 확정한다.
5. **게임 종료**는 게임 종료 요청을 실행한다. 로그인·닉네임에서 **돌아가기/Esc**는 진행 중 요청을 취소하고 타이틀로 돌아온다.

**`UCFEntrySubsystem`을 인증·프로필·로비 이동 제공자로 연결했다. 클라이언트 EOS 설정·권한은 미구성이므로 실계정 로그인은 아직 검증하지 않았다.** 설정 누락·통신 실패는 오류와 재시도 UI를 표시한다. [구현·설정·검증 범위](../Network/LoginEntry.md)를 참고한다. 성공 응답을 가정하거나 LAN 방을 자동 생성하지 않는다.

화면 기준은 1920×1080이며 ScaleBox로 비율을 유지한다. 다른 화면 비율의 여백도 해당 화면 배경색으로 채운다. Inter Regular/Bold 폰트는 FontFace에 내장하며 한글은 엔진 composite font의 대체 글꼴을 사용한다. 로딩 링은 Figma의 실제 PNG이며 접속 중에 회전한다. 폰트 라이선스는 `Content/UI/MainMenu/Fonts/Inter-LICENSE.txt`에 포함한다.

## 상태와 통합 연결

`UCFFrontendSubsystem`은 로컬 GameInstance 수명의 UI 상태를 관리한다. 인증 토큰·AWS 주소·EOS 세션·매칭 티켓은 저장하거나 직접 처리하지 않는다. 서버의 경기 권한·복제 상태와도 별개다.

`Title → Login → LoggingIn → Nickname → SavingNickname → Connecting`

실패 시 해당 입력 화면 또는 타이틀로 돌아간다. 등록된 프로필은 같은 실행 중에 유지되므로 뒤로 갔다 다시 시작하면 이미 끝낸 단계를 건너뛴다. 프로필 초기화는 `InvalidateProfile()`, 오류와 함께 재로그인 화면으로 돌아가기는 `RequireLogin(Error)`를 사용한다. Entry는 인증 만료를 후자로 처리한다.

`UCFEntrySubsystem`이 GameInstance 초기화 때 아래 이벤트를 한 번씩 바인딩한다. 통합 담당은 현재 제공자를 재사용하며 교체 시 중복 바인딩을 피한다. 작업 응답은 게임 스레드에서 같은 요청 ID로 완료 함수를 호출한다.

| UI가 보내는 이벤트 | 통합 담당이 할 작업 | 완료 응답 |
| --- | --- | --- |
| `OnLoginRequested(RequestId)` | Epic Account Portal 인증, Connect 인증, 프로필 조회 | `CompleteLogin(RequestId, bSuccess, RegisteredNickname, Error)` |
| `OnNicknameRequested(RequestId, Nickname)` | 서버의 최초 닉네임 등록 API 호출 | `CompleteNickname(RequestId, bSuccess, RegisteredNickname, Error)` |
| `OnLobbyRequested(RequestId)` | 실제 로비 이동·진입 준비 | `CompleteLobbyEntry(RequestId, bSuccess, Error)` |
| `OnRequestCancelled(RequestId)` | 해당 요청의 취소·이후 응답 억제 등 정리 | 완료 함수로 성공을 돌려보내지 않음 |

- `RequestId`는 로컬 UI 요청 식별자다. 백엔드의 `ticketId`, `matchId`, 멱등성 키와 다르다.
- 로그인 성공에서 빈 `RegisteredNickname`은 최초 등록이 필요하다는 의미다. 등록 성공에서는 서버가 확정한 이름을 넘긴다.
- `CompleteLobbyEntry` 성공 호출은 실제 로비 준비가 완료됐을 때 한다. 이 함수 자체는 맵을 열지 않으며 다음 타이틀 복귀에 남을 로딩 상태만 초기화한다.
- 대기 중 중복 요청은 무시한다. 취소·시간 초과·재시도 뒤 도착한 과거 ID나 다른 단계의 응답은 화면과 프로필을 바꾸지 못한다.
- UI 요청 기한은 로그인 120초, 닉네임 등록 30초, 로비 진입 60초다. 시간 초과 시 취소 이벤트와 재시도 안내를 표시한다. 이 로비 기한은 백엔드 매치 준비 단계의 기한을 대체하지 않는다.
- `Error`에는 사용자용 문구를 전달한다. 인증 토큰·원시 HTTP 응답·내부 키를 전달하지 않는다.

닉네임은 로컬에서 한글 완성형·영문·숫자·공백 2~16자를 검사하고, 앞뒤 공백 제거와 연속 공백 축소 후 요청한다. 기호·이모지·탭·줄바꿈은 거절한다. UI 검사는 기본 입력 안내이며 서버의 NFKC 정규화·중복 검사·최초 등록 여부 판정이 최종 기준이다. 닉네임 변경 기능은 없다.

## 수정 위치

| 위치 | 역할 |
| --- | --- |
| `Content/UI/MainMenu/WBP_CF_TitleMenu` | 새 타이틀·로그인·닉네임·로딩의 Designer 배치·폰트·색상. 부모 `CFTitleMenuWidget` |
| `Source/DKUEndGame/UI/CFTitleMenuWidget.h/.cpp` | 버튼·키보드·입력 활성화·오류·로딩 링 표시 |
| `Source/DKUEndGame/UI/CFFrontendSubsystem.h/.cpp` | 로컬 화면 상태·비동기 요청 ID·취소·시간 초과·통합 이벤트 |
| `Source/DKUEndGame/UI/CFTitleMenuController.h/.cpp` | 로컬 위젯 생성·UI Only 입력·커서·개발용 LAN 진입 |
| `Content/UI/MainMenu/BP_CF_TitleController` | 기존 위젯 지정과 `LobbyMap = L_Lobby_Playground` soft 참조 |
| `Source/DKUEndGame/Network/CFEntrySubsystem.h/.cpp` | EOS 인증·프로필 HTTP·실제 로비 이동 제공자 |
| `Content/UI/MainMenu/BP_CF_MenuGameMode` | 새 타이틀 컨트롤러 지정. 플레이어 캐릭터는 생성하지 않음 |
| `Content/UI/MainMenu/Fonts/`, `Textures/` | Inter 폰트·OFL 라이선스·Figma 로딩 링 |

버튼 동작은 C++에서 처리한다. `BindWidget`으로 연결된 위젯 이름은 유지해야 한다. 실행에는 프로젝트 C++ 빌드가 필요하며 생성·검증에 쓴 외부 에디터 도구 플러그인은 필요하지 않다.

## 기존 LAN 개발 메뉴

기존 `WBP_CF_MainMenu`, `CFMainMenuWidget`, `BP_CF_MenuController`, `CFSessionSubsystem`은 유지한다. 새 타이틀에서 **F9**, 또는 Development 실행 인자 **`-CFLanMenu`**로 LAN 메뉴에 들어간다. 컨트롤러의 `CFMenuLan`, `CFMenuTitle` Exec 함수로 전환할 수도 있다. 이 진입 기능은 Shipping에서 비활성이다.

1. LAN용 닉네임은 앞뒤 공백·제어 문자를 제거하고 최대 24자로 사용한다. 제품 프로필 닉네임과는 별도다.
2. **방 만들기**: 호스트 포함 정원 4명의 LAN 세션을 만들고 `Lvl_CF_Arena`를 리슨 서버로 연다. 대기 중 최소 2명이면 호스트가 수동 시작한다.
3. **참가하기**: 같은 네트워크의 방을 검색하고 목록에서 방을 선택해 참가한다.
4. 직접 IP 입력 시 목록 대신 해당 주소로 연결한다. IPv4와 선택적 포트를 받는다. 예: `192.168.0.10:7777`, 같은 PC는 `127.0.0.1:7777`.
5. **방 새로고침**으로 재검색하며 검색 중 **돌아가기**는 검색을 취소한다.

처리 중 중복 요청과 모달 뒤쪽 입력을 제한한다. 방 정원 초과·연결 실패 안내와 조작 복구, 실행 중 닉네임 유지도 기존 동작이다. `-CFLanMenu` 실행은 퇴장 후에도 LAN 메뉴로 돌아온다. 일반 실행에서 F9로 연 경우 퇴장 후 타이틀에서 다시 F9로 진입한다.

세션 검증 이력은 [LAN 세션](../Network/ListenServerSessions.md), 대기 캐릭터는 [대기 UI](WaitingRoomUI.md), 제품 연동은 [EOS 인계서](../EOS/unreal-client-handoff.md)를 참고한다.

## 화면 제작 당시 검증 (2026-11-04)

- UE 5.8.2 C++ Editor 빌드와 새 메뉴 Blueprint 컴파일·저장 확인.
- 실제 PIE 위젯으로 37개 UI 상태 검사 통과: Enter/Esc/F9, 한글 문자 입력, 입력 검증·중복 요청 차단, 취소 후 늦은 응답 무시, 실패 후 재시도, 기존 닉네임 경로, 로딩 링, 로그인 120초·등록 30초·로비 60초 시간 초과 복구.
- 큰 창과 작은 창의 실제 렌더에서 타이틀·로그인·닉네임·접속 중 배치 확인. 실제 테스트 뷰포트는 창 테두리를 제외한 1920×1082, 954×636이었다. 별도 작은 창 검사에서 종료 버튼으로 PIE가 종료되는 것도 확인했다.
- 로그인·프로필·로비 완료 응답은 외부 테스트 도구가 주입했다. 실제 Epic 인증, AWS 호출, EOS 세션·맵 이동, Windows IME 조합 입력, Shipping 패키징 검증을 의미하지 않는다. 기존 LAN 메뉴 진입은 확인했지만 LAN 멀티플레이 전체 회귀 테스트는 이번 범위에서 재실행하지 않았다.
