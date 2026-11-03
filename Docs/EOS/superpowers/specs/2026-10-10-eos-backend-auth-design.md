# EOS 백엔드 인증 계층 설계

## 1. 목적

Unreal 클라이언트가 AWS API 요청에 전달한 EOS Connect ID Token을 백엔드에서 검증하고, 검증 결과의 Product User ID(PUID)를 요청자의 신원으로 사용하는 인증 계층을 구축한다.

첫 구현은 개발 환경의 실제 AWS 배포와 API Gateway 호출 검증까지 포함한다. 프로필 데이터, 닉네임, 매칭 기능은 이 인증 계층을 기반으로 후속 구현한다.

## 2. 확정된 전제

- 클라이언트는 Epic Account Services(EAS) 로그인 후 `EOS_Auth_CopyUserAuthToken`으로 얻은 EAS access token을 `EOS_ECT_EPIC` 자격 증명으로 EOS Connect에 전달한다.
- EOS Connect 로그인 후 얻은 PUID가 게임의 내부 `playerId`다.
- 클라이언트는 `EOS_Connect_CopyIdToken`으로 Connect ID Token을 복사하고 AWS API 요청의 `Authorization: Bearer <token>` 헤더에 넣는다.
- Connect ID Token은 일반 Epic OAuth JWKS에 일치하는 공개키가 없어 순수 Node.js JWKS 방식으로 검증하지 않는다.
- Amazon Linux 2023과 EOS SDK 1.19.2.1에서 `EOS_Connect_VerifyIdToken` 성공 및 결과 PUID 일치를 실제 토큰으로 확인했다.
- AWS 리전은 `ap-northeast-2`, 개발자 프로필은 `dku-dev`다.

## 3. 범위

### 포함

- API Gateway HTTP API의 `GET /v1/me`
- TypeScript/Node.js Lambda 요청 처리 계층
- C++ EOS Connect ID Token 검증 실행 파일
- Lambda 컨테이너 이미지 패키징
- AWS SAM 인프라 정의
- EOS Client Secret의 Secrets Manager 보관
- 단위·컨테이너·로컬 SAM·실제 AWS 통합 테스트
- CloudWatch 구조화 로그와 민감정보 비노출 검증
- 개발 환경 배포·롤백·제거 절차 문서화

### 제외

- DynamoDB 프로필 조회와 닉네임 등록
- 매칭과 EOS Session API
- API Gateway JWT Authorizer
- 검증 결과 캐싱
- Node.js 네이티브 애드온 최적화
- 운영 환경 배포

## 4. 기술 선택

| 영역 | 선택 |
|---|---|
| 인프라 코드화 | AWS SAM |
| Lambda 패키징 | 컨테이너 이미지 |
| API 런타임 | TypeScript, Node.js 24 |
| 컨테이너 기반 | Amazon Linux 2023 기반 AWS Lambda Node.js 24 이미지 |
| CPU 아키텍처 | `x86_64` |
| 네이티브 검증 | C++ 실행 파일과 Linux EOS SDK |
| 비밀정보 저장 | AWS Secrets Manager |
| 리전 | `ap-northeast-2` |

Node.js API와 C++ 검증기는 같은 Lambda 컨테이너 이미지에 넣는다. Node.js는 요청마다 검증 실행 파일을 셸 없이 직접 실행한다. 이 구조는 이미 검증된 C++ 흐름을 재사용하고 네이티브 오류를 Node.js 프로세스에서 격리한다. 요청당 프로세스 시작과 EOS 초기화 비용은 측정 후 문제가 될 때만 네이티브 애드온 또는 지속 프로세스 방식으로 최적화한다.

## 5. 시스템 구조

