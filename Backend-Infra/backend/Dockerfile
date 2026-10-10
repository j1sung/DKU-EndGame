FROM public.ecr.aws/amazonlinux/amazonlinux:2023 AS native-build

RUN dnf -y install gcc-c++ cmake make pkgconf-pkg-config json-c-devel \
    && dnf clean all
WORKDIR /workspace
COPY vendor/eos-sdk /opt/eos
COPY backend/native ./native
RUN cmake -S native -B /tmp/eos-build \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=ON \
      -DEOS_SDK_ROOT=/opt/eos \
    && cmake --build /tmp/eos-build --parallel 2 \
    && ctest --test-dir /tmp/eos-build --output-on-failure

FROM node:24-bookworm-slim AS node-build

WORKDIR /workspace
COPY backend/package.json backend/package-lock.json ./
RUN npm ci
COPY backend/ ./
RUN npm test \
    && npm run typecheck \
    && npm run build

FROM public.ecr.aws/lambda/nodejs:24 AS runtime

RUN dnf -y install json-c \
    && dnf clean all
COPY --from=node-build /workspace/dist ${LAMBDA_TASK_ROOT}/dist
COPY --from=native-build /tmp/eos-build/eos-token-verifier /opt/bin/eos-token-verifier
COPY --from=native-build /opt/eos/lib/libEOSSDK-Linux-Shipping.so /opt/lib/libEOSSDK-Linux-Shipping.so

ENV EOS_VERIFIER_PATH=/opt/bin/eos-token-verifier

CMD ["dist/handler.handler"]
