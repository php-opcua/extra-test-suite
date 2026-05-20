---
eyebrow: 'Docs · Servers'
lede:    'The HistoryRead + HistoryUpdate-enabled open62541 server on port 24842. One historizing Double node, an in-memory backend, every history operation a client can issue against it.'

see_also:
  - { href: './overview.md',                   meta: '4 min' }
  - { href: '../reference/ports-and-endpoints.md', meta: '5 min' }

prev: { label: 'open62541-all-security',  href: './open62541-all-security.md' }
next: { label: 'Authentication',          href: '../security-and-auth/authentication.md' }
---

# `open62541-historizing`

A vanilla open62541 v1.4.8 server with **HistoryRead and HistoryUpdate
enabled** on a single `Double` variable. Its only job is to let a
client library exercise the OPC UA HistoryUpdate service set against a
conformant implementation — UA-.NETStandard does not implement
HistoryUpdate on its demo nodes, so a different server is needed to
test the call path end-to-end.

## Endpoint

<!-- @code-block language="text" label="endpoint" -->
```text
opc.tcp://localhost:24842
security policy: None
security mode:   None
identity:        Anonymous
```
<!-- @endcode-block -->

No discovery URL, no encryption, no auth.

## What's in the address space

Exactly one application-namespace node:

| NodeId                          | Type     | AccessLevel                                     | Historizing |
| ------------------------------- | -------- | ----------------------------------------------- | ----------- |
| `ns=2;s=Historizing.Counter`    | `Double` | `READ`+`WRITE`+`HISTORYREAD`+`HISTORYWRITE`    | `true`      |

The node sits under the standard `Objects` folder (`ns=0;i=85`),
organised via `Organizes` (`ns=0;i=35`), with `BaseDataVariableType`
as its type definition. Initial value is `0.0`.

`ns=2` resolves to the `urn:php-opcua:historizing` URI declared by
`UA_Server_addNamespace` at boot. Resolve the namespace index at
runtime — open62541 may shift it if other namespaces appear before
yours.

Plus the standard server nodes from `UA_NAMESPACE_ZERO=FULL`
(`Server`, `Objects`, `Types`, …).

## The history backend

| Property                          | Value                                          |
| --------------------------------- | ---------------------------------------------- |
| Backend                           | `UA_HistoryDataBackend_Memory` (in-memory)     |
| Initial capacity                  | `3` slots                                      |
| Maximum capacity                  | `1024` samples                                 |
| `maxHistoryDataResponseSize`      | `1024`                                         |
| `historizingUpdateStrategy`       | `UA_HISTORIZINGUPDATESTRATEGY_USER`            |

`USER` update strategy means the server does **not** sample the
node's value on writes — the history is populated **exclusively**
through `HistoryUpdate` calls from the client. A regular `write()` to
the node updates the live value but does **not** add a history point.

Restart the container to wipe the backend — there is no persistence.

## Supported history operations

| HistoryUpdate / HistoryRead detail | Behaviour                                                       |
| ---------------------------------- | --------------------------------------------------------------- |
| `HistoryReadRaw`                   | Returns stored samples in `[startTime, endTime]`                |
| `HistoryInsertData`                | Adds new samples — fails with `Bad_EntryExists` if the source timestamp collides |
| `HistoryReplaceData`               | Overwrites the sample at a matching timestamp                   |
| `HistoryUpdateData`                | Insert-or-update — does whichever applies per timestamp         |
| `HistoryDeleteRawModified`         | Deletes samples in a time range                                 |
| `HistoryDeleteAtTime`              | Decoded (no protocol error), but the Memory backend does not implement the actual delete; expect `Bad_HistoryOperationUnsupported` per operation |
| `HistoryUpdateEvent`               | Decoded (no protocol error), Memory backend has no event path; expect `Bad_HistoryOperationUnsupported` |
| `HistoryReadProcessed`             | Not implemented by the Memory backend — for aggregates use the client-side `AggregateModule` against the raw read |

The split between "accepted at the wire level" and "actually executes
the operation" matters when writing integration tests: the framing
checks succeed on every operation, but only the five rows in the top
block produce real side-effects on the in-memory buffer.

## Why this exists

The OPC UA spec defines **HistoryRead** and **HistoryUpdate** as
optional service sets. UA-.NETStandard (the reference stack used by
`uanetstandard-test-suite`) implements HistoryRead on its historian
nodes but **does not expose** HistoryUpdate to clients. A client
library that ships `historyInsert` / `historyReplace` /
`historyDeleteRawModified` needs a server that round-trips those calls
end-to-end.

