# Sprint 1 체크리스트 — DB 설계와 기반 환경 구축

## 1. 스프린트 목표

인증·프로필·매칭 기능을 구현하기 전에 데이터 구조, API 계약, AWS·EOS 개발 환경을 확정하고 최소 통합 경로를 검증한다.

```text
클라이언트 또는 테스트 요청
→ API Gateway
→ 테스트 Lambda
→ DynamoDB 읽기·쓰기
→ CloudWatch 로그 확인
```

## 2. 이번 스프린트의 범위

### 포함

- [ ] 백엔드 개발 언어와 Lambda 런타임 결정
- [ ] AWS 인프라 코드화 도구 결정
- [ ] DynamoDB 접근 패턴과 상세 구조 설계
- [ ] 티켓·매치 상태와 만료·복구 원칙 설계
- [ ] 매칭 워커의 실행·재시도 방식 설계
- [ ] API 요청·응답·오류 형식 초안 작성
- [ ] AWS 개발 환경 구축
- [ ] EOS 개발 환경 준비 상태 확인
- [ ] Unreal 클라이언트팀과 연동 계약 초안 합의
- [ ] API Gateway부터 DynamoDB와 CloudWatch까지 최소 통합 검증

### 제외

- [ ] EOS 토큰 검증 기능의 전체 구현
- [ ] 플레이어 프로필과 닉네임 API의 전체 구현
- [ ] 실제 4인 매칭 알고리즘 구현
- [ ] EOS 세션 생성·참가 구현
- [ ] Unreal 게임 플레이와 네트워크 코드 구현
- [ ] 운영 환경 배포와 최종 시연 환경 구축

## 3. 선행 결정 사항

### 개발 환경과 기술 스택

- [ ] Lambda 구현 언어 결정
- [ ] Lambda 런타임 버전 결정
- [ ] AWS SAM과 AWS CDK 중 인프라 코드화 도구 결정
- [ ] 패키지 관리자와 의존성 관리 방식 결정
- [ ] 단위 테스트 도구 결정
- [ ] 코드 검사와 서식 도구 결정
- [ ] 로컬 실행·테스트 방법 결정
- [x] 개발 환경에서 사용할 AWS 리전 결정
- [ ] AWS 리소스 이름 규칙 결정
- [ ] 개발·발표 환경 구분 방식 결정
- [ ] 환경 변수와 비밀 정보 관리 방식 결정

### 식별자와 공통 규칙

- [ ] `requestId` 생성 주체와 형식 결정
- [ ] `playerId`에 EOS PUID를 사용하는 규칙 명시
- [ ] `ticketId` 형식과 생성 주체 결정
- [ ] `matchId` 형식과 생성 주체 결정
- [ ] 모든 시간 값의 기준을 UTC로 통일
- [ ] API에서 사용할 날짜·시간 표현 형식 결정
- [ ] 빌드 또는 클라이언트 버전 전달 방식 결정

## 4. DynamoDB 상세 설계

### 접근 패턴 정리

- [ ] PUID로 플레이어 프로필 조회
- [ ] 닉네임 정규화 값으로 중복 여부 확인
- [ ] 프로필과 닉네임 점유 항목을 함께 생성
- [ ] 플레이어의 현재 활성 티켓 조회
- [ ] 새로운 매칭 티켓 생성
- [ ] 티켓 ID를 검증하며 현재 티켓 취소
- [ ] 같은 버전과 조건의 대기 티켓 조회
- [ ] 후보 티켓 4개의 최신 상태 재확인
- [ ] 티켓 4개 선점과 매치 생성을 원자적으로 처리
- [ ] 매치 ID로 매치와 구성원 조회
- [ ] 매치 구성원인지 확인
- [ ] 지정된 호스트인지 확인
- [ ] EOS 세션 정보 등록·조회
- [ ] 참가 상태와 경기 시작 상태 변경
- [ ] 만료·실패·종료 후 활성 상태 해제
- [ ] 유효한 사용자의 재매칭 가능 상태 복구

### 테이블과 인덱스

