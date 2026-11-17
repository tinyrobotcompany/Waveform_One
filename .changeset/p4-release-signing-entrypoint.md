---
"waveform-one": patch
---

Fix the P4 release-signing command's temporary key creation so validated releases
can be signed and published. Test the actual CI command with a temporary publisher
key, verify the resulting signature, and check owner-only permissions and cleanup
on signing failure.
