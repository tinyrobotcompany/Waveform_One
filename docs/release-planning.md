# Release planning

Release planning and changeset validation use Node.js 22 and its standard library.
No dependency installation is required:

```sh
node scripts/release.mjs check origin/main
node scripts/release.mjs plan --output .artifacts/release-plan.json
node --test scripts/tests/release*.test.mjs
```

Create `.artifacts/` before choosing it as the output directory. The planner uses
committed `HEAD`, reachable semantic version tags and immutable changesets. It
returns JSON `null` when there are no pending notes; otherwise the document
contains `tag`, `commit` and `notes`. The largest pending bump wins. Repeating
planning after tagging the same commit produces the same release document,
including the committed accuracy/cost disclosures and microphone privacy notice.
Working-tree edits never become release claims. Invalid notes or disclosures,
missing Git references and output errors fail the command.

The PR changeset job checks changes against the merge base. Firmware, backend, Pi,
protocol and automation changes require a new valid changeset; documentation-only
changes still validate release evidence. Existing changesets cannot be edited,
deleted or renamed.

Both planning calls in the release workflow use this same implementation. Release
publication continues to validate assets, resume only a draft for the exact
commit, verify downloaded assets and refresh the notes before publishing. These
hosted operations require their normal workflow validation; local planner tests
do not exercise GitHub publication or signing.

## Remaining Python migration

This change removes the Python release planner and its Python release/evidence
tests. P4 packaging, release-asset verification, production build controls and
their associated tests remain separate migration work. The shared validation
entry point includes `scripts/tests/*.test.mjs` alongside existing repository
Node tests and propagates failures from both suites. Other Python tooling remains subject to the approved language-policy
inventory; this release migration does not establish a permanent exception.
