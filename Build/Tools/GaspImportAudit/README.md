# GASP import-reference audit

This tooling reports references; it has no deletion operation. Candidates are
the exact `source` entries of provenance mappings, never inferred from a folder
name. Every other inventoried package is conservatively retained.

The initial 2156-source capture is historical. The follow-up removal is recorded
in `docs/gasp-original-import-removal.md`, with the full 2298-package provenance
in `docs/assets/gasp-original-import-inventory.json`. After that removal, those
sources are intentionally absent: `missing_source` for a historical mapping is
not evidence that its retained project-owned target is missing.

## Capture and reproduce

1. Verify the checkout-local NetworkPrediction/Mover overrides as documented in
   `Build/Patches/NetworkPrediction/README.md`. Open the current project with the
   configured Unreal MCP environment and stop PIE. Do not share this editor with
   another authoring task.
2. Import `Build/Tools/Unreal/gasp_import_audit.py` from an editor Python bootstrap
   with its directory on `sys.path`. The module registers
   `Game.Build.Tools.Unreal.gasp_import_audit.GaspImportAuditTools` in the configured
   toolset registry. Describe that toolset through MCP, then call `collect_registry`
   with a fresh lowercase `evidence_name`, for example `registry-check`.
   The result is written exclusively to `Saved/GaspImportAudit/registry-check.json`;
   existing evidence is never overwritten. No assets are loaded, compiled or saved.
3. Run the offline planner from the repository root:

```powershell
python Build/Tools/GaspImportAudit/plan.py --records Saved/GaspImportAudit/registry-check.json --mappings docs/assets/gasp-import-cleanup-audit.json --output Saved/GaspImportAudit/plan-check.json
python -m unittest discover -s Build/Tools/GaspImportAudit -p test_plan.py -v
```

The current versioned manifest supplies exactly 2156 `mappings` from the
foundation and Ragdoll provenance manifests. For a different approved scope,
pass a separate JSON list of `{source, target, provenance}`. Duplicate evidence
is deduplicated; distinct targets and provenance are retained. `--retain` adds
explicit protected roots. Use `--external-mount` only with independent evidence
for that mount. Existing records remain graph nodes even on a declared mount.

`build_plan(records, mappings, retained_roots=None, external_mounts=None)` is the
portable Python API. Each record must explicitly provide `package_dependencies`
and `management_dependencies`. The output reports shortest reference witnesses,
direct referencers, missing sources/targets/roots, and unknown dependency edges.
`candidate_unreferenced` is an inventory observation, not permission to delete.

## Coverage

The exporter enumerates registered on-disk AssetData packages outside `/Engine`
and `/Script`, including GameFeature/plugin and external actor/object packages.
Package edges include both hard/soft and game/editor-only references. Its
management queries use **package identifiers only**: the Python wrapper does not
enumerate the AssetManager's `PrimaryAssetId` management graph. A zero count does
not mean all management dependencies are absent.

Unknown files/packages, boundaries, runtime-generated strings, user configuration,
directory scans and cook inclusion remain separate audit obligations. The
27.09.2026 capture covers all 6964 tracked project assets, but that observed result
must be checked again in another checkout. `Saved/` evidence is local; the
versioned manifest preserves the exact package mapping, hashes and baseline-map
reference paths. See `docs/gasp-import-cleanup-audit.md` for the decision and
remaining work. Do not remove originals before the required load/compile/cook
validation of an explicitly bounded removal step.
