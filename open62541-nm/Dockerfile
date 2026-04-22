# php-opcua/extra-test-suite — v1.0.0
#
# open62541 test server built with NodeManagement enabled.
# Published to ghcr.io/php-opcua/extra-test-suite by .github/workflows/publish.yml
# on every tag push.

FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake build-essential python3 python3-pip ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG OPEN62541_REF=v1.4.8
WORKDIR /src
RUN git clone --depth 1 --branch ${OPEN62541_REF} https://github.com/open62541/open62541.git .
RUN git submodule update --init --recursive

RUN mkdir build && cd build && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DUA_ENABLE_NODEMANAGEMENT=ON \
        -DUA_ENABLE_SUBSCRIPTIONS=ON \
        -DUA_ENABLE_METHODCALLS=ON \
        -DUA_ENABLE_HISTORIZING=ON \
        -DUA_NAMESPACE_ZERO=FULL \
        -DUA_BUILD_EXAMPLES=ON \
        -DBUILD_SHARED_LIBS=OFF \
        .. \
    && make -j"$(nproc)"

FROM debian:bookworm-slim

LABEL org.opencontainers.image.source="https://github.com/php-opcua/extra-test-suite"
LABEL org.opencontainers.image.description="OPC UA test servers not covered by uanetstandard-test-suite (open62541 with NodeManagement)"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/bin/examples/ /usr/local/bin/examples/

EXPOSE 4840

# Fallback chain in case the preferred binary is renamed/removed in a future
# open62541 bump. ci_server is the most permissive (used by upstream CI).
CMD ["sh", "-c", "\
  for s in ci_server server_ctt access_control_server server_mainloop tutorial_server_firststeps server tutorial_server_variable; do \
    bin=/usr/local/bin/examples/$s; \
    if [ -x \"$bin\" ]; then echo \"[entrypoint] starting $s on :4840\"; exec \"$bin\"; fi; \
  done; \
  echo '[entrypoint] no recognized example server binary found. Contents:' >&2; \
  ls -la /usr/local/bin/examples/ >&2; \
  exit 1"]