```text
Unreal Client
    │ Authorization: Bearer <Connect ID Token>
    ▼
API Gateway HTTP API
    ▼
TypeScript Lambda Handler
    ├─ requestId 생성 또는 전달
    ├─ Authorization 헤더 검사
    ├─ JWT 기본 구조와 PUID 후보 검사
    └─ NativeTokenVerifier 호출
             │ stdin: PUID 후보 + 토큰
             ▼
       C++ EOS Verifier
             │
             ▼
 EOS_Connect_VerifyIdToken
      ┌──────┴──────┐
      │             │
   성공 + PUID     실패
      │             │
      ▼             ▼
 request.auth     401 또는 5xx
 .playerId
```

### API Gateway

- HTTPS 요청을 수신해 Lambda에 전달한다.
- Connect ID Token을 자체 JWT Authorizer로 검증하지 않는다.
- CORS는 Unreal 네이티브 클라이언트에 필요하지 않으므로 초기 범위에서 활성화하지 않는다.

### TypeScript Lambda

- `Authorization` 헤더에서 Bearer 토큰을 추출한다.
- 토큰 원문을 로그, 응답, 예외 메시지에 포함하지 않는다.
- JWT payload의 `sub`를 PUID 후보로 읽지만 검증 성공 전에는 신뢰하지 않는다.
- 실행 환경 최초 요청에서 Secrets Manager의 Client Secret을 읽고 메모리에 캐시한다.
- 네이티브 검증기를 호출하고 결과 PUID가 후보 PUID와 일치하는지 확인한다.
- 성공한 PUID를 `request.auth.playerId` 형태의 인증 문맥으로 API 처리 코드에 전달한다.
- 클라이언트가 본문이나 쿼리에 넣은 자신의 `playerId`는 사용하지 않는다.

### C++ EOS 검증기

- EOS SDK와 Platform을 서버 모드로 초기화한다.
- `EOS_Connect_IdToken`의 `ProductUserId`에는 Node.js가 전달한 PUID 후보를 넣는다.
- `EOS_Connect_VerifyIdToken` 완료까지 EOS Platform을 Tick한다.
- 성공 결과의 Client ID, Product ID, Sandbox ID, Deployment ID가 현재 백엔드 설정과 일치하는지 확인한다.
- 성공 시 EOS 결과 PUID만 제한된 JSON 형식으로 표준 출력에 쓴다.
- EOS 로그와 진단은 표준 오류로 분리하며 토큰과 Client Secret을 출력하지 않는다.
- 제한시간 초과, 초기화 실패, 토큰 검증 실패를 서로 구분되는 종료 코드와 결과 코드로 표현한다.

### TokenVerifier 경계

TypeScript API는 다음과 같은 추상 경계를 사용한다.

```text
TokenVerifier
├── NativeTokenVerifier  실제 C++ 실행 파일 호출
└── FakeTokenVerifier    단위 테스트용
```

API 코드가 네이티브 구현의 프로세스·EOS 세부사항을 알지 않게 해 향후 네이티브 애드온 방식으로 교체할 수 있게 한다.

## 6. 요청 처리 흐름

1. API Gateway가 요청을 Lambda에 전달한다.
2. Lambda가 `requestId`를 생성하거나 API Gateway 요청 ID를 사용한다.
3. 인증 미들웨어가 `Authorization: Bearer <token>`을 파싱한다.
4. 토큰 길이, JWT 세 부분, `sub`의 PUID 기본 형식을 검사한다.
5. 검증 전 `sub`는 네이티브 검증기에 제공할 후보값으로만 사용한다.
6. Node.js가 Secrets Manager에서 Client Secret을 읽어 실행 환경 메모리에 캐시한다.
7. Node.js가 셸 없이 C++ 검증 실행 파일을 시작한다. 토큰과 PUID 후보는 표준 입력으로, 자격 증명과 EOS 설정은 통제된 자식 프로세스 환경으로 전달한다.
8. 네이티브 검증기가 전달받은 설정을 이용해 EOS Platform을 초기화한다.
9. `EOS_Connect_VerifyIdToken` 성공 결과의 PUID와 후보 PUID를 비교한다.
10. 검증 결과의 Client ID, Product ID, Sandbox ID, Deployment ID가 백엔드 설정과 일치하는지 확인한다.
11. 모두 일치하면 검증된 PUID를 인증 문맥에 넣고 `/v1/me` 처리기를 실행한다.
12. 처리기가 `playerId`와 `requestId`를 반환한다.

