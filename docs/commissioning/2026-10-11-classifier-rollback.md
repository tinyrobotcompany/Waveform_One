# Targeted music-classifier rollback — 11 October 2026

> Superseded: this partial rollback was insufficient. See the [complete pre-filter rollback](2026-10-11-complete-filter-rollback.md). The retained experimental paths described below are no longer the active implementation.

The user requested reverting only the music-versus-speech filter, retaining all
other improvements. Work remains in `feature/p4-recognition-stability`, worktree
`.worktrees/p4-recognition-stability`. `main` and `origin/main` are both
`0a25902733f5cd35177a0d46342e2c8fac396ce8`; classifier commit `92103a6` is not an
ancestor of main. The feature branch was deployed directly to the hardware.

## Evidence and scope

The announced repeat jazz test correctly identified Take Five / Dave Brubeck.
Playback began at 395.276 seconds on its log clock; activity started at 400.636,
the correct match arrived at 402.570 and artwork was ready at 402.969. There was
no activity interruption during playback. Activity ended at 419.098, 5.585
seconds after playback ended. This did not meet the five-second target. The
preceding announced rock test needed approximately 16 seconds to match and had
weak classifier evidence during playback. Earlier runs contaminated by speech
must not be treated as controlled silence tests.

The rollback removes the classifier task, classifier window feeding, model
inference, confidence gating and classifier readiness requirement from the S3
application. Its build excludes `music_detector.cpp` and `music_features.cpp`.
The existing calibrated spectral gate again directly drives LED frames. P4
activity opens on the first active half-second report and closes after three
quiet reports, matching the policy before the classifier. The existing
`WF1 MUSIC` activity protocol is retained for compatibility; its name does not
indicate that speech is being classified. Capture history clears only on a
confirmed activity stop, not a brief quiet beat.

The UI, phone remote, fonts, search animation, rolling raw capture, compact PCM,
fingerprint fixes, bounded completion handling, retry cooldowns and Wi-Fi RX
ownership fix remain. Classifier source, model partition and historical tests
remain available for future work, but no classifier runs in this S3 application.
Saved configuration, partition layout and model data are not erased.

## Validation

Full firmware host tests passed with ASan/UBSan, including a regression test for
immediate sound-gate entry, three-report exit, quiet-beat retention and calibration.
ESP-IDF 6.1 S3 build passed. The linked map contains no music-detector or model
inference symbols.

S3 rollback application SHA-256:
`0f39da5e58b9b2d5ad8df79f6dae71faaec5a131c4851ea02e91310581973c80`.

Production P4 application (development bridge OFF), retaining recognition/UI fixes:
`d1d4a1dc076a3c57401bd4fa11c0c199fc1c8bbbe2d7c7e785db725142e83368`.

Both applications were flashed app-only and their written hashes verified.
The production P4 startup trace showed the S3 connected in Mirrored mode,
live sound-gate activity and the Wi-Fi phone remote ready. No classifier trace,
maintenance key or fatal/reboot marker appeared during the 35-second check.
The temporary maintenance port (8765) refused connections after deployment.

The announced startup check did **not** pass its quiet-state assertion: activity
was Playing throughout the observation. The sound gate can react to ambient
sound now that classification is disabled. No controlled music fixture was
played. Two no-match requests occurred and the service returned HTTP 429 on the
fallback; the worker entered its failure retry schedule. This is not evidence
of successful song recognition or restored five-second timing.

Recognition timing after rollback is not yet established. Speech/background
noise can once again activate the system; restored latency and stability require
an announced controlled audio test before any claim of improvement.
