# 메인 메뉴

`Content/Maps/Lvl_CF_MainMenu`를 열고 Play합니다. 배경 액터가 없는 메뉴 전용 맵이므로 편집 뷰포트가 검은 것은 정상입니다. Game Default Map도 이 레벨입니다.

## 사용 방법

1. 닉네임을 입력합니다. 앞뒤 공백·제어 문자는 제거하고 최대 24자로 사용합니다.
2. **방 만들기**: 정원 2명의 LAN 세션을 만들고 `Lvl_CF_Arena`를 리슨 서버로 엽니다.
3. **참가하기**: 같은 네트워크의 방을 검색합니다. 목록에서 방을 선택하고 **참가**를 누릅니다.
4. 직접 접속 IP를 입력한 경우에는 목록 선택 대신 해당 주소로 연결합니다. IPv4와 선택적 포트만 받습니다. 예: `192.168.0.10:7777`. 같은 PC에서는 `127.0.0.1:7777`을 사용할 수 있습니다.
5. **방 새로고침**으로 다시 검색합니다. 검색 도중 **돌아가기**를 누르면 검색을 취소합니다.

처리 중에는 중복 요청을 막고 진행 상태를 표시합니다. 참가 창이 열린 동안 뒤쪽 메뉴는 입력을 받지 않습니다. 방이 가득 찼거나 연결이 실패하면 안내를 표시하고 메뉴 조작을 복구합니다. 닉네임은 같은 게임 실행 중 맵 이동·퇴장 후에도 유지되며 디스크에는 저장하지 않습니다.

## 수정 위치

| 위치 | 역할 |
| --- | --- |
| `Content/UI/MainMenu/WBP_CF_MainMenu` | Designer에서 배치·폰트·색상 수정. 부모는 `CFMainMenuWidget` |
| `Source/DKUEndGame/UI/CFMainMenuWidget.h/.cpp` | 버튼 연결, 검색 목록, 입력 잠금, 상태 표시 |
| `Source/DKUEndGame/Network/CFSessionSubsystem.h/.cpp` | 방 생성·검색·참가·연결 종료 및 오류 처리 |
| `Content/UI/MainMenu/BP_CF_MenuController` | 로컬 메뉴 생성, UI Only 입력과 커서 표시 |
| `Content/UI/MainMenu/BP_CF_MenuGameMode` | 메뉴 컨트롤러 지정, 게임 캐릭터 미생성 |

버튼 이벤트는 C++에서 처리합니다. 이전 Event Graph의 ‘준비 중’ 안내는 제거했습니다. `BindWidget`으로 연결된 위젯 이름은 유지해야 합니다. 이 메뉴를 사용하려면 프로젝트 C++ 빌드가 필요하며 임시 생성 도구 플러그인은 필요하지 않습니다.

세션 범위와 테스트 방법은 [ListenServerSessions.md](ListenServerSessions.md), 대기 캐릭터는 [WaitingRoomUI.md](WaitingRoomUI.md)를 참고하세요.
