# 프로젝트 문서 목차

카테고리 대표 문서는 현재 구조를 설명하고, 상세 문서는 설정·연결·기술 설명을 보충한다. 코드·에셋 확인 범위는 각 문서의 제한을 참고한다.

현재 LAN 방 생성·수동 시작은 게임플레이 테스트 구현이다. 목표 흐름은 [EOS 자동 4인 매칭 인계서](EOS/unreal-client-handoff.md)와 [매칭 API 계약](EOS/unreal-matchmaking-ticket-api.md)을 따른다. 백엔드는 `Backend-Infra/`에 구현·개발 AWS 배포됐고, Unreal의 [EOS Round1 연결 코드](Network/EOSRound1Session.md)는 작업 브랜치에 추가했으며 새 Editor 빌드·실계정 4인 검증 전이다.

| 카테고리 | 대표 문서 | 상세 문서 |
|---|---|---|
| Network | [개요](Network/Overview.md) | [LAN 세션·연결 종료](Network/ListenServerSessions.md), [로그인·로비 진입](Network/LoginEntry.md), [자동 매칭 티켓](Network/MatchmakingTickets.md), [EOS Round1 연결](Network/EOSRound1Session.md) |
| UI | [개요](UI/Overview.md) | [메인 메뉴](UI/MainMenu.md), [연습 로비](UI/Lobby.md), [대기 UI](UI/WaitingRoomUI.md) |
| Gameplay | [개요](Gameplay/Overview.md) | [경기 시작](MatchStart.md), [탈락·관전](RoundEnd.md), [라운드 점수](RoundProgression.md), [최종 결과·복귀](FinalResults.md), [스폰 위치 연결](Gameplay/RoundSpawns.md), [경기 설정](Gameplay/RoundGameplaySetup.md) |
| Animation | [개요](Animation/Overview.md) | [기본 상태](Animation/BasicAnimations.md), [서기·걷기](Animation/StandWalkAnimations.md), [넘어짐](Animation/DirectionalKnockdown.md) |
| EOS 자동 매칭 | [클라이언트 인계](EOS/unreal-client-handoff.md) | [API 계약](EOS/unreal-matchmaking-ticket-api.md), [백엔드 운영](../Backend-Infra/README.md) |

## 관리와 기록
- [변경 이력](Changes.md)
- [AI 작업·문서 관리 설계와 사용 안내](Guides/AI-Workflow-Design.md) — 사람용 참고, 일반 작업의 필수 지침 아님.
- [전체 문서 갱신 요청문](../.agents/prompts/refresh-project-docs.md)

## 최근 기록

