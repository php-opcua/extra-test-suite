---
eyebrow: 'Docs · CI integration'
lede:    'The reusable GitHub Composite Action. Inputs, version pinning, and how it pairs with uanetstandard-test-suite in a typical CI matrix.'

see_also:
  - { href: './docker-compose-and-other-ci.md',           meta: '4 min' }
  - { href: '../reference/ports-and-endpoints.md',         meta: '2 min' }

prev: { label: 'User accounts',  href: '../security-and-auth/user-accounts.md' }
next: { label: 'Docker Compose and other CI', href: './docker-compose-and-other-ci.md' }
---

# GitHub Action

The repo is a GitHub **Composite Action**. One step brings both
servers up on the runner.

## Minimal example

<!-- @code-block language="text" label=".github/workflows/test.yml" -->
```text
name: Integration tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: php-opcua/extra-test-suite@v1.1.0

      - run: vendor/bin/pest --group=integration
```
<!-- @endcode-block -->

When the `uses:` step finishes, both ports are open:

- `opc.tcp://localhost:24840` — `open62541-nm`
- `opc.tcp://localhost:24841` — `open62541-all-security`

## Inputs

| Input             | Default     | Effect                                                      |
| ----------------- | ----------- | ----------------------------------------------------------- |
| `image-tag`       | `latest`    | Image tag for every service in compose. `v1.1.0`, `latest`, … |
| `startup-timeout` | `30`        | Seconds to wait for each service's TCP port                  |

No outputs. The endpoint URLs are part of the suite's
**versioned contract** — `24840` and `24841` are stable. Your
test code hardcodes them.

## What it does internally

The action runs three things:

1. `docker compose pull` (with the CI override)
2. `docker compose up -d`
3. Polls `nc -z localhost <port>` for each service until success
   or `startup-timeout` seconds elapse

On failure, it dumps `docker compose logs` to the runner output
and exits non-zero.

## Version pinning

| Form                                | Recommendation                          |
| ----------------------------------- | --------------------------------------- |
| `@v1.1.0`                            | **Recommended.** Reproducible.          |
| `@master`                           | Tracks tip — may break                  |
| `@<sha>`                             | Maximum reproducibility (rare)          |

Pin both `uses:` and the inner `image-tag` if you want a
double-locked version:

<!-- @code-block language="text" label="double-pin" -->
```text
- uses: php-opcua/extra-test-suite@v1.1.0
  with:
    image-tag: v1.1.0
```
<!-- @endcode-block -->

The action's published `v1.1.0` is the same set of files as the
GHCR image's `v1.1.0`. Both pinning to the same tag is the
common pattern.

## Paired with `uanetstandard-test-suite`

The two suites are designed to be used **side by side**:

<!-- @code-block language="text" label="full CI matrix" -->
```text
jobs:
  integration:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      # Main suite: 10 classic servers on 4840-4849, SKS on 4851, PubSub on UDP 14850
      - uses: php-opcua/uanetstandard-test-suite@v1.2.0

      # Extras: 2 open62541 servers on 24840-24841
      - uses: php-opcua/extra-test-suite@v1.1.0

      - run: vendor/bin/pest --group=integration
```
<!-- @endcode-block -->

12 OPC UA servers on the runner, no port conflicts. Tests target
whichever endpoint they need.

## Selecting a subset

The action has no `services:` input — it always brings up
**both**. To start only one, skip the composite action entirely
and call `docker compose` directly against a clone of the repo:

<!-- @code-block language="text" label="single-service via direct compose" -->
```text
- run: |
    git clone --depth 1 --branch v1.1.0 \
      https://github.com/php-opcua/extra-test-suite.git /tmp/extras
    docker compose -f /tmp/extras/docker-compose.yml \
                   -f /tmp/extras/docker-compose.ci.yml \
                   up -d open62541-nm
```
<!-- @endcode-block -->

In practice, both services are cheap (~120 MB combined). The
overhead of pulling just one isn't worth the complexity.

## Real-world example

`php-opcua/opcua-client` uses this pattern in its integration
job — same `uses:` step, then `vendor/bin/pest` with a group
filter that picks up NodeManagement tests:

<!-- @code-block language="text" label="opcua-client.github/workflows/tests.yml" -->
```text
- uses: php-opcua/uanetstandard-test-suite@v1.2.0
- uses: php-opcua/extra-test-suite@v1.1.0
- run: vendor/bin/pest --group=integration
```
<!-- @endcode-block -->

NodeManagement tests target `:24840`; all other integration
tests target the main suite's ports.

## Resource cost on GitHub runners

Standard `ubuntu-latest` runners have 2-4 vCPU and 7-16 GB RAM.
Both this suite and `uanetstandard-test-suite` running together
use ~700 MB RAM — comfortable.

## Common failures

| Symptom                                | Cause / fix                                       |
| -------------------------------------- | ------------------------------------------------- |
| Step times out                         | `startup-timeout` too low. Increase to 60.        |
| Image pull fails                       | GHCR rate-limited or image-tag doesn't exist      |
| `nc` not found                         | Older runner image — should be on `ubuntu-22.04+` |
| Port already in use                    | Another job is sharing the runner (rare)          |

## Where to read next

- [Docker Compose and other CI](./docker-compose-and-other-ci.md) —
  for GitLab, Jenkins, etc.
- [Reference · Ports and endpoints](../reference/ports-and-endpoints.md) —
  the URL contract.
