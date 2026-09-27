"""Read-only GASP import inventory for the configured Unreal MCP session.

Import this module from an editor Python bootstrap, then call
GaspImportAuditTools.collect_registry. This never loads, compiles, saves or
deletes assets. Offline decisions belong to GaspImportAudit/plan.py.
"""
import json
import os
import re
from collections import defaultdict
from pathlib import Path

import unreal
import toolset_registry
from toolset_registry.registration import Registration


@unreal.uclass()
class GaspImportAuditTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def collect_registry(evidence_name: str) -> str:
        """Capture on-disk package dependencies and management edges, including incoming edges from other project content.

        Writes one new evidence JSON under Saved/GaspImportAudit. /Engine and
        /Script remain declared boundaries. Other mounted content, including
        GameFeatures and external actor packages, is retained in the graph.
        Management queries use package identifiers only. PrimaryAssetId
        manager identifiers are not enumerated by this Python package API.
        This is a registry snapshot, not proof of arbitrary dynamic loads or
        cooked availability, and contains no deletion operation.
        """
        if not re.fullmatch(r'[a-z0-9-]+', evidence_name):
            raise ValueError('Use a simple lowercase evidence name.')
        project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
        if project.name != 'SurvivalRpg' or not (project / 'SurvivalRpg.uproject').is_file():
            raise RuntimeError('This audit is scoped to the SurvivalRpg project.')
        if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
            raise RuntimeError('Stop PIE before collecting an import inventory.')
        directory = (project / 'Saved/GaspImportAudit').resolve()
        if not directory.is_relative_to(project):
            raise RuntimeError('Evidence directory escapes the project.')
        directory.mkdir(parents=True, exist_ok=True)
        target = directory / (evidence_name + '.json')
        if target.exists():
            raise FileExistsError('Preserve previous evidence; use another name.')
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        registry.wait_for_completion()
        groups = defaultdict(list)
        for asset in registry.get_all_assets(True):
            package = str(asset.package_name)
            if not package.startswith(('/Engine/', '/Script/')):
                groups[package].append(asset)
        def options(hard=False, soft=False, hard_management=False, soft_management=False):
            return unreal.AssetRegistryDependencyOptions(
                include_soft_package_references=soft,
                include_hard_package_references=hard,
                include_game_package_references=hard or soft,
                include_editor_only_package_references=hard or soft,
                include_searchable_names=False,
                include_soft_management_references=soft_management,
                include_hard_management_references=hard_management)
        queries = {'hard_dependencies': options(hard=True),
                   'soft_dependencies': options(soft=True),
                   'hard_management_dependencies': options(hard_management=True),
                   'soft_management_dependencies': options(soft_management=True)}
        records = {}
        for package, assets in sorted(groups.items()):
            record = {'classes': sorted({str(a.asset_class_path.package_name) + '.' + str(a.asset_class_path.asset_name) for a in assets}),
                      'asset_names': sorted(str(a.asset_name) for a in assets)}
            for name, query in queries.items():
                record[name] = sorted({str(d) for d in registry.get_dependencies(package, query)})
            record['package_dependencies'] = sorted(set(record['hard_dependencies'] + record['soft_dependencies']))
            record['management_dependencies'] = sorted(set(record['hard_management_dependencies'] + record['soft_management_dependencies']))
            records[package] = record
        all_edges = {d for r in records.values() for d in r['package_dependencies'] + r['management_dependencies']}
        result = {'schema_version': 1, 'project': str(project), 'pid': os.getpid(),
                  'engine_version': unreal.SystemLibrary.get_engine_version(),
                  'include_only_on_disk_assets': True,
                  'coverage': 'All registered non-Engine/non-Script on-disk packages, including project/plugin/external actor content. Hard/soft package edges include editor-only and game references. Management queries cover package identifiers only; PrimaryAssetId management identifiers are not enumerated. Searchable-name identifiers are not load dependencies. Dynamic string construction and cook inclusion remain unproven.',
                  'excluded_mounts': ['/Engine', '/Script'],
                  'records': records,
                  'boundary_dependencies': sorted(d for d in all_edges if d not in records),
                  'counts': {'packages': len(records), 'package_edges': sum(len(r['package_dependencies']) for r in records.values()),
                             'management_edges': sum(len(r['management_dependencies']) for r in records.values())}}
        with target.open('x', encoding='utf-8') as stream:
            json.dump(result, stream, indent=2, ensure_ascii=False)
        return json.dumps({'path': str(target), **result['counts']})


registration = Registration([GaspImportAuditTools])
registration.register()
