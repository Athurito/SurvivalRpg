# Listen-server animation timing

The listen server could display a remote player's locomotion too slowly while
clients displayed each other normally. Temporarily setting
`a.ParallelAnimUpdate 0` corrected the symptom; restoring `1` reproduced it.

## Cause and scope

CharacterMovement can submit several autonomous pose ticks during one server
frame. Deferred animation graph updates retain the latest delta rather than
accumulating those separate movement deltas. A native control test reproduces
this in UE 5.8.2 without an animation Blueprint, montage, or GASP content:
10 ms followed by 20 ms produces a single 20 ms graph update.

This does not establish when the engine behavior first appeared. Historical
project logs use UE 5.8.1 and later UE 5.8.2; checking out older project content
does not roll back the installed engine or local editor preferences.

## Project integration

`URpgAnimInstance::CanRunParallelWork` requests immediate graph updates only
while an authoritative RPG character on a listen server processes the pose of
its remote autonomous owner. The mesh must be inside its movement-owned
autonomous pose tick. Outside that scope, the normal engine policy applies.

The player's `ABP_Unarmed` adopts the existing `URpgAnimInstance` foundation.
Animation graphs, blend spaces, play rates, montage slots, and the
`RootMotionFromMontagesOnly` policy remain designer-owned. Other animation bases
do not inherit this hook automatically; review their ownership when adopting
them for RPG characters.

Keep `a.ParallelAnimUpdate` enabled. This fix does not change NetworkPrediction
or Mover settings and does not restore the archived native GASP locomotion port.

## Verification

`SurvivalRpg.Animation.ListenServer` covers actual engine graph dispatch with
multiple deltas in one frame, the ordinary deferred path, the role/net-mode/pose
scope, and the player's native animation-base contract. The timing fixture uses
an empty transient world and never starts a GameMode, Experience, or save
lifecycle.

The native control loses the first delta; the guarded instance consumes both
updates for a total of 30 ms. These tests cover the scheduling mechanism, not
end-to-end network packet delivery or montage notify behavior. Also compare
remote locomotion on a listen server with parallel animation enabled, client
views, and montage playback when validating an affected animation setup.
