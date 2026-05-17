---
eyebrow: 'Docs · Getting started'
lede:    'One docker compose command brings up both servers. Prerequisites, what to expect on first boot, image vs build modes.'

see_also:
  - { href: './quick-start.md',                     meta: '3 min' }
  - { href: '../servers/overview.md',               meta: '4 min' }
  - { href: '../reference/troubleshooting.md',      meta: '4 min' }

prev: { label: 'Overview',     href: '../overview.md' }
next: { label: 'Quick start',  href: './quick-start.md' }
---

# Installation

## Prerequisites

- **Docker** with Compose v2 (`docker compose ...`).
- **Free host ports:** 24840 and 24841 (TCP).
- ~250 MB of disk for both images.

No language toolchain on the host — everything runs in
containers.

## Two install modes

The repo's `docker-compose.yml` uses the `image:` + `build:`
dual pattern. Two modes:

| Mode                 | When                                                  | Command                       |
| -------------------- | ----------------------------------------------------- | ----------------------------- |
| **Pull from GHCR**    | Default. Fast (~5 s once images cached). For users.    | `docker compose pull && docker compose up -d` |
| **Build from source** | Fork / dev. Compiles open62541 (~3-5 min cold).        | `docker compose up -d --build` |

Without an explicit flag, `docker compose up -d` will **pull**
the `image:` from GHCR if it isn't already in your local Docker
cache, and only fall back to `build:` when neither the cache nor
the registry has a copy. Pass `--build` to force a rebuild from
the local `Dockerfile` regardless.

## Pull-from-GHCR (recommended)

<!-- @code-block language="bash" label="terminal — pull mode" -->
```bash
docker compose pull
docker compose up -d
```
<!-- @endcode-block -->

This grabs:

- `ghcr.io/php-opcua/extra-test-suite/open62541-nm:latest`
- `ghcr.io/php-opcua/extra-test-suite/open62541-all-security:latest`

…and starts both. First-time pull is ~120 MB total.

## Pin a specific version

For CI stability, pin a tag:

<!-- @code-block language="bash" label="terminal — pin" -->
```bash
TAG=v1.1.0 docker compose pull
TAG=v1.1.0 docker compose up -d
```
<!-- @endcode-block -->

Same `TAG` env var the GitHub Action uses internally. `latest` is
the default; any published tag works.

## Build from source

If you've forked the suite or are working on the Dockerfiles:

<!-- @code-block language="bash" label="terminal — build" -->
```bash
docker compose up -d --build
```
<!-- @endcode-block -->

Compiles open62541 v1.4.8 with the required CMake flags
(`UA_ENABLE_NODEMANAGEMENT=ON`, etc.) — 3-5 minutes cold, ~1
minute with the build cache warm.

## Verify

<!-- @code-block language="bash" label="terminal — verify" -->
```bash
docker compose ps
```
<!-- @endcode-block -->

Both `opcua-extra-nm` and `opcua-extra-all-security` should show
`running`. Probe the TCP ports:

<!-- @code-block language="bash" label="terminal — TCP probe" -->
```bash
nc -z localhost 24840 && echo "24840 OK"
nc -z localhost 24841 && echo "24841 OK"
```
<!-- @endcode-block -->

If a port doesn't answer, see
[Troubleshooting](../reference/troubleshooting.md).

## What lands on disk

The `open62541-all-security` service generates a self-signed
RSA-2048 certificate on first boot at `/certs/server.der` +
`/certs/server.key.der` **inside the container**. The Dockerfile
declares `VOLUME ["/certs"]`, so Docker attaches an **anonymous
volume** at that path the first time the container starts. That
anonymous volume **survives** a plain `docker compose down` and
gets reattached on the next `docker compose up`, which means
`entrypoint.sh` finds the existing cert and reuses it (see the
`if [ ! -f "$CERT_FILE" ]` guard there).

To pin the cert path explicitly (so you can inspect or share
it from the host), mount your own volume on top. Paths are
relative to the directory containing your `docker-compose.yml`
(typically the repo root, e.g. `extra-test-suite/`):

<!-- @code-block language="text" label="compose override (optional)" -->
```text
services:
  open62541-all-security:
    volumes:
      - ./certs-extra:/certs       # resolves to <repo>/certs-extra
```
<!-- @endcode-block -->

The cert is **only** regenerated when:

- The container starts and `/certs/server.der` doesn't exist
  (first boot, or after wiping the volume).
- You ran `docker compose down -v` — `-v` removes the anonymous
  volume, so the next `up` starts from an empty `/certs`.

A plain `docker compose down && docker compose up -d` keeps the
existing cert; only `down -v` (or explicitly deleting the
volume) regenerates it.

`open62541-nm` runs without security and doesn't generate any
certs.

## Stop

<!-- @code-block language="bash" label="terminal — stop" -->
```bash
docker compose down
```
<!-- @endcode-block -->

For a clean slate (also wipes the mounted cert volume if you
added one):

<!-- @code-block language="bash" label="terminal — wipe" -->
```bash
docker compose down -v
```
<!-- @endcode-block -->

## Resource footprint

Both servers combined:

| Resource | Idle      | Under load  |
| -------- | --------- | ----------- |
| RAM      | ~80 MB    | ~150 MB     |
| CPU      | < 1%      | 1-5%        |
| Disk     | ~120 MB   | —           |

Comfortable on any laptop or CI runner.

## restart: unless-stopped

The base compose file sets `restart: unless-stopped` — the
containers survive host reboots. The CI override
(`docker-compose.ci.yml`) sets `restart: "no"` because runners
are ephemeral.

You typically start the suite once on a dev machine and forget
about it.

## Where to read next

- [Quick start](./quick-start.md) — first connection.
- [Servers · Overview](../servers/overview.md) — what each
  server does.
