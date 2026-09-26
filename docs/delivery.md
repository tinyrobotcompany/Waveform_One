# Delivery and device updates

## What is implemented

| Event | Behaviour |
| --- | --- |
| Local commit | All host unit tests run through `.githooks/pre-commit` after setup |
| PR | Host tests, changeset validation, five ESP-IDF builds and an ARM64 Pi controller build |
| Non-draft PR | Codex review of the diff using trusted base-branch code |
| Push to main | The same tests/builds; publish pending notes, version tag, USB firmware bundle and Pi controller archive |
| Device in the field | No automatic installation yet |

The release ZIP contains the visualizer application, bootloader, partition table,
flash arguments, installation instructions and a manifest identifying the commit
and file hashes. `SHA256SUMS` checks the ZIP for corruption; it is **not a signature**.
Demo programs are build-checked but are not distributed as production updates.
An interrupted release can resume its draft on a rerun; published assets are not
overwritten. Docs-only pushes without new changesets publish nothing.

## GitHub activation

1. Merge these workflows into `main` after review. `pull_request_target` uses the
   base branch, so the bootstrap PR cannot use its own unmerged review workflow.
2. Add `OPENAI_API_KEY` in repository Settings → Secrets and variables → Actions.
   Use a project key with an appropriate spending limit. Do not commit it or paste
   it into chat. As in Voxa, the workflow skips review when the key is missing.
3. The workflow matches Voxa exactly: `gpt-5.5`, `low` reasoning, a 120,000-character
   diff limit and a 6,000-token output limit. These settings are explicit in the workflow.
4. Enable GitHub Actions and allow it to submit pull-request reviews if repository
   or organisation policy disables that feature. The workflow grants only contents
   read and PR write. The release job separately has contents write.
5. Add `REPO_ADMIN_TOKEN`, a token scoped to this repository with administration
   read/write permission, to enable Voxa's `Ensure Codex Review Ruleset` workflow.
   On main pushes it creates or updates `Require Codex Review` for `refs/heads/main`.
   It preserves Voxa's bypass settings (repository role ID `2`, integration ID
   `15368`, with a role-only fallback if GitHub rejects the integration).
   Configure both secrets before merging to activate reviews and enforcement.
   Once checks have appeared, also protect `main` with required host tests,
   changeset and all five firmware build checks.
   Inspect the review body before merging: the Voxa workflow can succeed after a
   skipped review or a fallback comment. It posts comments rather than approvals.

The repository had no OpenAI secret or release at integration time. Repository
secrets are not supplied by these files. The enforcement workflow requires the
admin token and will fail clearly if it is absent. No release is
published until a tested main build runs. The README links to the Releases page
and displays the latest version automatically after that first release.

PR jobs have no deployment secrets. The privileged Codex workflow never checks
out or executes PR code, including fork code: it fetches the diff as untrusted
text. The model cannot run tools. Large diffs are truncated at 120,000 characters
and the review prompt records the limitation. The Voxa runner posts a fallback
comment when the response does not meet its output contract; it skips Dependabot
and changeset-release PRs. Its comment marker records the head SHA.

## ESP32 OTA: possible, not enabled yet

The current partition table has a single factory application. ESP-IDF OTA requires
an OTA data partition and alternating application slots. Plan one initial USB
migration to an OTA-capable partition table and updater firmware. Do not try to
feed the current full USB bundle into an OTA app installer.

Before enabling updates, implement and test:

- Wi-Fi provisioning or an authenticated Pi-to-ESP transport.
- Two adequately sized OTA app slots and boot validation/rollback.
- Signed firmware/manifest verification, hardware and protocol compatibility checks,
  version policy, and HTTPS download with certificate validation.
- Recovery tests for power loss, corrupt downloads, failed boot and unavailable network.
- A startup health check before marking the new app valid, plus USB recovery.

Official reference: [ESP-IDF ESP32-S3 OTA documentation](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/ota.html).

## Pi deployment and future customer devices

The first Pi application is the Rust USB command-line controller in `pi/core`.
Its tests run through `scripts/test.sh`; CI builds a native ARM64 Linux release
and publishes its archive alongside the firmware. The persistent service,
Qt/QML touchscreen interface and automatic installation/updater are not built
yet. The prototype is installed manually on the development Pi for USB testing.

For the development Pi, use a scoped deployment target with a known service,
health check and rollback. For customers, prefer an updater on each Pi that pulls
signed releases over HTTPS; customers should not need inbound SSH, GitHub runner
credentials or access to our signing keys. Use stable/beta channels, explicit
update preferences and staged rollout. Retain the previous working Pi release.
Coordinate ESP/Pi protocol compatibility before updating either component.

The Pi can eventually coordinate ESP updates over local Wi-Fi or USB. That choice
belongs with Pi integration and enclosure/connectivity requirements. Publishing
releases now gives both updaters a future distribution source; it does not itself
provide OTA, rollback, signatures or fleet management.

## Template provenance

Codex review scripts, tests and rubrics were imported from
[`simonholmes001/project-template`](https://github.com/simonholmes001/project-template/tree/ded9fcfb24cbe0bb741c2aed71d9a42bb1ffd0af/template/base).
The review setup was subsequently replaced with the exact Voxa files described below.
Hooks, changeset handling and CI follow the template's approach but use the existing
C++ host tests and ESP-IDF builds instead of its Node/.NET/Azure deployment targets.

The review workflow, runner, core helper, core tests and three review rubrics now
match [Voxa commit 714ac8f](https://github.com/simonholmes001/voxa/tree/714ac8fb5f70d617a50def04f159d483c4e4a167/.github)
byte for byte, as requested. The additional local runner integration tests verify
Voxa's model settings, dependency-PR skip and fallback behaviour. The imported
ruleset enforcement runs on main with its configured secret.

### Affected-component PR checks

PR validation compares the merge base with the PR head, including deleted files
and both sides of renames. Each ESP32 application builds only when its files
change; mic-test changes also select the visualizer because it compiles the
shared audio pipeline. Shared firmware code selects all firmware builds.
Documentation and changeset-only edits skip builds and unit tests, while the
changeset policy check still runs. Pi changes select the ARM64 build, automation
changes select host tests, and firmware packaging changes select the visualizer.
Changes to the validation workflows or scope selector run the complete suite.
The complete PR diff is considered on every update, not only the latest commit.
Main-branch release validation retains full builds to supply all release assets.
