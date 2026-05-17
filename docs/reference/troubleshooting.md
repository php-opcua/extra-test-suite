---
eyebrow: 'Docs · Reference'
lede:    'Common failures and what causes them — port collisions, cert mismatches, NodeManagement errors.'

see_also:
  - { href: './ports-and-endpoints.md',         meta: '2 min' }
  - { href: './environment-variables.md',        meta: '3 min' }
  - { href: '../getting-started/installation.md', meta: '3 min' }

prev: { label: 'Environment variables',  href: './environment-variables.md' }
next: { label: 'Top of docs',            href: '../index.md' }
---

# Troubleshooting

## Startup

### `docker compose up` fails

| Symptom                                            | Cause / fix                                          |
| -------------------------------------------------- | ---------------------------------------------------- |
| `Error response from daemon: Conflict.` … `container name "/opcua-extra-nm" is already in use by container "<id>"` | A previous `docker compose down` was incomplete. Run `docker rm -f opcua-extra-nm opcua-extra-all-security`. |
| `Error response from daemon: Ports are not available` | Another process on the host is using 24840 or 24841. `lsof -i :24840` to find it. |
| `pull access denied`                                | Image tag doesn't exist. Check `:latest` or `:v1.1.0` is published on GHCR. |
| `failed to solve: failed to compute cache key`       | `docker compose build` cache issue. Run with `--no-cache`. |

### `open62541-all-security` fails to start

```text
docker compose logs --tail=50 open62541-all-security
```

Look for:

| Log line                                                          | Cause                                       |
| ----------------------------------------------------------------- | ------------------------------------------- |
| `cannot open <path>`                                              | `entrypoint.sh` couldn't create/read cert dir. Check `/certs` is writable. |
| `setDefaultWithSecurityPolicies failed: <status>`                  | OpenSSL initialisation failed. Rebuild the image. |
| `AccessControl_default failed: <status>`                          | Internal — file a bug if you see this.       |
| `(no startup banner ever printed)`                                 | Cert generation succeeded but server hung. Check `docker compose logs` for OpenSSL errors. |

## Connection

### `Connection refused` on `localhost:24840`

| Cause                          | Check                                       |
| ------------------------------ | ------------------------------------------- |
| Container not running          | `docker compose ps`                          |
| Port not exposed                | `docker port opcua-extra-nm`                 |
| Connecting from a different host (e.g. Docker Desktop VM)  | Use the host's actual IP, not the VM's |

### `Bad_NodeIdUnknown` from `Read` after `AddNodes`

You added a node but the server assigned a **different** NodeId.

| Cause                            | Fix                                                |
| -------------------------------- | -------------------------------------------------- |
| `requestedNewNodeId` was ignored | Use the NodeId returned in `AddNodesResult.addedNodeId` instead of the requested one |

open62541 honours `requestedNewNodeId` in practice, but the spec
allows the server to choose differently. Your test should
**trust the returned NodeId**, not the requested one.

### `Bad_CertificateUntrusted` on `open62541-all-security`

The server's cert isn't in your client's trust store.

| Strategy                                  | What to do                                     |
| ----------------------------------------- | ---------------------------------------------- |
| TOFU (pin fingerprint)                    | First run, capture fingerprint; pin it in subsequent runs. Mount `/certs` volume so the fingerprint is stable. |
| Auto-accept on the client side             | Configure your client to accept any server cert. Test-only. |
| Pre-stage the cert in client trust store  | Copy `/certs/server.der` out of the container, add to client's trust dir. |

### `Bad_IdentityTokenRejected` or `Bad_UserAccessDenied` on connect

open62541's default access-control plugin uses **two distinct**
status codes:

- `Bad_IdentityTokenRejected` — the **token type** is wrong for
  the endpoint (e.g. sending Anonymous to a server that requires
  UserName).
- `Bad_UserAccessDenied` — the token type matches but the
  credentials are invalid (wrong username or password).

