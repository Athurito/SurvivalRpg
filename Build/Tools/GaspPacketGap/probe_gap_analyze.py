"""Check that a recorded incoming-packet pause holds a frozen proxy's montage."""
import argparse
import json
from pathlib import Path


def frozen_intervals(rows, target, start, end):
    """Retain all frozen pairs, but measure sufficiency on contiguous sample runs only."""
    pairs, runs, current = [], [], []
    previous = None

    def finish_run():
        if current:
            runs.append({'pair_count': len(current), 'duration_ms': sum(pair['interval_ms'] for pair in current),
                         'start_utc': current[0]['before_utc'], 'end_utc': current[-1]['utc']})
            current.clear()

    for row in rows:
        if row['kind'] != 'sample' or not start <= row['utc'] <= end:
            continue
        pawns = [pawn for pawn in row['pawns'] if pawn['player_id'] == target]
        if len(pawns) != 1:
            finish_run()
            previous = None
            continue
        pawn = pawns[0]
        frozen_pair = None
        if previous:
            old_time, old = previous
            distance = sum((a - b) ** 2 for a, b in zip(pawn['visual_position'], old['visual_position'])) ** .5
            if row['utc'] > old_time and distance < .001 and old['mode'] == pawn['mode'] == 'Traversing':
                same = pawn['montage'] is not None and pawn['montage'] == old['montage']
                delta = (pawn['montage_position'] - old['montage_position']) * 1000 if same else None
                frozen_pair = {'before_utc': old_time, 'utc': row['utc'], 'interval_ms': (row['utc'] - old_time) * 1000,
                               'distance_cm': distance, 'same_montage': same, 'phase_advance_ms': delta}
                pairs.append(frozen_pair)
        if frozen_pair:
            current.append(frozen_pair)
        else:
            finish_run()
        previous = row['utc'], pawn
    finish_run()
    return pairs, runs


def analyze_role(rows, target, log):
    start, end = None, None
    for row in rows:
        if start is None and row['kind'] == 'packet_gap_start':
            start = row
        elif start and row['kind'] in ('packet_gap_end', 'packet_gap_aborted'):
            end = row
            break
    complete = bool(start and end and end['kind'] == 'packet_gap_end' and end['utc'] >= start['utc'])
    pairs, runs = frozen_intervals(rows, target, start['utc'], end['utc']) if start and end else ([], [])
    activated = 'PktIncomingLoss set to 100' in log and 'PktIncomingLoss set to 0' in log
    sufficient = any(run['pair_count'] >= 3 and run['duration_ms'] >= 40 for run in runs)
    held = sufficient and all(pair['same_montage'] and abs(pair['phase_advance_ms']) < 1 for pair in pairs)
    return {'gap_observed': bool(start and end), 'gap_completed': complete,
            'gap_aborted': bool(end and end['kind'] == 'packet_gap_aborted'),
            'driver_confirmed_packet_loss': activated, 'frozen_frame_pairs': pairs, 'consecutive_frozen_runs': runs,
            'sufficient_frozen_interval': sufficient, 'montage_held_with_frozen_movement': held,
            'passed': complete and activated and held}


def analyze_run(run):
    run = Path(run)
    manifest = json.loads((run / 'manifest.json').read_text(encoding='utf-8'))
    target = json.loads((run / 'trigger.json').read_text(encoding='utf-8'))['owner_player_id']
    results = {}
    for role in manifest['processes']:
        if role == manifest['arguments']['driver']:
            continue
        rows = [json.loads(line) for line in (run / (role + '-samples.jsonl')).read_text(encoding='utf-8').splitlines()]
        log = (run / (role + '.log')).read_text(encoding='utf-8', errors='replace')
        results[role] = analyze_role(rows, target, log)
    return {'contract_version': 2, 'process_status': manifest.get('status'),
            'contract': 'During a completed incoming packet gap, require one contiguous run of >=3 frozen Traversing frame pairs spanning >=40ms with a present, equally frozen montage. Every observed frozen pair must hold its phase. No claim of smoothness during loss.',
            'roles': results, 'passed': manifest.get('status') == 'measured' and bool(results) and all(role['passed'] for role in results.values())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', type=Path, help='New output JSON; defaults to RUN/gap-analysis.json')
    args = parser.parse_args()
    result = analyze_run(args.run)
    output = args.output or args.run / 'gap-analysis.json'
    with output.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
