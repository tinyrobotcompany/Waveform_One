# ADR 0002: C++ application firmware with C dependency boundaries

Status: Accepted by Simon, 28 November 2026. Implementation policy; no SDK or device change is established by this record.

## Context

Waveform coordinates continuous audio, display, recognition, phone authorization and OTA. Tempo's C certificate implementation is a behavioral reference. Copying its language is not a requirement. The existing P4 application already uses C++ while vendor libraries and generated fonts use C.

## Decision

Use C++ for application-owned P4 modules. Retain upstream C libraries and generated C assets in their existing language. Maintain explicit C-linkage boundaries where required. Prefer small modules, explicit error returns, bounded allocations and scope-bound resource cleanup. Do not enable exceptions or RTTI without a separately justified need.

Keep the application language subset compatible with C++17 host tests. The SDK alignment work must explicitly verify the target compiler configuration; this decision does not claim that firmware currently passes an explicit C++17 compiler flag.

Adapt Tempo's identity/storage/enrollment behavior with equivalence and negative-path tests. Preserve Waveform's authorization, TLS lifecycle and OTA/network coordination. Do not combine this with a broad object-oriented rewrite.

## Alternatives and consequences

- A C rewrite would simplify textual similarity with Tempo but discard working Waveform resource-management and type boundaries without a demonstrated runtime benefit.
- An unchanged C component behind a C interface remains appropriate for genuinely upstream-maintained dependencies. These identity modules require product-specific integration and are application code.
- C++ permits automatic cleanup but does not establish memory safety. Sanitizers, concurrency tests, allocation bounds and actual firmware measurements remain required.

## Validation and review triggers

Run strict host tests and relevant sanitizer coverage, firmware builds and real SDK TLS handshakes. Measure image size, heap and stack headroom on hardware. Revisit if compiler compatibility, measured resource use or a separately maintained upstream component changes the tradeoff.
