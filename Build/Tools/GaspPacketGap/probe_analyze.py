"""Compare a remote Vault pose with authority at the same displayed spatial passage.

UTC is used only to select this single action, not to demand zero network latency.
The correspondence is the first forward crossing of each obstacle-relative plane.
All locations are observed; this analyzer cannot alter gameplay or test output.
"""
import argparse
import json
from pathlib import Path
import statistics


def phase(pawn):
    montage = pawn.get('montage')
    return pawn.get('montage_position') if montage and '/Vault/' in montage else None


def crossing(rows, plane, earliest):
    for (before, a), (after, b) in zip(rows, rows[1:]):
        if after['utc'] < earliest:
            continue
        ax, bx = a['visual_position'][0], b['visual_position'][0]
        if not ax <= plane < bx:
            continue
        alpha = (plane - ax) / (bx - ax)
        pa, pb = phase(a), phase(b)
        same = bool(a.get('montage')) and a.get('montage') == b.get('montage')
        return {
            'utc': before['utc'] + (after['utc'] - before['utc']) * alpha,
            'phase': pa + (pb - pa) * alpha if same and pa is not None and pb is not None else None,
            'montage': a.get('montage') if same else None,
            'bracket_ms': (after['utc'] - before['utc']) * 1000,
            'before': {'mode': a['mode'], 'x': ax, 'phase': pa},
            'after': {'mode': b['mode'], 'x': bx, 'phase': pb},
        }
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('run', type=Path)
    parser.add_argument('--output', default='analysis-v2.json')
    args = parser.parse_args()
    run = args.run.resolve()
    manifest = json.loads((run / 'manifest.json').read_text(encoding='utf-8'))
    trigger = json.loads((run / 'trigger.json').read_text(encoding='utf-8'))
    player_id, lane = trigger['owner_player_id'], trigger['lane']
    records, samples = {}, {}
    for role in manifest['processes']:
        records[role] = [json.loads(line) for line in (run / (role + '-samples.jsonl')).read_text(encoding='utf-8').splitlines()]
        samples[role] = [(record, pawn) for record in records[role]
                         if record['kind'] == 'sample' and record['utc'] >= trigger['start_utc']
                         for pawn in record['pawns'] if pawn['player_id'] == player_id and pawn.get('visual_position')]
    authority = samples['host']
    authority_active = [(record, pawn) for record, pawn in authority if phase(pawn) is not None]
    if not authority_active:
        raise RuntimeError('No authority Vault montage observed; no phase comparison is possible')
    earliest = authority_active[0][0]['utc'] - 0.1
    front, rear = lane['barrier']['min'][0], lane['barrier']['max'][0]
    planes = sorted(set([front - 150 + step * 20 for step in range(int((rear - front + 230) / 20) + 1)] + [front, rear]))
    authority_crossings = []
    excluded_reference_crossings = []
    for plane in planes:
        observed = crossing(authority, plane, earliest)
        if observed and observed['phase'] is not None and observed['phase'] > 0.03:
            # Authority GAS can start before its first fixed movement step. A Walking->Traversing
            # sampling bracket is not an already-active physical Vault reference for proxy pose.
            # Keep excluded points visible; proxy startup gaps in active references remain failures.
            if observed['before']['mode'] == observed['after']['mode'] == 'Traversing':
                authority_crossings.append((plane, observed))
            else:
                excluded_reference_crossings.append({'plane_x': plane, 'authority': observed,
                                                     'reason': 'Authority movement is not Traversing throughout this sampling bracket'})
    result = {'run': str(run), 'driver': trigger.get('driver', 'owner'), 'gait': trigger.get('gait', 'stand'),
              'player_id': player_id, 'process_status': manifest['status'], 'authority_crossing_count': len(authority_crossings),
              'contract_version': 2, 'excluded_reference_crossings': excluded_reference_crossings,
              'contract': 'Compare visible montage phase when each peer displays the same forward obstacle-relative plane; fixed-step plus actual sampling-bracket tolerance. No zero-latency requirement.',
              'limits': 'Uncooked loopback, no bone-pose readback, no native NP traversal payload; crossing correspondence requires this single forward route. Normal run only unless launcher explicitly adds another scenario.',
              'roles': {}}
    for role, rows in samples.items():
        comparisons = []
        for plane, expected in authority_crossings:
            actual = crossing(rows, plane, earliest)
            # A single fixed step plus the two independently observed sampling brackets.
            # This admits render/sample quantization; it does not admit the 100-ms NP buffer.
            allowance = 20.0 + expected['bracket_ms'] + (actual['bracket_ms'] if actual else 0)
            lead = ((actual['phase'] - expected['phase']) * 1000
                    if actual and actual['phase'] is not None else None)
            same_montage = bool(actual) and actual['montage'] == expected['montage']
            comparisons.append({'plane_x': plane, 'authority': expected, 'observed': actual,
                                'phase_lead_ms': lead, 'allowance_ms': allowance,
                                'same_montage': same_montage,
                                'passed': same_montage and lead is not None and abs(lead) <= allowance})
        traversing = [(r, p) for r, p in rows if p['mode'] == 'Traversing']
        falling = [(r, p) for r, p in rows if p['falling'] and p['position'][0] > rear
                   and traversing and r['utc'] > traversing[0][0]['utc']]
        landed = [(r, p) for r, p in rows if p['grounded'] and p['position'][0] > rear
                  and p['feet_z'] < lane['deck']['max'][2] - 50 and falling and r['utc'] > falling[0][0]['utc']]
        deltas = [r['delta_seconds'] for r, _ in rows if r['delta_seconds'] > 0]
        leads = [entry['phase_lead_ms'] for entry in comparisons if entry['phase_lead_ms'] is not None]
        text = (run / (role + '.log')).read_text(encoding='utf-8', errors='replace')
        findings = {needle: text.count(needle) for needle in ('Ensure condition failed', 'Fatal error',
                    'are stuck giving time back', 'Independent Interpolation starved', 'FailureType = ')}
        checks = {'authority_crossing_coverage': len(authority_crossings) >= 2,
                  'phase_agrees_at_all_observed_crossings': bool(comparisons) and all(item['passed'] for item in comparisons),
                  'crossed_rear_while_traversing': any(p['position'][0] > rear for _, p in traversing),
                  'fell_beyond_rear': bool(falling), 'landed_on_lower_floor': bool(landed),
                  'no_ensure_fatal_or_time_refund': all(findings[n] == 0 for n in ('Ensure condition failed', 'Fatal error', 'are stuck giving time back'))}
        result['roles'][role] = {'checks': checks, 'passed': all(checks.values()), 'samples': len(rows),
                                'median_fps': statistics.median(1 / d for d in deltas) if deltas else None,
                                'maximum_abs_phase_error_ms': max(map(abs, leads)) if leads else None,
                                'median_phase_lead_ms': statistics.median(leads) if leads else None,
                                'comparisons': comparisons, 'log_findings': findings}
    result['passed'] = manifest['status'] == 'measured' and all(value['passed'] for value in result['roles'].values())
    with (run / args.output).open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result, indent=2))
    print(json.dumps({key: value for key, value in result.items() if key != 'roles'}, indent=2))
    for role, value in result['roles'].items():
        print(role, json.dumps({key: val for key, val in value.items() if key != 'comparisons'}))


if __name__ == '__main__':
    main()
