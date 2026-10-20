"""GASP-VAL-02: detect a client-only flying phase after an owner-driven CMC Vault and size the server correction.

Reads a probe run made with --variant cmc --driver owner. After the owner's local Vault montage ends, a local sample
in MOVE_FLYING without any montage is client-only traversal movement the server does not share. The script reports
its duration and forward travel, plus the server corrections (p.NetShowCorrections lines in host.log). Writes
cmc-handoff.json into the run folder. Diagnostic only; it has no pass threshold.
"""
import json
from pathlib import Path
import re
import sys

CORRECTION = re.compile(r"\*\*\* Server: Error for \S+ at Time=\S+ is (?P<distance>[\d.]+)")


def analyse(run: Path) -> dict:
    vault_seen, phase, phases = False, None, []
    for line in (run / 'owner-samples.jsonl').open(encoding='utf-8'):
        data = json.loads(line)
        if data['kind'] != 'sample':
            continue
        local = next((pawn for pawn in data['pawns'] if pawn['local']), None)
        if not local:
            continue
        montage = local.get('montage') or ''
        vault_seen |= 'Vault' in montage
        flying_without_montage = vault_seen and local['mode'] == 'MOVE_FLYING' and not montage
        if flying_without_montage:
            if phase is None:
                phase = {'start_utc': data['utc'], 'start_x': local['position'][0], 'speed': local['velocity'][0]}
            # The sample after the phase may already carry the server's correction; keep the last flying position.
            phase.update(end_utc=data['utc'], end_x=local['position'][0])
        elif phase is not None:
            phases.append(phase)
            phase = None
    if phase is not None:
        phases.append(phase)
    corrections = [float(match['distance']) for line in (run / 'host.log').open(encoding='utf-8', errors='replace')
                   if (match := CORRECTION.search(line))]
    result = {
        'run': run.name,
        'client_only_flying_phases': [{'duration_s': round(p['end_utc'] - p['start_utc'], 3),
                                       'travel_cm': round(p['end_x'] - p['start_x'], 1),
                                       'speed_cm_s': round(p['speed'], 1)} for p in phases],
        'server_corrections': len(corrections),
        'max_server_correction_cm': round(max(corrections), 2) if corrections else 0.0,
    }
    (run / 'cmc-handoff.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    for folder in sys.argv[1:]:
        if (Path(folder) / 'owner-samples.jsonl').is_file():
            print(json.dumps(analyse(Path(folder))))
