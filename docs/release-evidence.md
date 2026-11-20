## Recognition accuracy

**Not benchmarked for this release.** No validated recording-accuracy percentage,
false-positive rate, acquisition/transition latency or requests-per-listening-hour
measurement is available for this release. Host tests and successful API requests
are not evidence of acoustic recognition accuracy. Previous informal listening
observations must not be treated as a repeatable benchmark.

The planned benchmark separates recording identity from album/edition identity,
includes quiet/noisy conditions, transitions and non-music trials, and reports
sample counts, failures, timing and uncertainty. Results must identify the tested
firmware commit, protocol/corpus version and date. See the
[buyer-readiness tracker](https://github.com/tinyrobotcompany/Waveform_One/issues/25)
for protocol, evidence and outstanding hardware testing.

## Recognition service costs

Last source/terms review: **18 November 2026**, firmware baseline **55e625c**.
That audit found **ACRCloud only on the current P4**, with its legacy Shazam branch
inactive. **Legacy Pi software uses ShazamIO** and requires separate commercial
clearance; it is not a P4 fallback. Provider selection must be re-audited when
recognition or build configuration changes.

**Per-lookup price and customer-facing/OEM entitlement have not been verified
against the account's plan/order.** Measured billable use is not available for
this release. The audited P4 code limits active requests to 24/minute (up to
1,440/active hour); this is a configured ceiling, not typical usage or a bill.
ACRCloud's [terms](https://www.acrcloud.com/terms/) make customer-facing use
dependent on explicit plan/order/documentation permission and place pricing in
the applicable commercial agreement. Unofficial legacy endpoint access must not
be presented as a free commercial entitlement.

**Commercial offering not finalized:** no included recognition allowance,
optional subscription price or lifetime cloud-service commitment is established
by this release. Provider availability is required for new cloud identifications.
Future published cost figures must state currency, evidence date, billing unit,
included allowance, overages and usage assumptions without exposing credentials
or confidential account documents.
