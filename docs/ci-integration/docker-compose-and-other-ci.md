---
eyebrow: 'Docs · CI integration'
lede:    'Running the suite outside GitHub Actions — GitLab CI, Jenkins, CircleCI, local dev. The plain Docker Compose pattern.'

see_also:
  - { href: './github-action.md',                  meta: '4 min' }
  - { href: '../getting-started/installation.md',  meta: '3 min' }

prev: { label: 'GitHub Action',                       href: './github-action.md' }
next: { label: 'Forking and adding a server', href: '../customization/forking-and-adding-a-server.md' }
---

# Docker Compose and other CI

Outside GitHub Actions, the canonical pattern is **clone, pull,
compose up**.

## The two compose files

| File                       | Purpose                                              |
| -------------------------- | ---------------------------------------------------- |
| `docker-compose.yml`        | Base — uses `image:` + `build:` dual mode            |
| `docker-compose.ci.yml`     | CI override — disables `restart`                      |

For CI: use **both**, in that order:

<!-- @code-block language="bash" label="terminal — CI start" -->
```bash
git clone https://github.com/php-opcua/extra-test-suite.git
cd extra-test-suite

TAG=v1.1.0 docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
TAG=v1.1.0 docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d
```
<!-- @endcode-block -->

## Wait for both ports

The action handles this internally. Outside the action, your CI
job needs to wait:

<!-- @code-block language="bash" label="wait loop" -->
```bash
for port in 24840 24841; do
  for i in $(seq 1 30); do
    nc -z localhost "$port" 2>/dev/null && break
    sleep 1
  done
done
```
<!-- @endcode-block -->

## GitLab CI

<!-- @code-block language="text" label=".gitlab-ci.yml" -->
```text
integration-tests:
  image: docker:24
  services:
    - docker:24-dind
  variables:
    TAG: v1.1.0
  before_script:
    - apk add --no-cache git docker-cli-compose netcat-openbsd
    - git clone https://github.com/php-opcua/extra-test-suite.git /tmp/extras
    - cd /tmp/extras
    - docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
    - docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d
    - |
      for port in 24840 24841; do
        for i in $(seq 1 30); do
          nc -z localhost "$port" 2>/dev/null && break
          sleep 1
        done
      done
  script:
    - cd "$CI_PROJECT_DIR"
    - vendor/bin/pest --group=integration
  after_script:
    - cd /tmp/extras
    - docker compose -f docker-compose.yml -f docker-compose.ci.yml down
```
<!-- @endcode-block -->

`docker:dind` provides the Docker daemon the runner needs.

## Jenkins (declarative pipeline)

<!-- @code-block language="text" label="Jenkinsfile (excerpt)" -->
```text
pipeline {
  agent any
  environment { TAG = 'v1.1.0' }
  stages {
    stage('Start extras') {
      steps {
        sh '''
          git clone https://github.com/php-opcua/extra-test-suite.git extras
          cd extras
          docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
          docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d
        '''
        sh '''
          for port in 24840 24841; do
            for i in $(seq 1 30); do
              nc -z localhost "$port" 2>/dev/null && break
              sleep 1
            done
          done
        '''
      }
    }
    stage('Test') {
      steps {
        sh 'vendor/bin/pest --group=integration'
      }
    }
  }
  post {
    always {
      sh '''
        cd extras
        docker compose -f docker-compose.yml -f docker-compose.ci.yml down
      '''
    }
  }
}
```
<!-- @endcode-block -->

## CircleCI

<!-- @code-block language="text" label=".circleci/config.yml" -->
```text
jobs:
  test:
    machine:
      image: ubuntu-2204:current
    environment:
      TAG: v1.1.0
    steps:
      - checkout
      - run:
          name: Start extras
          command: |
            git clone https://github.com/php-opcua/extra-test-suite.git /tmp/extras
            cd /tmp/extras
            docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
            docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d
      - run:
          name: Wait for servers
          command: |
            for port in 24840 24841; do
              for i in $(seq 1 30); do
                nc -z localhost "$port" 2>/dev/null && break
                sleep 1
              done
            done
      - run:
          name: Tests
          command: vendor/bin/pest --group=integration
      - run:
          name: Teardown
          command: |
            cd /tmp/extras
            docker compose -f docker-compose.yml -f docker-compose.ci.yml down
```
<!-- @endcode-block -->

## Local development

For working on a client library that targets the suite:

<!-- @code-block language="bash" label="local" -->
```bash
git clone https://github.com/php-opcua/extra-test-suite.git
cd extra-test-suite
docker compose up -d
```
<!-- @endcode-block -->

The base compose file uses `restart: unless-stopped`, so the
containers survive host reboots. Start once, leave running.

Or pull pre-built:

<!-- @code-block language="bash" label="local pull" -->
```bash
TAG=v1.1.0 docker compose pull
TAG=v1.1.0 docker compose up -d
```
<!-- @endcode-block -->

## Cleanup

```text
docker compose down -v
```

`-v` removes anonymous volumes — important because
`open62541-all-security`'s Dockerfile declares
`VOLUME ["/certs"]`, which Docker backs with an **anonymous**
volume by default. Without `-v` that volume (and the server
cert in it) survives `down`, and stale fingerprints across runs
can cause cache mismatches in your tests.

## Image freshness

The publish workflow tags images with both `:vX.Y.Z` (immutable)
and `:latest` (the last vX.Y.Z published). CI should pin
`:vX.Y.Z` for reproducibility.

## Combining with the main suite

For Jenkins / GitLab / etc., run **both** suites in parallel:

<!-- @code-block language="bash" label="both suites" -->
```bash
# Pull and start main suite
git clone https://github.com/php-opcua/uanetstandard-test-suite.git /tmp/main
cd /tmp/main
docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d

# Pull and start extras
git clone https://github.com/php-opcua/extra-test-suite.git /tmp/extras
cd /tmp/extras
docker compose -f docker-compose.yml -f docker-compose.ci.yml pull
docker compose -f docker-compose.yml -f docker-compose.ci.yml up -d

# Wait for all ports
for port in 4840 4841 4842 4843 4844 4845 4846 4847 4848 4849 4851 24840 24841; do
  for i in $(seq 1 60); do
    nc -z localhost "$port" 2>/dev/null && break
    sleep 1
  done
done

# Run tests
vendor/bin/pest --group=integration

# Cleanup both
cd /tmp/main && docker compose -f docker-compose.yml -f docker-compose.ci.yml down
cd /tmp/extras && docker compose -f docker-compose.yml -f docker-compose.ci.yml down
```
<!-- @endcode-block -->

The ports don't overlap (4840-4851 vs 24840-24841), so this
just works.

## Where to read next

- [Forking and adding a server](../customization/forking-and-adding-a-server.md) —
  to add your own services.
