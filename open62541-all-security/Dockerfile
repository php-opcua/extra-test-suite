# php-opcua/extra-test-suite — open62541 all-security
#
# Mirrors uanetstandard-test-suite port 4843 (opcua-all-security): every
# security policy the build supports, every security mode, Anonymous and
# Username/Password authentication. Built on top of open62541 with the
# OpenSSL encryption backend.
#
# Published to ghcr.io/php-opcua/extra-test-suite by .github/workflows/publish.yml
# on every tag push.

FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake build-essential python3 python3-pip ca-certificates \
        libssl-dev openssl pkg-config \
    && rm -rf /var/lib/apt/lists/*

ARG OPEN62541_REF=v1.4.8
WORKDIR /src
RUN git clone --depth 1 --branch ${OPEN62541_REF} https://github.com/open62541/open62541.git .
RUN git submodule update --init --recursive

# Drop our server source alongside the upstream examples and append it to
# the examples CMakeLists so the open62541 build emits the binary.
COPY server.c examples/all_security_server.c
RUN printf '\nadd_example(all_security_server all_security_server.c)\n' \
        >> examples/CMakeLists.txt

RUN mkdir build && cd build && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DUA_ENABLE_ENCRYPTION=OPENSSL \
        -DUA_ENABLE_NODEMANAGEMENT=ON \
        -DUA_ENABLE_SUBSCRIPTIONS=ON \
        -DUA_ENABLE_METHODCALLS=ON \
        -DUA_ENABLE_HISTORIZING=ON \
        -DUA_NAMESPACE_ZERO=FULL \
        -DUA_BUILD_EXAMPLES=ON \
        -DBUILD_SHARED_LIBS=OFF \
        .. \
    && make -j"$(nproc)" all_security_server

FROM debian:bookworm-slim

LABEL org.opencontainers.image.source="https://github.com/php-opcua/extra-test-suite"
LABEL org.opencontainers.image.description="OPC UA test server: open62541 with all RSA security policies and anonymous + username/password authentication (mirrors uanetstandard-test-suite port 4843)"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 ca-certificates openssl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/bin/examples/all_security_server /usr/local/bin/

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

VOLUME ["/certs"]
EXPOSE 4840

CMD ["/usr/local/bin/entrypoint.sh"]