- [ ] 물리적 테이블 수와 분리 기준 결정
- [ ] `PlayerProfiles` 영역의 항목 유형 정의
- [ ] `MatchmakingTickets` 영역의 항목 유형 정의
- [ ] `Matches` 영역의 항목 유형 정의
- [ ] 각 항목의 PK와 SK 결정
- [ ] 각 조회 패턴에 필요한 GSI 결정
- [ ] GSI 파티션 쏠림 가능성 검토
- [ ] 프로필과 닉네임 점유 항목의 속성 정의
- [ ] 티켓 항목의 상태·조건·유효시간 속성 정의
- [ ] 매치 항목의 구성원·호스트·세션·상태 속성 정의
- [ ] 항목별 예시 데이터를 문서에 추가

### 동시성과 트랜잭션

- [ ] 닉네임 동시 등록을 막는 조건식 정의
- [ ] 한 사용자에게 활성 티켓이 하나만 존재하도록 하는 조건 정의
- [ ] 한 사용자가 두 활성 매치에 포함되지 않도록 하는 조건 정의
- [ ] 티켓 4개 변경과 매치 생성을 묶는 트랜잭션 정의
- [ ] 실제 선점 시 상태·티켓 ID·유효시간을 다시 검사하도록 정의
- [ ] 지연된 취소 요청이 새 티켓을 취소하지 못하도록 정의
- [ ] 중복 요청에 대한 멱등성 기준 정의
- [ ] 트랜잭션 충돌 시 재시도 조건과 최대 횟수 정의

### 상태와 수명

- [ ] 티켓 상태 목록과 전이 조건 정의
- [ ] 매치 상태 목록과 전이 조건 정의
- [ ] 대기 티켓의 유효시간 정의
- [ ] 배정된 티켓과 매치의 준비 제한시간 정의
- [ ] 애플리케이션의 만료 판단 방식 정의
- [ ] DynamoDB TTL 대상 속성과 보존 기간 정의
- [ ] TTL 삭제를 정확한 타이머로 사용하지 않는 원칙 명시
- [ ] 취소·실패·이탈·종료 시 정리 절차 정의

### 매칭 워커

- [ ] 매칭 워커 실행 계기 결정
- [ ] 동시에 실행되는 워커 사이의 충돌 처리 방식 정의
- [ ] 처리할 대기열과 버전·조건 구분 방식 정의
- [ ] 4명 미만일 때 종료 또는 재실행하는 방식 정의
- [ ] 일시적 오류와 조건 충돌의 재시도 방식 정의
- [ ] 반복 실패한 작업의 추적 방식 정의
- [ ] 워커 실행과 결과를 기록할 로그 항목 정의

## 5. API 계약 초안

### 공통 계약

- [ ] API 기본 경로와 버전 관리 방식 결정
- [ ] 인증 헤더 형식 결정
- [ ] 요청과 응답의 `Content-Type` 결정
- [ ] 공통 성공 응답 원칙 결정
- [ ] 공통 오류 응답 구조 정의
- [ ] 오류 코드 목록 초안 작성
- [ ] 입력 검증 실패와 인증·권한 실패를 구분
- [ ] 응답에 `requestId`를 포함하도록 정의
- [ ] 중복 요청 또는 멱등성 키 처리 방식 결정
- [ ] 매칭 상태 폴링 주기와 백오프 원칙 정의
- [ ] 클라이언트 버전 불일치 처리 방식 정의

### 프로필 API

- [ ] `GET /me` 요청·응답 정의
- [ ] `POST /me/nickname` 요청·응답 정의
- [ ] 닉네임 길이와 허용 문자 초안 정의
- [ ] 닉네임 정규화 규칙 초안 정의
- [ ] 닉네임 중복 오류 정의

### 매칭 API

- [ ] `POST /matchmaking/tickets` 요청·응답 정의
- [ ] `GET /matchmaking/tickets/current` 요청·응답 정의
- [ ] `DELETE /matchmaking/tickets/current` 요청·응답 정의
- [ ] 취소 요청에서 대상 티켓 ID를 검증하도록 정의
- [ ] 이미 대기·배정·취소된 경우의 응답 정의

