# MST-04: Atomic choice swap

Task report for MST-04 in the [mastery map roadmap](mastery-map-roadmap.md), [PR #313](https://github.com/Athurito/SurvivalRpg/pull/313); the design is in the [implementation plan](mastery-map-implementation-plan.md), section "Shared harvesting progression" (one server request for replacing an exclusive choice) and the detail card contract. It builds on the single-map card of [MST-03c](mastery-map-single-map.md) and the trees of [MST-10a](mastery-map-several-trees.md) and [MST-10b](mastery-map-combat-tree.md).

## C++ boundary decision

- **Classification:** an extension of the existing skill tree mechanism (`URpgSkillTreeComponent`) and its view models; card text and layout stay in `CUI_Skills`. No new class.
- **Runtime truth:**
  - `URpgSkillTreeComponent` owns learned nodes, and the server validates every request.
  - The equipment manager reconciles grants from the learned set.
  - Request status and rejections are local UI state of the tree view model and are never saved.
- **Native changes:**
  - `URpgSkillTreeComponent::EvaluateSwap` and `SwapNode`, plus the server RPC `RequestSwapNode`.
    - A swap builds a candidate state: every other purchase is replayed in its order without the learned alternative of the node's exclusive group.
    - It rejects the swap when any of those purchases would no longer be learnable (`ChoiceStillNeeded`, a new `ERpgSkillTreeUnlockResult` value).
    - It then evaluates the new node on the candidate, so the replaced node's points count as free.
    - `SwapNode` removes the old node and appends the new one in one change, and `HandleSkillTreeChanged` runs once. Grants, slots, replication and the save therefore never see both or neither.
  - **Rejection feedback:** `ERpgSkillTreeRequestRejection` and the owner-only client RPC `ClientReportRejectedRequest`.
    - Rejected unlock, refund and swap requests report their node and reason to the owner through `OnRequestRejected`.
    - Before, a rejected request was only logged on the server.
  - **View models:**
    - `URpgSkillTreeNodeViewModel` gains `bCanSwap`, `SwapReplacesNodeTag`, `SwapReplacesName`, `RequestStatus` (`Idle`, `Pending`, `Rejected`), `bIsRequestPending`, `Rejection` and the command `RequestSwap`.
    - `URpgSkillTreeViewModel` gains `RequestSwapNode` and tracks open requests.
      - A request waits until the tree's next replicated change or its rejection.
      - A rejection stays until the selection moves or the node gets a new request.
- **Assets (Unreal MCP):** `CUI_Skills`.
  - **Learn button:** reads Switch and sends `RequestSwap` for an excluded node that can be swapped.
  - **Card text:** for such a node the card says "Switch possible" and names the choice it replaces.
  - **Pending and rejected:** the card's buttons wait while a request is open, and a rejection replaces the requirement line with its reason.
  - **Graph changes:** new function `F_ApplyRequestFeedback`; `F_RefreshDetailCard`, `F_RefreshNodeActions`, `F_LearnSelected`, `F_RefundSelected` and `F_Refresh` were rewritten.
  - The card now reads the progression view model's `SelectedNode`, with no lookup through the selected tree.

## Behavior

### Problem

Exclusive choices such as Shatter and Follow Through, or Riposte and Bulwark, could only be changed by refunding one and learning the other. With every point spent, the refund had to come first, and the player passed through a state with neither choice. A rejected request left the screen unchanged without saying why.

### Change

- **Switch:** selecting the excluded alternative of a learned choice shows "Switch possible" and "Replaces <choice>. Its points move over."
  - The card's button reads Switch.
  - Switch, or F or the left face button, swaps the choices in one server step, also with no free point.
  - The old choice's grants leave and the new one's arrive together.
- **Validity:** a swap is refused, and changes nothing, when other learned talents still need the current choice or the new choice costs more than the freed and free points together. The card then says the alternative is learned and the switch is not possible.
- **Feedback:** while a request waits for the server, Learn, Switch and Refund are disabled and the card says "Waiting for the server". When the server rejects a request, the card says "Not changed" and why, for example "Not enough free points."
- **Repeated requests:** a repeated or stale swap is rejected as already done and changes nothing.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings (after one compile error in the new tests: a braced list of native gameplay tags cannot be copied; the loops now use `TArray<FGameplayTag>`).
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Progression SurvivalRpg.UI.SkillTreeMap --null-rhi`: **34 of 34 passed** with the new tests.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE`: **1 of 1 passed** with the new steps.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip SurvivalRpg.Combat --null-rhi`: **144 of 144 passed**. The 13 with warnings are the known AdvancedFriends voice-interface messages of the block and weapon-hold PIE tests and an asset log of `CUI_RespawnScreen`.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.KillXPCuePIE`: **2 of 2 passed**, with only the usual `LogNetPackageMap` warnings of the temporary PIE worlds.
- `python Build/Tools/Unreal/ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **New tests:**
  - `SurvivalRpg.Progression.SkillTrees.SwapExclusiveChoice`:
    - A swap at zero free points succeeds: the points move over, and the new choice joins the end of the purchase order.
    - A repeated swap is rejected as `AlreadyUnlocked`.
    - The swap request switches back, and outside a learned group a swap equals an unlock.
    - A choice another learned node needs stays (`ChoiceStillNeeded`), and so does a choice that costs more than the freed and free points (`NotEnoughPoints`).
    - The owner hears each rejected unlock, refund and swap with its node and reason.
  - `SurvivalRpg.Progression.SkillTrees.SwapReconcilesGrants`: with the tool equipped, a swap removes the old choice's granted tag and grants the new one once. A repeated swap grants nothing twice, and swapping back restores the original.
  - `SurvivalRpg.Progression.SkillTrees.ViewModel.SwapAndRejectionFeedback`:
    - The excluded form offers a swap that names the learned one.
    - On the server the swap lands at once with nothing waiting.
    - A rejected unlock shows `Rejected` with `NotEnoughPoints`, and selecting another node clears it.
  - `SurvivalRpg.Network.SkillTreePIE`, dedicated server with two clients:
    - The owner hears why the unaffordable node was rejected.
    - The owner learns FormA with its last point and swaps it for FormB; the server and the owner then show FormB with no free point.
    - The repeated swap is rejected as already done and changes nothing.
    - The other client still never receives the owner's progress.
- **Agent PIE check** in `Lvl_ThirdPerson` with its dev save (Combat level 5):
  - **Standalone:** Battle Rhythm and Bulwark spent all 4 points.
    - Selecting Riposte showed "Switch possible", "Replaces Bulwark. Its points move over." and the button Switch.
    - Switch learned Riposte and excluded Bulwark. Combat stayed at 0 free and 4 spent.
    - The ability system showed only `GE_Talent_Combat_Riposte`, with `BlockStaminaCost` back at 20.
  - **Listen server with two players:**
    - The host switched Riposte back to Bulwark with no free point: its `BlockStaminaCost` went to 15.
    - The client kept no talent effect and 20.
    - The host's combat tree was reset afterwards (level 5 with 4 free points stays for the playtest), and the play settings are the listen server with two clients again.
- **Failed attempt:** `CUI_ApplyButtonBase.SetButtonText` only sets the variable its Construct reads. The button kept "Learn" until the card also set the button's text block.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.

## Playtest

1. **Switch without points:** in `Lvl_ThirdPerson` (Combat level 5), press H.
   - Learn Battle Rhythm and Bulwark; no point is free.
   - Select Riposte: the card says "Switch possible" and "Replaces Bulwark", and its button reads Switch.
   - Press Switch or F: Riposte is learned and Bulwark is excluded, still with no free point.
2. **Effect follows:** with the sword, blocking costs the normal stamina again after the switch, and a perfect block gives back more stamina.
3. **Harvesting:** with enough Harvesting points, learn Cleaving Force and Shatter, then switch to Follow Through the same way.
4. **Blocked switch:** this needs a choice another talent depends on. The shipped trees have none yet, so the automated test covers it.
5. **Multiplayer:** in a two-player listen server, a player's switch changes only their own talents and blocking.

## Limits

- **Pending state not seen:** in standalone and on a listen server host the server answers at once, and the PIE client windows ruled out clicking on the client. The pending state was covered by the view model logic and the network test, but not seen on screen.
- **Wait can end early:** a request waits until the tree's next replicated change. An unrelated change of the same tree, arriving before the server handled the request, ends the wait early.
- **Action bar:** it still shows the Learn action's name ("F Learn") while the button reads Switch.
- **Not localizable:** the card's switch and rejection lines are built from strings, like the other card lines.
- **No blocking content yet:** no shipped tree has a node that depends on an exclusive choice, so `ChoiceStillNeeded` appears only in tests.

## Next

MST-05: physical rune instances with ranks and three sockets on harvesting tools.
