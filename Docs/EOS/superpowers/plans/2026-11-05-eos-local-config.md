# EOS 로컬 개발 설정 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 기존 Peer2Peer Client의 자격 증명을 로컬에서 마스킹 입력하고, Unreal 로그인 구현이 읽을 개발 설정을 준비한다.

**Architecture:** 공개 식별자는 `Backend-Infra/samconfig.toml`의 dev 설정에서 읽는다. Secret은 `Read-Host -AsSecureString`으로 입력해 Git 제외 경로 `Config/Custom/EOSLocal/DefaultEngine.ini`에만 저장한다. 실행 시 `-CustomConfig=EOSLocal`로 선택한다. 설정 생성·정적 검사는 Unreal 없이 수행한다.

**Tech Stack:** Windows PowerShell 5.1+/PowerShell 7, UE 5.8 OnlineSubsystemEOS의 Artifacts 설정.

**Spec:** [로그인·프로필·로비 진입](../../../Network/LoginEntry.md). 사용자가 확인한 기존 Client ID와 Peer2Peer 정책/EAS Linked Clients를 재사용한다.

## 제약과 검증 초점

- Secret·토큰을 명령행 인수·환경 변수·콘솔 출력·추적 파일에 넣지 않는다.
- Git 제외 여부를 확인하기 전에는 Secret 입력·파일 쓰기를 시작하지 않는다.
- 다른 Client ID가 저장된 설정을 사용하면 백엔드 검증에 실패하므로 설정 검사에서 거절한다.
- 설정 생성 성공은 실계정 인증 성공을 뜻하지 않는다. 현재 PC에는 Unreal이 없다.
- AWS 배포와 게임플레이 코드는 변경하지 않는다.

## 작업

- [x] 설정 모듈과 입력/검사 도구: 마스킹 입력, 식별자 파싱, 안전한 INI 값 검증, Git 제외 검사. 테스트로 잘못된 값·ID 불일치·제외 누락·출력 비노출을 확인한다.
- [x] 실행 도구: UnrealEditor 경로와 설정을 확인한 뒤 Secret 없는 인수로 실행한다.
- [x] `LoginEntry.md`에 설정 명령, 실제 테스트 순서, 현재 미검증 항목을 반영한다.
- [x] PowerShell 검사와 문서 링크·Git 제외를 검증한다. 실제 Client Secret은 사용자가 로컬 마스킹 입력창에서 입력했고 `-CheckOnly`가 READY를 반환했다.

실제 Unreal 실행·설정 로딩·Epic 인증은 아직 미검증이다. 현재 PC에는 Unreal이 없다. PowerShell 7에서 자동 검사를 실행했다.