### 매치와 세션 API

- [ ] `GET /matches/{matchId}` 요청·응답 정의
- [ ] `POST /matches/{matchId}/session` 요청·응답 정의
- [ ] `POST /matches/{matchId}/joined` 요청·응답 정의
- [ ] `POST /matches/{matchId}/started` 요청·응답 정의
- [ ] 매치 구성원과 호스트 권한 오류 정의
- [ ] EOS 세션 정보 전달 형식 초안 정의
- [ ] 호스트 시도 번호 또는 동일 역할의 검증 값 정의

## 6. AWS 개발 환경 구축

### 인프라 코드

- [ ] `Backend-Infra` 아래 백엔드·인프라 프로젝트 구조 생성
- [ ] 선택한 인프라 코드화 도구 초기화
- [ ] 개발 환경 설정 파일 구조 작성
- [ ] API Gateway 정의
- [ ] 테스트 Lambda 정의
- [ ] DynamoDB 리소스 정의
- [ ] Lambda 실행 역할과 최소 권한 정책 정의
- [ ] CloudWatch 로그 그룹과 보존 기간 정의
- [ ] 리소스 태그 규칙 적용
- [ ] 배포 명령과 제거 방법 문서화

### 테스트 Lambda

- [ ] 상태 확인 응답 구현
- [ ] `requestId` 생성 또는 전달 구현
- [ ] 구조화 로그 출력 구현
- [ ] DynamoDB 테스트 항목 쓰기 구현
- [ ] DynamoDB 테스트 항목 읽기 구현
- [ ] 오류 발생 시 공통 오류 형식으로 응답
- [ ] 로그에 토큰이나 비밀 정보가 포함되지 않는지 확인

### 권한과 설정

- [ ] Lambda에 필요한 DynamoDB 권한만 부여
- [ ] CloudWatch Logs 권한 확인
- [ ] 개발 환경 변수 주입 방식 확인
- [ ] 저장소에 비밀 정보가 포함되지 않도록 확인
- [x] 배포에 필요한 AWS 자격 증명 사용 방법 문서화

### AWS 개발 자격 증명 사용 방법

- 루트 계정의 Access Key는 생성하거나 사용하지 않는다.
- 개발자는 IAM Identity Center 사용자로 로그인하고 임시 자격 증명을 사용한다.
- 로컬 AWS CLI 프로필 이름은 `dku-dev`로 통일한다.
- 기본 AWS 리전은 서울 리전인 `ap-northeast-2`를 사용한다.
- 로그인 세션이 만료되면 `aws sso login --profile dku-dev`로 다시 인증한다.
- 연결 상태는 `aws sts get-caller-identity --profile dku-dev`로 확인한다.

## 7. EOS 개발 환경 준비

- [x] Epic Developer Portal 접근 권한 확인
- [x] EOS Product 생성 또는 기존 Product 확인
- [x] 개발용 Sandbox 확인
- [x] 개발용 Deployment 확인
- [x] 클라이언트 정책과 권한 확인
- [x] EAS 로그인 설정 확인
- [x] EOS Connect 설정 확인
- [x] 내부 사용자 식별에 PUID를 사용한다는 계약 확인
- [x] 백엔드에 전달할 인증 증명 후보 확인
- [x] 백엔드 검증 방법과 필요한 공개 정보 확인
- [x] 토큰 만료와 갱신 책임 구분
- [x] 개발용 Epic 계정 준비
- [ ] 최종 4인 시연에 사용할 서로 다른 계정 4개의 준비 방법 확인
- [ ] Product·Sandbox·Deployment 식별자의 공유·보관 방법 결정

### EOS 개발 로그인과 Connect 흐름