- [2026-11-08 · EOS 참가 주소의 대괄호 손실 수정](Changes.md#change-20261108-eos-travel-url)

- [2026-11-08 · EOS 세션 코드의 Editor 빌드 오류 수정](Changes.md#change-20261108-eos-session-build-fix)

- [2026-11-08 · EOS Round1 4인 접속과 자동 시작 연결 코드](Changes.md#change-20261108-eos-round1-session)

- [2026-11-08 · 라운드 경기 설정 적용과 로컬 다인 생성 검증](Changes.md#change-20261108-round-gameplay-apply)

- [2026-11-08 · 네 라운드 맵에 스폰 위치 적용](Changes.md#change-20261108-round-spawn-link-apply)

- [2026-11-08 · Round1 스파링장을 16m로 확대](Changes.md#change-20261108-round1-ring-expand)

- [2026-11-08 · Round4 복합 순환 전투 맵 초안](Changes.md#change-20261108-round4-finale)

- [2026-11-08 · Round3 다리 높이·회전 막대·통나무 재생성](Changes.md#change-20261108-round3-sweeper-log-respawn)

- [2026-11-08 · Round3 2층·흔들다리·주기 분수](Changes.md#change-20261108-round3-upper-bridge-jets)

- [2026-11-07 · Round3 장애물 전투 아레나 재구성](Changes.md#change-20261107-round3-arena)

- [2026-11-07 · Round3 장애물 코스 초안](Changes.md#change-20261107-round3-draft)

- [2026-11-07 · 개별 철망 패널의 옥상 공사용 리프트](Changes.md#change-20261107-round2-construction-lift)

- [2026-11-07 · 옥상 플레이 구역 외곽 높은 펜스 추가](Changes.md#change-20261107-round2-rooftop-perimeter)

- [2026-11-07 · Round2 Organic 도심 옥상 테마 적용](Changes.md#change-20261107-round2-rooftop)

- [2026-11-06 · Round2 울타리를 개별 네모 패널로 분리](Changes.md#change-20261106-round2-fence-panels)

- [2026-11-06 · Round2 연결 구조를 보존한 펜스와 이동 기믹 초안](Changes.md#change-20261106-round2-organic)

- [2026-11-06 · 네 높이의 캐주얼 입체 아레나 생성](Changes.md#change-20261106-round2-skyplay)

- [2026-11-06 · 놀이터 회전판 손잡이와 오뚝이 비율 복구](Changes.md#change-20261106-playground-roundabout-repair)

- [2026-11-06 · 일시적 불안정 상태와 상체 입력 보간](Changes.md#change-20261106-unstable-balance)

- [2026-11-06 · 일반 이동 기울기 제한·자동 복귀](Changes.md#change-20261106-safe-input-tilt)

- [2026-11-06 · 시점 기준 이동과 종료 크래시 진단](Changes.md#change-20261106-view-movement)

- [2026-11-06 · 점프 조준과 기울기 분리](Changes.md#change-20261106-jump-aim)

- [2026-11-06 · 효과음 컴파일 오류 수정](Changes.md#change-20261106-audio-compile)

- [2026-11-06 · 로비 자동 매칭 티켓 API 연결](Changes.md#change-20261106-matchmaking-tickets)

- [2026-11-05 · 닭싸움 모션 완화·효과음](Changes.md#change-20261105-combat-feedback)

- [2026-11-05 · L_TestJS 도심 야외 스파링 광장 디테일](Changes.md#change-20261105-urban-sparring-plaza)

- [2026-11-05 · 로그인·프로필·연습 로비 진입 연결](Changes.md#change-20261105-login-entry)

- [2026-11-05 · Figma 연습 로비 UI와 매칭 상태 인터페이스](Changes.md#change-20261105-lobby-ui)

- [2026-11-04 · Figma 기반 타이틀·로그인·닉네임 UI](Changes.md#change-20261104-title-ui)

- [2026-11-04 · 높은 외벽과 2층 창문의 놀이터 테마 적용](Changes.md#change-20261104-playground-wall-theme)

- [2026-11-04 · 놀이터 외벽 높이와 2층 창문 복원](Changes.md#change-20261104-playground-enclosure)

- [2026-11-04 · 놀이터 오뚝이 회전판 연결과 항아리 물리 설정](Changes.md#change-20261104-playground-interactions)

- [2026-11-04 · 별도 놀이터 튜토리얼 로비 생성](Changes.md#change-20261104-playground-lobby)

- [2026-11-03 · EOS 자동 매칭 백엔드 통합과 목표 흐름 정리](Changes.md#change-20261103-eos-integration)

- [2026-11-06 · 일시적 불안정 상태와 상체 입력 보간](Changes.md#change-20261106-unstable-balance)

- [2026-11-06 · 일반 이동 기울기 제한·자동 복귀](Changes.md#change-20261106-safe-input-tilt)

- [2026-11-06 · 시점 기준 이동과 종료 크래시 진단](Changes.md#change-20261106-view-movement)

- [2026-11-06 · 점프 조준과 기울기 분리](Changes.md#change-20261106-jump-aim)

- [2026-11-06 · 효과음 컴파일 오류 수정](Changes.md#change-20261106-audio-compile)

- [2026-11-05 · 닭싸움 모션 완화·효과음](Changes.md#change-20261105-combat-feedback)

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
