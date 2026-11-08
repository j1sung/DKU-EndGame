# Unreal Development

## Scope and Official References
Applies to Source, build/module files, runtime configuration, Blueprint-facing APIs, Content assets and Unreal MCP work. Verify engine association/patch version and local conventions before implementation.
- [Epic C++ coding standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)
- [Recommended asset naming conventions](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)
Consult version-relevant official sections as needed. Distinguish recommendations from mandatory engine constraints and project-specific choices.

## Existing Compatibility and Naming
Match neighboring format, names, class/file organization and asset folders. New identifiers follow Unreal conventions where applicable (A/U/F/E/I/T and b for booleans). New assets follow appropriate Epic recommendations consistently with project prefixes. Adopting these rules never authorizes bulk renames, folder moves or code restyling. Explain material official/local conflicts and let the worker choose before compatibility-changing migration.

## Headers, Reflection and Dependencies
- Keep generated headers in the required include position. Use valid forward declarations for pointer/reference-only types; include complete definitions for inheritance, by-value storage or reflection requirements.
- Verify actual delegate declarations and parameter order; use local examples or official API/engine declarations instead of guessing.
- For OnlineSubsystem value types, ensure the relevant definition header is present (for example OnlineSessionSettings.h where constructing session settings/search results).
- Check required module dependencies when exposing APIs; avoid unrelated dependency additions and inherited member name shadowing.

## Runtime Ownership and Lifecycle
Use appropriate reflected/weak/soft references for lifetime and loading requirements. Check actor/world validity, destruction, timers and delegate binding/unbinding for the affected lifecycle. Keep UObject/editor operations on supported threads. Prefer explicit initialization and existing ownership boundaries.

## Gameplay and Networking
Identify state owner, server authority, client intent and replication visibility before adding RPCs or replicated properties. Validate client requests on the server for authoritative gameplay. Separate local UI/camera/prediction/cache concerns from authority; do not make client-needed caches server-only. Consider host and remote-client paths. Local animation flags, TakeoffSerial or tool success do not imply synchronization. Do not add GAS/CommonUI or other frameworks without a concrete approved need.

## UI and Blueprint Boundaries
Preserve current C++/UMG data flow; this project does not acquire a new MVVM requirement from a reference. Prefer Designer for visual layout and C++ for existing state/event responsibilities unless code-driven layout is requested. Verify BindWidget names, parent classes, input assets, sockets and property names before wiring. Do not force an Editor-only assignment into a runtime C++ workaround.

## Asset References and Packaging
Choose hard/soft/editor-assigned references based on actual lifecycle and dependency needs. Be cautious about heavy Blueprint/AI/animation chains loaded during CDO construction; do not blanket-replace existing loaders. When adding runtime string/soft paths or map travel, check the project's actual cook/packaging ownership. PIE availability is not packaged coverage. Do not invent Pragmata-specific PrimaryAssetLabels or broad cook rules.

## Comments and Simplicity
Keep non-trivial changed logic understandable with short intent/constraint comments, especially lifecycle, RPC, callbacks and state transitions. Preserve existing accurate comments and encoding; do not rewrite language for style alone. Use Korean comments when appropriate to surrounding code, preserve technical terms, and keep long reasoning in Docs. Obvious getters/forwarding need no ceremonial comments. Avoid speculative abstractions; remove only newly unused code caused by the task.

## MCP Workflow
### Discovery and Read-Only Inspection
Verify connected editor/project, locate exact target assets and discover the smallest relevant tools and argument schema. Never assume UnreelMCP or built-in MCP tool names. Inspection requests permit read-only operations.
### Authorized Changes and Readback
Read current state, perform only authorized mutations, prefer supported transactions when available, and reread resulting state. Saves, compilation, editor launch and PIE must be within authorized scope. Never overwrite unrelated dirty assets or enable arbitrary Python execution simply because a reference did.
### Missing Capabilities
Report exact unavailable operations or unresolved asset state. Provide manual guidance with known Content paths, panels, values and expected outcomes; do not invent UI labels. Do not install a replacement MCP without a request.

## Validation and Completion
Choose verification proportional to the changed behavior. Request build/run authority if it is missing; do not trigger expensive builds automatically. When authorized, relevant Editor builds can catch UHT/include issues and runtime checks can cover ownership/UI/network behavior. Record no mandatory runtime fields in docs, but never claim unperformed tests or packaged/network success. Preserve code/assets outside scope.
