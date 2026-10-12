# Silent benchmark — 12 October 2026

> This first stage was network-free and did not verify song identification. The separate
> [catalogue check](2026-10-12-catalogue-check.md) now records five four-second song matches;
> device timing remains unmeasured.

Album-colour prototype committed and pushed as `29f8bcd` on
`feature/p4-recognition-stability`. Work here is isolated in the stacked
`feature/music-recognition-benchmark` worktree, based on that commit because main
has not received the P4 feature work. No merge, deployment or device-code change.

No laptop audio playback, microphone access, serial access, flashing or recognition-service
request occurred. Existing public source previews were decoded into fresh PCM WAV files.
All generated files live under the project's ignored `build/music-recognition-benchmark/`.

## Recognition input-duration experiment

| Input seconds | Cases | Median MacBook CPU ms | Median peaks | Median audio used by signature (s) | Catalogue accuracy |
|---|---|---|---|---|---|
| 2 | 15 | 3.15 | 73 | 2.000 | Not tested |
| 3 | 15 | 4.73 | 117 | 3.000 | Not tested |
| 4 | 15 | 6.37 | 159 | 4.000 | Not tested |
| 6 | 15 | 9.53 | 254 | 6.000 | Not tested |
| 8 | 15 | 9.76 | 255 | 6.064 | Not tested |

All 75 cases produced deterministic signatures. All fifteen eight-second adapted outputs
matched the production implementation exactly. Only the input-length guard changes in the
generated experimental source; device capture, FFT, gain, peak selection and encoding remain
unchanged. Four-second signatures contained fewer peaks than the eight-second baseline.
These are viable experimental inputs, not evidence that short clips identify songs reliably.
Desktop fingerprint processing is a few milliseconds; live collection and service timing
still dominate the unmeasured end-to-end budget. No five-second success claim is made.

## Reference music classifier

713 overlapping windows cover five music previews, speech, six real environmental noises,
synthetic silence/white noise, three attenuated music cases, nine speech/music mixtures, and
two music-to-non-music transitions. The isolated native YAMNet reference uses the pinned model
from `92103a6`; its presence in generated build artifacts does not reintroduce it into firmware.

| Case | Ground truth | Active windows | First activation (s of input) | State exits |
|---|---|---|---|---|
| beyonce | music | 59/60 | 1.462 | 0 |
| wicked | music | 59/60 | 1.462 | 0 |
| jazz | music | 59/60 | 1.462 | 0 |
| classical | music | 59/60 | 1.462 | 0 |
| rock | music | 59/60 | 1.462 | 0 |
| speech | non_music | 0/7 | - | 0 |
| vacuum_cleaner | non_music | 0/9 | - | 0 |
| washing_machine | non_music | 0/9 | - | 0 |
| keyboard_typing | non_music | 0/9 | - | 0 |
| door_wood_knock | non_music | 0/9 | - | 0 |
| engine | non_music | 0/9 | - | 0 |
| rain | non_music | 0/9 | - | 0 |
| silence | non_music | 0/15 | - | 0 |
| white_noise | non_music | 0/15 | - | 0 |
| jazz_quiet | music | 22/23 | 1.462 | 0 |
| jazz_speech_-6 | music | 22/23 | 1.462 | 0 |
| jazz_speech_0 | music | 18/23 | 3.413 | 0 |
| jazz_speech_6 | music | 15/23 | 4.875 | 0 |
| classical_quiet | music | 22/23 | 1.462 | 0 |
| classical_speech_-6 | music | 22/23 | 1.462 | 0 |
| classical_speech_0 | music | 19/23 | 2.925 | 0 |
| classical_speech_6 | music | 15/23 | 4.875 | 0 |
| rock_quiet | music | 22/23 | 1.462 | 0 |
| rock_speech_-6 | music | 16/23 | 4.388 | 0 |
| rock_speech_0 | music | 11/23 | 6.825 | 0 |
| rock_speech_6 | music | 7/23 | 8.775 | 0 |
| jazz_then_speech | transition | 15/23 | 1.462 | 1 |
| jazz_then_silence | transition | 12/23 | 1.462 | 1 |

The five clean music excerpts entered at 1.4625 seconds and had no later exits. One initial
window is deliberately insufficient to enter. All nine non-music conditions remained inactive
in this small sample. Attenuated clean music is not the same as quiet music in room noise.

Constructed speech mixtures exposed the remaining problem: rock plus speech took 4.3875,
6.825 and 8.775 seconds to enter at -6/0/+6 dB speech-to-music RMS. Smoothing stabilizes decisions
after entry but cannot manufacture music evidence that the model fails to supply initially.
This reference policy is not suitable for deployment as the final filter.

Music-to-speech exited 2.775 seconds after the synthetic boundary; music-to-silence exited
1.3125 seconds after it. The windowed silence result does not meet a prompt silence response:
a future device design needs a separate high-frequency silence detector while retaining raw
capture and album state independently of short classifier uncertainty.

## Verification and interpretation

The five benchmark regression tests cover bounded uncertainty retention, speech/uncertainty
interruptions, fresh entry after silence, RMS mixing/no clipping and invalid WAV formats.
The full repository automated suite passed, including Node, Python, firmware ASan/UBSan
and Rust checks; the run is recorded in `repository-tests.log`.
No firmware sources differ from the album-colour parent.

Reference classifier host pipeline median: 82.675 ms per window, including process launch and
model initialization. This is not a P4/S3 inference benchmark. Ground truth comes from source
metadata and constructed mixtures, not a validated independent holdout. Real microphone SNR,
room speech/TV, broader genres, embedded scheduling and catalogue-match accuracy remain open.

Next: controlled public-fixture catalogue checks for shorter signatures, broader held-out
music/noise/mixture data, and a classifier candidate that recognizes music under speech sooner.
Only after these pass should we change the capture path or introduce a device filter. Keep the
current pre-filter firmware as the recovery baseline and announce any future hardware/audio test.

Sources are recorded with checksums in the local manifests. Non-music data includes
[TensorFlow public speech](https://raw.githubusercontent.com/tensorflow/tflite-support/master/tensorflow_lite_support/cc/test/testdata/task/audio/speech.wav)
and [ESC-50](https://github.com/karolpiczak/ESC-50). Model/feature/TFLM provenance and exact binary hashes
are recorded in `results/results.json`.
