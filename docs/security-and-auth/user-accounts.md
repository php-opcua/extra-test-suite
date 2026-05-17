---
eyebrow: 'Docs · Security and authentication'
lede:    'Four hard-coded user accounts on open62541-all-security — same as the main suite. The role-based access details and how the four logins differ.'

see_also:
  - { href: './policies-and-modes.md',                     meta: '4 min' }
  - { href: './trust-posture.md',                          meta: '4 min' }
  - { href: '../servers/open62541-all-security.md',         meta: '5 min' }

prev: { label: 'Trust posture',  href: './trust-posture.md' }
next: { label: 'GitHub Action',  href: '../ci-integration/github-action.md' }
---

# User accounts

`open62541-all-security` ships four hard-coded user accounts.
Identical credentials to `uanetstandard-test-suite`'s
`opcua-userpass` and `opcua-all-security` servers — so test code
can target either server with the same login config.

## The four accounts

| Username    | Password       | Notes                                            |
| ----------- | -------------- | ------------------------------------------------ |
| `admin`     | `admin123`     | Full access (same role-ID semantics as the main suite) |
| `operator`  | `operator123`  | Operator-level access                            |
| `viewer`    | `viewer123`    | Read-only access                                 |
| `test`      | `test`         | Convenience admin login for ad-hoc testing       |

Plaintext passwords are deliberate — this is a test target, not
a real authentication system.

## How they're wired

In `server.c`:

<!-- @code-block language="text" label="server.c (excerpt)" -->
```text
static const UA_UsernamePasswordLogin logins[] = {
    {UA_STRING_STATIC("admin"),    UA_STRING_STATIC("admin123")},
    {UA_STRING_STATIC("operator"), UA_STRING_STATIC("operator123")},
    {UA_STRING_STATIC("viewer"),   UA_STRING_STATIC("viewer123")},
    {UA_STRING_STATIC("test"),     UA_STRING_STATIC("test")},
};

UA_AccessControl_default(
    config,
    /* allowAnonymous = */ true,
    &policyUri,
    sizeof(logins) / sizeof(logins[0]),
    logins);
```
<!-- @endcode-block -->

Unlike `uanetstandard-test-suite` (which loads
`config/users.json`), open62541's `UA_AccessControl_default`
embeds the list directly. To change users:

1. Edit `server.c` in your fork.
2. `docker compose build`.
3. `docker compose up -d --force-recreate`.

There is no runtime config file.

## Access control behaviour

open62541's `UA_AccessControl_default` provides a permissive
default: **all authenticated users can read and write all
nodes**. It does not enforce role-based per-node ACLs the way
`uanetstandard-test-suite`'s userpass server does.

Implications:

| Test                                                | uanetstandard-test-suite | extra-test-suite     |
| --------------------------------------------------- | ------------------------ | -------------------- |
| Valid credentials → session created                  | ✓                        | ✓                    |
| Wrong password → `Bad_UserAccessDenied`              | ✓                        | ✓                    |
| `viewer` writes `OperatorLevel` → `Bad_UserAccessDenied` | ✓ (enforced)         | ❌ (write succeeds)   |
| `viewer` reads any variable                          | ✓                        | ✓                    |

For **role-based write rejection tests**, use
`uanetstandard-test-suite`'s `opcua-userpass` (port 4841). This
server is fine for "does username auth work at all" but not for
"is the role check enforced".

## Anonymous

`allowAnonymous = true` is set, so anonymous identity is also
valid. Use cases:

- Connect to test plain `None/None` connectivity without
  credentials.
- Run NodeManagement-style tests where auth isn't the focus.

Anonymous users have **the same permissions as authenticated**
under `UA_AccessControl_default` — no role differentiation.

## Test patterns

### Happy path

```text
connect(opc.tcp://localhost:24841, policy=Basic256Sha256,
        mode=SignAndEncrypt, identity=UserName("admin", "admin123"))
→ Good
```

Repeat for the other three users — all succeed.

### Wrong password

```text
connect(..., identity=UserName("admin", "wrong"))
→ Bad_UserAccessDenied
```

### Unknown user

```text
connect(..., identity=UserName("alice", "anything"))
→ Bad_UserAccessDenied
```

open62541's `UA_AccessControl_default` returns
`Bad_UserAccessDenied` for any credential mismatch.
`Bad_IdentityTokenRejected` is reserved for the "wrong token
type" case (e.g. sending Anonymous to a server that requires
UserName).

### Plain-text password over `None`

```text
connect(opc.tcp://localhost:24841, policy=None, mode=None,
        identity=UserName("admin", "admin123"))
→ Good  (because allowNonePolicyPassword = true)
```

On a stricter server this combination fails —
`Bad_SecurityPolicyRejected` or `Bad_SecurityChecksFailed`
(the rejection happens at the transport layer because the
password isn't encrypted with the server's public key). The
permissive `allowNonePolicyPassword = true` posture lets it
through here.

## Compatibility with the main suite

Your test code can target either:

```text
opc.tcp://localhost:24841                 # extra-test-suite (bare URL — no resource path)
opc.tcp://localhost:4841/UA/TestServer    # uanetstandard-test-suite userpass
opc.tcp://localhost:4843/UA/TestServer    # uanetstandard-test-suite all-security
```

Note that this suite's open62541 servers serve at the **bare**
URL (no `/UA/TestServer` path) — see
[Ports and endpoints](../reference/ports-and-endpoints.md#endpoint-urls-from-the-host).

…with the **same credentials**. The wire interactions are
identical for the username/password path. Differences:

| Aspect                  | Main suite (4841/4843)         | This suite (24841)              |
| ----------------------- | ------------------------------ | ------------------------------- |
| Role-based ACLs         | Enforced via `rolePermissions`  | Not enforced                    |
| User config source       | `config/users.json`             | Hard-coded in C                  |
| Cert validation         | Strict (4843) / auto (4845)     | Auto-accept                      |
| Plain-text on None       | Rejected by default             | Allowed                          |

The right server depends on what you're testing.

## Where to read next

- [Trust posture](./trust-posture.md) — the cert-validation
  context.
- [Policies and modes](./policies-and-modes.md) — what combinations
  the server advertises.
