# Certificate migration: local implementation checkpoint

28 November 2026. This is local execution evidence, not hosted CI, deployment or device acceptance.

## Baseline and reviewable slices

Local `main` and `origin/main` are both `86de63a88e97675c2aab3d36ce2160cd8f73ac06`. Main remains clean. Changes are uncommitted, with no pushes, PR creation, paid review runs, deployment or hardware operations in this batch.

Worktrees are under `/Users/simonholmes/Projects/Applications/Waveform_One/.worktrees/`.

| Branch / worktree leaf | Result | Local verification |
| --- | --- | --- |
| `docs/certificate-architecture` / `certificate-architecture` | Accepted language/infrastructure decisions, agent protocol, migration plan | Existing full suite passed; subsequent documentation-only edits passed whitespace checks |
| `fix/review-fail-closed` / `review-fail-closed` | Strict structured review results, blocking findings, exact-head checks, no truncation/fallback, workflow missing-key failure | Integrated full suite passed; no live provider request |
| `refactor/release-node` / `release-node` | Node release planner and changeset validation; shared test discovery; backend changes require release notes | Full suite passed; final focused suite passed after backend-note regression fix |
| `ci/sdk-alignment` / `sdk-alignment` | Pinned SDK, Node production/P4 packagers, Node scope selector, updated callers and locks | Integrated full suite, workflow lint and SDK guard passed; six app builds and production signature checks completed |
| `feature/certificate-dotnet` / `certificate-dotnet` | .NET 10 Domain/Application validation libraries and shared/CI test integration | Integrated full suite passed; 35 Domain and 24 Application tests; zero-test discovery fails |

The `refactor/ci-scope-node` worktree is the isolated source of the selector integration, already included in SDK/backend slices. It is not an additional required PR. The selector is duplicated identically across those pending branches so each selects backend/build validation correctly before integration.

Full proposed diffs, including new files, were measured below the unchanged 120,000-character review limit. At this checkpoint: review 39,359; release 43,010; SDK 95,255; backend 69,046 characters. Re-measure after any delivery preparation. Keep the slices separate.

## Combined execution

`test/certificate-integration`, worktree `certificate-integration`, combines the slices for validation only. Its combined diff is not suitable for one PR. Workflow changes combined cleanly; the shared runner required retaining both the new Node test invocation and the backend invocation.

Executed from that worktree:

```sh
DOTNET=/opt/homebrew/bin/dotnet \
  NUGET_PACKAGES=/Users/simonholmes/Projects/Applications/Waveform_One/.worktrees/certificate-dotnet/backend/.artifacts/nuget \
  sh scripts/test.sh
actionlint .github/workflows/validate.yml .github/workflows/release.yml \
  .github/workflows/pr-checks.yml .github/workflows/codex-pr-review.yaml
git diff --check
```

All passed. The shared command ran 119 repository Node tests, 19 release-tool Node tests, 35 Domain tests, 24 Application tests, the remaining legacy suites and firmware host tests. Evidence: `certificate-integration/.artifacts/combined-validation.log`.

Local Node is 24.6.0; workflows select Node 22. Exact Node 22 and hosted-container execution remain unverified. .NET execution used SDK 10.0.400 at `/opt/homebrew/bin/dotnet`; another installation on this machine resolves to SDK 9. The backend wrapper supports an explicit `DOTNET` executable and fails rather than skipping an unavailable SDK. Test-discovery failure, locked restores and coverage collection remain enabled.

The certificate agent measured 100% executable-line coverage in both owning production assemblies, with Domain branch coverage 87.9% and Application 97.72%. These are measured results, not a new declared threshold.

## Firmware build evidence and limits

The SDK agent built `led_test`, `mic_test`, `visualizer`, `kovi_scroll`, `quinie_scroll` and `p4_display` in an isolated source copy with SDK commit `051e4c4eb20346e043cdd47bc388fde75ff8af63`. The production profile's bootloader/application signatures were verified with an ephemeral RSA-3072 test key, then key files were removed. Old/new P4 packagers produced eight byte-identical artifacts, including the detached signature and Unicode release notes.

P4 development image: 8,094,704 bytes, 293,904 bytes (3.50%) free. Production image: 8,130,560 bytes, 258,048 bytes (3.08%) free in its 8 MiB slot. Future certificate firmware must retain the size gate; this margin does not establish runtime heap/socket headroom. Logs are in `sdk-alignment/.artifacts/`, including `p4-build.log` and `factory-build.log`.

No firmware was flashed, no eFuses were programmed, and no real issuance/browser/renewal/OTA acceptance was performed.

## Outstanding work and decisions

- Approve the exact Waveform signing context, header, route, hostname namespace and response contract before production envelope binding. Current .NET code validates caller-supplied exact bytes and strict request/domain rules; it is not request admission, replay protection or a running certificate service.
- Implement manufacturing admission, atomic nonce/work acceptance, persistence, DNS/ACME and recovery after their contracts are settled.
- Complete hardware-derived identity storage and enrollment integration, then execute physical acceptance. Confirm the HMAC slot and migration/rollback policy first.
- Inventory shared Azure resources and inherited permissions, assign shared-resource ownership and confirm incremental cost. No live Azure inspection or modification occurred in this batch.
- Continue removing general-purpose Python: generic firmware packaging, release-asset verification, hosted-memory patching, phone tools/tests, and history/catalogue/benchmark/product-evidence utilities. Classify PCB/CAD boundaries and enforce the final specialist allowlist. Existing Python is not a permanent exception.
- Staged-snapshot hooks and remaining delivery-control hardening are still pending. Existing repository review exemptions remain; this slice does not remove them. The review bootstrap PR runs the current base scripts and needs human review of the new control itself.
- Commit/push/PR creation awaits Simon's delivery checkpoint. Merges and deployment remain separately controlled.