| Cause                                                       | Status                          | Fix                              |
| ----------------------------------------------------------- | ------------------------------- | -------------------------------- |
| Connecting to `open62541-nm` with username (it doesn't support it) | `Bad_IdentityTokenRejected` | Use `open62541-all-security` (24841) |
| Wrong username (case-sensitive)                              | `Bad_UserAccessDenied`           | The four logins are case-sensitive |
| Wrong password for a known user                              | `Bad_UserAccessDenied`           | Double-check the credential table in [User accounts](../security-and-auth/user-accounts.md) |
| Plain-text password on a stricter server                     | `Bad_IdentityTokenRejected` or `Bad_SecurityChecksFailed` | Not applicable here — this suite sets `allowNonePolicyPassword = true` |

## NodeManagement

### `Bad_ServiceUnsupported` from `AddNodes`

You're connected to the wrong server. UA-.NETStandard doesn't
implement NodeManagement over the wire — that's the whole reason
this suite exists.

```text
endpoint:        opc.tcp://localhost:24840   ← use this for NodeManagement
                 (not 4840-4849 from uanetstandard-test-suite)
```

### `Bad_ParentNodeIdInvalid`

The parent NodeId you specified doesn't exist on the server.

| Common cause                                | Check                                                 |
| ------------------------------------------- | ----------------------------------------------------- |
| Hard-coded `ns=1;s=…` of a node from the main suite | The main suite's nodes don't exist on `open62541-nm` — create them yourself first |
| Typo in the NodeId                           | Browse the address space first to confirm             |

### `Bad_BrowseNameDuplicated`

Two children of the same parent with the same BrowseName.

| Cause                                             | Fix                              |
| ------------------------------------------------- | -------------------------------- |
| Previous test left a node behind                   | Delete it first, or use a unique BrowseName per test |
| Re-running the test against a still-running server  | The server has no isolation between test runs — clean up after each run |

For test isolation, structure tests to use unique node names
(prefix with test name or UUID).

### `Bad_TypeMismatch` on `Write`

The Variable's `DataType` doesn't match what you wrote.

| Cause                                            | Fix                                              |
| ------------------------------------------------ | ------------------------------------------------ |
| `AddNodes` used `DataType=Int32`, then you wrote a `Double` | Match the DataType or recreate the node with `Double` |
| Implicit type coercion in your client            | Explicit type tags on writes                      |

## Persistence

### Nodes disappear after `docker compose restart`

This is expected. `open62541-nm` is **in-memory** — restart =
reset.

For tests that need persistent state, structure them so they:

1. Connect.
2. Create everything they need.
3. Run.
4. Don't depend on inheriting state from a previous run.

### Server certificate changes after restart

By default this **doesn't** happen — the Dockerfile declares
`VOLUME ["/certs"]`, so Docker keeps an anonymous volume mounted
there across `docker compose down && up` and `entrypoint.sh`
reuses the existing cert. The cert only changes after
`docker compose down -v` (which wipes the anonymous volume) or
if you manually delete the volume.

If you want the cert files on the host (for inspection or
backup), bind-mount the directory:

<!-- @code-block language="text" label="compose override" -->
```text
services:
  open62541-all-security:
    volumes:
      - ./certs-extra:/certs
```
<!-- @endcode-block -->

…and the cert lives under `./certs-extra/` on the host. Your
client's fingerprint pin stays valid across restarts in both
configurations.

## CI

### GitHub Action times out

`startup-timeout: '30'` is the default. Some runners are slower
(cold image pull, busy network). Bump it:

<!-- @code-block language="text" label="action override" -->
```text
- uses: php-opcua/extra-test-suite@v1.1.0
  with:
    startup-timeout: '60'
```
<!-- @endcode-block -->

### Image pull rate-limited

GHCR has a per-IP pull rate limit. On busy CI environments you
may hit it. Two mitigations:

1. **Pin a version** — `:v1.1.0` is cached more aggressively
   than `:latest`.
2. **Authenticated pull** — log into GHCR before the pull step
   (no rate limit for authenticated users).

## Comparing behaviour across suites

If a test passes against `uanetstandard-test-suite` but fails
against `extra-test-suite` (or vice versa), check:

| Difference                                                       | Likely cause                                       |
| ---------------------------------------------------------------- | -------------------------------------------------- |
| Strict cert validation                                            | Main suite (4842) is strict; this suite is permissive |
| Role-based ACLs                                                   | Main suite enforces; this suite does not           |
| Available address space                                           | Main suite has ~300 nodes; this suite has none      |
| ECC policies                                                      | Main suite has them; this suite does not            |
| `requestedNewNodeId` honoured                                     | Both honour it in practice; spec allows divergence  |

## When to file a bug

The suite is small. If the issue is:

- The suite doesn't start
- A documented feature doesn't behave as documented
- A new open62541 version breaks the build

…file a bug at
[github.com/php-opcua/extra-test-suite/issues](https://github.com/php-opcua/extra-test-suite/issues).

If the issue is:

- Your client library can't decode an `AddNodes` response
- A specific policy is failing

…that's likely an open62541 or client-side bug. File against the
relevant project.

## Where to read next

- [Top of docs](../index.md).
