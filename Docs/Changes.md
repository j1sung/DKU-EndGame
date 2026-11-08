# 변경 이력

<a id="change-20261108-round4-finale"></a>
### 2026-11-08 · Round4 복합 순환 전투 맵 초안

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 사용자가 조정한 저장된 Round3 기믹 크기를 보존해 새 `L_Round4`를 만들었다. 0·3·6m의 네 테라스와 상부 흔들다리·중층 왕복 발판·승강기·계단/경사를 연결하고, 물줄기·회전 막대·통나무·왕복 추·구체·낙하물·구덩이·언덕·밀리는 상자를 배치했다. 원본 맵과 공유 기믹은 보존했다.
- 확인: 저장·재로딩과 75개 액터 스케일, 신규 이동부 3개의 경로 및 주요 상면 20곳의 형상을 확인했다. 화면 잠금으로 Commandlet에서 검사했으며 실제 충돌·탑승·4인 경기·일반 에디터 시각 검토는 미검증이다. [현재 구성과 편집 경로](Gameplay/Overview.md#round4-finale).

<a id="change-20261108-round3-sweeper-log-respawn"></a>
### 2026-11-08 · Round3 다리 높이·회전 막대·통나무 재생성

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 다리와 양쪽 2층을 150cm 높여 600cm로 맞추고 계단·경사로를 연결했다. 다리 아래에 지름 9m 회전판과 함께 도는 길이 8m 막대를 배치했다. 통나무는 경사 아래에서 제거 후 2초 뒤 새 액터로 생성하고, 끼일 경우 최대 12초 후 제거한다. 사용자 캐릭터의 XY·설정은 보존하고 다리 높이에 Z를 맞췄다.
- 확인: 새 C++ 에디터 정식 빌드와 맵 저장·재로딩, 11곳 보행면 및 다리 아래 캡슐 통과, 약 16초 시뮬레이션에서 각 통나무의 4회 생성과 회전판/막대 추종을 확인했다. 다인 네트워크·실전 넘어짐 강도는 미검증이다. [현재 구성과 편집 경로](Gameplay/Overview.md#round3-draft).

<a id="change-20261108-round3-upper-bridge-jets"></a>
### 2026-11-08 · Round3 2층 연결과 주기 분수 추가

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 느린 이동속도를 고려해 아레나 폭을 44m에서 32m로 줄이고, 양쪽 높이 450cm의 2층·계단/경사로와 길이 18m의 흔들다리를 추가했다. 아래층 기믹을 유지하면서 실제 굴림 통나무 2개와 서로 다른 주기의 캐릭터 발사 분수 3개를 넣었다. 테마는 그레이박스로 유지했다.
- 확인: 저장·재로딩, 13곳의 보행면과 다리 아래 캡슐 통과, 시뮬레이션의 다리/분수 왕복·통나무 굴림·캐릭터 상향 발사를 확인했다. 실제 4인 경기와 넘어짐 강도는 미검증이다. [현재 구성과 편집 경로](Gameplay/Overview.md#round3-draft).

<a id="change-20261107-round3-arena"></a>
### 2026-11-07 · Round3를 장애물 전투 아레나로 재구성

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 사용자의 최후 1인 생존·자유 왕복 의도에 맞춰 기존 완주형 초안을 44m 정방형 아레나로 교체했다. 중앙 교전장과 네 방향 경사로·외곽 순환로, 분산된 장애물과 네 모서리 시작점을 배치하고 조명을 보완했다. 이전 초안은 로컬 백업으로 보존했다.
- 확인: 공간·기믹 배치와 저장·재로딩 검사. 실제 전투·네트워크·승패 연동은 미검증. [현재 구성](Gameplay/Overview.md#round3-draft).

<a id="change-20261107-round3-draft"></a>
### 2026-11-07 · Round3 장애물 코스 그레이박스 초안

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 새 `L_Round3`에 구덩이·언덕·경사로, 좁은 다리/우회로 분기, 회전판·점프대와 왕복 장애물, 낙하물 및 회수 바닥을 배치했다. 기존 맵·공유 기믹을 보존했다.
- 확인: 저장·재로딩과 전용 이동부 6개 경로 유지. 실제 완주·넘어짐·네트워크는 미검증. [구성과 초안 제약](Gameplay/Overview.md#round3-draft).

<a id="change-20261107-round2-construction-lift"></a>
### 2026-11-07 · 옥상 승강판을 개별 철망 패널의 공사용 리프트로 변경

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: Organic 맵의 기존 승강판에 옥상 금속 테마의 개방형 프레임·가림판·고정 마스트·구동기용 메시 9종과 안전색 재질 1개를 추가했다. 가림판 14개는 독립 액터이며 북·동쪽 중앙은 문 없이 열어뒀으며, 후속 요청에 따라 열린 면 위의 긴 가로대도 제거했다. 사용자가 지운 기존 케이지를 복구하지 않고 현재 배치 777개에 37개를 더했다.
- 확인: 저장·재로딩 후 패널 좌표·크기·충돌과 기존 배치 보존, 승강기 1600cm 이동 시 장식 추종·고정부 정지·복구를 검사했다. 재로딩 시 부착 좌표가 초기화되는 문제를 수정했다. 실제 플레이·네트워크는 미검증. [구성과 편집 위치](Gameplay/Overview.md#round2-organic).

<a id="change-20261107-round2-rooftop-perimeter"></a>
### 2026-11-07 · 옥상 플레이 구역 외곽 높은 펜스 추가

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 주변 배경 건물 사이의 외부 바닥으로 떨어지지 않도록 플레이 구역 외곽에 독립 철망 패널 481개를 추가했다. 윗선은 최고 플레이 옥상보다 4m 높은 Z=3150.5cm로 통일하고 기존 파라펫·설비·동선을 보존했다. 펜스 메시 1종과 최초 적용용 도구를 추가했다.
- 확인: 저장·재로딩 후 기존 액터 400개 보존, 모든 패널의 Pawn·PhysicsBody 차단 및 Camera 무시, 패널별 세 높이 충돌면과 내부 진입부 7곳 통과 검사. 실제 플레이는 미검증. [편집 폴더·구성·제약](Gameplay/Overview.md#round2-organic).

<a id="change-20261107-round2-rooftop"></a>
### 2026-11-07 · Round2 Organic 도심 옥상 테마 적용

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 사용자 수정본의 기존 액터 326개 변환·동선을 보존하고 옥상 설비·외장·주변 도시용 메시 33개와 재질 12개를 생성했다. 외곽은 낮은 콘크리트 벽과 일부 철망, 허브·계단은 철제 난간으로 바꾸고 기존 개별 패널 편집과 열린 구간을 유지했다. 테마 액터 74개를 추가했다.
- 확인: 저장·재로딩, 기존 변환·울타리 외 메시·기믹 설정 보존, 주요 진입부 7곳과 시작 위치 4곳의 캡슐 충돌 검사 및 고해상도 외형 캡처. 실제 플레이·네트워크·성능은 미검증. [현재 구성·편집 위치·제약](Gameplay/Overview.md#round2-organic).

<a id="change-20261106-round2-fence-panels"></a>
### 2026-11-06 · Round2 울타리를 개별 네모 패널로 분리

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `L_Round2_Organic`의 울타리를 가로 최대 240cm의 독립 패널 302개로 교체했다. 기둥·철망은 패널 안에 묶고 충돌도 각 패널에 포함했다. 이전 일체형 액터 4개·투명 벽 52개를 레벨에서 제거해 패널 삭제만으로 통로를 열 수 있다. 지형·기믹·원본 맵은 보존했다.
- 확인: 저장·재로딩에서 패널별 변환·충돌과 울타리 외 액터 52개 보존, 대표 한 칸의 충돌 제거 시 개방 확인. PIE는 미실행. [편집 위치와 분리 도구](Gameplay/Overview.md#round2-organic).

<a id="change-20261106-round2-organic"></a>
### 2026-11-06 · Round2 연결 구조를 보존한 펜스와 이동 기믹 초안

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 현재 `L_Round2`의 땅·계단·엘리베이터·허브와 기존 액터 35개를 그대로 복제한 `L_Round2_Organic`을 생성했다. 시야가 열린 외곽·허브 펜스, 회전봉 2개·수평 왕복 블록 1개·범퍼 1개·물리 블록과 공 8개를 추가하고 원래 그레이박스 표면을 유지했다.
- 확인: 저장 후 재로딩에서 원본 파일과 기존 액터 보존, 기믹 설정·물리 속성·펜스 충돌 52곳·캐릭터 크기의 출입구 7곳을 확인했다. 실제 움직임·밀침·4인 경기는 PIE 미검증이다. [구성·생성 도구·제약](Gameplay/Overview.md#round2-organic).

<a id="change-20261106-round2-skyplay"></a>
### 2026-11-06 · 네 높이의 캐주얼 입체 아레나 생성

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `L_Round2`를 보존하고 별도 `L_Round2_SkyPlay`에 1.8·4.8·7.8·10.8m의 네 구역, 외곽 순환 경사로·중앙 합류 허브·점프대 3개·상하 발판 2개·낙하 후 복귀 경로와 시작점 4개를 구성했다. 캐릭터 캡슐과 보행 경사 규격을 기준으로 폭·높이를 맞췄다.
- 확인: 저장·재로딩, 경사로 충돌 27개 표본과 이동 장치·시작점 설정, 원본 파일 해시 보존. 실제 이동·점프·4인 경기는 PIE 미검증이다. [맵 구성과 제약](Gameplay/Overview.md#round2-skyplay).

<a id="change-20261106-playground-roundabout-repair"></a>
### 2026-11-06 · 놀이터 회전판 손잡이와 오뚝이 비율 복구

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `L_Lobby_Playground`의 회전판 장식 19개를 배치 형상이 포함된 전용 메시로 교체하고, 장식·오뚝이가 얇은 판의 비균일 스케일을 상속하지 않도록 수정했다. 기존 6방향 놀이터 디자인·부착·오뚝이 몸체 시뮬레이션과 다른 액터는 유지했다.
- 확인: 저장 후 맵 재로딩, 전용 메시·부착·스케일·단순 충돌 없음과 액터 204개 보존 및 에디터 외형 확인. PIE는 미실행. [구성·복구 도구](Gameplay/Overview.md#playground-roundabout-repair).

<a id="change-20261105-profile-api-url-quoting"></a>
### 2026-11-05 · 프로필 API URL 설정 문자열 잘림 수정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요
- 원인 확인: 팀원 `GetIni` 실행 로그에서 `ProfileApiBaseUrl = https:`를 확인했다. 이전 명시적 재로딩만으로 해결되지 않았으며 설정 문자열 자체가 잘렸다.
- 변경: `Config/DefaultGame.ini`의 URL 전체를 큰따옴표로 감싼다. 인증 코드나 주소 검증 조건은 변경하지 않는다.
- 확인 범위: 설정 및 문서 검토·공백 검사. 기존 최종 Game 설정의 유효 HTTPS 검사로 런타임 회귀를 확인할 수 있으나 Unreal 미설치로 직접 실행하지 않았다. 최신 코드 빌드가 된 PC는 설정 갱신 후 게임 재시작으로 재확인한다.
- 문서: [로그인 설정](Network/LoginEntry.md#통합-담당이-구성할-것), [진단 절차](Network/LoginEntry.md#로그인-준비-실패-진단), [Network 개요](Network/Overview.md#login-entry).

<a id="change-20261105-eos-login-config-fix"></a>
### 2026-11-05 · EOS 요청 권한 제한과 API 주소 로딩 보완

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요
- 변경: 상위 `AuthScopeFlags` 목록을 비우고 `BasicProfile`만 요청한다. `CFEntrySubsystem` 초기화 시 최종 `GGameIni` 캐시의 API 주소를 명시적으로 읽고 발견·유효 여부를 기록한다. 주소 고정값이나 인증 우회를 추가하지 않는다.
- 근거: 팀원 로그에서 일반 실행의 API 주소 검사 실패와 명령행 주소 지정 후 EOS 준비 성공, Account Portal의 `friends_list` 권한 거절을 확인했다. 비어 있는 프로퍼티의 최종 설정 재로딩 회귀 검사를 추가했다. Unreal 빌드·회귀 검사 실행·수정 후 실제 로그인은 미확인이다.
- 문서: [로그인 설정](Network/LoginEntry.md#통합-담당이-구성할-것), [진단 절차](Network/LoginEntry.md#로그인-준비-실패-진단), [Network 개요](Network/Overview.md#login-entry).

<a id="change-20261105-eos-login-diagnostics"></a>
### 2026-11-05 · EOS 로그인 준비 진단 로그

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요
- 변경: `CFEntrySubsystem`의 API 주소 검사, EOS 옵션, Artifact 선택·필수 필드 존재, 서브시스템·Identity 준비와 Account Portal 호출 진입을 `LogCFEntry`로 구분한다. 설정값·자격 증명 원문과 토큰은 출력하지 않는다. 로그인 조건과 UI 오류 문구는 유지한다.
- 확인 범위: 소스·문서 검토. Unreal 빌드 및 실제 계정 재실행은 미확인이다.
- 문서: [로그인 준비 실패 진단](Network/LoginEntry.md#로그인-준비-실패-진단), [Network 개요](Network/Overview.md#login-entry).

<a id="change-20261105-urban-sparring-plaza"></a>
### 2026-11-05 · L_TestJS 도심 야외 스파링 광장 디테일

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 기존 링·계단·캐릭터 배치를 유지하며 주변 건물 11동과 관중석 4곳에 전용 메시를 적용했다. 창호·한글 상가 간판·차양·옥상 설비, 포장석·가로등·펜스·휴지통, 바깥 배경 건물 18동을 추가했다. 신규 에셋은 메시 18개·머티리얼 25개·텍스처 6개이며 건물·광장 포장에 Nanite를 사용한다.
- 확인: 에셋·맵 저장과 에디터 외형, 기존 액터 및 경기 영역 변환 보존과 재질 연결 조회. PIE·네트워크·패키징·실기기 성능은 미검증. [광장 구성·생성 도구](Gameplay/Overview.md#urban-sparring-plaza).

<a id="change-20261105-eos-local-config"></a>
### 2026-11-05 · EOS 로컬 설정 입력·검사·실행 도구

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `Tools/Network/Set-EOSLocal.ps1`로 기존 Peer2Peer Client의 Secret을 마스킹 입력하고 Git 제외 `Config/Custom/EOSLocal/DefaultEngine.ini`를 생성한다. dev ID 불일치·추적 파일을 거절하며 `Start-EOSLoginTest.ps1`은 `-CustomConfig=EOSLocal`로 실행한다. 설정 검사는 Unreal 설치 없이 가능하다.
- 문서: [설정과 실계정 테스트](Network/LoginEntry.md#eos-로컬-설정-먼저-준비하기). Unreal 설치·실계정 인증·패키징 테스트는 미완료다.

<a id="change-20261105-login-entry"></a>
### 2026-11-05 · 로그인·프로필·연습 로비 진입 연결

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `CFEntrySubsystem`으로 EOS Account Portal·Connect 인증, 프로필 GET·닉네임 POST와 실제 로비 이동을 기존 타이틀에 연결했다. 등록 재시도·멱등성·401·지연/취소 응답을 처리하고 `BP_CF_TitleController.LobbyMap`을 지정했다. LAN Idle 상태가 별도 로비 이동 오류를 가로채지 않도록 보완했다.
- 확인: UE 5.8.2 Editor 빌드, 프로필 응답 자동화 검사, PIE 20개 검사와 저장된 로비 참조 확인. 인증 성공 응답은 테스트 도구에서 주입했으며, 클라이언트 EOS 설정·권한과 실계정 검증·패키징은 남아 있다.
- 상세: [로그인·로비 진입](Network/LoginEntry.md), [Network 개요](Network/Overview.md#login-entry), [메인 메뉴](UI/MainMenu.md).

<a id="change-20261105-lobby-ui"></a>
### 2026-11-05 · Figma 연습 로비 UI와 매칭 상태 인터페이스

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `L_Lobby_Playground`에 `BP_CF_LobbyGameMode`·`WBP_CF_Lobby`를 연결했다. 빠른 대전·매칭/취소/복구·설정 UI, BG/VFX 볼륨 저장과 분리 제어, 취소 확인 후 타이틀 복귀를 추가했다. 기존 메인 UI·연습 캐릭터 코드는 유지했다.
- 확인: Editor C++·Blueprint 컴파일, 큰 창 30개·작은 창 28개 PIE 검사 통과. 실제 EOS·음원 청취·패키징은 미검증이며 API 제공자는 후속 연결이 필요하다.
- 상세: [연습 로비 UI](UI/Lobby.md), [UI 개요](UI/Overview.md#practice-lobby).


<a id="change-20261104-title-ui"></a>
### 2026-11-04 · Figma 기반 타이틀·로그인·닉네임 UI

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `WBP_CF_TitleMenu`와 `BP_CF_TitleController`를 기본 메뉴 맵에 연결했다. 타이틀·로그인·최초 닉네임·접속 중 화면과 C++ 비동기 상태 인터페이스를 추가하고, 중복 요청·취소 후 늦은 응답·시간 초과를 처리한다. 기존 LAN 메뉴는 F9/`-CFLanMenu`로 유지한다.
- 확인: C++ Editor 빌드·Blueprint 컴파일, PIE UI 상태 37개 검사, 큰 창/작은 창 렌더 및 종료 버튼 검증 통과. 실제 EOS 통합·패키징 테스트는 미실행.
- 범위: 실제 EOS 인증·프로필 API·로비 이동 제공자는 미연결이다. UI 상태 검증의 모의 응답은 외부 테스트 도구에만 있다. [UI 현황](UI/Overview.md#menu), [화면·통합 계약](UI/MainMenu.md).

<a id="change-20261104-playground-wall-theme"></a>
### 2026-11-04 · 높은 외벽과 2층 창문의 놀이터 테마 적용

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 높은 외벽에 민트·크림 테마 재질, 노란 상부 마감, 둥근 모서리 캡과 원형 장식을 적용했다. 2층 전망창 3개는 유리를 유지하며 청록 창틀·노란 창턱으로 꾸몄다. 원본 에셋과 놀이기구·물리 설정은 변경하지 않았다.
- 확인: 저장 후 벽·창틀 12개의 테마 재질, 추가 장식 29개, 에디터 외형 확인. PIE 미실행. [놀이터 로비 구성](Gameplay/Overview.md#playground-lobby).

<a id="change-20261104-playground-enclosure"></a>
### 2026-11-04 · 놀이터 외벽 높이와 2층 창문 복원

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 놀이터 레벨의 외벽 높이를 원본 로비 구성으로 복원하고, 숨겨뒀던 상단 벽과 2층 창틀·유리창 3세트의 표시·충돌을 복원했다. 원본 맵과 놀이기구·물리 설정은 수정하지 않았다.
- 확인: 맵 저장, 벽·창문 액터 15개의 스케일·표시·충돌 재조회 및 에디터 외형 확인. PIE 미실행. [놀이터 로비 구성](Gameplay/Overview.md#playground-lobby).

<a id="change-20261104-playground-interactions"></a>
### 2026-11-04 · 놀이터 오뚝이 회전판 연결과 항아리 물리 설정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 놀이터 맵의 회전판 위 오뚝이를 판에 부착하고 내부 받침대 기준의 물리 흔들림을 유지했다. 아치 옆 항아리 2개에는 별도 복제 메시의 볼록 충돌과 12kg 물리 시뮬레이션을 적용했다. 기존 공유 에셋·원본 맵은 보존했다.
- 확인: 저장 후 연결·제약·충돌·질량·시뮬레이션 설정 재조회. PIE·밀침 강도·네트워크 미검증. [놀이터 로비 구성](Gameplay/Overview.md#playground-lobby).

<a id="change-20261104-playground-lobby"></a>
### 2026-11-04 · 별도 놀이터 튜토리얼 로비 생성

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 원본 `L_LobbyJS`를 보존하고 `L_LobbyJS_Playground`에 탄성포장 표현, 놀이대·열린 미끄럼틀·회전 손잡이·점프대 패딩·벤치·물리 블록을 구성했다. 기존 지형과 연습 액터를 유지했다.
- 확인: 새 맵 저장·에디터 외형·액터 구성 조회. PIE와 네트워크 미검증. [놀이터 로비 구성](Gameplay/Overview.md#playground-lobby).

<a id="change-20261103-eos-integration"></a>
### 2026-11-03 · EOS 자동 매칭 백엔드 통합과 목표 흐름 정리

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요
- 변경: EJ의 `Backend-Infra/` 전체를 `dev`에 통합했다. 기존 LAN 방 생성·2명 이상 수동 시작은 게임플레이 테스트 구현으로 유지하고, 제품 목표를 EOS 자동 4인 매칭으로 명시했다. Unreal의 실제 EOS 세션·화면 연동은 아직 완료되지 않았다.
- 문서: [EOS 클라이언트 인계](EOS/unreal-client-handoff.md), [API 계약](EOS/unreal-matchmaking-ticket-api.md), [UI 현황](UI/Overview.md), [Network 현황](Network/Overview.md).

<a id="change-20261101-window-glass"></a>
### 2026-11-01 · LD_Gym 반투명 유리창 메시 추가

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 지정한 LDProps/StaticMeshes 폴더에 SM_WindowGlass와 M_WindowGlass 추가. 120×4×180cm의 프레임 없는 유리판에 반투명 머티리얼과 단순 박스 충돌을 연결했다.
- 확인: 에셋 저장·렌더 정점 크기·미리보기 배경 투과 확인. 맵 배치·PIE 미실행. [유리창 구성](Gameplay/Overview.md#ld-window-glass).

<a id="change-20261101-ld-moving-obstacles"></a>
### 2026-11-01 · LD_Gym 상하·회전 발판의 Mover 의존성 제거

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 배경: 이관된 부모 BP_PhysicsObstacle의 Mover·ChaosPathedMovementController 구성 누락으로 자식 발판의 정상 표시와 컴파일에 문제가 있었다.
- 변경: 두 자식의 부모를 BP_KinematicObstacle로 변경하고 기본 InterpToMovement 왕복 이동(300cm·편도 3초)과 RotatingMovement 자전(45도/초)을 연결했다. 기존 부모와 맵의 저장되지 않은 편집은 저장하지 않았다.
- 확인: Blueprint 컴파일·개별 저장·컴포넌트 설정 재조회·메시 표시 확인. PIE·탑승·네트워크 미검증. [이동 발판 구성](Gameplay/Overview.md#ld-moving-obstacles).

<a id="change-20261106-unstable-balance"></a>
### 2026-11-06 · 일시적 불안정 상태와 상체 입력 보간

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 평상시 안정 이동을 유지하고 50% 이상 방향 점프·충돌 후 2초간 기존 기울기 누적·중력·반대 입력 보정을 적용한다. 재발생하면 시간을 갱신하며 만료 후 현재 자세에서 안정 계산으로 복귀한다. 강한 방향 점프에는 충전량 비례 기울기 충격을 추가한다. 실제 입력을 RawTiltInput으로 분리하고 기존 AnimBP 참조 CurrentTilt는 보간된 상체 표현값으로 유지했다.
- 검증: 30/60/120 FPS 수치 계산에서 평상시 유지, 강한 충격 한계 초과, 짧은 반대 입력으로 버티기, 약한 충격 후 복귀 및 상체 입력 변화량 제한을 확인했다. 정식 빌드·PIE·AnimGraph·멀티플레이 검증은 미수행이다.
- 문서: [불안정 상태](Gameplay/Overview.md#unstable-balance), [상체 표현](Animation/BasicAnimations.md).

<a id="change-20261106-safe-input-tilt"></a>
### 2026-11-06 · 일반 이동 기울기 제한·자동 복귀

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 배경: WASD로 이동하려고 누르면 너무 쉽게 기울어 넘어지는 문제.
- 변경: 입력 기울기 목표를 넘어짐 한계의 기본 35%로 제한하고 임계 감쇠 스프링으로 따라간다. 입력 해제 시 중심으로 자동 복귀한다. 충돌은 기존 기울기 속도에 반영하며 한계 초과 넘어짐 판정은 유지한다. 기존 누적 기울기·반대 입력 보정 수치는 현재 계산에서 사용하지 않는다.
- 검증: 30/60/120 FPS 수치 시뮬레이션으로 장시간 입력·해제 복귀와 한계 근처 충격 판정을 확인했다. 사용자 요청에 따라 빌드는 실행하지 않았으며 실제 PIE·멀티플레이·충돌 강도 검증은 남아 있다.
- 문서: [이동과 균형](Gameplay/Overview.md#balance).

<a id="change-20261106-view-movement"></a>
### 2026-11-06 · 시점 기준 이동과 종료 크래시 진단

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: WASD 이동과 차징 점프 방향을 시점의 수평 회전 기준으로 통일하여 캐릭터 회전 보간에 따른 방향 지연을 제거했다. 균형·충돌 규칙은 유지한다.
- 진단: Live Coding 후 종료 시 캐릭터 맵 소멸 경로의 접근 위반을 확인했다. 클래스 메모리 배치 불일치가 유력하며 정식 빌드·재실행으로 확인해야 한다.
- 문서: [이동](Gameplay/Overview.md#balance), [크래시 복구와 플레이 검증](Gameplay/DebugLogging.md). 변경 후 빌드·PIE 검증은 아직 미실행.

<a id="change-20261106-jump-aim"></a>
### 2026-11-06 · 점프 조준과 기울기 분리

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 충전 중 WASD로 점프 방향을 정하고 입력 해제 후 마지막 방향을 유지한다. 새 충전에서 방향을 지정하지 않으면 제자리 점프한다. 기울기 대신 충전량으로 발사 세기를 결정하며 수평 계수 0.95·수직 계수 1을 사용한다. 기존 균형·넘어짐은 유지하고 SpaceBar 충전을 추가했다(LeftMouseButton 유지).
- 문서: [차징 점프](Gameplay/Overview.md#jump). 빌드·PIE·멀티플레이 검증 미실행.

<a id="change-20261106-audio-compile"></a>
### 2026-11-06 · 효과음 컴파일 오류 수정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `ChickenAudioComponent::Update`의 두 `Voices.FindRef()` 결과를 `auto*` 대신 `UAudioComponent*`로 받아 `TObjectPtr` 타입 추론 오류(C3535/C2440)를 수정했다. 기존 재생·정지 동작은 유지한다.
- 문서: [효과음](Gameplay/Overview.md#combat-audio). 소스 정적 검토만 수행했으며 빌드·PIE는 미실행.

<a id="change-20261105-combat-feedback"></a>
### 2026-11-05 · 닭싸움 기울기·모션 속도와 효과음

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 배경: WASD 기울기가 급하게 변하는 느낌을 완화하고 점프·차징·충돌 소리를 추가해 달라는 요청.
- 변경: 로컬 시각 기울기에 지수 보간·100도/초 제한을 적용했다. 점프 시작 최소 0.22초, 착지 회복 최소 0.3초로 모션을 완화하며 자동 홉 간격도 늘어난다. ChickenAudioComponent에 교체 가능한 기본 합성 효과음 5종, 공간 감쇠, 서버 사건 전달, 충돌 중복 억제를 연결했다. 기존 작업 중이던 점프 파워 100~400과 이동 모드 디버그 수정은 보존했다.
- 문서: [이동·균형](Gameplay/Overview.md#balance), [효과음](Gameplay/Overview.md#combat-audio), [기본 모션](Animation/BasicAnimations.md). AnimBP 에셋 편집·청음·멀티플레이 검증은 미수행.

<a id="change-20261030-collision-timing"></a>
### 2026-10-30 · 충돌 속도 판정 시점 보완

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 배경: 간헐적으로만 충돌 반응이 발생한다는 보고. 로컬 UE 5.8.2 소스에서 캡슐 BlockingHit 지연 전달과 HandleImpact → MoveBlockedBy → 슬라이드 처리 순서를 확인했다. 기존 플레이 로그에는 충격 성공 사례만 있고 반복 충돌·거절 로그가 없어 증상의 직접 원인은 미확정이다.
- 변경: 서버 MoveBlockedBy에서도 속도 보정 전 공통 충돌 처리를 호출한다. 기존 캡슐 이벤트와 쌍별 쿨다운을 공유하고 MovementBlocked 진단 로그를 추가했다. 속도 비례 파워와 최소 접근 속도는 유지한다.
- 문서: [충돌 처리](Gameplay/Overview.md#knockdown), [검증 절차](Gameplay/DebugLogging.md). 코드·엔진 호출 순서 정적 검토, 변경 후 빌드·PIE 미실행.


<a id="change-20261030-counter-tilt"></a>
### 2026-10-30 · 반대 방향 입력의 균형 회복 개선

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 축별 반대 입력에만 가속도 2배 적용. 바깥쪽으로 기우는 관성에는 추가 제동 6을 적용하고 중심 통과·입력 해제 시 기본 계산으로 복귀한다. 자동 중심 복원은 추가하지 않았다.
- 문서: [이동과 균형](Gameplay/Overview.md#balance), [로그·확인 절차](Gameplay/DebugLogging.md). 빌드·PIE 검증 미실행.


<a id="change-20261030-character-tuning"></a>
### 2026-10-30 · 이동·차징·기울기 한계와 충돌 반응 조정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 전투 MaxWalkSpeed 275, 점프 파워 200~700, 축별 기울기 한계 60도 상당으로 완화. 고정 수평 충격을 접근 속도 비례 충격으로 바꾸고 작은 수직 충격을 추가했다. 기존 기울기 속도 누적·전체 메시 보간과 디버깅 로그를 유지한다.
- 문서: [이동·균형](Gameplay/Overview.md#balance), [차징 점프](Gameplay/Overview.md#jump), [충돌](Gameplay/Overview.md#knockdown), [로그·플레이 확인 절차](Gameplay/DebugLogging.md).
- 검증 범위: 코드 정적 검토. 빌드·PIE·Blueprint 별도 기본값 확인은 미실행.


<a id="change-20261023-character-debug"></a>
### 2026-10-23 · 캐릭터 충돌·기울기·이동 디버깅 로그

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: UE_LOG 카테고리 3개와 사건/반복 출력 콘솔 설정 추가. 충돌 쿨다운·밀치기 요청/후속 속도, 기울기 계산·입력 해제·넘어짐, 이동 차단·점프·착지·모드 전환을 관측한다. 기존 활성 PushStop/PushDispatch 로그를 통합하고 물리 계산은 유지했다.
- 문서: [Gameplay 로그](Gameplay/Overview.md#debug-logging), [설정과 테스트](Gameplay/DebugLogging.md). 기존 충돌 진단 경로·차징 즉시 정지에 맞게 개요를 정정했다.


<a id="change-20261006-tutorial-popup"></a>
### 2026-10-06 · 튜토리얼 팝업 표시 영역 수정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: BP_TutorialPopup의 Widget Draw Size를 500 × 500에서 1920 × 1080으로 변경해 제목·그림 간격을 WBP 디자인과 맞춤. 액터 미리보기·컴파일·저장 확인, 게임 실행 미검증.
- 문서: [튜토리얼 팝업](UI/Overview.md#튜토리얼-팝업).

의미 있는 구현·이름·경로·설정 변경을 짧게 기록하고 관련 현재 문서로 연결한다. 기존 작업 전체의 이력을 소급 복원하지 않으며 과거 상세는 Git과 기존 문서 참고.

<a id="change-20261011-ship"></a>
### 2026-10-11 · PR 생성 및 머지 스킬 추가

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: `ship` 스킬과 AGENTS 읽기 경로 추가. 기본은 `commit` 스킬의 전체 변경 검토·커밋 → 푸시 → 지정 브랜치 PR → 머지. 스테이징된 것만·기존 커밋만 등 명시적 제한 우선. 영어 태그·한국어 제목 사용, 충돌 시 자동 해결 없이 PR을 열린 상태로 유지.
- 문서: [PR 생성 및 머지 스킬](Guides/AI-Workflow-Design.md#17-pr-생성-및-머지-스킬).

<a id="change-20261004-gitignore"></a>
### 2026-10-04 · 생성 파일 제외와 솔루션 추적 해제

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: 생성된 솔루션·Xcode 프로젝트·컴파일 중간 파일·플러그인 출력 제외 보강. Build 아이콘과 PakBlacklist 파일 예외 유지. 솔루션 4개는 로컬 파일을 유지하고 Git 추적만 해제했다.
- 문서: [생성된 IDE 파일과 Git 추적](Guides/AI-Workflow-Design.md#16-생성된-ide-파일과-git-추적).

<a id="change-20261002-match-integration"></a>
### 2026-10-02 · 4인 경기 진행 및 dev 충돌 병합

- 작업자: 경기 진행 @KyuminChung (`c8a7452`, `60d0b2f`, `99aeaf1`), dev 충돌·차징 @sunsi-game (`7ed2786`, `2ee4755`). 병합 정리: 이번 CKM 작업, 커밋 전.
- 변경: 최대 4인·최소 2인 경기, 탈락·관전, 4R 점수·최종 MVP·같은 방 걷기 대기 복귀. 친구의 충돌·차징 코드와 통합하고 중복 UpdateBodyTilt 블록을 제거했다. 충돌 요청에 서버 권한·경기 입력 잠금 검사를 적용했다. dev 에셋과 문서 폴더 구조를 보존하고 이동된 문서 링크를 수정했다.
- 문서: [경기 상태](Gameplay/Overview.md#match), [넘어짐·충돌](Gameplay/Overview.md#knockdown), [라운드](RoundProgression.md), [최종 결과](FinalResults.md).

<a id="change-20261001-06"></a>
### 2026-10-01 · 문서 갱신 스킬 이름 단축

- 작업자: @j1sung · Codex 수행
- 변경: `project-docs` → `docs`. 폴더·스킬 식별자·지침·설계 안내·요청문 참조 갱신. 동작 변경 없음.
- 문서: [문서 갱신 스킬](../.agents/skills/docs/SKILL.md) · [설계 안내](Guides/AI-Workflow-Design.md#12-스킬템플릿프롬프트).

<a id="change-20261001-05"></a>
### 2026-10-01 · 커밋 스킬 이름 단축

- 작업자: @j1sung · Codex 수행
- 변경: `commit-staged` → `commit`. 스킬 폴더·식별자·현재 안내·기존 기록의 링크 갱신. 동작 변경 없음.
- 문서: [커밋 스킬](../.agents/skills/commit/SKILL.md) · [설계 안내](Guides/AI-Workflow-Design.md#15-스테이징-커밋-스킬).

<a id="change-20261001-04"></a>
### 2026-10-01 · 커밋 스킬의 자동 스테이징 추가

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: 스테이징을 따로 하지 않아도 현재 작업을 묶어서 커밋하도록 요청.
- 변경: 기본 실행은 현재 변경 검토→선택 스테이징→작업별 커밋. 스테이징 전용·메시지 초안 모드 유지, 생성물·비밀정보 제외 및 부분 스테이징 처리 명시.
- 문서: [커밋 스킬 설계](Guides/AI-Workflow-Design.md#15-스테이징-커밋-스킬) · [스킬](../.agents/skills/commit/SKILL.md).

<a id="change-20261001-03"></a>
### 2026-10-01 · 스테이징 커밋 스킬 추가

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: 스테이징된 여러 작업을 지나치게 세분화하지 않고 관련 작업끼리 묶어 기록할 필요.
- 변경: commit-staged 스킬과 한국어 메시지 템플릿 추가. 큰 작업 단위 분류, 영어 태그·한국어 제목·본문, 부분 스테이징 보존 및 커밋 승인 범위 정의.
- 문서: [커밋 스킬 설계](Guides/AI-Workflow-Design.md#15-스테이징-커밋-스킬) · [스킬](../.agents/skills/commit/SKILL.md).

<a id="change-20261001-02"></a>
### 2026-10-01 · AI 지침 구조화 및 공통 규칙 보강

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: 단순 목록 형태의 지침을 참고 레포처럼 적용 조건과 주제별로 구분할 필요.
- 변경: 지침 5개에 소제목·판단 조건·완료 점검 추가. 기존 설계 의도 충돌, Unreal 헤더·delegate·수명·네트워크·UI·패키징·MCP, Git 인계·작업 보존 규칙 보강. 기존 승인 정책과 팀 전략 유지.
- 문서: [설계·운영 안내](Guides/AI-Workflow-Design.md#14-지침-체계화-및-참고-구성-반영) · [지침 입구](../AGENTS.md).

<a id="change-20261001-01"></a>
### 2026-10-01 · AI 관리 체계 적용 및 초기 문서 정리

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: AI 및 팀 작업의 기술 부채와 현재 구조를 카테고리별로 파악하기 위한 체계 필요.
- 변경: AGENTS·5개 지침·문서화 스킬·템플릿·요청문 작성. 기존 6개 문서 재배치, 중복 설명·링크·오래된 문구 정리, 4개 대표 문서 작성. 코드·에셋 변경 없음.
- 문서: [Network](Network/Overview.md) · [LAN 상세](Network/ListenServerSessions.md), [UI](UI/Overview.md) · [메인 메뉴](UI/MainMenu.md) · [대기 UI](UI/WaitingRoomUI.md), [Gameplay](Gameplay/Overview.md), [Animation](Animation/Overview.md) · [기본 상태](Animation/BasicAnimations.md) · [서기·걷기](Animation/StandWalkAnimations.md) · [넘어짐](Animation/DirectionalKnockdown.md).
- 관리 설계: [최종 설계·운영 안내](Guides/AI-Workflow-Design.md).
