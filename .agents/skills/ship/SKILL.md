---
name: ship
description: Review and commit current changes using the commit skill, push the current branch, create or reuse a PR to the user-specified target, and merge when conflict-free. Honor staged-only and existing-commits-only limits; leave conflicting PRs open.
---

# Ship a branch through a PR

Read ../../directives/05-git-and-team-workflow.md and applicable behavior rules.

## Request and authorization
- An execution request using this skill with a target branch authorizes reviewing and committing eligible current changes, pushing the current branch, creating or reusing its PR to that target, and merging it. Under this skill, asking to open a PR also includes merging unless the user explicitly requests PR-only or no merge.
- Resolve the target from the user's request, including a branch name alone alongside `$ship` or a request to open a PR or merge into that branch. Do not assume dev when no target is supplied; ask for the target before external mutations.
- Creating/editing this skill, quoting examples, or discussing the workflow is not authorization to execute it.
- Default to the full commit workflow in ../commit/SKILL.md: inspect all current changes, stage eligible tracked and untracked project work, and group it into coherent commits. A ship execution request supplies authorization for that commit phase; do not require a separate `$commit` invocation.
- Explicit user limits take precedence: staged-only means use the commit skill's staged-only mode and preserve unstaged changes; existing-commits-only or no-commit means skip staging and committing entirely. Honor explicit file/category restrictions through the commit skill. PR-only/no-merge limits the merge phase, not the default commit phase, unless the user also restricts commits.
- These limits select new local commits, not a subset of existing branch history. Inspect and disclose all existing commits that the PR would include. If those conflict with an explicit scope restriction, stop before pushing rather than silently publishing them or rewriting history.
- No implicit conflict resolution, local merge/rebase, force push, branch deletion, checkout switch, build, test execution, or Discord message.

## Inspect, commit and publish
1. Verify repository, current named branch, remotes and authentication. Fetch the relevant remote refs; confirm the target exists and differs from the source. If the remote/repository is ambiguous, ask before publishing. Stop for an unfinished merge/rebase or unmerged index entries.
2. Inspect existing source-to-target commits and local status. Summarize source, target, commit mode and scope. Ensure the remote source can be updated by a normal push; never repair divergence automatically. Unless commits are excluded, read and execute ../commit/SKILL.md with the user's restrictions, including its review, exclusions, snapshot preservation and verification. If there are no eligible local changes, continue with existing commits. If committing fails or is partially completed, stop before publishing and report the resulting state.
3. After the commit phase, inspect the complete committed source-to-target diff and commit list again. Preserve and report remaining staged/unstaged/untracked work. Do not sweep in edits arriving after the reviewed snapshot. If there is no PR diff, report that instead of making an empty PR.
4. Prefer GitHub CLI when available, otherwise use an available authenticated GitHub API or browser. Do not print credentials or install tooling without authorization. Push only the verified current source branch to the verified remote. Keep its reviewed head SHA for the merge guard.
5. Find an open PR with the exact source repository/branch and target repository/branch. Reuse it instead of duplicating it. Do not retarget another PR or reopen a closed PR implicitly. Otherwise create the PR even if GitHub reports conflicts, so the user can inspect it. Attach each created or reused task PR with the app's attach_artifact tool when available.

## Titles and descriptions
- Use `<type>: <concise Korean action summary>` for the PR title and merge/squash commit subject. Choose an English tag matching the final diff: feat, fix, refactor, docs, test, perf, build, ci, or chore. Follow ../commit/templates/commit-message.md for message style; do not rewrite existing commits just to normalize their titles.
- Describe the complete PR diff, not just the last commit. Use a short Korean description and relevant validation or unresolved work. Never claim unperformed tests. For CLI multiline bodies use a UTF-8 temporary file and --body-file.
- Reuse an existing PR's relevant context, updating title/body only as needed to describe this workflow's final diff. Keep unrelated review discussion intact.

## Conflict gate and merge
1. After the PR exists, read GitHub's current mergeability and PR head SHA. If computation is pending/unknown, allow a few bounded rechecks; unknown does not mean conflict-free. If still unresolved, leave the PR open and report the uncertainty.
2. If GitHub reports conflicts, stop immediately. Leave the PR open and unmerged; report the conflict and PR link. Do not merge the target into source, rebase, edit conflicting files, or attempt an automatic resolution. The user will inspect conflict locations in GitHub.
3. For an authorized merge, verify the head still matches the reviewed published SHA and the target is unchanged. If the source head changed, stop and report it rather than merging unreviewed work. Target-branch advances require a fresh mergeability check.
4. Respect required checks, reviews, draft state and branch protections. If blocked, leave the PR open and report the actual blocker; do not bypass with admin privileges, enable auto-merge, or silently convert a draft. If a merge queue is required, report that restriction without invoking a CLI operation that implicitly queues the PR or enables auto-merge.
5. Prefer a normal merge commit with the tagged Korean subject. If the repository disallows merge commits, use an enabled squash method with that subject. If neither is available, report the restriction instead of rewriting source history. Guard the merge with the reviewed head SHA (for example gh pr merge --match-head-commit). Do not delete the source branch.
6. Re-read PR state to confirm MERGED and record the merge commit. A successful push or PR creation is not merge completion. If the merge races with new conflicts, leave the PR open and apply the same conflict stop rule; never switch to local conflict resolution.

## Completion
Report new commit hashes/titles (or that committing was skipped), the PR link, source and target, merged/open/conflicting status, and merge commit when available. Mention excluded or remaining local changes and blockers. Keep the local checkout and branch intact.
