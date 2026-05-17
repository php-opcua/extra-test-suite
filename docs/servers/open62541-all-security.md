---
eyebrow: 'Docs · Servers'
lede:    'open62541 v1.4.8 with every RSA security policy, every mode, plus Anonymous and username/password authentication. The second-implementation mirror of uanetstandard-test-suite port 4843.'

see_also:
  - { href: '../security-and-auth/policies-and-modes.md',  meta: '4 min' }
  - { href: '../security-and-auth/trust-posture.md',       meta: '4 min' }
  - { href: '../security-and-auth/user-accounts.md',       meta: '3 min' }

prev: { label: 'open62541-nm',  href: './open62541-nm.md' }
next: { label: 'NodeManagement · Overview', href: '../nodemanagement/overview.md' }
---

# `open62541-all-security`

The companion to `open62541-nm` — open62541 with every RSA
security policy enabled and a permissive trust posture so client
libraries can exercise the encryption + auth paths without
fighting PKI setup.

## Endpoint

```text
opc.tcp://localhost:24841
container internal port: 4840
```

## What it covers

Mirror of `uanetstandard-test-suite` port 4843 (`opcua-all-security`):

| Axis                          | Values                                      |
| ----------------------------- | ------------------------------------------- |
| Security policies              | Every RSA policy the open62541 build supports |
| Security modes                 | `None`, `Sign`, `SignAndEncrypt`              |
| Identity tokens                | Anonymous + Username/Password               |

The exact policy list depends on the open62541 build flags;
`UA_ENABLE_ENCRYPTION=OPENSSL` enables the standard set. See
[Policies and modes](../security-and-auth/policies-and-modes.md).

## Why a mirror

Two reasons:

1. **Second-implementation validation.** A client that passes
   tests against `uanetstandard-test-suite` 4843 might still
   fail against a different stack. open62541's RSA path is
   different code; same wire, different implementation.
2. **When .NET isn't available.** Some CI environments are
   constrained — pulling the UA-.NETStandard image is heavier
   than pulling open62541. This service lets you exercise the
   security paths against a smaller container.

The credentials and policies match the main suite exactly, so
your tests don't need a separate code path for "which server".

## User accounts

Same four logins as `uanetstandard-test-suite`'s userpass and
all-security servers:

| Username    | Password       |
| ----------- | -------------- |
| `admin`     | `admin123`     |
| `operator`  | `operator123`  |
| `viewer`    | `viewer123`    |
| `test`      | `test`         |

Hard-coded in `server.c`. No `users.json` config file (unlike
the main suite — open62541 wires this directly in code).

See [User accounts](../security-and-auth/user-accounts.md).

## Permissive trust posture

This is the **deliberate** part — and the part that differs from
a real production setup.

| Behaviour                                              | Why                                  |
| ------------------------------------------------------ | ------------------------------------ |
| Accepts **any** client certificate (no trust check)     | Tests can use ephemeral / self-signed certs without server-side pre-staging |
| Allows plain-text passwords over `SecurityPolicy#None` | Lets the "None + UserName/Password" combination work without RSA-encrypting the password |

Implementation in `server.c`:

```text
UA_CertificateVerification_AcceptAll(&config->secureChannelPKI);
UA_CertificateVerification_AcceptAll(&config->sessionPKI);
config->allowNonePolicyPassword = true;
```

This mirrors `uanetstandard-test-suite` port 4845
(`opcua-auto-accept`) behaviour. **Not** a production posture —
documented in `server.c` and CHANGELOG as deliberately
test-only. Don't copy into production server templates.

See [Trust posture](../security-and-auth/trust-posture.md).

## Server certificate

A self-signed RSA-2048 certificate is generated on first boot by
`entrypoint.sh`:

```text
Subject:           CN=open62541-all-security, O=php-opcua extra-test-suite
Key:               RSA 2048-bit
Validity:          10 years
subjectAltName URI: urn:open62541.server.application
DNS:               localhost
IP:                127.0.0.1
```

Stored inside the container at:

```text
/certs/server.der
/certs/server.key.der
```

Both **DER** format (not PEM) — open62541's convention.

The Dockerfile declares `VOLUME ["/certs"]`, so the directory
is backed by an anonymous Docker volume by default. That volume
persists across `docker compose down && up`, so the cert is
**generated once** on first boot and **reused** on every
subsequent restart — `entrypoint.sh` only regenerates when
`/certs/server.der` is missing.

To override the storage location (or share the cert with the
host), mount your own volume on top:

<!-- @code-block language="text" label="compose override" -->
```text
services:
  open62541-all-security:
    volumes:
      - ./certs-extra:/certs
```
<!-- @endcode-block -->

Your tests need to **re-pin the fingerprint** only after
`docker compose down -v` (or after explicitly deleting the
anonymous volume) — `-v` is what clears `/certs` and forces a
fresh cert on the next boot.

## How `server.c` works

The binary (`all_security_server`) is a small wrapper around
open62541's defaults:

1. Load cert and key from the paths in argv.
2. `UA_ServerConfig_setDefaultWithSecurityPolicies(...)` —
   advertises every policy the build supports.
3. Replace PKI verification with `AcceptAll` (permissive).
4. Enable `allowNonePolicyPassword`.
5. `UA_AccessControl_default(...)` with the 4 hard-coded logins
   and `allowAnonymous = true`.
6. Run loop.

About 150 lines of C. Lives at
`open62541-all-security/server.c` in the repo.

## Logs

```text
docker compose logs -f open62541-all-security
```

Look for the startup line:

```text
[server] open62541 all-security: opc.tcp://0.0.0.0:4840 — N policies, anonymous + 4 username/password logins
```

`N` is the number of policy+mode combinations open62541
advertises. With the build flags in this repo
(`UA_ENABLE_ENCRYPTION=OPENSSL`, no policy explicitly disabled),
the count is **11** — see
[Policies and modes · Endpoint count](../security-and-auth/policies-and-modes.md#endpoint-count)
for the breakdown.

## CMake build flags

Same as `open62541-nm` plus:

```text
-DUA_ENABLE_ENCRYPTION=OPENSSL
```

This enables the RSA policies that open62541 v1.4.8 exposes via
its `UA_ServerConfig_setDefaultWithSecurityPolicies` convenience
wrapper when built against OpenSSL — currently
`Basic128Rsa15`, `Basic256`, `Basic256Sha256`,
`Aes128_Sha256_RsaOaep`, and `Aes256_Sha256_RsaPss`. The exact
list reflects what upstream open62541 ships behind that wrapper
at the pinned version, not a hand-picked subset in this repo.

ECC policies are **not** supported by open62541 at the time of
v1.4.8 in this configuration — for ECC tests, use
`uanetstandard-test-suite`'s `opcua-ecc-nist` (4848) or
`opcua-ecc-brainpool` (4849).

## When to use this vs the main suite's 4843

| Need                                                  | Pick                                      |
| ----------------------------------------------------- | ----------------------------------------- |
| Reference compliance                                  | `uanetstandard-test-suite` 4843            |
| Test against open62541's RSA implementation           | This server (24841)                       |
| Run security tests without pulling the .NET image     | This server                               |
| Run side-by-side both implementations                 | Both                                      |

Both servers accept the same credentials and the same policy
list — your test code should be the same modulo the endpoint
URL.

## Where to read next

- [Security and authentication · Policies and modes](../security-and-auth/policies-and-modes.md) —
  the exact policy list this server advertises.
- [Trust posture](../security-and-auth/trust-posture.md) — the
  permissive-trust details.
- [User accounts](../security-and-auth/user-accounts.md) — the
  4 hard-coded logins.
