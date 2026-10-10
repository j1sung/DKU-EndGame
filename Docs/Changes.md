# 변경 이력

<a id="change-20261006-tutorial-popup"></a>
### 2026-10-06 · 튜토리얼 팝업 표시 영역 수정

- 작업자: Codex 수행 · 요청자 GitHub ID 확인 필요 · 미커밋
- 변경: BP_TutorialPopup의 Widget Draw Size를 500 × 500에서 1920 × 1080으로 변경해 제목·그림 간격을 WBP 디자인과 맞춤. 액터 미리보기·컴파일·저장 확인, 게임 실행 미검증.
- 문서: [튜토리얼 팝업](UI/Overview.md#튜토리얼-팝업).

의미 있는 구현·이름·경로·설정 변경을 짧게 기록하고 관련 현재 문서로 연결한다. 기존 작업 전체의 이력을 소급 복원하지 않으며 과거 상세는 Git과 기존 문서 참고.

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
