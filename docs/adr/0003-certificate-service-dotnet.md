# ADR 0003: Implement the certificate service in .NET 10

Status: Accepted by Simon, 28 November 2026. Service implementation is not yet present.

## Context

Tempo main at `4eae79b34c7848be7ba871e0e82f44fc47b14fa1` contains a Python/FastAPI certificate service. Simon explicitly selected .NET 10 for Waveform and stated that Tempo's service will also be rewritten. That statement is a direction, not evidence that Tempo's rewrite exists or authorization for this repository to change Tempo.

## Decision

Implement Waveform's service in .NET 10. Use the pinned Tempo service as a behavior and test reference, not a runtime dependency. Keep protocol/application policy separate from Azure Table, Key Vault, DNS and ACME adapters. Choose concrete libraries and hosting through focused compatibility and cost evaluation before implementation; no package or hosting choice is implicitly approved here.

Preserve strict signed-request validation, manufacturing admission, atomic nonce consumption and work scheduling, independent DNS and issuance processing, bounded retries, durable recovery, certificate verification, secret-cache expiry and redacted logs. Device private keys must never enter the service.

Waveform owns its API, worker, factory admission and deployment artifacts. The shared Azure account/vault decision does not turn the service into Tempo's application or a shared-product dispatcher. Product signing contexts, namespaces, tables and identities remain separate.

## Alternatives and consequences

- Adapting Python would minimize translation and reuse more existing tests directly. It was considered and superseded by the explicit .NET decision.
- A rewrite aligns the backend stack but requires new proof of cryptographic encoding, ACME behavior, Table transaction semantics, leases and recovery. Passing Python tests cannot certify the .NET implementation.
- New repository automation uses shell/Node. General-purpose existing Python must also be replaced under [ADR 0005](0005-language-policy-and-python-migration.md); only specifically justified specialist tooling remains.

## Validation and review triggers

Approve exact wire bytes and response semantics before implementing client/server independently. Use public synthetic vectors and ephemeral test keys; test both accepted requests and replay/tampering/foreign-product denial. Add adapter and recovery tests plus an end-to-end device enrollment/renewal sequence. Live Azure/provider/browser acceptance remains a separate gate.

Revisit library or hosting choices if protocol fidelity, maintained dependencies, licensing or measured operational cost cannot satisfy the acceptance criteria. Do not switch languages implicitly.
