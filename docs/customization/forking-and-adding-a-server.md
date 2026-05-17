---
eyebrow: 'Docs · Customization'
lede:    'Drop a new server into the suite — a Dockerfile, a compose entry, an action.yml wait step. The repo was designed to be extended this way.'

see_also:
  - { href: '../servers/overview.md',                  meta: '4 min' }
  - { href: '../ci-integration/github-action.md',       meta: '4 min' }

prev: { label: 'Docker Compose and other CI',  href: '../ci-integration/docker-compose-and-other-ci.md' }
next: { label: 'Ports and endpoints',          href: '../reference/ports-and-endpoints.md' }
---

# Forking and adding a server

The repo is tiny by design. Adding a server is **four small
edits**.

## What the repo looks like

```text
extra-test-suite/
├── README.md
├── CHANGELOG.md
├── action.yml
├── docker-compose.yml
├── docker-compose.ci.yml
├── open62541-nm/
│   └── Dockerfile
├── open62541-all-security/
│   ├── Dockerfile
│   ├── entrypoint.sh
│   └── server.c
└── .github/workflows/
    └── publish.yml
```

Each server is one directory containing a `Dockerfile` and
whatever extra files it needs.

## Step 1 — Create the service directory

From the root of your forked `extra-test-suite/` checkout:

```text
mkdir my-new-server
cd my-new-server
```

Inside that directory, write a `Dockerfile`. The two existing
ones are templates:

| Source you want to base on              | Look at                           |
| --------------------------------------- | --------------------------------- |
| open62541 with custom build flags        | `open62541-nm/Dockerfile`         |
| open62541 with custom server.c           | `open62541-all-security/Dockerfile` |
| A different OPC UA stack (Milo, Prosys, …) | Build from a base image yourself |

Both existing Dockerfiles use:

- **Multi-stage build** — build stage uses `debian:bookworm-slim`
  with build tools; runtime stage strips everything except the
  binary.
- **`ARG OPEN62541_REF=v1.4.8`** — pin the upstream version.
- **`EXPOSE 4840`** — internal port is always 4840.
- **OCI labels** — `org.opencontainers.image.source` etc. for GHCR.

A minimal Dockerfile template:

<!-- @code-block language="text" label="my-new-server/Dockerfile" -->
```text
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake build-essential ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ... build steps producing /usr/local/bin/myserver ...

FROM debian:bookworm-slim

LABEL org.opencontainers.image.source="https://github.com/<owner>/extra-test-suite"
LABEL org.opencontainers.image.description="My custom OPC UA test server"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/bin/myserver /usr/local/bin/

EXPOSE 4840

CMD ["/usr/local/bin/myserver"]
```
<!-- @endcode-block -->

The two existing Dockerfiles in this repo don't follow this
shape exactly — they `COPY --from=build /src/build/bin/examples/<binary>`
because open62541's CMake build drops example binaries there.
If you're forking from one of those Dockerfiles, either move
your binary to `/usr/local/bin/` during the build stage so the
runtime `COPY` above works, or change the runtime `COPY` source
path to wherever your build stage actually produces the binary.

## Step 2 — Add the service to `docker-compose.yml`

Pick a host port that **doesn't** conflict with anything already
in use:

```text
4840-4849   reserved for uanetstandard-test-suite
4851         reserved for uanetstandard-test-suite (SKS)
14850        reserved for uanetstandard-test-suite (PubSub)
24840         taken by open62541-nm
24841         taken by open62541-all-security
24842+        available
```

For convenience, follow the `2484x` convention. Add to
`docker-compose.yml`:

<!-- @code-block language="text" label="docker-compose.yml" -->
```text
services:
  open62541-nm:
    # ... existing ...

  open62541-all-security:
    # ... existing ...

  my-new-server:
    image: ghcr.io/<owner>/extra-test-suite/my-new-server:${TAG:-latest}
    build:
      context: ./my-new-server
    container_name: opcua-extra-my-new
    ports:
      - "24842:4840"
    restart: unless-stopped
```
<!-- @endcode-block -->

The `image:` + `build:` dual pattern means:

- `docker compose up -d --build` in local dev → **builds** from
  the Dockerfile (force).
- `docker compose pull && docker compose up -d` in CI → **pulls**
  from GHCR.
- Plain `docker compose up -d` (no flag) prefers the `image:` if
  it's available locally or in the registry, and only falls back
  to `build:` when neither has a copy — so pass `--build`
  explicitly when you want to test your local Dockerfile
  changes.

## Step 3 — Add to `docker-compose.ci.yml`

The CI override disables auto-restart for every service:

<!-- @code-block language="text" label="docker-compose.ci.yml" -->
```text
services:
  open62541-nm:
    restart: "no"

  open62541-all-security:
    restart: "no"

  my-new-server:
    restart: "no"
```
<!-- @endcode-block -->

## Step 4 — Add a port-wait to `action.yml`

The composite action polls each service's port. Add yours:

<!-- @code-block language="text" label="action.yml (excerpt)" -->
```text
rc=0
wait_port open62541-nm 24840 || rc=1
wait_port open62541-all-security 24841 || rc=1
wait_port my-new-server 24842 || rc=1     # <-- add

if [ "$rc" -ne 0 ]; then
  docker compose -f "$COMPOSE_FILE" -f "$COMPOSE_CI_FILE" logs || true
  exit "$rc"
fi
```
<!-- @endcode-block -->

## Step 5 — Bump the minor version

Adding a service is a **minor bump** per the suite's versioning
rules.

```text
v1.1.0 → v1.2.0
```

Tag and push:

```text
git tag v1.2.0
git push origin v1.2.0
```

The publish workflow runs on `v*` tag push:

1. `docker compose build --push` — builds every service tagged
   with `:v1.2.0` and uploads them to GHCR in one step.
2. Re-tags and pushes each image as `:latest`.

Both your existing services and the new one get published in
one workflow run — no per-service workflow changes needed.

## When to add a server

| Scenario                                                  | Add a server? |
| --------------------------------------------------------- | ------------- |
| You found a gap in `uanetstandard-test-suite` coverage     | Yes            |
| You need a vendor-specific stack (Prosys, Milo, …) in CI    | Yes            |
| You need a different version of open62541                  | Yes (new service) |
| You need a different security config of the same stack     | Yes (new service) |
| You want a richer address space                            | No — fork `uanetstandard-test-suite` and add a builder |

The suite stays small **because** the main suite is the right
place for most things. The "extras" are explicitly the things
the main suite doesn't or can't cover.

## Common gaps that could become services

| Gap                                                  | Possible service                                |
| ---------------------------------------------------- | ----------------------------------------------- |
| Prosys-style rich type model                          | A Prosys Simulation Server image                |
| Eclipse Milo (Java stack)                             | A Milo demo server image                         |
| node-opcua wire details                                | A node-opcua-based server                        |
| MQTT-PubSub                                           | A separate broker + a PubSub publisher           |
| Vendor-specific ExtensionObject payloads              | Whatever vendor stack ships those types          |

For a real fork-and-PR scenario, see the suite's
[GitHub issues](https://github.com/php-opcua/extra-test-suite/issues)
for what the maintainers are considering.

## Where to read next

- [Servers · Overview](../servers/overview.md) — the existing
  services for reference.
- [GitHub Action](../ci-integration/github-action.md) — the
  action your new service plugs into.