토큰은 명령줄 인수로 전달하지 않는다. Node.js는 `child_process.spawn`을 셸 비활성 상태로 사용하며 토큰은 표준 입력으로만 전달한다.

## 7. API 계약

### 요청

```http
GET /v1/me
Authorization: Bearer <Connect ID Token>
Accept: application/json
```

### 성공 응답

```json
{
  "playerId": "검증된 PUID",
  "requestId": "요청 추적 ID"
}
```

초기 `/v1/me`는 인증 확인만 담당한다. 프로필 기능을 구현할 때 같은 경로의 응답에 닉네임과 프로필 필드를 추가한다.

### 오류 응답

```json
{
  "error": {
    "code": "AUTH_INVALID",
    "message": "Authentication failed"
  },
  "requestId": "요청 추적 ID"
}
```

| 상황 | HTTP | 오류 코드 |
|---|---:|---|
| Authorization 헤더 없음 | 401 | `AUTH_MISSING` |
| Bearer 형식 오류 | 401 | `AUTH_MALFORMED` |
| JWT 기본 구조 또는 PUID 후보 오류 | 401 | `AUTH_INVALID` |
| 만료·조작·범위 불일치 토큰 | 401 | `AUTH_INVALID` |
| 검증 제한시간 초과 또는 EOS 연결 실패 | 503 | `AUTH_VERIFIER_UNAVAILABLE` |
| 검증기 실행·출력 오류 | 500 | `AUTH_VERIFIER_FAILURE` |
| Secret 또는 서버 설정 누락 | 500 | `INTERNAL_CONFIGURATION_ERROR` |

## 8. 보안 정책

- 토큰의 최대 허용 길이는 16KB다.
- 검증 전 토큰 payload는 신뢰하지 않는다.
- 검증 성공 콜백의 PUID만 인증된 `playerId`로 사용한다.
- Connect ID Token과 Client Secret은 로그, 응답, 예외에 포함하지 않는다.
- Client Secret은 Secrets Manager에 저장한다.
- Lambda가 실행 시 읽어 캐시한 Client Secret은 검증기 실행 중에만 통제된 자식 프로세스 환경으로 전달하며 명령줄 인자에는 넣지 않는다.
- Lambda 실행 역할에는 해당 Secret에 대한 `secretsmanager:GetSecretValue`만 허용한다.
- Product ID, Sandbox ID, Deployment ID, Client ID는 Lambda 환경 변수로 주입한다.
- Client Secret은 컨테이너 이미지, SAM 템플릿, Lambda 함수의 정적 환경 변수, Docker build argument에 넣지 않는다.
- 네이티브 검증기는 토큰을 표준 입력으로 받고 셸을 사용하지 않는다.
- Lambda 컨테이너는 쓰기 가능한 `/tmp` 외 영역을 불변으로 취급하며 검증 토큰을 파일에 저장하지 않는다.
- Secrets Manager Secret에는 삭제 정책 `Retain`을 적용한다.

## 9. 로그와 관측

성공 로그 예시:

```json
{
  "requestId": "...",
  "route": "GET /v1/me",
  "statusCode": 200,
  "authOutcome": "success",
  "playerId": "검증된 PUID",
  "verificationMs": 234
}
```

실패 로그에는 PUID 후보와 토큰을 넣지 않는다.

```json
{
  "requestId": "...",
  "route": "GET /v1/me",
  "statusCode": 401,
  "authOutcome": "invalid_token",
  "verificationMs": 181
}
```

다음 값을 측정한다.

