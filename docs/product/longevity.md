# Ownership and service continuity

Status: source-based dependency assessment, 18 November 2026. Physical offline
acceptance and independent rebuild/recovery validation are outstanding. The
[firmware repository](https://github.com/tinyrobotcompany/Waveform_One) is public;
public visibility is not itself a license audit or a permanent hosting guarantee.

| Feature | Dependency | If the service/company disappears |
| --- | --- | --- |
| Microphone-to-LED visualization | Local P4 capture, classifier, LED driver; current startup also requires provisioned recognition configuration | Designed around local processing; cold-start and no-credentials acceptance pending |
| Touchscreen styles/settings | Local firmware and saved configuration | No company server required by these paths; verify offline responsiveness |
| Phone remote | Device LAN server, network address, pairing and usable expiry clock | No company relay; no-WAN clock/session restoration must be tested |
| Song identification | ACRCloud credentials and reachable provider | New identifications unavailable; own keys help only while provider remains available |
| Album information/artwork | External catalogue/image services and local caches | Uncached enrichment unavailable; cached content is not an availability guarantee |
| Correct wall clock | Valid time source/state | Offline cold-start accuracy is not guaranteed |
| OTA updates | GitHub release hosting and trusted signing key | Installed firmware remains, but new official downloads cease if hosting/publishing stops |
| USB repair/community firmware | Public source, dependencies, toolchain, physical access and recovery instructions | Candidate continuity route; independent build/flash must be demonstrated |

Do not promise perpetual cloud identification. Do not bypass OTA signature
verification to solve publisher disappearance, and never distribute the company
private signing key. Document an owner-controlled USB recovery/build route and
any hardware secure-boot restrictions before making community-firmware claims.

## Acceptance work

- [ ] Complete [offline acceptance](../validation/offline-visualizer.md), including
  missing credentials on a spare device, and publish a consented demonstration.
- [ ] Inventory software/model/dependency licenses and redistribution rights;
  distinguish public source availability from an open-source licensing claim.
- [ ] Verify a clean independent build with pinned tools/dependencies; archive
  distributable source, build instructions, binaries and checksums outside a
  single hosting account, without credentials or private signing material.
- [ ] Validate USB recovery and owner-controlled firmware on representative
  hardware with settings-retention/reset behavior documented.
- [ ] Design owner-supplied ACRCloud configuration with secret-safe validation,
  replacement and removal, and OTA persistence. Not currently a shipped UI.
- [ ] Describe honest offline/error states and avoid indefinitely showing old
  song metadata as a current recognition result.

Physical mute covers BOTH the existing prototype retrofit and the production
PCB. Its electrical design must disable microphone capture independently of
software; buffered audio and pending requests need separate firmware handling.
No circuit or pin assignment is approved by this document. Hardware inspection
and bench validation precede implementation. While muted, microphone-reactive
visualization cannot continue from that microphone.

Evidence coverage: current P4 recognition/LED paths, legacy Pi adapter, OTA
documentation and GitHub repository visibility were inspected. Schematic-level
mute verification, hardware fault tests, dependency-license clearance and an
independent rebuild were not performed. Keep those limitations visible in
customer-facing claims until acceptance evidence is available.
