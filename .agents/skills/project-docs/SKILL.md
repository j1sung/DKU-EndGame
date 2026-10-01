---
name: project-docs
description: Create initial Korean category documentation, reconcile requested categories with current Unreal implementation and Git history, or maintain affected docs after authorized implementation. Full refresh requires an explicit request.
---

# Project documentation
Read ../../directives/04-documentation.md and applicable context/team rules. Use Docs/index.md to locate existing topics; do not read the human management design during ordinary refresh.

## Select mode
- Bounded: after authorized implementation, update only affected Overview sections/details/Changes. Preserve overall refresh headers.
- Initial: investigate an undocumented category sufficiently to explain its current responsibilities, flows, assets, constraints and links.
- Refresh: reconcile explicitly selected categories (all only when requested), including work performed without AI.

## Reconcile
1. Inspect status, HEAD, selected Overview baselines and pending paths. Identify committed changes since each valid baseline, working-tree changes, and undocumented existing subjects. If evidence is missing or conflicting, expand inspection rather than assuming unchanged means correct.
2. Read relevant implementation and asset interfaces. Exclude untouched engine templates from exhaustive documentation; identify custom integration where relevant. Existing historical tests are historical evidence, not new runs. If live Blueprint inspection is necessary and unavailable, state the incomplete area and do not advance a complete-category baseline.
3. Use focused git log/blame/show to find the latest semantic feature change. Corroborate GitHub IDs; otherwise preserve Git evidence and mark identity unresolved. Do not attribute old features to the refresh requester. For uncommitted changes use confirmed current task identity/date only, flag pending commit coverage.
4. Update current-truth prose in place; consolidate duplicated explanations and preserve durable constraints. Use templates/overview.md, detail.md, change-entry.md only as needed, not as a destructive overwrite format. Keep old validation records explicitly historical.
5. Add brief missing Changes entries using original dates and stable unique anchors. Avoid reconstructing every past commit; group initial migration as one entry. Link to Overview sections and details, not only to category folders.
6. For completed category refresh set KST date, confirmed requester ID or 확인 필요, and inspected HEAD. Record paths not covered by that commit, including pending asset inspection. Bounded updates never change overall refresh metadata.
7. Check local Markdown destinations/anchors, stale moved paths, git diff --check and scoped diff/status. Do not run builds, launch editor, install software, commit, push or mutate code/assets without separate authorization.
8. Report updated categories, unresolved identities/evidence and any unadvanced baselines. No automatic schedule or background execution.
