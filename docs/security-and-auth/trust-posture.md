---
eyebrow: 'Docs · Security and authentication'
lede:    'The permissive trust posture of open62541-all-security — what it accepts, why, and the production warning.'

see_also:
  - { href: './policies-and-modes.md',                     meta: '4 min' }
  - { href: '../servers/open62541-all-security.md',         meta: '5 min' }

prev: { label: 'Policies and modes',  href: './policies-and-modes.md' }
next: { label: 'User accounts',       href: './user-accounts.md' }
---

# Trust posture

`open62541-all-security` (port 24841) is **deliberately
permissive**. Two specific behaviours:

1. **Accepts any client certificate.** No trust-list check.
2. **Allows plain-text passwords over `SecurityPolicy#None`.**

Both make sense for an integration test target and are wrong for
production. The differences from a normal open62541 server are
spelled out in `server.c`.

## What's permissive

### `AcceptAll` PKI

```text
UA_CertificateVerification_AcceptAll(&config->secureChannelPKI);
UA_CertificateVerification_AcceptAll(&config->sessionPKI);
```

The default open62541 PKI verifier loads a trust list from a
directory and rejects certs that aren't in it. `AcceptAll`
replaces it with a verifier that returns `Good` for **every**
certificate, regardless of signer, validity dates, or content.

This mirrors `uanetstandard-test-suite` port 4845
(`opcua-auto-accept`) behaviour.

### `allowNonePolicyPassword`

```text
config->allowNonePolicyPassword = true;
```

By default open62541 refuses to accept plain-text passwords on a
`SecurityPolicy#None` channel — the spec says the password must
be encrypted with the server's public key. Setting this to
`true` skips that check.

Combined with the no-trust posture, this lets a test client say:

```text
endpoint:        opc.tcp://localhost:24841
security policy: None
security mode:   None
identity:        UserName(admin, "admin123")
```

…with no certificate setup, no encryption, no key exchange.
Maximum ease for tests.

## What this enables

| Test scenario                                              | Without permissive | With permissive |
| ---------------------------------------------------------- | ------------------ | --------------- |
| Connect with a self-signed client cert                     | `Bad_CertificateUntrusted` | Accepted |
| Connect with an expired client cert                        | `Bad_CertificateTimeInvalid` | Accepted (!!) |
| Username/password over `None` (unencrypted)                | `Bad_SecurityPolicyRejected` or `Bad_SecurityChecksFailed` | Accepted |
| Quick CI test without distributing client cert to server   | Painful            | One line        |

The trade-off: **none** of these failure paths are testable on
this server. To test "expired client cert" rejection or
"untrusted client cert" rejection, point your tests at
`uanetstandard-test-suite`'s `opcua-certificate` (port 4842) —
which **does** reject these.

## Why mirror auto-accept rather than strict

`uanetstandard-test-suite` already has a strict cert-validating
server (`opcua-certificate`, port 4842). Replicating it in
open62541 would be redundant. The **gap** for open62541 testing
is the auto-accept path — exercising encryption + auth without
setup overhead.

## Server certificate

Even though the server doesn't validate **client** certs, it
still presents **its own** cert during the secure-channel
handshake. The server cert is auto-generated on first boot:

```text
Subject:            CN=open62541-all-security, O=php-opcua extra-test-suite
Key:                RSA 2048
Validity:           10 years from generation
subjectAltName URI: urn:open62541.server.application
DNS:                localhost
IP:                 127.0.0.1
```

Your client decides whether to trust **it**.

| Strategy on the client side                | Behaviour                                |
| ------------------------------------------ | ---------------------------------------- |
| TOFU (capture fingerprint, pin)             | Works. Cert is stable across restarts if `/certs` volume is mounted. |
| Auto-accept any server cert                 | Works.                                   |
| Strict CA chain validation                  | Fails — the cert is self-signed, not CA-signed |

For most tests, TOFU is the right strategy. See
`uanetstandard-test-suite`'s [Trust flow](https://github.com/php-opcua/uanetstandard-test-suite/blob/master/docs/security/trust-flow.md)
for the trust-flow patterns.

## Persistent cert

The Dockerfile declares `VOLUME ["/certs"]`, so the cert
directory is backed by an anonymous Docker volume out of the
box. That volume survives `docker compose down && up`, so the
cert is **stable across restarts** by default — `entrypoint.sh`
reuses the existing files when present, only regenerating when
`/certs/server.der` is missing.

To make the storage location explicit (e.g. for inspection from
the host or backup), bind-mount the directory:

<!-- @code-block language="text" label="compose override" -->
```text
services:
  open62541-all-security:
    volumes:
      - ./certs-extra:/certs
```
<!-- @endcode-block -->

Your test's fingerprint-pin only needs re-pinning after
`docker compose down -v` (which removes the anonymous volume) or
after you explicitly delete the volume — at which point the next
boot generates a fresh cert.

## What you can't test here

| You want to test                                          | Use                                                    |
| --------------------------------------------------------- | ------------------------------------------------------ |
| `Bad_CertificateUntrusted` on client cert                  | `uanetstandard-test-suite` 4842 (`opcua-certificate`)  |
| `Bad_CertificateTimeInvalid` on client cert                | Same                                                    |
| X.509 user-identity authentication                         | Same                                                    |
| ECC policies                                               | `uanetstandard-test-suite` 4848 / 4849                  |

This server **complements** the strict-validating servers of the
main suite. They cover the negative paths; this one covers the
"happy path with minimal setup".

## The production warning

`server.c` includes this comment:

```text
This is a permissive TEST server: it accepts every client certificate
(no trust-list check) and allows plain-text passwords over the
SecurityPolicy#None channel. Do NOT use as a template for production.
```

It's there because the `server.c` is **otherwise** a reasonable
open62541 server template. Someone reading it might copy the
wiring into a production server and inadvertently disable
security.

The CHANGELOG also flags this for v1.1.0.

## Where to read next

- [User accounts](./user-accounts.md) — the four hard-coded
  logins.
- [open62541-all-security](../servers/open62541-all-security.md) —
  the server itself.