- Lambda cold start 시간
- Lambda warm start 시간
- EOS 검증 시간
- 전체 API 응답 시간
- 컨테이너 이미지 크기
- Lambda 최대 메모리 사용량

초기 성능 목표는 warm 요청 2초 이내다. 네이티브 검증 제한시간은 8초, Lambda 전체 제한시간은 15초다. 반복 측정에서 목표를 넘으면 네이티브 애드온이나 지속 프로세스 구조를 후속 검토한다.

## 10. 저장소 구조와 SDK 취급

```text
Backend-Infra/
├── template.yaml
├── samconfig.toml
├── backend/
│   ├── Dockerfile
│   ├── package.json
│   ├── tsconfig.json
│   ├── src/
│   │   ├── handler.ts
│   │   ├── auth/
│   │   │   ├── middleware.ts
│   │   │   ├── token-verifier.ts
│   │   │   └── native-token-verifier.ts
│   │   └── http/
│   │       ├── responses.ts
│   │       └── request-context.ts
│   ├── native/
│   │   ├── CMakeLists.txt
│   │   └── eos-token-verifier.cpp
│   └── test/
└── vendor/
    └── eos-sdk/
```

`Backend-Infra/vendor/eos-sdk/`는 Git에서 제외한다. 개발자가 Epic에서 받은 SDK의 `Include`와 Linux x64 공유 라이브러리를 이 경로에 준비한다. Epic SDK 원본과 바이너리는 공개 저장소에 커밋하지 않는다.

커밋 대상은 자체 TypeScript·C++ 코드, Dockerfile, SAM 템플릿, 테스트, 문서다.

## 11. SAM 리소스

개발 스택 이름은 `chickengame-backend-dev`다.

| 리소스 | 설정 |
|---|---|
| API Gateway | HTTP API, `GET /v1/me` |
| Lambda | 컨테이너 이미지, `x86_64`, 512MB, 제한시간 15초 |
| Secrets Manager | 빈 EOS Client Secret 컨테이너, 삭제 정책 `Retain` |
| IAM Role | CloudWatch Logs와 특정 Secret의 `GetSecretValue` |
| CloudWatch Logs | 보존 14일 |

SAM 템플릿이 빈 Secret을 생성한다. 배포 후 마스킹 입력 스크립트가 Secret 값을 설정한다. Secret 값은 파일, 셸 인수, Git에 남기지 않는다.

SAM CLI는 함수의 `PackageType: Image`와 Docker metadata를 사용해 컨테이너를 빌드한다. 개발 배포 시 `--resolve-image-repos`로 ECR 저장소를 해결한다.

## 12. 컨테이너 빌드

멀티스테이지 Dockerfile을 사용한다.

1. Amazon Linux 2023 빌드 단계에서 C++ 검증기를 컴파일한다.
2. Node.js 24 빌드 단계에서 TypeScript를 컴파일하고 운영 의존성을 정리한다.
3. AWS Lambda Node.js 24 런타임 이미지에 JavaScript, 검증 실행 파일, `libEOSSDK-Linux-Shipping.so`를 복사한다.

EOS SDK나 Client Secret을 Docker build argument로 전달하지 않는다. SDK는 로컬의 Git 제외 디렉터리에서 빌드 컨텍스트로 들어가며 Secret은 실행 시 Secrets Manager에서 가져온다.

## 13. 배포와 롤백

배포 순서:

```text
EOS SDK 로컬 준비
→ npm test
→ sam validate
→ sam build --use-container
→ sam local start-api
→ sam deploy --guided --resolve-image-repos --profile dku-dev --region ap-northeast-2
→ 마스킹 입력 스크립트로 Client Secret 등록
→ 실제 Connect ID Token으로 API 호출
→ CloudWatch 로그 확인
```

