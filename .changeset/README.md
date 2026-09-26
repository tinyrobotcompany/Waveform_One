# Changesets

For firmware, Pi, protocol or automation changes, add a uniquely named Markdown file:

```md
---
"waveform-one": patch
---
Explain the user-visible change and any installation requirements.
```

Use `patch` for fixes, `minor` for additions, `major` for incompatible changes.
The largest pending bump determines the next version, starting from `0.0.0`.
Documentation-only changes do not need a changeset.

Files are immutable once merged: add a correction in a new file. Keep old files;
the release script uses reachable `vX.Y.Z` Git tags to determine which are new.
Do not hand-create version tags: the validated main workflow owns them.

This uses the template's changeset format with a small Python release runner,
not the npm Changesets CLI. There is no Node package to version or publish.
