---
eyebrow: 'Docs · Reference'
lede:    'The two endpoints, the URL format, and how they sit alongside uanetstandard-test-suite.'

see_also:
  - { href: '../servers/overview.md',                meta: '4 min' }
  - { href: './environment-variables.md',             meta: '3 min' }

prev: { label: 'Forking and adding a server',  href: '../customization/forking-and-adding-a-server.md' }
next: { label: 'Environment variables',       href: './environment-variables.md' }
---

# Ports and endpoints

## Endpoint URLs (from the host)

| Service                    | Endpoint URL                          | Transport |
| -------------------------- | ------------------------------------- | --------- |
| `open62541-nm`              | `opc.tcp://localhost:24840`            | TCP       |
| `open62541-all-security`    | `opc.tcp://localhost:24841`            | TCP       |

Neither endpoint uses a **resource path** — open62541 servers
serve at the bare URL. Compare with `uanetstandard-test-suite`
which uses `/UA/TestServer` on every endpoint.

## Endpoint URLs (container-to-container)

For another container on the same compose network:

| Service                    | Internal URL                                  |
| -------------------------- | --------------------------------------------- |
| `open62541-nm`              | `opc.tcp://open62541-nm:4840` (service name)  |
| `open62541-all-security`    | `opc.tcp://open62541-all-security:4840`        |

The container-internal port is **always 4840**. The host
mapping (`24840`, `24841`) is just for outside access.

## Ports

Mapped on the host:

| Port  | Protocol | Service                  |
| ----- | -------- | ------------------------ |
| 24840 | TCP      | `open62541-nm`            |
| 24841 | TCP      | `open62541-all-security`  |

## Container names

Set explicitly in `docker-compose.yml`:

| Container name              | Service                  |
| --------------------------- | ------------------------ |
| `opcua-extra-nm`             | `open62541-nm`            |
| `opcua-extra-all-security`   | `open62541-all-security`  |

Useful when you need to `docker exec` or `docker logs` against a
specific container.

## Image registry

| Image                                                            | Service                    |
| ---------------------------------------------------------------- | -------------------------- |
| `ghcr.io/php-opcua/extra-test-suite/open62541-nm`                | `open62541-nm`              |
| `ghcr.io/php-opcua/extra-test-suite/open62541-all-security`      | `open62541-all-security`    |

Tagged with both `:vX.Y.Z` (immutable) and `:latest` (the most
recent stable).

## Port-allocation rule

This suite reserves **24840-24849** for its own services. New
services pick from `24842` onward, avoiding the **4840-4849**
range that `uanetstandard-test-suite` owns, plus its `4851` (SKS)
and `14850` (PubSub).

Combined contract across both suites:

| Range / port | Owned by                                                   |
| ------------ | ---------------------------------------------------------- |
| 4840-4849    | `uanetstandard-test-suite` — 10 classic servers             |
| 4851         | `uanetstandard-test-suite` — Security Key Service           |
| 14850 (UDP)  | `uanetstandard-test-suite` — PubSub publisher (via relay)   |
| 24840-24849  | `extra-test-suite` — open62541-based extras                  |

A client that runs both suites simultaneously sees 12 OPC UA
servers, no port conflicts.

## Application URIs

| Service                    | ApplicationUri                              |
| -------------------------- | ------------------------------------------- |
| `open62541-nm`              | `urn:open62541.server.application` (open62541 default) |
| `open62541-all-security`    | `urn:open62541.server.application` (open62541 default) |

Both inherit open62541's hard-coded default. Different from
`uanetstandard-test-suite` (which uses
`urn:opcua:testserver:nodes`). If your client trusts by
ApplicationUri, you need separate entries for each suite.

> **Note:** the `OPCUA_APPLICATION_URI` env var on
> `open62541-all-security` only changes the **server cert's
> `subjectAltName URI:`** — it does **not** change the
> ApplicationUri the running server reports, because that value
> is baked into the open62541 build. See
> [Environment variables](./environment-variables.md#open62541-all-security).

## NodeId conventions

For `open62541-nm`, since the address space starts empty:

| Namespace | URI                                       | Use                                            |
| --------- | ----------------------------------------- | ---------------------------------------------- |
| `ns=0`    | `http://opcfoundation.org/UA/`            | Standard nodes (`Objects`, `Server`, …)         |
| `ns=1`    | `urn:open62541.server.application`         | Write your test nodes here                      |

When `AddNodes` creates new nodes, use `ns=1;s=<your-key>` as
the convention. The server doesn't enforce this — `ns=2..N` also
work — but `ns=1` is canonical.

For `open62541-all-security`, the address space is open62541's
default — almost empty, with the standard server nodes only.

## Health probe

The action uses `nc -z` to probe each port. For your own health
checks:

<!-- @code-block language="bash" label="probe" -->
```bash
nc -z localhost 24840 && echo "open62541-nm: OK"
nc -z localhost 24841 && echo "open62541-all-security: OK"
```
<!-- @endcode-block -->

OPC UA also has a `GetEndpoints` service that's lighter than
opening a full session — useful for HTTP-style probes:

<!-- @code-block language="bash" label="endpoint probe" -->
```bash
opcua-cli get-endpoints opc.tcp://localhost:24840
```
<!-- @endcode-block -->

Should return at least one EndpointDescription (the `None/None`
endpoint for `open62541-nm`, **11** for `all-security` — one per
`(policy, mode)` combination; see
[Policies and modes · Endpoint count](../security-and-auth/policies-and-modes.md#endpoint-count)).

## Where to read next

- [Environment variables](./environment-variables.md) — the
  config surface.
- [Troubleshooting](./troubleshooting.md) — when things don't
  match this table.
