---
"waveform-one": patch
---
Make the Codex review runner fail on incomplete or malformed output, provider errors,
and Critical/High findings. Reject oversized diffs instead of truncating them, and
reuse only validated nonblocking reviews tied to the exact PR head. No fallback
review can produce a passing result. The workflow invokes the runner without a
successful missing-credential skip, so an absent review key fails the check.
