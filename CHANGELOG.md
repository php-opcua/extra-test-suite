# Changelog


## [v1.1.0] - 2026-05-12

Second release. Adds a permissive open62541-based mirror of the
`uanetstandard-test-suite` port 4843 (`opcua-all-security`) so the encryption
and username/password authentication paths can be exercised without the
UA-.NETStandard stack running, and so CI clients can validate every RSA
security policy against a second implementation.

### Added

- **`open62541-all-security` service** — open62541 v1.4.8 built with
  `UA_ENABLE_ENCRYPTION=OPENSSL` plus a small custom example
  (`open62541-all-security/server.c` in the build context) that wires every
  RSA security policy the build supports, every security mode
  (`None`/`Sign`/`SignAndEncrypt`), Anonymous + Username/Password
  authentication with the same four logins as `uanetstandard-test-suite`
  (`admin`/`admin123`, `operator`/`operator123`, `viewer`/`viewer123`,
  `test`/`test`). Host port `24841:4840`. Self-signed RSA-2048 server cert
  generated on first boot by `entrypoint.sh` (DER, `subjectAltName`
  `URI:urn:open62541.server.application`, `IP:127.0.0.1`, `DNS:localhost`).
- **Permissive trust posture** for the new service: replaces the default
  trust-list-based PKI with `UA_CertificateVerification_AcceptAll` on both
  `secureChannelPKI` and `sessionPKI`, and sets `allowNonePolicyPassword =
  true`. Mirrors `uanetstandard-test-suite` port 4845 (`opcua-auto-accept`)
  behaviour: ephemeral client certificates and plain-text passwords on
  `SecurityPolicy#None` are both accepted, no server-side pre-trust step
  required. Documented in README as a test posture — do NOT lift the wiring
  into a production server template.
- **Composite action waits for both ports.** `action.yml` now polls
  `24840` and `24841` in sequence and exits non-zero if either port fails to
  accept TCP within `startup-timeout` seconds. The compose logs are dumped
  to the runner on any failure (previously dumped only when the single
  port-24840 check timed out).

### Changed

- Action `description` and README rewritten to reflect the now-two-server
  inventory; the "single open62541 server" framing from v1.0.0 is gone. The
  "Future services" rule about staying clear of `4840–4849` still holds.

## [v1.0.0] - initial release

First public release. Extracted from `php-opcua/opcua-client` where the
equivalent Dockerfile was living under `.github/opcua-nodemanagement/` and was
being rebuilt on every CI run (3–5 min cold / ~1 min warm).

### Added

- **docker-compose.yml** — source of truth for every server this suite ships.
  Uses the `image:` + `build:` dual pattern: local dev / publish workflow
  builds the services and tags them with the declared image name; the composite
  action pulls them pre-built from GHCR. `TAG` env var is set to `v<semver>`
  by the publish workflow and defaults to `latest` for consumers of the action.
  Local default policy is `restart: unless-stopped` so developers start the
  suite once and it survives reboots — same pattern as `uanetstandard-test-suite`.
- **docker-compose.ci.yml** — CI-only override merged on top of the base file
  by the composite action. Sets `restart: "no"` to match the ephemeral runner
  lifecycle (any restart would be meaningless on a VM that is about to be
  destroyed).
- **`open62541-nm` service** — first and only service at v1.0.0. Dockerfile
  lives in `open62541-nm/Dockerfile` and builds `open62541 v1.4.8` (pinned via
  `ARG OPEN62541_REF`) with `UA_ENABLE_NODEMANAGEMENT=ON`,
  `UA_ENABLE_SUBSCRIPTIONS=ON`, `UA_ENABLE_METHODCALLS=ON`,
  `UA_ENABLE_HISTORIZING=ON`, `UA_NAMESPACE_ZERO=FULL`. Multi-stage to keep
  the runtime image small (debian-slim + libssl3 + the built example binary).
  Fallback chain in the entrypoint (`ci_server` → `server_ctt` →
  `access_control_server` → …) so the image keeps working if upstream renames
  or removes an example. Host port mapped to `24840:4840` to avoid clashes with
  `uanetstandard-test-suite`'s `4840–4849` range.
- **Composite action (`action.yml`)** — `uses: php-opcua/extra-test-suite@v1.0.0`
  does `docker compose pull && docker compose up -d` using the action's own
  checkout as compose file source (`github.action_path`), merging
  `docker-compose.yml` and `docker-compose.ci.yml`. Inputs: `image-tag`
  (default `latest`) and `startup-timeout` (default `30`). No outputs: the
  endpoint URLs are part of the suite's versioned contract (port `24840` for
  `open62541-nm`), just like `uanetstandard-test-suite` on `4840-4849` — the
  caller hardcodes them in its own test helpers.
- **Publish workflow (`.github/workflows/publish.yml`)** — on tag push `v*`,
  runs `docker compose build --push` (tags every service as
  `ghcr.io/<owner>/extra-test-suite/<service>:<tag>` and pushes), then re-tags
  and pushes each image as `:latest`. GHA layer cache is reused across runs.
