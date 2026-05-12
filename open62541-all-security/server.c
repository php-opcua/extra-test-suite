/*
 * extra-test-suite — open62541 "all-security" server.
 *
 * Mirrors the uanetstandard-test-suite port 4843 (opcua-all-security):
 * every security policy the build supports, every security mode, plus
 * Anonymous + Username/Password authentication.
 *
 * Users (same as uanetstandard-test-suite opcua-userpass / opcua-all-security):
 *   admin    / admin123
 *   operator / operator123
 *   viewer   / viewer123
 *   test     / test
 *
 * Usage: all_security_server <server-cert.der> <server-key.der>
 *
 * The server certificate must be RSA (>=2048 bit) in DER format and must
 * carry a subjectAltName URI matching the application URI advertised by
 * the server (urn:open62541.server.application by default).
 *
 * This is a permissive TEST server: it accepts every client certificate
 * (no trust-list check) and allows plain-text passwords over the
 * SecurityPolicy#None channel. Do NOT use as a template for production.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

static volatile UA_Boolean running = true;

static void stopHandler(int sig) {
    (void) sig;
    running = false;
}

static UA_ByteString loadFile(const char *path) {
    UA_ByteString out = UA_BYTESTRING_NULL;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "[server] cannot open %s\n", path);
        return out;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return out;
    }

    out.length = (size_t) size;
    out.data = (UA_Byte *) UA_malloc(out.length);
    if (out.data == NULL || fread(out.data, 1, out.length, fp) != out.length) {
        UA_ByteString_clear(&out);
    }

    fclose(fp);
    return out;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server-cert.der> <server-key.der>\n", argv[0]);
        return EXIT_FAILURE;
    }

    UA_ByteString cert = loadFile(argv[1]);
    UA_ByteString key  = loadFile(argv[2]);
    if (cert.length == 0 || key.length == 0) {
        UA_ByteString_clear(&cert);
        UA_ByteString_clear(&key);
        return EXIT_FAILURE;
    }

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);

    UA_StatusCode rv = UA_ServerConfig_setDefaultWithSecurityPolicies(
            config, 4840,
            &cert, &key,
            NULL, 0,   /* trust list */
            NULL, 0,   /* issuer list */
            NULL, 0);  /* revocation list */
    if (rv != UA_STATUSCODE_GOOD) {
        fprintf(stderr, "[server] setDefaultWithSecurityPolicies failed: %s\n",
                UA_StatusCode_name(rv));
        UA_Server_delete(server);
        UA_ByteString_clear(&cert);
        UA_ByteString_clear(&key);
        return EXIT_FAILURE;
    }

    /* Replace the default trust-list-based PKI with one that accepts any client
     * certificate. Needed so clients without server-side pre-trust (e.g. a
     * fresh self-signed cert generated on each test run) can complete the
     * OpenSecureChannel handshake. */
    UA_CertificateVerification_AcceptAll(&config->secureChannelPKI);
    UA_CertificateVerification_AcceptAll(&config->sessionPKI);

    /* Allow plain-text passwords over the SecurityPolicy#None channel.
     * open62541 rejects this by default — turning it on lets the
     * "None + None + UserName/Password" scenario succeed without requiring
     * the client to encrypt the password with the server's RSA pubkey. */
    config->allowNonePolicyPassword = true;

    static const UA_UsernamePasswordLogin logins[] = {
            {UA_STRING_STATIC("admin"),    UA_STRING_STATIC("admin123")},
            {UA_STRING_STATIC("operator"), UA_STRING_STATIC("operator123")},
            {UA_STRING_STATIC("viewer"),   UA_STRING_STATIC("viewer123")},
            {UA_STRING_STATIC("test"),     UA_STRING_STATIC("test")},
    };
    const size_t loginsSize = sizeof(logins) / sizeof(logins[0]);

    if (config->accessControl.clear != NULL) {
        config->accessControl.clear(&config->accessControl);
    }

    rv = UA_AccessControl_default(
            config,
            true,                                                                                       /* allowAnonymous */
            (const UA_ByteString *) &config->securityPolicies[config->securityPoliciesSize - 1].policyUri, /* userTokenPolicyUri */
            loginsSize, logins);
    if (rv != UA_STATUSCODE_GOOD) {
        fprintf(stderr, "[server] AccessControl_default failed: %s\n",
                UA_StatusCode_name(rv));
        UA_Server_delete(server);
        UA_ByteString_clear(&cert);
        UA_ByteString_clear(&key);
        return EXIT_FAILURE;
    }

    UA_ByteString_clear(&cert);
    UA_ByteString_clear(&key);

    fprintf(stdout,
            "[server] open62541 all-security: opc.tcp://0.0.0.0:4840 — %u policies, anonymous + 4 username/password logins\n",
            (unsigned) config->endpointsSize);

    UA_StatusCode runRv = UA_Server_runUntilInterrupt(server);

    UA_Server_delete(server);
    return runRv == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
