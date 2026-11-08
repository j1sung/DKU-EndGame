# Git and Team Workflow

## Branch Strategy
- main receives dev only at version-up releases; no routine direct work integration into main.
- dev is the team integration branch.
- Workers create personal-name branches from dev and integrate frequently.
- Do not invent worker/branch identity. Inspect selected checkout and status; ask for the desired name if creation is necessary. Documentation changes alone do not authorize switching branches.

## Coordination and Handoff
Changes touching another worker's work or handing off follow-up require a dev-target PR plus a separate Discord comment in the team process. Identify affected interface/assets and expected follow-up. Prepare clear summaries when useful; PR creation/message transmission requires explicit authorization. Do not import automatic ship/land behaviors from references.

## Authorization and Existing Work
Commit, push, merge, release, branch deletion or history rewrite only when requested. Inspect staged/unstaged/untracked changes before action; preserve unrelated work and dirty binary assets. Never reset/revert user work to get a clean tree. Do not bypass validation hooks or force-push merely to finish.

## Integration and Documents
Review renamed/moved document links and semantic conflicts. A clean Git merge can leave stale explanations. When committing is authorized, include docs for the same concern and separate unrelated changes. Team members need the updated dev contents in their branch for the shared guidance to apply.

## Attribution Evidence
Use focused log/blame/show to locate semantic edits and corroborate GitHub identity with verified evidence. No guessing from similar names/emails or using the pusher/merger as implementer. Unknown identities remain explicit with useful Git name/commit evidence. Uncommitted work is not assigned a fabricated historical commit.

## Repository Hygiene and Sensitive Data
Do not expose or stage credentials, tokens, private configuration or machine-local runtime state. Reference repositories are read-only unless requested. Exclude generated Unreal/IDE/cache output (Binaries, Intermediate, Saved, DerivedDataCache and IDE caches) from authored change scope unless a concrete request requires inspecting it. Do not modify ignore rules or copy private reference settings incidentally.

## Completion Checkpoint
Check selected branch, scoped diff and file list, preserved unrelated work, related doc updates and any handoff requirement. Report which external actions were performed or remain awaiting authorization. No automatic branch cleanup or Discord notification.
