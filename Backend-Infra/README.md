# ChickenGame EOS 인증 백엔드 운영 가이드

이 디렉터리는 Unreal 클라이언트가 전달한 EOS Connect ID Token을 AWS에서 검증하고, EOS가 확인한 PUID를 반환하는 개발용 `GET /v1/me` API를 배포한다. Client Secret과 Connect ID Token은 Git, 셸 명령행, 파일, Docker 이미지, Lambda 환경 변수, 로그에 저장하지 않는다.

## 1. 사전 준비

필요한 도구는 Node.js 24, Docker Desktop, AWS CLI v2, AWS SAM CLI다. AWS 리전은 `ap-northeast-2`, 프로필은 `dku-dev`, 스택은 `chickengame-backend-dev`를 사용한다.

EOS SDK 1.19.2.1 Linux 파일을 다음처럼 로컬에 둔다. `vendor/eos-sdk/` 아래 SDK 원본은 `.gitignore`로 제외되며 커밋하지 않는다.

```text
Backend-Infra/vendor/eos-sdk/
├── Include/
│   ├── eos_sdk.h
│   ├── eos_connect.h
│   └── ...
└── lib/
    └── libEOSSDK-Linux-Shipping.so
```

AWS SSO 세션과 현재 역할을 확인한다.

```powershell
aws sso login --profile dku-dev
aws sts get-caller-identity --profile dku-dev
```

출력의 계정과 역할이 이 개발 환경용인지 확인한 후에만 배포한다.

## 2. 설치와 로컬 검증

저장소 루트에서 실행한다.

```powershell
Set-Location Backend-Infra/backend
npm ci
npm test
npm run typecheck
npm run build
Set-Location ../..
pwsh -NoProfile -File Backend-Infra/backend/test/container-smoke.ps1
Set-Location Backend-Infra
sam validate --template-file template.yaml --lint --profile dku-dev --region ap-northeast-2
sam build --template-file template.yaml --use-container
Set-Location ..
```

Windows 정책 때문에 SAM CLI를 설치할 수 없는 경우 공식 AWS SAM CLI 이미지로 같은 검증을 실행한다.

```powershell
$infra = (Resolve-Path Backend-Infra).Path
$awsDir = (Resolve-Path "$env:USERPROFILE\.aws").Path
docker run --rm --platform linux/amd64 -e SAM_CLI_TELEMETRY=0 --mount "type=bind,source=$infra,target=/workspace" --mount "type=bind,source=$awsDir,target=/root/.aws,readonly" -w /workspace public.ecr.aws/sam/build-nodejs24.x:1.168.0 sam validate --template-file template.yaml --lint --profile dku-dev --region ap-northeast-2
docker run --rm --platform linux/amd64 -e SAM_CLI_TELEMETRY=0 --mount "type=bind,source=$infra,target=/workspace" --mount "type=bind,source=//var/run/docker.sock,target=/var/run/docker.sock" -w /workspace public.ecr.aws/sam/build-nodejs24.x:1.168.0 sam build --template-file template.yaml --use-container
```

로컬 API의 공개 오류 계약은 `Backend-Infra`에서 확인한다.

```powershell
Set-Location Backend-Infra
sam local start-api --template-file .aws-sam/build/template.yaml --parameter-overrides "EosProductId=16646f3bec4d46d1a546820856e47486 EosSandboxId=f4a9863af26845ac90dbcaae53c6375f EosDeploymentId=f9dbca2f19ea4980b305355ff3bdbcea EosClientId=xyza7891MHpvCZ6hgPhGFHQwn6KZNTp9"
```

다른 터미널에서 실행한다.

```powershell
curl.exe -i http://127.0.0.1:3000/v1/me
```

응답은 `401`, `AUTH_MISSING`, 비어 있지 않은 `requestId`를 포함해야 한다. 확인 후 첫 터미널에서 `Ctrl+C`로 종료한다.

