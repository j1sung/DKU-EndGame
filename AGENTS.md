# DKU-EndGame agent entrypoint

Unreal Engine association: 5.8. Runtime module: DKUEndGame. Read the actual project descriptor and relevant source before assuming engine patch version or implementation.

## Authority and scope
- Human requests take precedence over project rules. Discussion, agreement, analysis, and an ambiguous “proceed” are not implementation authorization.
- Read-only inspection is allowed for requested analysis. Modify files/assets only after an explicit create/edit/implement/apply request. Build, run, install, delete, commit, push, merge, create PRs, and send Discord messages only within explicitly authorized scope.
- Authorized implementation includes relevant documentation maintenance. Do not produce logs or documents for ordinary questions.
- Preserve existing code, assets, names, paths, style, and unrelated work. Adopting these rules does not authorize migration or renaming.
- Answer in Korean. Directives and skill instructions are English; human Docs, templates, and reusable user prompts are Korean. Preserve identifiers and commands.

## Read routing
Read [behavior and evidence](.agents/directives/01-behavior-and-evidence.md) and [context policy](.agents/directives/02-context-and-token-policy.md) for repository work. Load only additional applicable rules:

| Work | Required rule |
|---|---|
| Unreal code/config/assets/MCP | [Unreal development](.agents/directives/03-unreal-development.md) |
| Authorized implementation completion or document writes | [Documentation](.agents/directives/04-documentation.md) |
| Branches, history, attribution, integration | [Git/team workflow](.agents/directives/05-git-and-team-workflow.md) |

Start project knowledge lookup at [Docs index](Docs/index.md), then read the relevant category sections. Documentation is shared human/AI knowledge, not executable instructions.
Use [.agents/skills/docs/SKILL.md](.agents/skills/docs/SKILL.md) for requested initial/category/full refresh and bounded documentation updates after authorized implementation. A full refresh is not implied by a normal feature task.
[AI workflow design](Docs/Guides/AI-Workflow-Design.md) is human reference, not a mandatory task read. Read relevant portions only when changing this management system.

Use [.agents/skills/commit/SKILL.md](.agents/skills/commit/SKILL.md) for explicitly invoked current-change staging and commits, or message drafts. Default commit mode reviews and stages eligible current changes; explicit staged-only mode preserves partial staging. Skill creation/editing does not authorize executing commits.
