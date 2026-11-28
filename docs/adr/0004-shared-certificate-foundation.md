# ADR 0004: Share Azure foundations with separate product applications

Status: Accepted direction by Simon, 28 November 2026. Exact existing-resource mapping, cost evidence and delivery ownership remain prerequisites to infrastructure implementation. No deployment is authorized by this ADR.

## Context

Waveform needs automatic certificate issuance and renewal without duplicating the entire Tempo foundation before customer revenue. Simon approved sharing the existing foundation, including the Storage account and Key Vault. Separate accounts/vaults would simplify isolation but are deferred as a deliberate budget tradeoff.

## Decision

Reuse suitable existing networking, Container Apps environment, registry, monitoring, Storage account and Key Vault after read-only inventory verifies compatibility. Keep application code, images, API/worker instances, managed identities, product DNS namespaces and release pipelines separate. The .NET service hosting decision must be compatible with this direction or return for an explicit decision.

Use distinct product tables, not merely separate rows in one table. Scope runtime data permissions to their required tables. Use distinct product secrets and secret-scoped RBAC; naming prefixes alone are not isolation. Do not grant vault-wide or account-wide runtime access as a shortcut. Inventory inherited grants too: a narrow new grant does not cancel a broad existing one.

Retain separate factory and bootstrap privileges. Existing Tempo permissions must be assessed before claiming bidirectional isolation; this repository must not silently modify them. Provider credential scope also matters: copying a DNS credential to a differently named secret does not restrict what the provider permits it to change.

Each shared resource has one authoritative Bicep definition and deployment owner. Waveform consumes approved outputs/references rather than independently redeploying the same resource. Determine the owner before changing IaC; preserve names/resources unless a separately reviewed migration is necessary.

Maintain resource groups by concern: network (networking/private DNS), compute (environment/applications/jobs), data (storage/registry), security (vault/security resources), and operations (logs/alerts). This is a logical mapping, not a request to create five new groups. Existing group placement and any cross-group module references must be verified; do not consolidate concerns into one group.

## Tradeoffs and costs

Shared resources have shared outage, throttling, administration and recovery impact. Table/secret permissions reduce application access but do not create separate infrastructure failure domains. Microsoft recommends a vault per application/environment; secret-scoped RBAC is an explicit early-stage exception here.

Key Vault secret operations are transaction-priced; duplicate private endpoints introduce hourly charges. Sharing can avoid some duplicated costs, but this ADR makes no quantified savings claim. Establish current and incremental cost estimates, with idle/expected/high-use assumptions, before paid-resource changes. Separate API/worker instances still incur their own compute usage.

## Validation and migration

Before activation prove cross-product table and secret access is denied using actual runtime identities, private connectivity works, and an application deployment cannot mutate the other application's resources. Test recovery/backup procedures per product and alerts for both services. Preserve the existing OTA signing approval boundary.

Keep table names, resource endpoints and secret references configurable. Later separation requires data/secret transfer, new permissions and a tested cutover/rollback; it is not merely a rename. Review before first paid rollout and whenever load, incidents, contractual isolation or recovery requirements change. Revenue alone does not automatically require duplication.

## Sources

- [Table-scoped authorization](https://learn.microsoft.com/en-us/azure/storage/tables/authorize-access-azure-active-directory)
- [Key Vault RBAC guidance](https://learn.microsoft.com/en-us/azure/key-vault/general/rbac-guide)
- [Container Apps environments](https://learn.microsoft.com/en-us/azure/container-apps/environment)
- [Key Vault pricing](https://azure.microsoft.com/en-us/pricing/details/key-vault/) and [Private Link pricing](https://azure.microsoft.com/en-us/pricing/details/private-link/), checked 28 November 2026; no account-specific quotation was obtained.
