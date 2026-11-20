# Product evidence tooling

These tools prepare reports; they do not manufacture measurements or contact a
recognition service. Run from the repository root with Python 3.10 or newer.
Use the trial CSV/protocol in PR #26 and keep tuning/repeated trials separate.

```sh
python3 tools/product_validation/report.py benchmark trials.csv run.json > report.json
python3 tools/product_validation/report.py cost plan.json > scenario.json
python3 tools/product_validation/evidence.py lookup-usage redacted-device.log > usage.json
python3 tools/product_validation/evidence.py offline-plan FORTY_CHARACTER_COMMIT_SHA > offline.json
python3 tools/product_validation/evidence.py verify-offline offline.json > offline-evidence.json
python3 tools/product_validation/evidence.py recovery-manifest firmware/esp32/p4_display/build > recovery.json
```

A run.json requires firmware_sha (40 lowercase hex characters), image_sha256
(64), corpus_version, protocol_version, tested_utc, hardware, acoustic_setup,
provider and evidence_path. Use no account credentials or private project IDs.
Trial types: acquisition, transition, mid_song, repeat, silence, non_music.
Empty numeric cells mean unknown. Request count and observation duration are
required; wrong-answer flags are true/false or 1/0. Duplicate IDs, mixed runs,
unknown outcomes and invalid measurements fail. An empty template cannot produce
an accuracy claim. Results are grouped by condition and trial type; repeats
are not added to primary denominators. Wilson intervals appear only for distinct
recordings within a group. No pooled-condition independence is assumed. p50/p95
use linear interpolation, successful timing sample counts, and explicit missing/
timeout counts. Event-level non-music false-positive counts and independently
labelled album/edition accuracy are not inferred from inadequate CSV fields.

Plan inputs: currency, monthly_fee, included_units, overage_per_1000, devices,
hours_per_day, days, requests_per_hour, usage_basis (measured/configured_ceiling/
assumption), terms_verified. This is only a shared monthly allowance plus flat
overage scenario, not a universal provider tariff. Enter actual contract figures
privately; never substitute invented values for an account quote. Device log
attempt counts include errors/retries but do not establish billable units.
Reconcile them with the provider ledger before publishing costs.

Offline-plan emits every case as not_run. Fill each case with measured duration,
pass/fail and a nonempty local evidence file after an actual hardware trial.
Verify-offline validates inventory and hashes, not the truth of the operator's
attestation. Use the detailed offline protocol in PR #26. No physical result is
included with this tooling. Keep raw audio, household speech and account secrets
out of public evidence. Attach a permitted demo video and exact firmware/image
hashes before making the public offline guarantee.

Release integration: PR #27 reads committed release evidence. Review the generated
report, commit the actual report and link it from the release's recognition/cost
evidence. Never use a report from another firmware SHA as current-release stats.
If no run exists, keep “not benchmarked for this release” and “cost not verified”.
