---
eyebrow: 'Docs · Servers'
lede:    'Two servers, two ports, two purposes — one fills the NodeManagement gap, the other exercises every RSA security policy. Both built on open62541.'

see_also:
  - { href: './open62541-nm.md',                    meta: '4 min' }
  - { href: './open62541-all-security.md',          meta: '5 min' }
  - { href: '../nodemanagement/overview.md',        meta: '4 min' }

prev: { label: 'Quick start',                   href: '../getting-started/quick-start.md' }
next: { label: 'open62541-nm',                  href: './open62541-nm.md' }
---

# Servers overview

This suite ships **two services**, both built on
[open62541](https://github.com/open62541/open62541) v1.4.8.

| Port  | Service                    | Purpose                                            |
| ----- | -------------------------- | -------------------------------------------------- |
| 24840 | `open62541-nm`              | NodeManagement (the gap in UA-.NETStandard)        |
| 24841 | `open62541-all-security`    | Every RSA policy, every mode, Anon + Username/Password |

## Why two

| You're testing…                                                  | Use                                  |
| ---------------------------------------------------------------- | ------------------------------------ |
| `AddNodes` / `DeleteNodes` / `AddReferences` / `DeleteReferences` | `open62541-nm` (24840)               |
| Endpoint negotiation against a second stack                       | `open62541-all-security` (24841)     |
| Username/password against open62541's PKI                         | `open62541-all-security` (24841)     |
| Behaviour delta between open62541 and UA-.NETStandard            | both, side-by-side                   |

Everything else — the test data, the methods, the dynamic
variables, the alarms — is covered by
[`uanetstandard-test-suite`](https://github.com/php-opcua/uanetstandard-test-suite).
This suite is the **extras**.

## Both servers share

| Property            | Value                                                   |
| ------------------- | ------------------------------------------------------- |
| Upstream stack      | open62541 v1.4.8                                        |
| Build mode          | Static link, multi-stage Debian slim                    |
| Inside the container | Listens on port 4840                                    |
| Outside the container | Mapped to 24840 / 24841 on the host                    |
| Restart policy (dev) | `unless-stopped`                                        |
| Restart policy (CI)  | `no`                                                    |
| Image registry       | `ghcr.io/php-opcua/extra-test-suite/<service>`           |

The internal port is always `4840` — the host-side mapping in
`docker-compose.yml` is what differentiates them.

## Address spaces

| Service                  | Address space contents                                |
| ------------------------ | ----------------------------------------------------- |
| `open62541-nm`            | open62541's `ci_server` example baseline (mostly empty; tests populate it) |
| `open62541-all-security`  | open62541's default (Server + Objects folders, no test data) |

Neither server ships the curated `~300-node` test address space
of `uanetstandard-test-suite`. The point of these servers is the
**service set** and the **security stack**, not test data.

If you need rich test data, point your client at
`uanetstandard-test-suite` for those operations and at this
suite only for NodeManagement / open62541-specific tests.

## CMake flags

Both servers are built with the same open62541 CMake config:

```text
-DUA_ENABLE_NODEMANAGEMENT=ON
-DUA_ENABLE_SUBSCRIPTIONS=ON
-DUA_ENABLE_METHODCALLS=ON
-DUA_ENABLE_HISTORIZING=ON
-DUA_NAMESPACE_ZERO=FULL
```

Plus, for `open62541-all-security`:

```text
-DUA_ENABLE_ENCRYPTION=OPENSSL
```

`UA_NAMESPACE_ZERO=FULL` includes the full standard namespace
(important for tests that depend on standard types and
references).

## What runs inside the container

| Service                  | Binary                       | How it starts                                  |
| ------------------------ | ----------------------------- | ---------------------------------------------- |
| `open62541-nm`            | `ci_server` (upstream example) | `sh -c` dispatch script — iterates a fallback chain and `exec`s the first binary that exists |
| `open62541-all-security`  | `all_security_server` (custom — `server.c`) | Via `entrypoint.sh` that generates a cert |

For `open62541-nm`, the entrypoint searches for a recognised
example binary in priority order (`ci_server` → `server_ctt` →
`access_control_server` → …). This way, the image keeps working
if upstream renames an example.

For `open62541-all-security`, the custom `server.c` is added to
open62541's `examples/CMakeLists.txt` during the build, so it
ships as another example binary.

## Image lifecycle

| Event             | What happens                                              |
| ----------------- | --------------------------------------------------------- |
| Tag push `v*`      | Publish workflow builds + pushes every service to GHCR    |
| Pull `:latest`     | Always the most recently-pushed `vX.Y.Z`                  |
| Pull `:vX.Y.Z`     | Immutable — exact version                                 |

The whole repo versions together. A new service in
`docker-compose.yml` triggers a minor bump; an open62541 version
bump is a patch.

## Resource budget

Each server is ~50-70 MB RAM at idle. Combined: ~120 MB. Under
load they typically stay under 200 MB total — cheap to leave
running.

## Where to read next

- [open62541-nm](./open62541-nm.md) — the NodeManagement server
  in detail.
- [open62541-all-security](./open62541-all-security.md) — the
  all-security server in detail.
