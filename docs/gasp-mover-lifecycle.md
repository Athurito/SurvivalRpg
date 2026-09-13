# GASP Mover health, death and respawn

This slice, accepted in PR #140, extends the equipment/montage integration
accepted in PR #139. The subsequent [grounded Mantle integration](gasp-mover-mantle.md)
adds traversal to the same Experience.
The existing PlayerState ASC, HealthSet, HealthComponent, DeathComponent,
`GA_Combat_Death`, GameMode respawn and controller-owned inventory/loadout
remain responsible for the RPG lifecycle. No second health or respawn system
is introduced.

## Ownership and behavior

`ARpgMoverPawn` composes the existing Health and Death components and binds
them through PawnExtension's ASC lifecycle. The server restores health on a
fresh living avatar and routes world-boundary damage through the existing
self-destruct GameplayEffect. Death completion returns to the GameMode's
inventory-drop, respawn delay and checkpoint selection path.

The shared HealthComponent now reconstructs death tags from a death state
received before ASC binding. A retired avatar cannot change the replacement
avatar's death tags through a late replication callback or cleanup.

The engine-facing movement addition is one native `URpgDeadMovementMode`.
`URpgCharacterMoverComponent::DisableMovementForDeath` enters that terminal
stationary mode through Mover simulation. Its mode and capsule state use
existing Network Prediction replication and correction. New movement inputs
and root-motion contributions cannot move the dead pawn; history preceding
death remains available for rollback. GAS continues to own death montage
playback. Respawn creates a fresh pawn with the authored GASP movement modes.

The Mover Blueprint selects the existing `RpgPawnMesh` and `RpgPawnCapsule`
collision profiles so RPG weapon traces can reach the gameplay mesh. The
optional retarget follower remains cosmetic. Animation graphs, clips, gait
tuning, the death montage and its two-second ability delay are unchanged.

## Manual validation

Open `Lvl_RpgGaspMover`. The marked **DEATH / RESPAWN** area is forward and
left of the player starts. Its test Blueprint checks the overlapping actor's
authority and calls the existing HealthComponent self-destruct path. Its
marker uses the original project-local GASP cube/grid material. Map lighting
and the isolated GameMode's disabled disk persistence remain unchanged.

1. Walk into the marked area while moving, attacking or holding block.
2. Check that the pawn stops, the death animation plays and the respawn screen
   appears. The existing GameMode uses a five-second respawn wait.
3. Request respawn with the existing UI. Check health, equipment attachment,
   movement, camera and attack/block input on the replacement pawn.
4. Repeat with a remote client and an observer joining during the respawn wait.

Death uses the existing inventory-drop policy; this slice does not introduce
new item-loss rules. Downed/revive, Mover traversal and Mover ragdoll remain
separate steps. The user-observed stationary legs while moving with block
held are accepted for now, as with CMC; combat/locomotion blending is deferred.

## Validation record

UE 5.8.2 Development Editor builds succeeded, including the final lifecycle
test source. All 50 distinct automation tests passed: two Health lifecycle
contracts, two Mover lifecycle network cases and 46 existing camera, CMC,
Mover, traversal, retarget and melee regressions. Results and source hashes
are recorded in `Saved/GaspMoverLifecycle20260913/verified-results.json`.

Nine relevant packages loaded and both affected Blueprints compiled. The
authored listen-server test zone reached the existing death screen; its
focused respawn button created a healthy replacement pawn on the same
PlayerState. The final sight check confirms the readable label and hidden
trigger helpers. PIE ended cleanly, prior play settings were restored and
no packages were dirty.

The hash comparison checks 7,623 existing asset/plugin files against
`4f78c214`: only `BP_RpgGasp_Mover` and `Lvl_RpgGaspMover` changed. The test-zone
Blueprint is the sole new package. All seven existing save files are
unchanged, with no new saves. Source animations, imported assets, baseline
content, death ability/montage and GameMode assets retain their prior bytes.

The lifecycle network tests exercise an incoming weapon-channel sweep against
the physical gameplay mesh, pass its bone hit through the real damage GE,
then trigger lethal damage during an input-driven attack or held block.
They observe cancellation, terminal movement, destruction, the existing
respawn RPC and replacement-pawn equipment/ASC composition. One case includes
an observer and optional retarget follower; the other joins during the respawn
wait and introduces a 50 cm owning-client prediction error while dead.

That correction check observes reconciliation at unchanged simulation
frame/time. It does not establish a rollback whose starting frame predates
death. Coverage uses the Independent game-thread backend in single-process
PIE; separate-process and packaged multiplayer remain untested. The incoming
body-hit check is not a PvP permission test. Existing CMC melee coverage also
exercises enemy damage. Transient-level NetGUID, voice and Independent
interpolation warnings can occur during these PIE joins and delay tests.
