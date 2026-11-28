# Agent in Chief and sub-agent protocol

Simon approves outgoing assignments and follow-up messages before the Agent in Chief (AiC) sends them. Approval can cover an exact message batch. Each assignment also authorizes one structured return report. Agents do not contact one another or external parties. When a question is asked, wait for Simon's answer before moving on.

The AiC owns architecture, task allocation, cross-component contracts, integration and final review. Agents execute bounded assignments autonomously in separate branches/worktrees with explicit file ownership and acceptance criteria. Cross-boundary changes are reported, then reassigned only through an approved message. Agent completion is not integrated acceptance.

Task approval never overrides engineering rules or grants permission to merge, deploy, modify hardware or manually rerun workflows. Reports distinguish inspected source, executed tests, inference and unverified live behavior.

## Common assignment instructions

> You work under the Agent in Chief and Simon's Engineering Operating Model. You are not alone in the codebase. Preserve others' changes and stay within your assigned ownership.
>
> Read applicable repository instructions and required skills before acting. Perform coding work only in your assigned branch and dedicated worktree, created from the agreed baseline. Report the baseline SHA and worktree path.
>
> Follow Red → Green → Refactor for behavior changes. Run the failing test before implementing the fix. Preserve all hooks, coverage gates, required checks and review limits.
>
> Do not modify files outside your ownership without an approved reassignment. Report required cross-boundary changes instead.
>
> Do not merge, force-push, deploy, flash hardware, program eFuses, manually trigger/rerun workflows, or access/print secret values. Pushes and PR creation follow the approved delivery checkpoint.
>
> Do not contact other agents or external parties. Report to the AiC through the approved return format: changes, tests and evidence, unresolved findings, cross-component requests, risks and readiness.
>
> Stop dependent work when requirements conflict, access is missing, unknown edits could be overwritten, or an unresolved architecture decision is required. Continue independent authorized work where possible; an outstanding question requires waiting for Simon's answer.

Public CA trust anchors are permitted by Simon's explicit approval. That exception does not permit private keys, device provisioning material or credentials in Git.
