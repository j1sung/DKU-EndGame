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
