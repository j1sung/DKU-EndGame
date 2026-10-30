# 프로젝트 문서 목차

카테고리 대표 문서는 현재 구조를 설명하고, 상세 문서는 설정·연결·기술 설명을 보충한다. 코드·에셋 확인 범위는 각 문서의 제한을 참고한다.

| 카테고리 | 대표 문서 | 상세 문서 |
|---|---|---|
| Network | [개요](Network/Overview.md) | [LAN 세션·연결 종료](Network/ListenServerSessions.md) |
| UI | [개요](UI/Overview.md) | [메인 메뉴](UI/MainMenu.md), [대기 UI](UI/WaitingRoomUI.md) |
| Gameplay | [개요](Gameplay/Overview.md) | [경기 시작](MatchStart.md), [탈락·관전](RoundEnd.md), [라운드 점수](RoundProgression.md), [최종 결과·복귀](FinalResults.md) |
| Animation | [개요](Animation/Overview.md) | [기본 상태](Animation/BasicAnimations.md), [서기·걷기](Animation/StandWalkAnimations.md), [넘어짐](Animation/DirectionalKnockdown.md) |

## 관리와 기록
- [변경 이력](Changes.md)
- [AI 작업·문서 관리 설계와 사용 안내](Guides/AI-Workflow-Design.md) — 사람용 참고, 일반 작업의 필수 지침 아님.
- [전체 문서 갱신 요청문](../.agents/prompts/refresh-project-docs.md)

## 최근 기록

- [2026-10-30 · 충돌 판정 시점 보완](Changes.md#change-20261030-collision-timing)

- [2026-10-30 · 역입력 균형 회복 개선](Changes.md#change-20261030-counter-tilt)

- [2026-10-30 · 이동·차징·기울기·충돌 조정](Changes.md#change-20261030-character-tuning)

- [2026-10-23 · 캐릭터 디버깅 로그](Changes.md#change-20261023-character-debug)

- [2026-10-11 · PR 생성 및 머지 스킬 추가](Changes.md#change-20261011-ship)
- [2026-10-04 · 생성 파일 제외와 솔루션 추적 해제](Changes.md#change-20261004-gitignore)
- [2026-10-02 · 4인 경기 진행 및 dev 충돌 병합](Changes.md#change-20261002-match-integration)
- [2026-10-01 · 문서 갱신 스킬 이름 단축](Changes.md#change-20261001-06)
- [2026-10-01 · 커밋 스킬 이름 단축](Changes.md#change-20261001-05)
- [2026-10-01 · 커밋 스킬 자동 스테이징](Changes.md#change-20261001-04)
- [2026-10-01 · 스테이징 커밋 스킬 추가](Changes.md#change-20261001-03)
- [2026-10-01 · 지침 구조화·규칙 보강](Changes.md#change-20261001-02)
- [2026-10-01 · 관리 체계 적용 및 초기 문서 정리](Changes.md#change-20261001-01)