open62541 implements both service sets, and its
`UA_HistoryDataBackend_Memory` plus `UA_HISTORIZINGUPDATESTRATEGY_USER`
combination gives us a clean fixture: the server adds nothing to
history on its own, so every sample observable by `HistoryReadRaw`
came from a client `HistoryUpdate` call we can assert against.

## What the entrypoint does

Nothing — the `Dockerfile` `CMD` is `/usr/local/bin/historizing_server`
directly. No shell wrapper, no cert generation, no fallback chain
(unlike `open62541-nm`). The binary starts, opens port 4840 inside
the container, and runs until `SIGINT` / `SIGTERM`.

The container logs two lines on startup:

<!-- @code-block language="text" label="startup logs" -->
```text
[historizing] open62541 historizing server starting on :4840
[historizing] exposed node: ns=<urn:php-opcua:historizing>;s=Historizing.Counter (Double, history enabled)
```
<!-- @endcode-block -->

## CMake build flags

<!-- @code-block language="text" label="cmake" -->
```text
-DUA_ENABLE_HISTORIZING=ON
-DUA_ENABLE_SUBSCRIPTIONS=ON
-DUA_ENABLE_METHODCALLS=ON
-DUA_NAMESPACE_ZERO=FULL
-DUA_BUILD_EXAMPLES=ON
-DBUILD_SHARED_LIBS=OFF
```
<!-- @endcode-block -->

Notably **not** built with `UA_ENABLE_NODEMANAGEMENT=ON` —
`AddNodes` / `DeleteNodes` calls against this server respond with
`Bad_ServiceUnsupported`. Use [`open62541-nm`](./open62541-nm.md) for
those.

## Logs

<!-- @code-block language="bash" label="logs" -->
```bash
docker compose logs -f open62541-historizing
```
<!-- @endcode-block -->

open62541's logger is verbose — every HistoryUpdate call logs the
operation, the timestamps it touched, and the per-sample status.
Useful for confirming that the backend is actually doing what the
client thinks it asked for.

## Limitations

| Limitation                                              | Workaround                                                             |
| ------------------------------------------------------- | ---------------------------------------------------------------------- |
| Single hard-coded node                                  | Add an `AddNodes`-capable server (`open62541-nm`) and create your own |
| In-memory backend, no persistence                        | Persistence is out of scope for a test fixture — restart wipes        |
| `HistoryDeleteAtTime` and event-history not implemented  | Test the framing only; assert `Bad_HistoryOperationUnsupported`        |
| `HistoryReadProcessed` not implemented                   | Use `historyReadRaw` + the client-side `AggregateModule`               |
| No security, no auth                                     | This is a fixture — point it at localhost only                         |
| Update strategy is `USER` only                           | Writes to the live value do not auto-historicise                       |

## Typical test pattern

<!-- @code-block language="text" label="test pattern" -->
```text
1. setup:     client.connect()
2. arrange:   client.historyUpdate(
                  nodeId: 'ns=2;s=Historizing.Counter',
                  operation: HistoryUpdateType::Insert,
                  samples:   [...timestamped DataValues...],
              )
3. act:       client.historyReadRaw(
                  nodeId: 'ns=2;s=Historizing.Counter',
                  startTime: ..., endTime: ...,
              )
4. assert:    inserted samples appear in the read response
5. cleanup:   restart the container, or HistoryDelete the range
6. teardown:  client.disconnect()
```
<!-- @endcode-block -->

## When NOT to use this server

- You need NodeManagement (`AddNodes`, `DeleteNodes`) — use
  [`open62541-nm`](./open62541-nm.md) (port 24840).
- You need secured connections or non-anonymous identity — use
  [`open62541-all-security`](./open62541-all-security.md) (port 24841).
- You need rich test data — use
  [`uanetstandard-test-suite`](https://github.com/php-opcua/uanetstandard-test-suite).

This server is **the** target for HistoryUpdate testing and not
much else.

## Where to read next

- [open62541-nm](./open62541-nm.md) — the NodeManagement sibling.
- [open62541-all-security](./open62541-all-security.md) — the
  security-focused sibling.
- [Ports and endpoints](../reference/ports-and-endpoints.md) — the
  whole port allocation in context.