## 3. 개발 스택 배포

`samconfig.toml`에는 Product/Sandbox/Deployment/Client ID만 있다. Client Secret은 없다.

```powershell
Set-Location Backend-Infra
sam build --template-file template.yaml --use-container
sam deploy --config-env dev --resolve-image-repos --profile dku-dev --region ap-northeast-2
```

Windows에 SAM CLI를 설치할 수 없어 공식 컨테이너로 배포할 때는 AWS SSO 캐시 갱신을 위해 `.aws`를 읽기/쓰기로 마운트한다. 읽기 전용 마운트는 세션 토큰 갱신 시 실패한다. 컨테이너에서 `samconfig.toml`을 읽지 못하는 환경을 피하기 위해 비밀이 아닌 배포 값을 명시한다.

```powershell
$infra = (Resolve-Path .).Path
$awsDir = (Resolve-Path "$env:USERPROFILE\.aws").Path
$parameters = @(
  'EosProductId=16646f3bec4d46d1a546820856e47486'
  'EosSandboxId=f4a9863af26845ac90dbcaae53c6375f'
  'EosDeploymentId=f9dbca2f19ea4980b305355ff3bdbcea'
  'EosClientId=xyza7891MHpvCZ6hgPhGFHQwn6KZNTp9'
  'VerifierTimeoutMs=8000'
  'VerifierMaxOutputBytes=65536'
)
docker run --rm --platform linux/amd64 -e SAM_CLI_TELEMETRY=0 --mount "type=bind,source=$infra,target=/workspace" --mount "type=bind,source=$awsDir,target=/root/.aws" --mount "type=bind,source=//var/run/docker.sock,target=/var/run/docker.sock" -w /workspace public.ecr.aws/sam/build-nodejs24.x:1.168.0 sam deploy --template-file .aws-sam/build/template.yaml --stack-name chickengame-backend-dev --capabilities CAPABILITY_IAM --resolve-s3 --resolve-image-repos --profile dku-dev --region ap-northeast-2 --parameter-overrides $parameters --no-confirm-changeset --no-fail-on-empty-changeset
```

`$parameters`에는 공개 식별자와 제한값만 두며 Client Secret을 넣지 않는다. `dev-timeout` 검증은 동일 명령에서 `VerifierTimeoutMs=1`만 사용하고, 확인 직후 반드시 `8000`으로 다시 배포한다.

변경 세트를 확인하고 승인한다. 완료 후 출력 세 값을 PowerShell 변수로 읽는다.

```powershell
$stack = 'chickengame-backend-dev'
$apiUrl = aws cloudformation describe-stacks --stack-name $stack --profile dku-dev --region ap-northeast-2 --query "Stacks[0].Outputs[?OutputKey=='ApiUrl'].OutputValue | [0]" --output text
$secretArn = aws cloudformation describe-stacks --stack-name $stack --profile dku-dev --region ap-northeast-2 --query "Stacks[0].Outputs[?OutputKey=='ClientSecretArn'].OutputValue | [0]" --output text
$functionName = aws cloudformation describe-stacks --stack-name $stack --profile dku-dev --region ap-northeast-2 --query "Stacks[0].Outputs[?OutputKey=='FunctionName'].OutputValue | [0]" --output text
$apiUrl
$functionName
```

## 4. Client Secret 등록

명령행에는 Secret ARN만 넣는다. Client Secret은 에코되지 않는 프롬프트에 붙여 넣고 Enter를 누른다.

```powershell
Set-Location backend
npm run secret:set -- --secret-id $secretArn
```

스크립트가 성공 메시지를 출력하기 전까지 브라우저나 터미널에 Client Secret을 다시 입력하지 않는다.

## 5. 실제 토큰 스모크 테스트

EOS Connect에서 방금 발급한 ID Token을 준비하되 파일로 저장하지 않는다. 스크립트의 마스킹 프롬프트에만 입력한다.