- Epic 계정 로그인은 EAS Auth Interface를 사용한다.
- Auth Interface에서 발급받은 Epic ID Token으로 EOS Connect 로그인을 수행한다.
- 첫 로그인에서 `EOS_InvalidUser`가 반환되면 사용자 동의 흐름 뒤 `EOS_Connect_CreateUser`로 Product User ID(PUID)를 생성한다.
- 내부 `playerId`는 Epic Account ID가 아니라 제품별 PUID를 사용한다.
- Epic 계정으로 Connect를 사용하는 경우 별도의 외부 ID 제공자 설정은 필요하지 않다.
- 클라이언트는 인증 만료 알림을 처리하고 만료 전에 다시 로그인한다. 백엔드는 만료되거나 검증되지 않은 인증 증명을 거부한다.
- 참고: [Epic 계정으로 EOS Connect 로그인](https://dev.epicgames.com/docs/epic-online-services/eos-fundamentals/connect-interface/connect-guide/log-in-with-an-epic-games-account), [Dev Auth Tool 로그인](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/eos-epic-account-services/auth-interface/auth-guide/log-in-with-the-dev-auth-tool)

### EOS 인증 증명과 백엔드 검증 결정

- Unreal 클라이언트는 Connect 로그인 후 `EOS_Connect_CopyIdToken`으로 Connect ID Token을 발급받아 AWS API 요청의 `Authorization: Bearer <token>` 헤더로 전달한다.
- API Gateway 뒤의 TypeScript/Node.js Lambda는 일반 요청 처리와 권한 흐름을 담당한다.
- Connect ID Token 검증은 Linux EOS SDK의 `EOS_Connect_VerifyIdToken`을 호출하는 격리된 네이티브 검증 계층에서 수행한다.
- 검증 성공 결과의 PUID만 내부 `playerId`로 사용하고, 클라이언트가 별도로 보낸 사용자 ID는 신뢰하지 않는다.
- 검증 시 서명·만료뿐 아니라 Client ID, Product ID, Sandbox ID, Deployment ID 일치 여부를 확인한다.
- Client Secret과 원본 토큰은 코드·문서·로그에 남기지 않으며 배포 환경의 비밀 저장소에서 주입한다.
- Epic OAuth 공개 JWKS는 EAS OAuth 액세스 토큰용이다. 실제 Connect ID Token은 해당 JWKS에 일치하는 키가 없으므로 순수 Node.js JWKS 검증 경로를 사용하지 않는다.

## 8. 클라이언트팀 협의

- [ ] Epic 로그인 이후 백엔드 호출 순서 합의
- [ ] EOS Connect 로그인과 PUID 확보 시점 합의
- [ ] 인증 증명 전달 형식 합의
- [ ] 토큰 만료·갱신·재로그인 처리 책임 합의
- [ ] API 기본 주소와 환경 전환 방식 합의
- [ ] 요청·응답 JSON 명명 규칙 합의
- [ ] 클라이언트 버전 전달 방식 합의
- [ ] 매칭 상태 폴링 주기와 백오프 합의
- [ ] EOS 세션 정보를 등록하고 전달받는 형식 합의
- [ ] 인증·네트워크·버전 오류의 UI 처리 방향 공유
- [ ] 합의 내용을 API 문서에 반영

## 9. 검증 체크리스트

### 인프라 검증

- [ ] 빈 환경에서 인프라 배포 성공
- [ ] API Gateway 엔드포인트 호출 성공
- [ ] 테스트 Lambda 정상 응답 확인
- [ ] Lambda에서 DynamoDB 테스트 항목 쓰기 성공
- [ ] Lambda에서 DynamoDB 테스트 항목 읽기 성공
- [ ] `requestId`로 CloudWatch 로그 검색 성공
- [ ] Lambda 오류가 공통 오류 형식으로 반환되는지 확인
- [ ] 로그에 인증 토큰과 비밀 정보가 없는지 확인
- [ ] 배포한 개발 리소스의 제거 또는 롤백 절차 확인

### 문서 검증

- [ ] 모든 접근 패턴이 하나 이상의 키 또는 인덱스로 설명됨
- [ ] 각 트랜잭션의 성공 조건과 실패 조건이 명시됨
- [ ] 티켓과 매치 상태 전이에 모순이 없음
- [ ] 애플리케이션 만료와 TTL 삭제가 구분됨
- [ ] 각 API에 요청·응답·오류 예시가 있음
- [ ] 클라이언트팀이 API 계약 초안을 검토함
- [ ] 미확정 사항과 담당자가 기록됨

## 10. Sprint 1 완료 기준

- [ ] 기술 스택과 인프라 코드화 도구가 결정되었다.
- [ ] DynamoDB 접근 패턴과 상세 구조가 문서화되었다.
- [ ] PK·SK·GSI·조건식·트랜잭션·TTL 정책이 정의되었다.
- [ ] 티켓과 매치의 상태 및 수명 정책이 정의되었다.
- [ ] 매칭 워커의 실행·충돌·재시도 방식이 정의되었다.
- [ ] API 요청·응답·오류 명세 초안이 준비되었다.
- [ ] AWS 개발 환경이 인프라 코드로 배포된다.
- [ ] API Gateway → Lambda → DynamoDB 경로가 동작한다.
- [ ] `requestId`로 CloudWatch 로그를 조회할 수 있다.
- [x] EOS Product·Sandbox·Deployment 준비 상태를 확인했다.
- [ ] 인증·버전·세션 정보에 관한 클라이언트 계약이 정리되었다.
- [ ] 남은 위험과 Sprint 2 인계 항목이 기록되었다.

## 11. 위험 요소와 대응 기록

| 위험 요소 | 영향 | 대응 방안 | 상태 |
|---|---|---|---|
| EOS 인증 증명과 백엔드 검증 방식 미확정 | 인증 구현 지연 | 실제 Connect ID Token으로 OAuth JWKS 부적합을 확인하고 EOS SDK 네이티브 검증으로 확정 | 대응 완료 |
| DynamoDB 접근 패턴 누락 | 스키마 재설계 | API별 읽기·쓰기 패턴을 먼저 검토 | 확인 필요 |
| 클라이언트 계약 지연 | 통합 일정 지연 | API 초안을 조기에 공유하고 검토 일정 고정 | 확인 필요 |
| AWS 또는 Epic 권한 부족 | 환경 구축 중단 | 스프린트 시작 시 계정과 권한 먼저 확인 | AWS·Epic 확인 완료 |

## 12. 결정 기록

| 날짜 | 결정 사항 | 선택 내용 | 이유 | 영향 범위 |
|---|---|---|---|---|
| 2026-10-06 | AWS 개발 리전 | `ap-northeast-2`(서울) | 개발 인력과 시연 환경에 가까운 리전을 사용 | AWS 인프라 전체 |
| 2026-10-06 | AWS 개발자 인증 | IAM Identity Center의 `dku-dev` SSO 프로필과 임시 자격 증명 사용 | 루트 계정과 장기 Access Key의 일상 사용 방지 | 로컬 개발·배포 |
| 2026-10-06 | 초기 개발 권한 | `AdministratorAccess`, 세션 기간 1시간 | 초기 인프라 구축 후 최소 권한으로 축소 예정 | AWS 개발 계정 |
| 2026-10-06 | EOS 개발 환경 | Product `chicken_game`의 기본 `Live` Sandbox·Deployment 사용 | 초기 통합 경로를 단순하게 유지 | EOS 개발·통합 |
| 2026-10-06 | EOS 게임 클라이언트 정책 | `Peer2Peer` 템플릿 사용 | 인증된 사용자 기반 리슨 서버와 EOS Lobby·Session 구조에 부합 | Windows 게임 클라이언트 |
| 2026-10-06 | EAS 개발 권한 | `Basic Profile`만 활성화하고 추가 권한은 비활성화 | 로그인에 필요한 최소 권한만 사용 | EAS 개발 로그인 |
| 2026-10-06 | EOS 사용자 식별 | EAS Auth의 Epic ID Token으로 Connect 로그인 후 발급되는 PUID를 내부 `playerId`로 사용 | EOS Game Services의 제품별 사용자 식별자와 일치 | 인증·프로필·매칭 |
| 2026-10-06 | EOS 인증 갱신 책임 | 클라이언트가 만료 전에 Auth·Connect 재로그인, 백엔드는 만료된 증명 거부 | SDK 인증 수명과 서버 접근 제어 책임을 분리 | Unreal 클라이언트·백엔드 |
| 2026-10-06 | 백엔드 기본 런타임 | TypeScript/Node.js Lambda | 팀 개발성과 AWS 관리형 서비스 연동성을 우선 | AWS API 백엔드 |
| 2026-10-06 | 백엔드 인증 증명 | Unreal이 발급받은 EOS Connect ID Token | PUID와 Product·Sandbox·Deployment·Client 범위를 한 증명으로 전달 | Unreal·AWS 인증 경계 |
| 2026-10-06 | Connect 토큰 검증 | TypeScript 요청 계층과 Linux EOS SDK 네이티브 검증 계층을 분리하고 `EOS_Connect_VerifyIdToken` 사용 | 실제 토큰의 `kid`가 Epic OAuth JWKS와 일치하지 않아 순수 Node.js 오프라인 검증 불가 | Lambda 패키징·인증 구현 |

## 13. 검증 결과

| 날짜 | 검증 항목 | 결과 | 증거 또는 참고 자료 |
|---|---|---|---|
| 2026-10-06 | IAM Identity Center 활성화와 개발자 권한 할당 | 성공 | `chickengame-dev-admin` 사용자 및 `AdministratorAccess` 권한 세트 할당 완료 |
| 2026-10-06 | AWS CLI SSO 연결 | 성공 | `aws sts get-caller-identity --profile dku-dev`에서 `AWSReservedSSO_AdministratorAccess` 역할 확인 |
| 2026-10-06 | Epic Developer Portal과 EOS Product | 성공 | 조직 `chicken_game`, Product `chicken_game`, 기본 `Live` Sandbox·Deployment 확인 |
| 2026-10-06 | EOS Client Policy와 Client | 성공 | `chickengame-p2p-client-policy`와 `chickengame-windows-client` 생성 및 연결 완료; Client Secret은 문서화하지 않음 |
| 2026-10-06 | EAS 개발 애플리케이션 | 성공 | 초안 애플리케이션에 `Basic Profile` 권한과 Windows Client 연결 완료; 공개 배포용 브랜드 리뷰는 미완료 |
| 2026-10-06 | EOS Connect 로그인 흐름 검토 | 성공 | Epic 공식 문서에서 Auth ID Token → Connect Login → PUID 흐름과 Epic 계정 사용 시 외부 ID 제공자 설정 불필요 확인 |
| 2026-10-06 | 실제 EOS Auth·Connect 로그인 | 성공 | Dev Auth Tool로 EAS 동의 후 Connect 로그인과 신규 PUID 생성 완료 |
| 2026-10-06 | Connect ID Token 구조 | 성공 | RS256 JWT, 만료 1시간, `sub` PUID와 `aud` Client ID 및 `pfpid`·`pfsid`·`pfdid` 범위 일치 확인; 토큰 원문은 저장하지 않음 |
| 2026-10-06 | 순수 Node.js OAuth JWKS 검증 | 부적합 확인 | Epic OAuth JWKS에 실제 Connect ID Token의 `kid`와 일치하는 키가 없어 `ERR_JWKS_NO_MATCHING_KEY` 발생 |
| 2026-10-06 | EOS SDK Lambda 배포 가능성 | 확인 | SDK 패키지에서 Linux x64·ARM64 공유 라이브러리를 확인하여 네이티브 검증 계층 패키징 가능 |

## 14. Sprint 2 인계

- [ ] 확정된 인증 증명과 검증 방법 전달
- [ ] 프로필과 닉네임 데이터 구조 전달
- [ ] `GET /me` 계약 전달
- [ ] `POST /me/nickname` 계약 전달
- [ ] 개발 환경 배포·실행 방법 전달
- [ ] 미해결 결정 사항과 위험 요소 전달
