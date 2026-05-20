---
eyebrow: 'Docs · Reference'
lede:    'Every environment variable the suite reads. Short list — most of the behaviour is hard-coded in the Dockerfiles and server.c.'

see_also:
  - { href: './ports-and-endpoints.md',                 meta: '2 min' }
  - { href: '../ci-integration/github-action.md',        meta: '4 min' }

prev: { label: 'Ports and endpoints',  href: './ports-and-endpoints.md' }
next: { label: 'Troubleshooting',      href: './troubleshooting.md' }
---

# Environment variables

The suite is **mostly hard-coded** — unlike
`uanetstandard-test-suite` which is heavily env-var-driven. The
few env vars below are all that exists.

## Compose-level

Read by `docker-compose.yml` itself (not by the servers):

| Variable | Default    | Effect                                                   |
| -------- | ---------- | -------------------------------------------------------- |
| `TAG`    | `latest`   | Image tag to pull for every service in compose.          |

Set this when pulling a pinned version:

<!-- @code-block language="bash" label="bash" -->
```bash
TAG=v1.1.0 docker compose pull
TAG=v1.1.0 docker compose up -d
```
<!-- @endcode-block -->

The GitHub Action sets `TAG` from its `image-tag` input.

## `open62541-nm`

No env vars. The Dockerfile's `CMD` runs a fallback chain of
upstream binaries; nothing in the runtime is configurable
without rebuilding.

To change behaviour, fork and edit the Dockerfile — see
[Customization](../customization/forking-and-adding-a-server.md).

## `open62541-all-security`

The entrypoint script reads two:

| Variable                  | Default                                | Effect                                          |
| ------------------------- | -------------------------------------- | ----------------------------------------------- |
| `CERT_DIR`                | `/certs`                                | Where the self-signed cert + key live           |
| `OPCUA_APPLICATION_URI`   | `urn:open62541.server.application`     | URI written into the server cert's `subjectAltName URI:` field at generation time. **Does not** change the server's runtime ApplicationUri. |

If you change `OPCUA_APPLICATION_URI`, the regenerated cert
carries the new URI in its `subjectAltName`. **The server's
ApplicationUri is unaffected** — it stays
`urn:open62541.server.application` because that value is
hard-coded by open62541's defaults and isn't wired to this env
var. Useful only if your client validates the cert's SAN against
a specific URI; clients that match the server's runtime
ApplicationUri will see the open62541 default regardless of
what you set here.

To change either, override in the compose file:

<!-- @code-block language="text" label="compose override" -->
```text
services:
  open62541-all-security:
    environment:
      OPCUA_APPLICATION_URI: "urn:custom:test-server"
    volumes:
      - ./certs-extra:/certs       # to persist the new cert
```
<!-- @endcode-block -->

Then `docker compose up -d --force-recreate` and the new cert is
generated on next boot.

## Build-time `ARG`s

Set at `docker compose build` time, not at runtime:

| ARG                  | Default     | Effect                                              |
| -------------------- | ----------- | --------------------------------------------------- |
| `OPEN62541_REF`      | `v1.4.8`    | The upstream open62541 git tag/branch to build from |

To build with a different open62541 version (e.g. testing a
release candidate):

<!-- @code-block language="bash" label="bash" -->
```bash
docker compose build --build-arg OPEN62541_REF=v1.5.0
```
<!-- @endcode-block -->

This applies to **both** services (they share the same ARG name).

## What's `NOT` configurable via env

Compared to `uanetstandard-test-suite`'s ~30 OPCUA_* env vars,
this suite hard-codes:

| Behaviour                         | Where it's set                                 |
| --------------------------------- | ---------------------------------------------- |
| Internal port (4840)              | `EXPOSE` in Dockerfile                          |
| Host port mapping (24840/24841/24842) | `ports:` in `docker-compose.yml`           |
| Security policies offered          | open62541's `setDefaultWithSecurityPolicies()` |
| Security modes offered             | Same                                            |
| User accounts                      | Hard-coded array in `server.c`                 |
| `allowAnonymous`                   | `true` in `server.c`                            |
| Permissive PKI (auto-accept)        | `server.c`                                      |
| `allowNonePolicyPassword`          | `true` in `server.c`                            |
| Cert validity                       | 10 years (in `entrypoint.sh`)                   |

To change any of these, **fork and edit** the relevant file.
This is intentional — the suite is small enough that compile-time
configuration is simpler than runtime configuration.

## Action inputs (not env vars)

`action.yml` exposes two inputs (passed via `with:` in
workflows):

| Input              | Default   | Effect                                              |
| ------------------ | --------- | --------------------------------------------------- |
| `image-tag`         | `latest`  | Becomes the `TAG` env var inside the action          |
| `startup-timeout`   | `30`      | Seconds per service to wait for TCP port              |

See [GitHub Action](../ci-integration/github-action.md).

## Where to read next

- [Troubleshooting](./troubleshooting.md) — common config /
  startup issues.
- [Customization](../customization/forking-and-adding-a-server.md) —
  to add real env-var-driven behaviour to a forked service.
