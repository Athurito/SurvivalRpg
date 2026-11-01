# SurvivalRpg

A dark-fantasy survival action RPG on Unreal Engine 5.8, built on Lyra
architecture.

## Start here

- [AGENTS.md](AGENTS.md): conventions, architecture guardrails and the
  skill routing for coding agents.
- [Game vision](docs/game-vision.md): the product direction.
- [Unreal tooling](Build/Tools/Unreal/README.md): build, test, cook and
  package with `ue.py`. A fresh worktree first needs the
  [NetworkPrediction/Mover overrides](Build/Patches/NetworkPrediction/README.md).
- [Agent skills](Build/Tools/AgentSkills/README.md): the canonical skills live
  in `.agents/skills/`.

## Roadmaps

| Roadmap | Status |
| --- | --- |
| [Harvesting rework](docs/harvesting-rework-roadmap.md) (HRW) | Active: HRW-01 to 04c merged, HRW-05 awaits design review |
| [VFX](docs/vfx-roadmap.md) | Planned tasks |
| [UI](docs/ui-roadmap.md) | Planned tasks |
| [Ranged combat](docs/ranged-combat-roadmap.md) | Deferred |
| [GASP integration](docs/gasp-integration-roadmap.md) with its [handoff](docs/gasp-integration-handoff.md) | No task ready; holds the accepted movement state |
| [Combat](docs/combat-roadmap.md) (CMB) | Complete |
| [Mastery map](docs/mastery-map-roadmap.md) (MST) | Complete; runes are redesigned in HRW-05 |
| [Harvesting](docs/harvesting-roadmap.md) (HARV) | Complete, partly superseded by HRW |
| [Inventory refactor](docs/inventory-refactor-plan.md) | Complete |

## System contracts

- [Skill trees](docs/skill-trees.md): mastery, skill trees and talents.
- [Character progression](docs/character-progression.md): the character level
  curve.
- [Physical storage](docs/physical-storage-implementation.md): chests, base
  areas and where construction and crafting take materials from.
- [GAS ability content](docs/gas-ability-content.md): native ability families
  and their Blueprint content.
- [Combat network smoke](docs/combat-network-smoke.md): the remote melee
  network acceptance.
- [Sprint](docs/gasp-cmc-sprint.md): sprint with GAS stamina.
- [Listen-server animation timing](docs/listen-server-animation-timing.md):
  when the animation graph updates immediately on a listen server.
- [UI style guide](docs/ui-style-guide.md): tokens, assets and rules for
  widgets.
- [RPG baseline](docs/rpg-baseline.md): the baseline Experience and its test
  map.
- [Third-party notices](docs/third-party-notices.md): licences of shipped
  third-party content.

## Task reports

Every roadmap links its task reports from its task table. Reports live next
to the roadmaps as `docs/gasp-*.md`, `docs/combat-*.md`,
`docs/mastery-map-*.md` and `docs/vfx-*.md`; the HARV, HRW and UI roadmaps
keep their reports in their own sections. Asset and evidence manifests are in
`docs/assets/`.
