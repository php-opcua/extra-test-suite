---
eyebrow: 'Docs · Security and authentication'
lede:    'Which security policies and modes open62541-all-security advertises, what the build flag does, and the matrix of accepted combinations.'

see_also:
  - { href: './trust-posture.md',                          meta: '4 min' }
  - { href: '../servers/open62541-all-security.md',         meta: '5 min' }

prev: { label: 'NodeManagement · Services reference',  href: '../nodemanagement/services-reference.md' }
next: { label: 'Trust posture',                       href: './trust-posture.md' }
---

# Policies and modes

`open62541-all-security` (port 24841) advertises every RSA
security policy supported by the open62541 build at the
`UA_ENABLE_ENCRYPTION=OPENSSL` flag.

## Policies the build supports

| Policy                       | Status (OPC UA)  | open62541 v1.4.8 support |
| ---------------------------- | ---------------- | ------------------------ |
| `None`                       | Required          | Yes                       |
| `Basic128Rsa15`              | Deprecated        | Yes                       |
| `Basic256`                   | Deprecated        | Yes                       |
| `Basic256Sha256`             | Current           | Yes                       |
| `Aes128_Sha256_RsaOaep`      | Current           | Yes                       |
| `Aes256_Sha256_RsaPss`       | Current           | Yes                       |
| `ECC_nistP256` / nistP384    | Current (ECC)     | **Not in this build**    |
| `ECC_brainpool*`             | Current (ECC)     | **Not in this build**    |

For ECC tests, use `uanetstandard-test-suite`'s
`opcua-ecc-nist` (4848) or `opcua-ecc-brainpool` (4849).

With the build flags in this repo, the count is **11** valid
`(policy, mode)` combinations — see
[Endpoint count](#endpoint-count) below for the breakdown.

## Modes

| Mode             | What's protected                                              |
| ---------------- | ------------------------------------------------------------- |
| `None`           | Nothing — cleartext                                            |
| `Sign`           | Integrity + authenticity (HMAC signature)                     |
| `SignAndEncrypt` | Integrity + authenticity + confidentiality                    |

## Valid `(policy, mode)` combinations

|                          | None | Sign | SignAndEncrypt |
| ------------------------ | ---- | ---- | -------------- |
| `None`                   | ✅   | ❌   | ❌             |
| `Basic128Rsa15`           | ❌   | ✅   | ✅             |
| `Basic256`               | ❌   | ✅   | ✅             |
| `Basic256Sha256`         | ❌   | ✅   | ✅             |
| `Aes128_Sha256_RsaOaep`   | ❌   | ✅   | ✅             |
| `Aes256_Sha256_RsaPss`    | ❌   | ✅   | ✅             |

`None/None` is the only valid combination with policy `None`.

## Identity tokens

For every endpoint, the server advertises:

| Token type                  | Accepted? |
| --------------------------- | --------- |
| `Anonymous`                 | Yes        |
| `UserName` (username/pass)   | Yes (see [User accounts](./user-accounts.md)) |
| `X509Certificate`            | Not implemented |
| `IssuedToken`                | Not implemented |

This mirrors `uanetstandard-test-suite`'s `opcua-userpass` and
`opcua-all-security` for the username path.

> **UserName token's `securityPolicyUri`.** `server.c` passes a
> single `userTokenPolicyUri` to `UA_AccessControl_default` —
> the URI of the **last** entry in the server's policy array
> (typically the strongest one, e.g. `Aes256_Sha256_RsaPss`).
> Every endpoint therefore advertises a `UserName` token policy
> whose `securityPolicyUri` is **that same single URI**, not one
> per endpoint. A client sending a `UserNameIdentityToken` must
> encrypt the password with that policy — except on the
> `None / None` endpoint, where `allowNonePolicyPassword = true`
> lets the password go through unencrypted.

## Endpoint count

`GetEndpoints` on `opc.tcp://localhost:24841` returns one
`EndpointDescription` per valid `(policy, mode)` combination:

| Policy                       | Modes        | Endpoints |
| ---------------------------- | ------------ | --------- |
| `None`                       | `None`       | 1         |
| `Basic128Rsa15`              | `Sign`, `SignAndEncrypt` | 2 |
| `Basic256`                   | `Sign`, `SignAndEncrypt` | 2 |
| `Basic256Sha256`             | `Sign`, `SignAndEncrypt` | 2 |
| `Aes128_Sha256_RsaOaep`      | `Sign`, `SignAndEncrypt` | 2 |
| `Aes256_Sha256_RsaPss`       | `Sign`, `SignAndEncrypt` | 2 |

Total: **11 endpoints**. Same shape as `uanetstandard-test-suite`
port 4843.

## SecurityPolicy URIs

Each endpoint reports a URI of the form:

```text
http://opcfoundation.org/UA/SecurityPolicy#<PolicyName>
```

Examples:

- `http://opcfoundation.org/UA/SecurityPolicy#None`
- `http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256`
- `http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss`

Your client's library typically exposes constants for these.

## Plain-text passwords over `None`

A deliberate behaviour for this test server:

```text
config->allowNonePolicyPassword = true;
```

This allows clients to send **plain-text** username/password
over a `SecurityPolicy#None` channel. open62541 rejects this by
default — turning it on means a test client doesn't need to
encrypt the password with the server's RSA public key on the
unsecured channel.

In production this is bad — anyone sniffing the wire reads the
password. **Test-only**. See [Trust posture](./trust-posture.md).

## Recommended test combinations

A useful subset for client-side test matrices:

| Test                                                | (policy, mode)                          |
| --------------------------------------------------- | --------------------------------------- |
| Anonymous, no security                              | `None / None`                           |
| Username, no security                                | `None / None` + UserName                |
| Username, signed only                                | `Basic256Sha256 / Sign` + UserName      |
| Username, encrypted                                  | `Basic256Sha256 / SignAndEncrypt` + UserName |
| Anonymous, encrypted                                 | `Aes256_Sha256_RsaPss / SignAndEncrypt` + Anon |
| Legacy policy                                        | `Basic128Rsa15 / SignAndEncrypt`        |
| Strongest non-deprecated                              | `Aes256_Sha256_RsaPss / SignAndEncrypt` |

A test that iterates these against the server is a strong
"security layer works against open62541" battery.

## Where to read next

- [Trust posture](./trust-posture.md) — why this server accepts
  any client cert.
- [User accounts](./user-accounts.md) — the four logins.
