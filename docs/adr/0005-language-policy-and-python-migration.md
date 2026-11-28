# ADR 0005: Replace general-purpose Python and allow specialist toolchains explicitly

Status: Accepted by Simon, 28 November 2026. Migration is required and not yet complete.

## Context

Simon rejected retaining Python simply because scripts already exist. The repository contains release/build orchestration, validation utilities, model tooling, CAD and PCB integrations in Python. These responsibilities have different appropriate implementations. Rewriting a supported vendor tool or replacing an entire CAD/model ecosystem solely to change its language would increase risk without satisfying the intent.

## Decision

| Responsibility | Implementation |
| --- | --- |
| Certificate backend and substantial desktop utilities | C# / .NET 10 |
| Repository controls, release computation, packaging and artifact checks | JavaScript `.mjs` on Node.js; standard library where practical |
| Short environment and command wrappers | Shell; move substantial logic into Node |
| Application firmware and native algorithms/tests | C++ with upstream/generated C boundaries |
| Tests for migrated tools | Node test runner or .NET tests, matching the implementation |

Do not create or extend general-purpose Python. Replace existing implementations in coherent slices with their tests and callers. A wrapper is not exempt merely because it invokes an ESP utility: production-build and signing orchestration belongs in Node/shell while it calls the official vendor tools.

## Approved specialist exceptions

1. Official, pinned Espressif SDK tools: `idf.py`, `esptool`, `espsecure`, NVS generation and their required toolchain dependencies. Invoke supported tools; do not reimplement cryptographic signing or flash formats.
2. Offline model preparation, numerical reference evaluation and directly related tests using LiteRT/NumPy/SciPy. The current candidates are `tools/music_detection/prepare_model.py`, `evaluate.py` and `verify_frontend.py`. General reporting, service calls and repository orchestration are not included in this exception.
3. build123d enclosure geometry and directly related CAD validation/tests. Keep the exception limited to the existing CAD workflow; generic release, export orchestration or unrelated utilities require individual classification.

KiCad integrations require individual review, not a directory-wide exemption. Prefer supported CLI operations from Node/shell where sufficient. Existing `pcbnew` SWIG bindings are deprecated; evaluate the supported IPC interface and coverage before changing board-generation/routing behavior. No automatic approval for new legacy-binding code.

Record the final accepted paths/tools in a tested exception allowlist. Vendor code belongs in pinned, ignored toolchain directories, not newly vendored source. Gate tests must reject an unapproved Python source and an unapproved Python invocation without breaking explicit vendor/model/CAD exceptions. The allowlist does not make remaining legacy Python compliant by grandfathering it.

## Migration inventory and sequence

1. Finish SDK alignment with a Node replacement for `scripts/build-production.py`. Port its configuration, layout, permission and signature-verification tests to Node. Remove the replaced Python entry point and its replaced tests; preserve all protections and update active callers.
2. Replace release/CI/package/verification scripts and their tests in focused slices: `release.py`, `ci-scope.py`, `package-p4-update.py`, `package-firmware.py`, `verify-release-assets.py` and `patch-hosted-memory.py`. Update workflows and the shared validation entry point with each slice. Retire an unused tool only after proving that it is unused and documenting the removed capability.
3. Replace general phone HTTPS checks and fixtures with Node or .NET according to responsibility. Retire laptop private-key provisioning only when the managed-identity migration has a supported recovery path; do not remove today's recovery capability prematurely.
4. Move history bridge, catalogue checks, benchmark orchestration and product-evidence/report utilities to .NET 10. Keep native signal-processing execution in C++. Preserve numerical/report outputs with reference fixtures and explicit tolerances where exact equivalence is inappropriate.
5. Classify each CAD/PCB/model file, enforce the final allowlist and remove obsolete runtime/test dependencies and documentation references. Full compliance is established only when all non-exempt Python is removed and all supported entry points are verified.

Temporary coexistence during a tested migration is not a permanent exception. Every remaining non-exempt source needs an assigned replacement/removal task; do not disable its tests to make a gate pass. Do not introduce a nominal Node/.NET entry point that secretly delegates the application implementation to the old Python file.

## Alternatives and consequences

- Keeping all existing Python was rejected by Simon; convenience is insufficient justification.
- Replacing everything with C# would unnecessarily add .NET startup/build requirements to small repository controls already supported by Node.
- Replacing official ESP tools or build123d/model libraries would be a separate toolchain change requiring evidence, not a routine language port.
- Migration adds work and regression risk. Preserve signing verification, protected files, deterministic artifact formats, error exits and all test coverage. Remove old implementations only after their replacements pass acceptance.

## Validation and review triggers

Use failing regression tests first, compare representative old/new outputs and failure behavior, then run the shared full validation command. No live credentials or hardware actions belong in ordinary unit tests. Record firmware builds and actual flashing separately. Review any new Python exception explicitly, including its toolchain rationale, path scope, owner and supported version.

## Sources

- [Official ESP-IDF frontend](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-py.html)
- [Google model-conversion guidance](https://developers.google.com/edge/litert/conversion/tensorflow/convert_tf)
- [build123d](https://build123d.readthedocs.io/en/stable/)
- [KiCad binding deprecation and IPC direction](https://dev-docs.kicad.org/en/apis-and-binding/pcbnew/index.html)

Repository inventory and official guidance reviewed 28 November 2026; this record does not establish migrated code or toolchain execution evidence.