```powershell
npm run smoke:auth -- --url $apiUrl
npm run smoke:auth -- --url $apiUrl
```

각 실행은 무토큰 `401 AUTH_MISSING`, 정상 토큰 `200`, 서명 한 글자를 바꾼 토큰 `401 AUTH_INVALID`를 확인한다. 성공 출력의 `playerId`, `requestId`, `validLatencyMs`를 체크리스트에 기록한다. 두 번째 실행의 지연시간을 warm 기준으로 사용한다.

만료 검증은 정상 확인에 사용한 같은 토큰이 만료될 때까지 기다린 뒤 실행한다. 토큰을 파일이나 셸 변수에 보관하지 말고 마스킹 프롬프트에 다시 입력한다.

```powershell
npm run smoke:auth -- --url $apiUrl --expect-invalid
```

`401 AUTH_INVALID`와 `requestId`가 출력되어야 한다.

## 6. 검증기 장애와 원복 확인

검증기 제한시간을 1ms로 낮춘 통제 배포를 한 번 실행한다.

```powershell
Set-Location ..
sam deploy --config-env dev-timeout --resolve-image-repos --profile dku-dev --region ap-northeast-2
Set-Location backend
npm run smoke:auth -- --url $apiUrl --expect-unavailable
```

`503 AUTH_VERIFIER_UNAVAILABLE`을 확인하면 즉시 정상 설정으로 재배포하고 새 토큰으로 정상 스모크를 다시 실행한다.

```powershell
Set-Location ..
sam deploy --config-env dev --resolve-image-repos --profile dku-dev --region ap-northeast-2
Set-Location backend
npm run smoke:auth -- --url $apiUrl
```

코드 배포 자체를 원복해야 한다면 마지막 정상 Git 커밋을 별도 깨끗한 작업 디렉터리에 체크아웃하고, 그 버전에서 `sam build --use-container`와 위 `dev` 배포 명령을 다시 실행한다. 작업 중인 디렉터리를 `git reset --hard`로 되돌리지 않는다.

## 7. CloudWatch 민감정보 검사

스모크 출력의 `requestId` 하나씩 검사한다. 토큰과 Client Secret은 모두 마스킹 프롬프트로만 받으며, 원문 및 SHA-256 지문이 해당 로그 메시지에 없는지 확인한다.

```powershell
npm run logs:check -- --request-id <request-id> --function-name $functionName
```

각 정상·변조·만료·장애 요청에서 `PASS`가 나와야 한다. AWS 콘솔에서 같은 요청의 `REPORT` 줄에 있는 Duration, Max Memory Used도 체크리스트에 기록한다. 로그 내용을 복사할 때 토큰이나 Secret이 없는지 다시 확인한다.

## 8. 재배포와 삭제

일반 재배포는 테스트와 빌드 후 같은 `dev` 명령을 사용한다.

```powershell
Set-Location ..
sam build --template-file template.yaml --use-container
sam deploy --config-env dev --resolve-image-repos --profile dku-dev --region ap-northeast-2
```

개발 스택을 제거하려면 삭제 전에 `$secretArn`을 별도로 기록한다.

```powershell
sam delete --stack-name chickengame-backend-dev --profile dku-dev --region ap-northeast-2
```

`EosClientSecret`에는 `DeletionPolicy: Retain`이 적용되어 스택 삭제 후에도 Secrets Manager에 남는다. 더 이상 필요 없음을 확인한 경우에만 30일 복구 기간을 두고 별도로 삭제한다.

```powershell
aws secretsmanager delete-secret --secret-id $secretArn --recovery-window-in-days 30 --profile dku-dev --region ap-northeast-2
```

`--force-delete-without-recovery`는 사용하지 않는다. 스택 삭제와 Secret 삭제는 비용과 복구 가능성에 영향을 주는 별도 작업이다.

## 9. 프로필·닉네임 확장 검증

