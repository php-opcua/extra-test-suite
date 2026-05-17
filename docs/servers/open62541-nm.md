---
eyebrow: 'Docs · Servers'
lede:    'The NodeManagement-enabled open62541 server on port 24840. Empty address space by design; tests populate it via AddNodes.'

see_also:
  - { href: '../nodemanagement/overview.md',           meta: '4 min' }
  - { href: '../nodemanagement/services-reference.md', meta: '5 min' }

prev: { label: 'Overview',                href: './overview.md' }
next: { label: 'open62541-all-security',  href: './open62541-all-security.md' }
---

# `open62541-nm`

A vanilla open62541 v1.4.8 server with **NodeManagement enabled**
and **anonymous full access** AccessControl. Its only job is to
let your tests exercise `AddNodes`, `DeleteNodes`,
`AddReferences`, `DeleteReferences` over the wire.

## Endpoint

```text
opc.tcp://localhost:24840
security policy: None
security mode:   None
identity:        Anonymous
```

No discovery URL, no encryption, no auth.

## Why this exists

The OPC UA spec defines a **NodeManagement service set** —
`AddNodes`, `DeleteNodes`, `AddReferences`, `DeleteReferences`.
UA-.NETStandard, the reference stack used by
`uanetstandard-test-suite`, **does not implement these over the
wire**. A client library that exposes them needs a different
server to test against.

open62541 implements them in full. This service exists for that
one reason.

## What's in the address space

Almost nothing. The server boots open62541's `ci_server` example
which is **expected to** provide:

- Standard `Objects`, `Server`, `Types` folders (from
  `UA_NAMESPACE_ZERO=FULL`)
- Empty `ns=1` namespace (`urn:open62541.server.application`)
  ready for your tests to add to

(The address-space layout and access-control behaviour described
in this page reflect upstream's `examples/ci_server.c` at the
pinned tag. This repo does not carry that source — confirm
against the upstream example if a test depends on a specific
detail.)

**Your tests populate it.** A test typically:

1. Connects.
2. `AddNodes` creates the test fixtures it needs.
3. Exercises the feature being tested (browse, read, write,
   subscribe).
4. `DeleteNodes` cleans up (optional — the next test usually
   creates fresh nodes anyway).
5. Disconnects.

This is the **opposite** posture from `uanetstandard-test-suite`,
where the address space is rich and fixed.

## Access control

Anonymous identity has **full access** — no role check. Any
client can:

- `AddNodes` in any namespace
- `DeleteNodes` anywhere
- Read / write any variable
- Call any method

This is **by design** — the purpose is to let tests run without
fighting permissions. Not safe to expose to untrusted networks.

## What the entrypoint does

The Dockerfile's `CMD` walks a **fallback chain** of upstream
example binaries:

```text
ci_server → server_ctt → access_control_server → server_mainloop →
tutorial_server_firststeps → server → tutorial_server_variable
```

The first one that exists in the build is executed. This keeps
the image working if upstream open62541 renames or removes an
example. As of v1.4.8, `ci_server` is the binary picked up by
this fallback chain — chosen because it is, to the best of our
knowledge, the most permissive of the upstream `examples/*`
binaries. The exact behaviour (anonymous full access, empty
address space, no username/password) reflects what upstream
ships in `examples/ci_server.c` at that tag; verify against the
upstream source if you depend on a specific detail.

## CMake build flags

```text
-DUA_ENABLE_NODEMANAGEMENT=ON
-DUA_ENABLE_SUBSCRIPTIONS=ON
-DUA_ENABLE_METHODCALLS=ON
-DUA_ENABLE_HISTORIZING=ON
-DUA_NAMESPACE_ZERO=FULL
-DUA_BUILD_EXAMPLES=ON
-DBUILD_SHARED_LIBS=OFF
```

`UA_NAMESPACE_ZERO=FULL` is critical — without it the `Organizes`
reference type and many standard types are missing from `ns=0`,
which breaks most `AddNodes` calls that reference them.

## Logs

```text
docker compose logs -f open62541-nm
```

open62541's logger is verbose by default — every service call is
logged. Useful for debugging client-side serialisation issues.

## Limitations

| Limitation                                       | Workaround                                              |
| ------------------------------------------------ | ------------------------------------------------------- |
| No security (None/None only)                     | Use `open62541-all-security` (24841) for security tests |
| No username/password                              | Same — see 24841                                        |
| Empty address space                               | Add what you need via `AddNodes`                        |
| No persistence — nodes vanish on restart          | Test pattern: create fresh nodes per test run            |

## Typical test pattern

```text
1. setup:   client.connect()
2. arrange: client.addNodes([...])
3. act:     client.browse(...) / read(...) / write(...) / call(...)
4. assert:  verify results
5. cleanup: client.deleteNodes([...])  # optional
6. teardown: client.disconnect()
```

For the full set of NodeManagement operations and their
signatures see [NodeManagement · Services reference](../nodemanagement/services-reference.md).

## When to NOT use this server

- You need rich test data — use `uanetstandard-test-suite`.
- You need secured connections — use the sibling `open62541-all-security`.
- You need user identity / role tests — same.

This server is **the** target for NodeManagement testing and
not much else.

## Where to read next

- [open62541-all-security](./open62541-all-security.md) — the
  security-focused sibling.
- [NodeManagement · Overview](../nodemanagement/overview.md) —
  what the service set does and why it's the main reason for
  this suite.
