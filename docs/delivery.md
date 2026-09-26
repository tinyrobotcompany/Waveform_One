# Delivery and device updates

## What is implemented

| Event | Behaviour |
| --- | --- |
| Local commit | All host unit tests run through `.githooks/pre-commit` after setup |
| PR | Host tests, changeset validation and five ESP-IDF builds |
| Non-draft PR | Codex review of the diff using trusted base-branch code |
| Push to main | The same tests/builds; publish pending notes, version tag and USB firmware bundle |
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
   it into chat. The review job intentionally fails if the key is missing.
3. Optionally set repository variable `CODEX_REVIEW_MODEL` (default `gpt-5.5`,
   inherited from the template); the chosen model must support Responses structured
   output and the configured reasoning effort.
4. Enable GitHub Actions and allow it to submit pull-request reviews if repository
   or organisation policy disables that feature. The workflow grants only contents
   read and PR write. The release job separately has contents write.
5. Once checks have appeared, protect `main` with required host tests, changeset
   and all five firmware build checks. Require `Codex Review` once the key is set.
   The Codex check means a review was completed, not that all findings were resolved;
   the script posts comments rather than approving or rejecting a PR.

The repository had no OpenAI secret or release at integration time. Repository
rulesets and secrets are not silently changed by these files. No release is
published until a tested main build runs. The README links to the Releases page
and displays the latest version automatically after that first release.

PR jobs have no deployment secrets. The privileged Codex workflow never checks
out or executes PR code, including fork code: it fetches the diff as untrusted
text. The model cannot run tools. Large diffs are truncated at 120,000 characters
and the review prompt records the limitation. The job rejects incomplete/invalid
responses and binds the review to the captured head commit.

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

There is currently no Pi application, systemd service or configured deployment
target in this repo, so an honest deployment pipeline cannot install one yet.
During Pi integration, add its unit tests to `scripts/test.sh`, its build to the
shared validation workflow, and its versioned package to the same release.

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
The runner was adapted to review all non-draft PRs and fail on incomplete responses.
Hooks, changeset handling and CI follow the template's approach but use the existing
C++ host tests and ESP-IDF builds instead of its Node/.NET/Azure deployment targets.