개발 스택은 `PlayerProfiles`, `MatchmakingTickets`, `Matches` 테이블을 사용한다. `GET /v1/me`는 검증된 PUID로 프로필을 조회하고, `POST /v1/me/nickname`은 전역 고유 닉네임을 최초 등록한다. 쓰기 요청에는 UUID 형식의 `Idempotency-Key`가 필요하다.

배포 전에 `Backend-Infra/backend`에서 `npm ci`, `npm test`, `npm run typecheck`, `npm run build`를 실행한다. 저장소 루트에서 `pwsh -NoProfile -File Backend-Infra/backend/test/container-smoke.ps1`로 이미지에 네이티브 검증기와 EOS 공유 라이브러리가 들어 있는지 확인한다. 이어 `Backend-Infra`에서 `sam validate --template-file template.yaml --lint --profile dku-dev --region ap-northeast-2`와 `sam build --template-file template.yaml --use-container`를 실행한다. 로컬 SDK는 Git 제외 경로 `Backend-Infra/vendor/eos-sdk/`에 준비해야 한다.

개발 스택에 반영하기 전 CloudFormation 변경 세트를 확인한다. 기존 EOS Secret이 교체되거나 다른 리소스가 삭제되는 변경 세트라면 배포를 멈추고 원인을 확인한다. 승인된 변경이면 기존 `dev` 설정으로 배포하고 `UPDATE_COMPLETE`를 확인한다.

배포 후 **새 프로필을 가진 테스트 계정 A**와 아직 닉네임이 없는 다른 **계정 B**를 준비한다. 둘 다 같은 신규 닉네임으로 검사한다. Connect ID Token은 스크립트의 마스킹 입력에만 넣고, 파일·셸 변수·명령행 인수에 저장하지 않는다.

```powershell
Set-Location Backend-Infra/backend
npm run smoke:profile -- --url $apiUrl --nickname 닭싸움왕
npm run smoke:profile -- --url $apiUrl --nickname 닭싸움왕 --expect-taken
```

첫 실행은 프로필 조회, 닉네임 등록 `201`, 같은 멱등성 키로 재시도한 원 응답, 다시 조회한 영속 닉네임을 확인한다. 두 번째 실행은 다른 계정에서 `409 NICKNAME_TAKEN`을 확인한다. 실행마다 출력된 `requestId`를 사용해 `npm run logs:check -- --request-id <request-id> --function-name $functionName`으로 토큰·Secret 비노출을 확인한다. AWS 콘솔에서 프로필과 닉네임 점유 항목을 확인하되 실제 PUID와 닉네임은 공개 기록에 남기지 않는다. 무토큰 `GET /v1/me`가 `401 AUTH_MISSING`인지도 재확인한다.

## 10. 매칭 티켓 API 개발 상태

`POST /v1/matchmaking/tickets`, `GET/DELETE /v1/matchmaking/tickets/current`, 매치 상태 보고 API, 4인 배정·기한 복구 워커를 2026-11-02 개발 스택에 배포했다. 티켓 수명은 3분이며 취소는 본인의 `WAITING` 티켓에만 적용된다. 배정 뒤 준비·참가·시작 단계는 각각 최대 60초이고, 필요한 보고가 오면 즉시 다음 단계로 넘어간다. [Unreal 연동 계약](../Docs/EOS/unreal-matchmaking-ticket-api.md)에 요청·응답과 UI 흐름을 기록했다.

초기 배포에서 컨테이너 스모크, 이미지 내부 테스트 157개, SAM의 API·워커 이미지 빌드가 통과했다. 변경 세트는 추가 17개·수정 4개·삭제 0개였고, 기존 리소스 교체 없이 `UPDATE_COMPLETE`로 적용됐다. `Matches`의 `DeadlineIndex`는 `ACTIVE`, SQS 이벤트 연결과 1분 주기 복구 규칙은 활성 상태다. 무토큰 티켓 조회는 `401 AUTH_MISSING`, 배포된 워커의 빈 주기 호출은 `200`이었다. 이후 4계정 실환경 검사에서 DynamoDB 조건식의 예약어 `mode` 때문에 배정 워커가 실패해 별칭 `#mode`로 수정했다. 수정 후 테스트 160개·타입 검사·SAM 컨테이너 빌드가 통과했고, 개발 스택은 `UPDATE_COMPLETE`로 갱신됐다.

