---
name: commit-staged
description: Review current repository changes, stage eligible tracked and untracked work, and group it into a few commits with English tags and Korean messages when explicitly invoked for committing. Supports explicit staged-only and draft-only requests.
---

# Commit Staged Work

## Scope and Authorization
Read ../../directives/05-git-and-team-workflow.md and applicable behavior rules. Creating/editing this skill is not authorization to execute it. An explicit invocation to commit (including the user selecting this skill alone) authorizes reviewing current changes, staging eligible files and making local commits. No implicit push, merge, PR, Discord message, build or test execution.
Default scope is current tracked additions/modifications/deletions and eligible untracked project files in the verified repository. An explicit file/category restriction narrows that scope. An explicit staged-only request preserves the original index and never adds unstaged content. Draft-only requests do not alter the index or commit. If no eligible changes exist, report that and stop.

## Inspect and Select Current Changes
- Inspect branch/status, cached and working-tree diffs, relevant untracked file contents, renames, binaries and partial staging. Do not traverse generated folders blindly.
- Default mode includes the final working-tree contents of partially staged files, not just previously selected hunks; disclose this in the grouping summary. Preserve staged-only differences when that mode was requested. If the index contains a distinct version not represented by the working tree and updating it would lose intended content, clarify before overwriting it.
- Include authored code/assets/config/docs in the requested scope, even if already present before this conversation. Do not omit work merely because a different teammate authored it; use scope/evidence, not guessed ownership. Ambiguous scope or active concurrent edits require a focused question before staging affected paths.
- Exclude ignored files, generated/cache/IDE output, temporary artifacts, credentials/private runtime settings and accidental nested repositories. Do not force-add ignored files. Report exclusions; do not silently discard user changes.
- Detect unmerged entries or concurrent index/HEAD changes and stop before mutation. Classify by actual content and dependencies; binary filenames alone do not prove behavior.

## Stage the Reviewed Selection
- Present a compact grouping and scope summary, then stage explicitly reviewed paths (including deletions), preferably using a literal pathspec file rather than an unreviewed git add . .
- Do not edit implementation or generate extra documentation simply to make a commit. Include existing project documentation changes that were selected; do not auto-refresh all docs.
- Check the resulting cached diff/stat and whitespace, and inspect for accidental secrets without printing values. A blocker is reported, not bypassed or silently fixed through worktree edits.
- Capture the exact reviewed post-staging tree/blob/mode/path entries before splitting. From this point the snapshot is fixed: later working-tree edits are excluded unless explicitly reviewed again.
- Staged entries deliberately outside the selected scope must remain staged and must not leak into any commit. Use an isolated group index when needed, even for a single selected group.

## Grouping Policy
Prefer a few broad, coherent commits; one commit is appropriate for one overall task.
- Keep a feature/fix with its supporting tests, assets, configuration and matching docs.
- Separate independently meaningful, unrelated substantial tasks.
- Attach small fixes, naming, formatting and documentation cleanup to the closest relevant group when they support it.
- If minor changes have no substantial parent, collect genuinely related maintenance together. Do not create an omnibus commit that hides unrelated major behavior changes.
- No fixed commit count and no commit per file/function. Avoid turning many small edits into ten tiny commits.
- Keep a mixed-purpose file in the closest cohesive group unless safe staged-hunk partitioning is necessary and justified. If distinct major changes cannot be safely separated, explain the tradeoff and request a choice rather than guess.
Show a compact grouping summary before execution; do not require a second approval when commits are already explicitly authorized and the grouping is clear.

## Message Format
Use templates/commit-message.md. Title: <type>: <concise Korean action summary>. English tags: feat (new behavior), fix (bug correction), refactor (behavior-preserving restructure), docs, test, perf, build, ci, chore as appropriate. Choose the group's dominant purpose; do not infer feat just from added files.
Body: usually 2–5 Korean '-' bullets describing what and, where useful, why. Mention relevant interfaces/assets rather than every filename. Optional ✅/⚠️ only for real checked results or material limitations; never claim unperformed tests. No automatic version suffix, release bump or attribution footer. Omit secrets/private context.

## Safe Index Partitioning
- One group with no out-of-scope staged entries: commit the reviewed index as-is with git commit --file <UTF-8 temporary message file>. Do not use git commit <paths> or git commit -a: these can capture unstaged content.
- Multiple groups: preserve the exact reviewed post-staging blobs and repartition using index-aware operations. Prefer an isolated temporary index per group initialized from the latest HEAD and populated with that group's reviewed snapshot entries (including deletions, modes and both sides of renames). Never repopulate from working-tree file content using git add.
- Before each commit, verify the group tree and ensure HEAD/real index have not changed unexpectedly. Respect normal hooks; never use --no-verify or commit-tree to bypass them.
- After successful split commits, reconcile committed paths in the real index to the resulting HEAD only if no concurrent index modification occurred. Preserve excluded staged entries, working-tree bytes and all later unstaged differences. On partial failure, preserve reviewed staged entries for uncommitted groups and expose the exact remaining state; do not reset or roll back successful commits.
- If exact snapshot preservation or hook behavior cannot be handled safely, stop and explain before altering the real index. Do not improvise destructive reset/checkout/stash operations.

## Completion Checkpoint
Verify each actual commit subject/body and changed paths against the planned groups. The union of committed changes must match the reviewed selected snapshot; no extra unstaged content. Report hashes, Korean titles, remaining staged/unstaged work and any failure. Do not amend previous commits or clear unrelated work. Do not auto-create Changes entries for the act of committing or modify category refresh metadata.
