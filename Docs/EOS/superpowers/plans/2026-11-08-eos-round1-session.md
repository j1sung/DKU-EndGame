# EOS Round1 세션 연결 구현 계획

> 실행: superpowers:executing-plans. 기존 `codex/eos-session-integration` 작업 브랜치를 사용한다. 커밋·푸시는 별도 요청 시 진행한다.

**목표:** 자동 배정된 4명이 실제 EOS P2P로 Round1에 접속하고 서버가 자동으로 3초 카운트다운 후 시작한다.

**구조:** 새 GameInstance 세션 통합 계층이 티켓 배정 이후 매치 API·EOS 세션·이동을 소유한다. 기존 LAN 계층과 스폰 로직을 보존하고 RoundGameMode에 명단 검사와 자동 시작 승인만 추가한다.

**기술:** UE 5.8 OnlineSubsystemEOS/Sessions, EOS P2P NetDriver, 기존 HTTP API. 실제 Editor 빌드는 이 PC에서 불가하다.

**근거:** 사용자 이번 요청, [기존 API 계약](../../unreal-matchmaking-ticket-api.md), [클라이언트 인계](../../unreal-client-handoff.md).

## 고정 조건

- `standard`/`0.1.0`, 4인, 배정 후 사용자 매칭 취소 없음. 호스트 이전·재접속 없음.
- 서버 지정 hostPlayerId와 memberIds만 사용. 접속의 EOS UniqueNetId에서 PUID를 검증한다.
- `/joined`는 실제 Round1 로드·소유 Pawn 확인 이후, `/started`는 실제 Playing 진입 후 보고한다.
- API 변경·AWS 배포·맵 에셋 재생성은 필요 없다. 토큰·Secret은 로그/이동 URL에 넣지 않는다.
- 이번 검증 목표는 Round1 Playing까지. 다음 맵 이동·최종 결과 영속화는 별도 단계다.

## 작업

- [x] EOSLocal 설정에 기본 OSS=EOS, UE5.8 P2P NetDriver를 추가하고 기존 로컬 Secret을 출력 없이 유지하는 `-Upgrade`를 제공한다. 가짜 자격 증명 기반 PowerShell 테스트를 먼저 추가한다.
- [x] 매치 응답 명단·단계·세션 ID 검증 native automation 테스트를 먼저 추가한다. 실제 서버 준비 조건은 4PC 검증 대상으로 남기고 실행 불가 제한을 명시한다.
- [x] 배정 인계, 멱등 HTTP 보고, 호스트 생성/리스닝 후 등록, 매치 속성 검색과 정확한 세션 검증, 참가·이동·실패 정리를 구현한다.
- [x] RoundGameMode의 접속 명단·중복 검사, 서버 실제 4인 준비 검사, 수동 시작 차단 및 자동 시작을 연결한다. LAN 수동 경로는 유지한다.
- [x] 관련 문서·진행 상황·4PC 실행/성공 기준을 갱신하고 코드 리뷰와 가능한 로컬 검증을 수행한다.
- [ ] Unreal 설치 PC에서 새 Editor 빌드, native automation 및 실제 EOS 4인 Round1 Playing을 검증한다.

## 검토 초점

지연된 생성/참가 콜백, 타임아웃 후 같은 멱등 키 재사용, 서로 다른 계정의 응답 혼입, 맵 이동 중 타이머 수명, 중복/비명단 접속, 이미 종료된 매치, 일부 접속 후 끊김을 검토한다. 서버 응답이 불명확하면 종료 확정으로 표시하지 않고 복구 상태를 유지한다.

## 권장안 결정 기록

- EOSLocal에서만 기본 OSS·NetDriver를 전환한다. 일반 실행의 NULL/LAN 설정은 유지한다.
- 세션 검색은 `CF_MATCH_ID` 속성으로 제한하고 결과의 정확한 sessionId·호스트 PUID를 다시 검사한다. 친구 권한은 요청하지 않는다.
- GameInstance ticker를 사용해 맵 이동에도 준비 확인과 API 복구가 계속되도록 한다.
- 실패 시 `/left`로 전체 매치 실패를 보고하고 로컬 세션 정리 후 연습 로비로 돌아간다. 이는 배정 전 취소와 다르다.

## 결과와 검증 제한

Windows PowerShell 5.1과 PowerShell 7에서 설정 테스트를 실행했다. 새 네트워크 설정 누락으로 먼저 실패하는 것을 확인하고 구현 후 통과했다. native 테스트는 Unreal 미설치로 RED/GREEN 실행이 불가하며 작성·코드 검토까지만 수행했다.

독립 코드 리뷰에서 비 const API 호출의 const 포인터와 검색 요청 동기 실패 시 delegate 미해제를 발견해 수정했다. 생성/참가/검색이 완료되지 않았는데 정리 완료로 표시하지 않도록 처리한다. 맵 이동 중 GI ticker로 상태를 이어가며 준비 단계 변경 때 서버 deadline을 기준으로 연결 기한을 갱신한다. terminal 확인 후 남은 쓰기 전송을 중단한다. 실제 EOS/NetDriver/HTTP 런타임 결과는 후속 Unreal PC 검증으로 확인해야 한다.