### 개발 스택 4계정 매칭 스모크

서로 다른 Epic 계정 4개를 `chicken_game` 조직에 가입시키고 앱 접근을 허용한 다음, 각 계정의 닉네임을 먼저 등록한다. Dev Auth Tool의 자격 증명 이름은 계정마다 다르게 지정한다. 각 계정으로 EOS Connect 로그인하여 **Connect ID Token**을 새로 발급받는다. Dev Auth Tool에 표시되는 자격 증명 문자열은 이 토큰이 아니다. 토큰을 명령행 인수, 환경 변수, 파일, 채팅에 넣지 않는다.

```powershell
Set-Location Backend-Infra/backend
$apiUrl = aws cloudformation describe-stacks --stack-name chickengame-backend-dev --profile dku-dev --region ap-northeast-2 --query "Stacks[0].Outputs[?OutputKey=='ApiUrl'].OutputValue | [0]" --output text
npm run smoke:matchmaking -- --url $apiUrl --scenario full
npm run smoke:matchmaking -- --url $apiUrl --scenario leave
npm run smoke:matchmaking -- --url $apiUrl --scenario phase-expiry
npm run smoke:matchmaking -- --url $apiUrl --scenario wait-expiry
```

각 실행은 토큰을 마스킹 입력으로 받는다. `full`, `leave`, `phase-expiry`는 A1~A4 순서로 4개, `wait-expiry`는 1개를 입력한다. 먼저 `GET /v1/me`로 서로 다른 PUID·등록된 닉네임을 확인하고 모든 계정에 활성 티켓이 없어야 시작한다. `full`은 A3 취소가 A1·A2를 유지하는지, 재신청 후 4명이 같은 매치에 배정되는지, 호스트 권한·세션 등록·4명 참가·시작·종료·가드 해제를 확인한다. `leave`는 멤버 이탈에 따른 전체 `FAILED`, `phase-expiry`는 `FORMING` 60초 기한과 `EXPIRED`, `wait-expiry`는 1인 대기 3분 만료를 확인한다. 실행 사이에는 이전 매치가 종료됐는지 확인한다.

이 도구의 `/session`, `/joined`, `/left`, `/started`, `/finished`는 **모의 클라이언트 보고**다. 실제 EOS 세션 생성·참가와 Unreal UI는 검증하지 않는다. 실패 시 상태·오류 코드·`requestId`만 출력하며 토큰과 응답 본문은 출력하지 않는다. 실패한 실행에서 남은 활성 매치는 해당 단계의 기한이 지나 복구된 후 재실행한다.

2026-11-02에 서로 다른 Epic 계정 4개로 `full`, `leave`, `phase-expiry`, `wait-expiry`를 개발 스택에서 실행해 모두 통과했다. 취소는 A3에만 적용됐고 A1·A2는 계속 대기했다. 네 계정은 같은 매치에 배정됐으며 호스트 외 시작 요청은 `403 NOT_MATCH_HOST`, 모의 세션·4명 참가·시작·종료 보고는 성공했다. 한 명 이탈 시 전체 `FAILED`, 세션 준비 60초 미보고 시 `EXPIRED`, 단독 대기 3분 후 활성 티켓 해제를 확인했다. 각 종료 뒤 네 계정의 활성 상태는 해제됐다. 수정 배포 후 워커의 CloudWatch `ERROR` 로그 건수는 0이었다. 실제 EOS 세션 생성·참가와 Unreal 화면 전환은 후속 연동 테스트로 남는다.
