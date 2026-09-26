Boundary discipline:
- Check dependency direction: policy/core should not depend on delivery/framework/infrastructure details.
- Verify adapters translate external formats cleanly without leaking them into core rules.

Design integrity:
- Identify architecture drift, mixed responsibilities, and coupling across layers.
- Validate interfaces/contracts are explicit and stable for changed boundaries.

Operational architecture concerns:
- Review migrations/deploy sequencing and failure modes for safe rollout.
- Prefer incremental, reversible changes when boundaries or contracts move.
