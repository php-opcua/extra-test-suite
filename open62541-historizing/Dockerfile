# php-opcua/extra-test-suite — open62541-historizing
#
# open62541 server with HistoryRead + HistoryUpdate enabled on a single
# Double node. Used by php-opcua/opcua-client to exercise the HistoryUpdate
# service against a real implementation (UA-.NETStandard does not support it
# on its demo nodes).
#
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

COPY server.c examples/historizing_server.c
RUN printf '\nadd_example(historizing_server historizing_server.c)\n' \
        >> examples/CMakeLists.txt

RUN mkdir build && cd build && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DUA_ENABLE_HISTORIZING=ON \
        -DUA_ENABLE_SUBSCRIPTIONS=ON \
        -DUA_ENABLE_METHODCALLS=ON \
        -DUA_NAMESPACE_ZERO=FULL \
        -DUA_BUILD_EXAMPLES=ON \
        -DBUILD_SHARED_LIBS=OFF \
        .. \
    && make -j"$(nproc)" historizing_server

FROM debian:bookworm-slim

LABEL org.opencontainers.image.source="https://github.com/php-opcua/extra-test-suite"
LABEL org.opencontainers.image.description="OPC UA test server: open62541 with HistoryRead + HistoryUpdate enabled on a Double node"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/bin/examples/historizing_server /usr/local/bin/

EXPOSE 4840

CMD ["/usr/local/bin/historizing_server"]
