<h1 align="center"><strong>Extra OPC UA Test Suite</strong></h1>

<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="Extra OPC UA Test Suite" src="./assets/logo-light.svg" width="290">
  </picture>
</div>

<p align="center">
  <a href="https://github.com/php-opcua/extra-test-suite/releases"><img src="https://img.shields.io/github/v/release/php-opcua/extra-test-suite?label=version&color=6366F1" alt="Version"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="License: MIT"></a>
  <a href="https://github.com/php-opcua/extra-test-suite/pkgs/container/extra-test-suite%2Fopen62541-nm"><img src="https://img.shields.io/badge/docker-ghcr.io-blue?logo=docker&logoColor=white" alt="Docker"></a>
</p>

---

The **companion suite** to [`php-opcua/uanetstandard-test-suite`](https://github.com/php-opcua/uanetstandard-test-suite): pre-configured OPC UA servers for the **service sets and scenarios the UA-.NETStandard reference implementation does not cover**. Same shape (single `docker compose up`, GitHub Action, GHCR-published images), different ports, explicitly non-overlapping with the main suite.

Today it ships **one** server — [open62541](https://github.com/open62541/open62541) with the NodeManagement service set enabled — because UA-.NETStandard does not implement `AddNodes` / `DeleteNodes` / `AddReferences` / `DeleteReferences` over the wire. The repository is structured around `docker-compose.yml` so **new servers drop in as new services** (Prosys Simulation Server, Eclipse Milo, node-opcua, …) without changing the action contract.

> **Why a separate repo:** `uanetstandard-test-suite` is the OPC Foundation reference stack — accurate, thorough, and the right default for 90% of client testing. But every reference implementation has blind spots (NodeManagement is UA-.NETStandard's most visible one), and any CI that wants to cover those corners needs a second server. Keeping the "extras" in their own versioned suite avoids bloating the main one and lets each member of the family move at its own cadence.

## What's Inside

| Port | Service (compose name) | Upstream | What it tests |
|---|---|---|---|
| 24840 | `open62541-nm` | [open62541 v1.4.8](https://github.com/open62541/open62541) built with `UA_ENABLE_NODEMANAGEMENT=ON`, runtime `ci_server` | `AddNodes` / `DeleteNodes` / `AddReferences` / `DeleteReferences` over the wire, plus anonymous full-access AccessControl so the tests don't fight permissions |

Port `24840` was picked to sit **well clear** of the `4840-4849` range `uanetstandard-test-suite` reserves. Future services will follow the same rule (no overlap with 4840-4849 or between each other).

## Fork It

Every industrial environment is different and the extras you'll want in your CI are different too: Prosys-style address spaces, Siemens S7-over-OPC gateways, PubSub over MQTT brokers, vendor-specific ExtensionObject payloads. **Fork this repository** and add what you need.

The layout was designed to be extended:

1. Drop a new `<service-name>/Dockerfile` into the repo root
2. Append a `services:` entry in [`docker-compose.yml`](docker-compose.yml) using the `image: ghcr.io/<owner>/extra-test-suite/<service>:${TAG:-latest}` + `build: context: ./<service-name>` dual pattern
3. If you publish it as a GitHub Action, add a wait step in [`action.yml`](action.yml) exposing an output for the new endpoint
4. Bump the minor version and tag

The publish workflow builds every service in `docker-compose.yml` on tag push — no per-service changes required. If you build something generally useful, open a PR so the upstream suite can pick it up.

## Already Enough for You?

If the single open62541 server covers your NodeManagement testing needs (as it does for [`php-opcua/opcua-client`](https://github.com/php-opcua/opcua-client)), jump straight to the [Quick Start](#quick-start) or the [CI Integration](#use-in-cicd-github-actions) section.

## Quick Start

```bash
docker compose up -d
```

That's it. The `open62541-nm` container is running on `opc.tcp://localhost:24840` with anonymous full-access AccessControl and all four NodeManagement services enabled.

```bash
# Stop everything
docker compose down
```

`restart: unless-stopped` means the container survives host reboots, exactly like `uanetstandard-test-suite`. You start it once, forget it, and your integration tests assume it's there.

## Endpoints

```
opc.tcp://localhost:24840       # open62541 — NodeManagement
```

## Certificates

None at v1.0.0. All services ship without security to keep the surface minimal — if you need a certificate-auth variant for a specific server, add it as a separate service in the compose file (see [Fork It](#fork-it)).

## Use in CI/CD (GitHub Actions)

This repository is also a **reusable GitHub Action**. Add a single step to your workflow and every server in the compose file is pulled from GHCR and started:

```yaml
steps:
  - uses: actions/checkout@v4

  - uses: php-opcua/extra-test-suite@v1.0.0

  - run: vendor/bin/pest --group=integration  # or the equivalent in your stack
```

The composite action runs `docker compose pull && docker compose up -d` with a CI-specific override that disables `restart:` (the runner VM is ephemeral — any restart policy would be moot). Port `24840` is part of this suite's versioned contract: your tests hardcode the endpoint the same way they hardcode the UA-.NETStandard ports `4840-4849`.

Pin the version for reproducibility:

```yaml
- uses: php-opcua/extra-test-suite@v1.0.0
  with:
    image-tag: v1.0.0          # pull this exact image from GHCR
    startup-timeout: '30'       # seconds, per-service TCP wait
```

For a real-world example, see the `integration` job in [`php-opcua/opcua-client`](https://github.com/php-opcua/opcua-client/blob/master/.github/workflows/tests.yml).

## Local Development

```bash
# First time: build from source (no published image yet)
docker compose up -d --build

# After v1.0.0 is published to GHCR — faster, no local compilation
docker compose pull
docker compose up -d

# Endpoint: opc.tcp://localhost:24840
# Probe:    php-opcua/opcua-client/scripts/prosys-nodemanagement-smoke.php
```

Stop / restart / inspect:

```bash
docker compose down
docker compose restart open62541-nm
docker compose logs -f open62541-nm
```

## Technology

- **Server stack:** [open62541](https://github.com/open62541/open62541) v1.4.8, pinned via `ARG OPEN62541_REF` in the service Dockerfile, built with `UA_ENABLE_NODEMANAGEMENT=ON`, `UA_ENABLE_SUBSCRIPTIONS=ON`, `UA_ENABLE_METHODCALLS=ON`, `UA_ENABLE_HISTORIZING=ON`, `UA_NAMESPACE_ZERO=FULL`
- **Runtime binary:** `ci_server` (most permissive of the upstream examples; fallback chain in the entrypoint if upstream renames it)
- **Base image:** `debian:bookworm-slim` multi-stage (build + runtime)
- **Orchestration:** Docker Compose v2
- **Registry:** `ghcr.io/php-opcua/extra-test-suite/<service>`

## Versioning

Semver, applied to the **whole repo** — all published images share the same tag.

- **Major bump:** breaking change to the action's inputs/outputs or to the service port map
- **Minor bump:** a new service is added, or a server's default binary / configuration changes in a user-visible way
- **Patch bump:** upstream version bump, dependency refresh, internal cleanup

## Support

For bug reports, feature requests, or suggestions for new servers to add (Prosys, Milo, node-opcua, …), open an issue on [GitHub Issues](https://github.com/php-opcua/extra-test-suite/issues).

## AI Disclosure

This project was built in part with the assistance of **Claude** (Anthropic). The AI contributed to code generation, documentation writing, and architecture decisions. All outputs were reviewed and validated by the author.

## License

This project is licensed under the [MIT License](LICENSE).
