Readability and intent:
- Prefer clear naming and direct control flow over cleverness.
- Keep diffs focused and responsibilities narrow.

Correctness and reliability:
- Ensure error paths are explicit and actionable.
- Flag hidden side effects, unclear state transitions, and fragile branching.

Testing and maintainability:
- Expect tests that prove behavior, not only happy paths.
- Call out missing edge-case coverage for changed logic.
- Prefer minimal, safe fixes over broad refactors unless necessary.
