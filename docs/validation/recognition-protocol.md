# Recognition acceptance protocol v1

Status: protocol only; no benchmark results have been collected under this
protocol. Manual observations and request-success logs are not accuracy figures.
Owner tracking: [buyer readiness #25](https://github.com/tinyrobotcompany/Waveform_One/issues/25).

The existing `tools/music_benchmark` utilities test desktop fingerprinting and
classification; they are not an end-to-end ACRCloud accuracy harness. Do not run
its optional legacy catalogue lookup as part of this protocol: that uses a
different provider. Reuse relevant offline fixtures without substituting their
results for physical P4 trials.

## Freeze the experiment

Before each run record firmware commit and image SHA-256, build configuration
(without credentials), P4/PCB/microphone/LED revisions, provider/project type,
protocol and corpus versions, UTC date, operator, speaker/source, room, distance,
measured music and background SPL, network latency and failure conditions.
Record whether the device was cold-booted and whether metadata caches were warm.
Use the same setup and playback for baseline/candidate comparisons. Do not change
recognition cadence or tune thresholds against the evaluation set.

Proposed initial corpus: 100 distinct recordings, stratified across at least ten
genre/production groups, including quiet intros, instrumental/acoustic material,
live recordings, remasters and covers. Reserve a separate 20-recording tuning
set; exclude it from reported evaluation. Freeze identifiers, expected artist,
recording/version, expected release where unambiguous, playback offsets, lawful
source and exclusions before testing. An album/edition is not always uniquely
inferable from audio: report that separately from recording identity.

Run every evaluation recording at three predeclared acoustic conditions:
near/quiet (1 m, music 65 dBA), far/quiet (3 m, music 55 dBA), and near/noisy
(1 m, music 65 dBA, background 55 dBA). Measure SPL at the microphone; document
room noise and meter accuracy. These are proposed lab conditions, not promised
operating limits. If the setup cannot reproduce them, publish actual conditions
and use a new corpus/run configuration identifier. Randomize order with a saved
seed. Repeat 20 preselected recordings three times per condition to assess
repeatability, reporting these separately from the 300 primary trials.

Add 30 full-song transitions (including gapless albums), 30 mid-song changes,
and 30 stops to silence. Add sixty one-minute non-music trials divided among
speech, ambient noise and silence. Include a separately reported out-of-catalogue
challenge set; an operator's unfamiliarity does not establish catalogue absence.
Select catalogue-known music before testing, not by excluding failed trials.

## Execution and observations

1. Synchronize playback markers, device monotonic logs and screen video. Record
   timing alignment error. Allow 15 seconds of silence before each acquisition
   trial; confirm prior title has cleared and save the initial state.
2. Start playback at the manifest offset; observe for a fixed 30-second window.
   Record the first displayed answer, every correction, first correct answer
   and first correct answer remaining stable for five seconds. Observe through
   35 seconds to verify stability of answers first shown at the deadline.
3. Record failures and timeouts; never silently rerun a failed trial. Any rerun
   receives a new ID and an explicit reason, and the original remains in data.
4. For transitions, start the timer at the audible change, not at provider
   response time. For silence, measure time until the displayed song clears
   and LEDs settle, separately. Include all eligible trials in denominators.
5. Count actual HTTP identification attempts, including retries/no-match/errors,
   from device diagnostics. Reconcile a run's isolated provider project usage
   window before treating those counts as billed units. Exclude unrelated
   devices or report that reconciliation is unavailable.
6. Review labels against ground truth independently of the provider response.
   Record ambiguous versions/editions explicitly. Keep raw audio out of logs;
   publish manifests, metadata, timing and redacted evidence, not copyrighted
   music recordings or household speech. Use staged demo audio with permission.

Use [recognition-trials.csv](recognition-trials.csv) for one row per trial.
The empty template contains no synthetic results. Store detailed answer events
in a companion CSV with trial_id, elapsed_ms, artist, recording_id, album_id and
judgement. Missing timestamps are empty, never zero. Outcome values are correct,
wrong, no_match, or service_error, determined by the answer displayed at the
30-second deadline; any earlier wrong answer is also recorded independently.
Use a separate non_music trial_type and mark its outcome wrong if any song is
shown. Retain failure subtypes (network, quota, HTTP, provider, audio capture).

## Required published metrics

| Metric | Definition |
| --- | --- |
| End-to-end recording accuracy | Correct at deadline / all eligible music trials, including service failures |
| Returned-answer precision | Correct at deadline / trials with an answer at deadline |
| Wrong / no-match / service-error rate | Each outcome count / all eligible music trials; the four outcomes sum to the music denominator |
| Transient wrong-answer rate | Music trials with any wrong answer / all eligible music trials |
| Non-music false positives | Non-music trials with any song displayed / all non-music trials; also report events per non-music hour |
| Acquisition / change latency | p50/p95 first-correct and stable-correct latency, with successful sample count and timeout count; never assign timeout zero |
| Album/edition accuracy | Correct / independently labelled unambiguous release trials; report ambiguous exclusions |
| Quiet release | p50/p95 display-clear and LED-settle latency with timeout count |
| Usage | All identification requests / music playback hours; separately requests / correct trial and / total elapsed hour |

Report per-condition results and totals, with denominators. Use 95% Wilson
intervals for proportions on distinct primary trials; repeated tracks are not
independent extra songs. For pooled conditions, use a recording-cluster bootstrap
or report condition-specific intervals rather than claiming 300 independent
recordings. Latencies need sample counts, method and uncertainty; p95 from a
small successful subset is not a worst-case service guarantee.

One hundred independent songs gives a worst-case approximate 95% margin of
error of ten percentage points, not a precise marketing percentage. Roughly
385 independent trials are needed for a worst-case five-point margin; acoustic
repeats do not replace catalogue diversity. Expand the corpus before making a
broader accuracy claim. Freeze regression acceptance margins after the baseline
and before evaluating optimizations; compare paired tracks, not unrelated runs.

## Release evidence contract

Every release must identify its tested commit, protocol/corpus, test date,
denominators, accuracy/false positives, latency and request usage, with a link to
the report. If no matching benchmark exists, say **Not benchmarked for this
release**. Historical observations must be clearly dated and must not become
current-release statistics. Automated host tests validate code behavior, not
microphone recognition accuracy. Release-note enforcement is a separate change
coordinated with privacy PR #23; this protocol does not claim it is implemented.
