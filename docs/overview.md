---
eyebrow: 'Docs · Getting started'
lede:    'The companion suite to uanetstandard-test-suite — open62541-backed servers that cover the corners the OPC Foundation .NET reference does not.'

see_also:
  - { href: './getting-started/installation.md',  meta: '3 min' }
  - { href: './getting-started/quick-start.md',   meta: '3 min' }
  - { href: './servers/overview.md',              meta: '4 min' }

next: { label: 'Installation', href: './getting-started/installation.md' }
---

# Overview

`extra-test-suite` ships OPC UA test servers for the **service
sets and scenarios the UA-.NETStandard reference implementation
does not cover**. It runs alongside
[`uanetstandard-test-suite`](https://github.com/php-opcua/uanetstandard-test-suite)
without overlap — different ports, different upstream stack,
explicitly non-conflicting.

## Why a separate suite

`uanetstandard-test-suite` is the OPC Foundation reference stack.
Accurate, thorough, the right default for 90% of client testing.
But every reference implementation has blind spots:

- **`NodeManagement` services** — `AddNodes`, `DeleteNodes`,
  `AddReferences`, `DeleteReferences`. Not implemented over the
  wire by UA-.NETStandard. A client library that needs to
  exercise dynamic node creation has nothing to test against
  unless a second server provides it.
- **Second-implementation validation** — testing the same scenario
  against a different stack (open62541 instead of UA-.NETStandard)
  flushes out wire-spec assumptions baked into either side.

Keeping these in their own versioned suite avoids bloating the
main one and lets each suite move at its own cadence.

## What's inside today

| Port  | Service                      | Upstream            | Purpose                                              |
| ----- | ---------------------------- | ------------------- | ---------------------------------------------------- |
| 24840 | `open62541-nm`                | open62541 v1.4.8    | NodeManagement service set, full access              |
| 24841 | `open62541-all-security`      | open62541 v1.4.8    | Every RSA policy + Anonymous + Username/Password     |
| 24842 | `open62541-historizing`       | open62541 v1.4.8    | HistoryRead + HistoryUpdate on a `Double` (v1.2.0+)  |

All three run from individual `Dockerfile`s under their service
folders, all published as separate images to
`ghcr.io/php-opcua/extra-test-suite/<service>`.

## Why these ports

Ports `24840`, `24841`, and `24842` sit **well clear** of the
`4840-4849` range `uanetstandard-test-suite` reserves. Future
services follow the same rule (no overlap with `4840-4849` or
between each other). Running both suites at once means **13 OPC UA
servers** on the same host, no port conflicts.

## What it doesn't cover

- **Address space variety.** The `open62541-nm` server doesn't
  ship a curated address space — it starts empty, and your tests
  create nodes during the test run. `uanetstandard-test-suite`
  is where the ~300-node test surface lives.
- **PubSub.** `uanetstandard-test-suite` ships the publisher and
  the Security Key Service. This suite stays out of the way.
- **Discovery.** Same — covered by the main suite.

This is a small, focused, **gap-filler** suite. Not a competitor
to the main one.

## Typical CI usage

A client library's CI matrix often has **both** suites running:

<!-- @code-block language="text" label=".github/workflows/test.yml (sketch)" -->
```text
jobs:
  integration:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: php-opcua/uanetstandard-test-suite@v1.2.0  # main: 10 servers on 4840-4849
      - uses: php-opcua/extra-test-suite@v1.2.0          # extras: 3 servers on 24840-24842
      - run: vendor/bin/pest --group=integration
```
<!-- @endcode-block -->

Tests target whichever port answers the scenario they're
exercising.

## Fork it

The repo's layout is deliberately small and replaceable:

1. Drop a new `<service>/Dockerfile` into the repo root.
2. Append a `services:` entry in `docker-compose.yml` using the
   `image:` + `build:` dual pattern.
3. If you want it in the GitHub Action, add a port-wait step in
   `action.yml`.
4. Bump the minor version and tag.

The publish workflow builds every service in `docker-compose.yml`
on tag push — no per-service workflow changes.

See [Customization · Forking and adding a server](./customization/forking-and-adding-a-server.md).

## Where to read next

- [Installation](./getting-started/installation.md) — bring it up.
- [Servers · Overview](./servers/overview.md) — the three services
  in detail.
- [NodeManagement · Overview](./nodemanagement/overview.md) —
  the main reason this suite exists.
