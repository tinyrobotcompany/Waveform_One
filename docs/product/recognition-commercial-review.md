# Recognition providers and commercial readiness

Source review: 18 November 2026; firmware baseline 55e625c. This is an evidence
register and costing worksheet, not confirmation of contractual entitlement or
a customer subscription offer. No provider has been changed by this review.

## Reachable services by product variant

| Variant/path | Evidence | Status |
| --- | --- | --- |
| Current P4 | `main/recognition.cpp`: `paid=true`; `identify_paid()` posts audio to configured ACRCloud host `/v1/identify` | ACRCloud only; credentials retained in device NVS |
| P4 legacy Shazam function | Same file contains `amp.shazam.com/discovery/...`, but the constant paid branch selects ACRCloud | Inactive; must not be described as a fallback |
| Legacy Pi | `pi/recognition/worker.py` imports Shazam and calls `recognize`; requirements pin shazamio 0.8.1 | Separate legacy recognition path; not current P4 behavior |
| Desktop catalogue experiment | `tools/music_benchmark/catalogue_check.py` calls the legacy Shazam endpoint | Developer-only optional tool, not P4 runtime; not invoked by the new protocol |
| Metadata/artwork | P4 catalogue/artwork requests follow identification | Separate dependencies, not alternative recognition services |

Recheck this inventory whenever provider selection, build flags, packaging or
legacy support changes. Reachability here is established by source inspection;
network capture of each released image is still required. The direct legacy
Shazam path is not evidence of authorization to use Apple's official ShazamKit.

## Official commercial evidence

[ACRCloud Terms of Use](https://www.acrcloud.com/terms/), updated 3 November 2026:
section 13 makes customer-facing product/resale use conditional on explicit
permission in the plan, order or documentation. Section 17.2 places fees, quotas,
taxes and usage charges in the applicable plan/order. Section 3 restricts account
access and requires credential confidentiality. Section 14.1 addresses branding
requirements. Check the actual contract before deciding whether one company
account may serve a fleet; development access alone is insufficient evidence.

The [pricing URL](https://www.acrcloud.com/pricing/) redirects to the account
console. No account-specific rate, allowance or invoice was available during
this review. **Effective per-lookup price: unknown. OEM entitlement: unverified.**
No zero-cost or unlimited-service claim is justified.

[ACRCloud service guidance](https://docs.acrcloud.com/faq/service-usage) locates
usage statistics in project details and recommends recorded-audio mode for
microphone/noisy sources. Confirm the actual project setting and usage ledger;
the firmware does not establish account configuration.

[Apple's official ShazamKit](https://developer.apple.com/shazamkit/) describes
its SDK/service and token registration. It does not grant rights to the legacy
unofficial endpoint or establish its per-request cost. Legacy Pi commercial
clearance remains unverified. Spotify/Apple Music recognition API exploration
is deferred by the owner; do not introduce those services or switch providers.

## Private evidence to obtain

| Question | Evidence needed | State |
| --- | --- | --- |
| Hardware/OEM/customer-facing use | Applicable plan/order or written provider confirmation | Unknown |
| Shared fleet versus customer keys | Permitted deployment, device/user limits and key management | Unknown |
| Billing unit | Treatment of successful, no-match, invalid, failed and retried requests | Unknown |
| Pricing | Currency, monthly minimum, included units, tiers, overages, tax, concurrency | Unknown |
| Scale/partnership | Volume/OEM/referral quote and attribution conditions | Not requested |
| Service end | Suspension, termination, data return/deletion and support commitments | Contract-specific |
| Legacy Pi | Authorized service access, distribution and commercial terms | Unknown |

Keep invoices, contracts and credentials private. Publish approved derived rates
and assumptions, not secrets. Prepare a provider enquiry for owner approval;
do not send outreach merely because this worksheet exists.

## Cost model

Current P4 scheduling targets 2.504 seconds while active with a rolling 24/minute
budget (`recognition_task`). **1,440 requests per active hour is a conservative
configured ceiling**, not measured typical use and not a billing rate. Network
latency, classification, silence and backoff change actual counts.

For each scenario let H = listening hours/day, D = days/month, R = measured
requests/listening hour, and N = active devices. Fleet requests Q = H × D × R × N.
Add measured non-listening requests separately. At the ceiling, 30 days gives
86,400 requests/device/month for 2 hours/day and 345,600 for 8 hours/day. These
are stress scenarios, not predictions. Retries already included in measured R
must not be added again.

For a simple monthly plan with fee F, included requests I and overage P per
1,000 billable requests: cost = F + max(0, billable_Q − I) × P / 1,000.
Use this formula ONLY if the contract actually uses that model. Tiered plans,
per-project allowances and different billing units require their own model.
Effective per-request cost = actual attributable invoice cost / billed requests;
cost/device-month = attributable fleet cost / active devices. Mark zero-use
periods undefined for per-request cost. Reconcile device counts to provider units
and record the reconciliation window, currency and tax treatment.

Before choosing retail price compare a hardware-funded allowance over an
explicit service horizon, optional subscription, and owner-supplied ACRCloud
credentials. Model heavy usage, growth, provider price increases and support
costs. Do not promise lifetime cloud recognition funded by a one-time hardware
sale without a reserve model. Own keys transfer billing responsibility but do
not protect against provider shutdown. Request optimization must pass the
[recognition protocol](../validation/recognition-protocol.md) before adoption.

## Required release-note disclosure

Every release must name active recognition services by variant, source/terms
review date, measured request usage or explicit unknown status, verified rate
and assumptions or **price not verified**, and the actual customer allowance/
subscription policy or **commercial offering not finalized**. Link the evidence
report; do not copy stale measured figures as if freshly tested. Privacy remains
a separate mandatory notice. Release-generator enforcement is pending follow-up
work coordinated with PR #23, not implemented by this document.
