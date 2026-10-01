# 변경 이력

의미 있는 구현·이름·경로·설정 변경을 짧게 기록하고 관련 현재 문서로 연결한다. 기존 작업 전체의 이력을 소급 복원하지 않으며 과거 상세는 Git과 기존 문서 참고.

<a id="change-20261001-04"></a>
### 2026-10-01 · 커밋 스킬의 자동 스테이징 추가

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: 스테이징을 따로 하지 않아도 현재 작업을 묶어서 커밋하도록 요청.
- 변경: 기본 실행은 현재 변경 검토→선택 스테이징→작업별 커밋. 스테이징 전용·메시지 초안 모드 유지, 생성물·비밀정보 제외 및 부분 스테이징 처리 명시.
- 문서: [커밋 스킬 설계](Guides/AI-Workflow-Design.md#15-스테이징-커밋-스킬) · [스킬](../.agents/skills/commit-staged/SKILL.md).

<a id="change-20261001-03"></a>
### 2026-10-01 · 스테이징 커밋 스킬 추가

- 작업자: @j1sung · Codex 수행 · 미커밋
- 배경: 스테이징된 여러 작업을 지나치게 세분화하지 않고 관련 작업끼리 묶어 기록할 필요.
- 변경: commit-staged 스킬과 한국어 메시지 템플릿 추가. 큰 작업 단위 분류, 영어 태그·한국어 제목·본문, 부분 스테이징 보존 및 커밋 승인 범위 정의.
- 문서: [커밋 스킬 설계](Guides/AI-Workflow-Design.md#15-스테이징-커밋-스킬) · [스킬](../.agents/skills/commit-staged/SKILL.md).

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
