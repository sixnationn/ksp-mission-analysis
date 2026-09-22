# Cost-conscious agent setup

The user's latest direction is to protect usage while using Extra High for coordination. Project defaults select gpt-6-sol at xhigh reasoning as coordinator, with gpt-6-sol medium children. There are at most two simultaneous children. Use one builder by default and add a reviewer only when the change warrants it. Luna low handles prescribed inventory and checks. Astra high is an exceptional, bounded escalation, never a fleet or an automatic milestone reviewer.

Configured files: .codex/config.toml, and .codex/agents/mission-builder.toml, mission-reviewer.toml, mission-runner.toml. The named agents are optional conveniences; the director can use explicit model/effort delegation where the host supports it. Keep briefs self-contained and short, use fork_turns="none", and assign nonoverlapping file ownership. Do not have a child coordinator recursively rebuild the team.

| Work | Default model | Effort |
|---|---|---|
| Routine direction/integration | gpt-6-sol | xhigh |
| Bounded implementation | gpt-6-sol | medium |
| Numerical or shared-contract review | gpt-6-sol | high |
| Prescribed checks/inventory | gpt-6-luna | low |
| Unresolved physics decision after evidence gathering | gpt-6-astra | high, one bounded call |

For a routine milestone budget one builder pass and one integration pass; add one review for numerical or shared contracts. Escalate only the unresolved question with a concise evidence packet. Record actual usage and outcomes before expanding parallelism. No paid API evaluation or unattended implementation was started.

Availability: this desktop session advertises GPT-6 Astra, Sol and Luna; the planning Astra and Sol subagents were accepted. The separately installed CLI catalog currently lists Astra alongside GPT-5.6 Sol/Terra/Luna, not GPT-6 Sol/Luna. If that CLI rejects a configured model, report the substitution and use GPT-5.6 Sol/Luna for the corresponding role. Do not silently claim the same deployment across clients. Model tiers are an engineering allocation, not a measured price/performance result.

Validation: a fresh local `codex.cmd debug prompt-input` successfully loaded project AGENTS.md. Its output did not expose the named agent definitions, so automatic role discovery is unverified. The CLI does not support --strict-config on debug/features commands. Do not interpret those rejected validation commands as a model run. Project settings apply when supported and loaded by a fresh session; they do not switch the already-running coordinator. No further model invocation was spent merely to smoke-test role files.

The planning run used an Astra coordinator, one Astra numerical adviser and one Sol compatibility researcher. Both children finished. No additional Astra agents were started after the cost instruction. This is planning/setup, not an ongoing background build.

Token benchmark: run scripts/Update-TokenBenchmark.ps1 before reporting a milestone and at the start of the next turn to capture the prior final response. The manifest includes this task and its two children. Add future child log paths to the manifest as they are created. Keep missing attribution explicit. Do not infer dollar cost or weekly-limit consumption from raw token totals.

References: [Codex subagents and custom agent configuration](https://learn.chatgpt.com/docs/agent-configuration/subagents), [Astra model documentation](https://developers.openai.com/api/docs/models/gpt-6-astra). Host-advertised availability and local CLI catalog were checked on 22 September 2026.