스택 업데이트 실패 시 CloudFormation 자동 롤백을 사용한다. 애플리케이션 문제는 이전 컨테이너 이미지가 빌드되는 Git 리비전을 다시 배포해 롤백한다. 개발 리소스 제거는 `sam delete`를 사용하되 Secrets Manager Secret은 유지한다.

## 14. 테스트 전략

### TypeScript 단위 테스트

- Authorization 헤더 누락과 Bearer 형식 오류
- 16KB 초과 토큰
- 잘못된 JWT 구조와 PUID 후보
- 네이티브 성공 결과의 인증 문맥 전달
- 네이티브 검증 실패, 제한시간, 잘못된 출력의 HTTP 매핑
- 로그와 응답의 토큰 비노출

### C++ 검증기 테스트

| 입력 | 기대 결과 |
|---|---|
| 빈 입력 | 종료 코드 1 |
| 잘못된 PUID | 종료 코드 1 |
| 조작된 JWT | 종료 코드 2 |
| 만료된 JWT | 종료 코드 2 |
| 정상 Connect ID Token | 종료 코드 0, `EOS_Success` |
| 토큰과 다른 PUID | 종료 코드 2 |
| Client ID/Product ID/Sandbox ID/Deployment ID 불일치 | 종료 코드 2 |

### 로컬 컨테이너·SAM 테스트

- C++ 실행 파일과 EOS 공유 라이브러리 로딩
- `sam validate`와 `sam build --use-container`
- `sam local start-api`의 정상·실패 요청
- 네이티브 결과 JSON 파싱과 제한시간

### 실제 AWS 개발 환경 테스트

- 정상 Connect ID Token: `200`
- Authorization 누락: `401 AUTH_MISSING`
- 한 글자를 조작한 토큰: `401 AUTH_INVALID`
- 만료 토큰: `401 AUTH_INVALID`
- 강제 검증 제한시간: `503 AUTH_VERIFIER_UNAVAILABLE`
- CloudWatch에 토큰과 Client Secret이 존재하지 않음

## 15. 완료 조건

- TypeScript 단위 테스트가 통과한다.
- C++ 검증기가 Amazon Linux 2023에서 빌드되고 실패 입력을 거부한다.
- Lambda 컨테이너 이미지가 빌드된다.
- `sam validate`가 통과한다.
- SAM 로컬 API가 정상·실패 응답을 반환한다.
- `dku-dev` 프로필로 서울 리전에 개발 스택이 배포된다.
- 실제 Connect ID Token으로 `GET /v1/me`가 `200`과 검증된 PUID를 반환한다.
- 조작된 토큰이 `401`을 반환한다.
- CloudWatch에 토큰과 Client Secret이 기록되지 않는다.
- 빌드, Secret 등록, 배포, 호출, 롤백 절차가 문서화된다.
- Sprint 체크리스트에 결과와 남은 위험이 반영된다.

## 16. 후속 확장

인증 계층 완료 후 `/v1/me`에 DynamoDB 프로필 조회를 연결한다. 그다음 닉네임과 매칭 API가 동일한 인증 미들웨어와 `request.auth.playerId`를 재사용한다.

측정 결과 요청당 프로세스 실행이 병목이면 `TokenVerifier` 경계를 유지한 채 Node.js 네이티브 애드온 또는 실행 환경당 지속 검증 프로세스로 교체한다.

## 17. 참고 자료

- [Epic Online Services: EOS_Connect_VerifyIdTokenCallbackInfo](https://dev.epicgames.com/docs/api-ref/structs/eos-connect-verify-id-token-callback-info)
- [AWS Lambda: Node.js 컨테이너 이미지](https://docs.aws.amazon.com/lambda/latest/dg/nodejs-image.html)
- [AWS SAM: 컨테이너 이미지 빌드](https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/serverless-sam-cli-using-build.html)
- [AWS CloudFormation: AWS::SecretsManager::Secret](https://docs.aws.amazon.com/AWSCloudFormation/latest/TemplateReference/aws-resource-secretsmanager-secret.html)
