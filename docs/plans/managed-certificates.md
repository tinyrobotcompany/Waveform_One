# Managed certificate migration

28 November 2026. This is an implementation plan, not a statement that managed enrollment or shared infrastructure is deployed.

See the [local implementation checkpoint](certificate-implementation-status.md) for completed slices, executed validation and remaining limits.

## Reference and accepted decisions

- Waveform main baseline: `86de63a88e97675c2aab3d36ce2160cd8f73ac06`.
- Tempo reference: `4eae79b34c7848be7ba871e0e82f44fc47b14fa1`.
- Waveform draft #48 (`01a687f`) is comparison material. Its shared-product service contract is obsolete and must not be merged unchanged. Tempo removed Waveform support in PR #44.
- [C++ application firmware](../adr/0002-firmware-language.md).
- [.NET 10 certificate backend](../adr/0003-certificate-service-dotnet.md); supersedes the proposed Python adaptation.
- [Language policy and required Python migration](../adr/0005-language-policy-and-python-migration.md): Node/shell controls, .NET utilities, and explicit Espressif/model/build123d exceptions. KiCad integrations require individual review.
- [Shared Azure foundations, Storage account and Key Vault](../adr/0004-shared-certificate-foundation.md), with separate applications, tables, secrets and identities.
- Public CA trust anchors are allowed; private keys/provisioning material remain prohibited.
- Simon approved superseding the older successful-skip/fallback review and working-tree-hook behavior with fail-closed review and staged-snapshot validation. These are targets until implemented and merged. Existing OTA approval controls remain.

## Delivery sequence

1. Pin and verify the reviewed SDK/HTTP parser; replace the custom Python production-build wrapper and its tests with Node. Build all firmware applications and applicable P4 profiles. Record size/headroom and dependency compatibility.
2. Correct the existing single Codex review, including workflow invocation. Test missing credentials, oversized input, malformed output, blocking findings and stale-head handling offline. Separately implement staged-snapshot hooks and remaining approved delivery controls. Do not bypass a required check or rerun a paid review to obtain a different answer.
3. Approve the exact Waveform wire contract and synthetic cross-language fixtures. Plan the .NET solution and maintained crypto/ACME adapters. Keep this separate from infrastructure ownership/cost decisions.
4. Implement protected on-device identities and HMAC-derived storage with tests first. Keep Waveform's existing settings/credential/session encryption, including the required `nvs_keys` partition. Never infer that replacing identity encryption permits deleting settings keys.
5. Implement the .NET service with protocol, persistence, provider, failure/recovery and isolation tests. Separate DNS and certificate processing; private device keys never leave firmware.
6. Integrate enrollment and renewal into Waveform's existing phone authorization/TLS lifecycle. Preserve network leases, bounded requests, recognition/OTA coordination, expiry handling and safe failed-stop recovery.
7. Inventory shared resources and inherited permissions, settle authoritative deployment ownership, approve incremental cost and prepare reviewed Bicep/pipeline changes. No local workload deployment.
8. Commission a test device and execute real issuance/browser/renewal/load/power-loss/OTA acceptance. Only then prepare customer activation.

Each coherent change gets its own tests, documentation and changeset where required. Keep complete PR diffs below the unchanged review limit with margin. Merges require Simon's explicit permission per PR. Agent assignments follow the [communication protocol](../engineering-agent-protocol.md).

## Approved implementation batch

The current batch uses separate worktrees from the Waveform baseline above. Assignment does not establish completion; the AiC reviews the returned diff and test evidence before integration.

| Owner | Branch | Assigned slice |
| --- | --- | --- |
| SDK/build agent | `ci/sdk-alignment` | Pinned SDK guard, Node production-build/P4 packaging replacements, equivalent tests and build callers |
| Repository-controls agent | `refactor/release-node` | Node release planning/changeset validation, equivalent tests and direct release callers |
| Certificate agent | `feature/certificate-dotnet` | .NET 10 protocol/domain slice and cryptographic validation tests; no cloud adapters or hosting |
| AiC | `docs/certificate-architecture` | Accepted decisions, communication protocol and migration plan |
| AiC | `fix/review-fail-closed` | Integrate the completed review-runner correction with workflow and documentation |
| AiC | `refactor/ci-scope-node` | Node CI selector with backend/build-tool recognition and conservative unknown-path handling |

The release workflow consumes the P4 packager owned by another slice. The AiC must reconcile its callers and validate the combined result. New Node/.NET tests must be included in the shared validation entry point and have their runtimes available in CI. Keep unmigrated tests running during the transition.

This batch does not complete the general-purpose Python migration. Remaining work includes generic firmware packaging/release-asset verification, hosted-memory patching, phone tooling/tests, .NET history/catalogue/benchmark/product-evidence utilities, and individual CAD/PCB classification. The tested specialist-exception allowlist follows that classification; existing Python is not automatically exempt.

## Migration boundaries

Manual imported-PEM identities and the draft's NVS-key-backed identities are not interchangeable with hardware-derived identity storage. Inspect actual flash layout, running slot, security state and key-slot availability before selecting a migration. Use Waveform offsets, not Tempo offsets. Preserve settings, recognition credentials, history and model data.

Application code must never program eFuses. Separately, Waveform's factory bootloader profile can permanently enable security on first boot. Record and approve that commissioning step explicitly; ordinary OTA cannot establish an arbitrary new partition/security layout. Do not flash a factory image as a shortcut.

No automatic erase, old-key import or destructive identity replacement is permitted on failure. Define re-enrollment, old-certificate revocation and compatible rollback before retiring manual provisioning. Keep public trust anchors distinct from untrusted delivered intermediates.

## Acceptance evidence

- Real SDK CSR/key/certificate checks and TLS 1.2/1.3 handshakes with persistent opaque keys reopened across processes; do not blindly copy Tempo's key-algorithm constant.
- Missing/incorrect hardware protection, corrupt storage, invalid/expired certificates, failed writes and failed shutdown preserve safe state.
- Monotonic retry deadlines withstand wall-clock changes; offline time is not a provider failure; server retry instructions and OTA cancellation are honored.
- .NET contract tests cover exact signed bytes, duplicate/unknown fields, replay, foreign-product denial and atomic nonce/state conflicts.
- Provider/recovery tests cover partial DNS/ACME failure, leases, retained certificates, secret rotation and safe logs.
- Live tests prove identity-scoped cross-product denial, private connectivity, trusted Safari/Chrome pairing, DHCP changes, renewal/outages and persistence.
- Physical load evidence includes audio/display/recognition alongside HTTPS and OTA, internal heap/largest block, sockets, task stack and two signed production OTA updates.

Host mocks do not prove flash power-loss atomicity, physical key protection or live private-network/RBAC behavior. Record exact tested commits and distinguish these evidence levels.

## Still to resolve

- Concrete .NET adapter/library/hosting choices and complete protocol fixture.
- Existing Azure resource inventory, authoritative shared-resource owner, inherited grants, DNS-provider credential scope and incremental cost.
- Device security state, factory HMAC slot, storage-format identifiers and migration/recovery procedure.
- User-selected Local mode: Tempo PR #55 was open at the reference review and requires a separate scope decision and completed integration. Do not silently add HTTP fallback.
- Staging-to-production CA activation and actual browser/device acceptance. A successful build or bootstrap workflow alone does not establish production trust.
