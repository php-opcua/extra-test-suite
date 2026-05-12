#!/bin/sh
# extra-test-suite — open62541 all-security entrypoint.
#
# Generates a self-signed RSA-2048 server certificate (DER) on first boot
# with a subjectAltName URI matching the application URI advertised by the
# open62541 default config (urn:open62541.server.application). Subsequent
# boots reuse the existing cert as long as $CERT_DIR is mounted as a volume.

set -e

CERT_DIR=${CERT_DIR:-/certs}
CERT_FILE="$CERT_DIR/server.der"
KEY_FILE="$CERT_DIR/server.key.der"
APP_URI=${OPCUA_APPLICATION_URI:-urn:open62541.server.application}

mkdir -p "$CERT_DIR"

if [ ! -f "$CERT_FILE" ] || [ ! -f "$KEY_FILE" ]; then
    echo "[entrypoint] generating self-signed RSA-2048 certificate in $CERT_DIR (appUri=$APP_URI)"

    tmp=$(mktemp -d)

    openssl genrsa -out "$tmp/server.key" 2048

    openssl req -new -x509 -days 3650 \
        -key "$tmp/server.key" \
        -out "$tmp/server.crt" \
        -subj "/CN=open62541-all-security/O=php-opcua extra-test-suite" \
        -addext "subjectAltName=URI:$APP_URI,IP:127.0.0.1,DNS:localhost" \
        -addext "keyUsage=digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyCertSign" \
        -addext "extendedKeyUsage=serverAuth,clientAuth"

    openssl x509 -in "$tmp/server.crt" -out "$CERT_FILE" -outform DER
    openssl rsa  -in "$tmp/server.key" -out "$KEY_FILE" -outform DER

    rm -rf "$tmp"
fi

echo "[entrypoint] starting open62541 all-security server on :4840"
exec /usr/local/bin/all_security_server "$CERT_FILE" "$KEY_FILE"
