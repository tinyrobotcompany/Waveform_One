Scope and risk triage:
- Prioritize behavior, security, and operational risk before style.
- Increase depth for auth, secrets, data integrity, migrations, infra, networking, and public API changes.

Required review method:
- Validate critical paths end-to-end, including error and rollback paths.
- Prefer concrete, code-backed findings over speculation.
- Check observability, rollout safety, and backward compatibility on risky changes.

Finding quality bar:
- Each finding must include Severity, Title, Evidence, Impact, Fix.
- Order findings by severity: Critical, High, Medium, Low.
- If no blockers exist, state that explicitly and list residual risks/testing gaps.
