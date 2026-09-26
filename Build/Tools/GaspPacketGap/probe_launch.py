"""Scoped, separate-process Vault probe. Launch only when the editor/build owner requests it.

Every input is an existing Enhanced Input action. No gameplay transform, velocity,
movement mode, experience, asset or save data is written. Exact child handles own
cleanup; quit is requested in each game process before bounded terminate fallback.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[2]
PROJECT = ROOT / 'SurvivalRpg.uproject'
MAP = '/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover'


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2), encoding='utf-8')
    temporary.replace(path)


def preserved_files():
    """Hash maps and existing saves; additions/removals count as changes too."""
    paths = sorted([*ROOT.glob('Content/**/*.umap'), *ROOT.glob('Plugins/GameFeatures/**/*.umap'),
                    *ROOT.glob('Saved/SaveGames/**/*')])
    return {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def provenance(editor):
    """Record exact local scripts and loaded binary candidates without claiming a fresh build."""
    paths = [*BASE.glob('*.py'), editor, PROJECT,
             ROOT / 'Binaries/Win64/UnrealEditor-SurvivalRpg.dll',
             ROOT / 'Plugins/Mover/Binaries/Win64/UnrealEditor-Mover.dll',
             ROOT / 'Plugins/NetworkPrediction/Binaries/Win64/UnrealEditor-NetworkPrediction.dll']
    hashes = {}
    for path in paths:
        if path.is_file():
            with path.open('rb') as stream:
                hashes[str(path)] = hashlib.file_digest(stream, 'sha256').hexdigest()
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    status = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
    return {'head': head, 'working_tree': status, 'sha256': hashes,
            'note': 'Binary hashes identify existing artifacts, not source/build equivalence. Check module paths in logs.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('label')
    parser.add_argument('--engine', type=Path, required=True, help='Licensed UE 5.8.2 Engine directory')
    parser.add_argument('--output', type=Path, default=ROOT / 'Saved/GaspPacketGap',
                        help='Evidence parent directory inside this checkout Saved directory')
    parser.add_argument('--host-fps', type=int, default=60)
    parser.add_argument('--driver', choices=('host', 'owner'), default='host')
    parser.add_argument('--gait', choices=('stand', 'run'), default='run')
    parser.add_argument('--owner-fps', type=int, default=60)
    parser.add_argument('--observer-fps', type=int, default=60)
    parser.add_argument('--without-observer', action='store_true')
    parser.add_argument('--smoke', action='store_true')
    parser.add_argument('--warmup', type=float, default=15)
    parser.add_argument('--lane', type=int, choices=(0, 1), default=0)
    parser.add_argument('--port', type=int, default=17992)
    parser.add_argument('--observer-gap-ms', type=int, default=0)
    args = parser.parse_args()
    editor = args.engine.resolve() / 'Binaries/Win64/UnrealEditor.exe'
    if os.name != 'nt' or not editor.is_file() or not PROJECT.is_file():
        parser.error('This probe requires Windows and a built SurvivalRpg checkout with UnrealEditor.exe')
    if any(fps < 1 or fps > 240 for fps in (args.host_fps, args.owner_fps, args.observer_fps)):
        parser.error('Render limits must be between 1 and 240 FPS')
    if not math.isfinite(args.warmup) or not 0 <= args.warmup <= 60 or not 1024 <= args.port <= 65535:
        parser.error('Warmup must be 0..60 seconds and port 1024..65535')
    if not 0 <= args.observer_gap_ms <= 1000:
        raise SystemExit('Observer gap must be between zero and 1000 ms')
    if args.observer_gap_ms and args.driver != 'host':
        parser.error('Packet-gap measurement requires --driver host; never drop Authority input')
    if not args.label.replace('-', '').replace('_', '').isalnum():
        raise SystemExit('Use an alphanumeric run label')
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / 'Saved').resolve()):
        parser.error('Evidence output must remain inside this checkout Saved directory')
    subprocess.run([sys.executable, str(ROOT / 'Build/Patches/NetworkPrediction/prepare.py'), 'verify'],
                   cwd=ROOT, check=True)
    output.mkdir(parents=True, exist_ok=True)
    run = output / ('probe_run_' + args.label)
    run.mkdir(exist_ok=False)
    before = preserved_files()
    write_json(run / 'preservation-before.json', before)
    children = {}
    manifest = {'arguments': {key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
                'created_utc': time.time(), 'processes': {}, 'provenance': provenance(editor),
                'input': 'IA_Move plus IA_Jump (the existing Space action), with real mapped handlers; no gameplay-state or transform writes',
                'scope': 'Uncooked UnrealEditor -game, ordinary separate-process transport, original saved Mover map',
                'role_names': 'host/owner/observer are process labels; inspect player_id/net_role. With driver=host, owner observes that pawn as a SimulatedProxy.',
                'limits': 'Not a cooked/packaged build, forced correction test, exact GAS end-delegate or historical NP-state verification'}

    def save():
        write_json(run / 'manifest.json', manifest)

    def launch(role, fps):
        url = MAP + '?listen' if role == 'host' else '127.0.0.1:' + str(args.port) + '?PlayerProfileId=' + str(uuid.uuid4())
        # UE 5.8 Python accepts an unquoted filename with spaces up to .py. An
        # inner quote would prematurely close UE's outer -ExecCmds value.
        commands = 't.MaxFPS ' + str(fps) + ',r.VSync 0,r.ScreenPercentage 25,np.PrintReconciles 1,py ' + (BASE / 'probe_runtime.py').as_posix()
        command = [str(editor), str(PROJECT), url, '-game', '-nosteam', '-NoSaveConfig', '-NoSplash',
                   '-NoLoadingScreen', '-NoSound', '-unattended', '-ForceEnablePython', '-windowed',
                   '-ResX=640', '-ResY=360', '-port=' + str(args.port), '-RpgVaultProbeRole=' + role,
                   '-RpgVaultProbeLane=' + str(args.lane),
                   '-RpgVaultProbeDriver=' + args.driver, '-RpgVaultProbeGait=' + args.gait,
                   '-RpgVaultProbeGapMS=' + str(args.observer_gap_ms),
                   '-LogCmds=LogRpgAbilitySystem Verbose',
                   '-abslog=' + str(run / (role + '.log')), '-ExecCmds=' + commands]
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        environment = os.environ.copy()
        # A child-only environment value avoids Windows/UE nested path quoting.
        environment['SURVIVALRPG_VAULT_PROBE_RUN'] = str(run)
        process = subprocess.Popen(command, cwd=str(PROJECT.parent), startupinfo=startup, env=environment)
        children[role] = process
        manifest['processes'][role] = {'pid': process.pid, 'command': command, 'target_fps': fps,
                                     'probe_run_environment': str(run)}
        save()
        print(json.dumps({'launched': role, 'pid': process.pid, 'run': str(run)}), flush=True)

    def wait_file(role, suffix, timeout):
        deadline = time.monotonic() + timeout
        filename = run / (role + suffix)
        while time.monotonic() < deadline:
            error = run / (role + '-error.txt')
            if error.exists():
                raise RuntimeError(error.read_text(encoding='utf-8'))
            if filename.exists():
                return json.loads(filename.read_text(encoding='utf-8'))
            if children[role].poll() is not None:
                raise RuntimeError(role + ' exited: ' + str(children[role].returncode))
            time.sleep(0.5)
        raise RuntimeError('Timed out waiting for ' + str(filename))

    try:
        launch('host', args.host_fps)
        host = wait_file('host', '-ready.json', 180)
        if args.smoke:
            manifest['status'] = 'smoke_passed'
            return
        launch('owner', args.owner_fps)
        owner = wait_file('owner', '-ready.json', 150)
        if not args.without_observer:
            launch('observer', args.observer_fps)
            wait_file('observer', '-ready.json', 150)
        print('All roles ready; warming for ' + str(args.warmup) + ' seconds.', flush=True)
        time.sleep(args.warmup)
        trigger = {'start_utc': time.time() + 2, 'duration_seconds': 75,
                   'owner_player_id': (host if args.driver == 'host' else owner)['player_id'],
                   'driver': args.driver, 'gait': args.gait, 'lane': owner['lane']}
        write_json(run / 'trigger.json', trigger)
        print(json.dumps({'measurement_started': trigger}), flush=True)
        manifest['route'] = wait_file(args.driver, '-route-done.json', 85)
        write_json(run / 'finish.json', {'finish_utc': time.time() + 5})
        for role in children:
            wait_file(role, '-finished.json', 20)
        manifest['status'] = 'measured'
        print('Vault route measured; requesting scoped game shutdown.', flush=True)
    except BaseException as error:
        manifest['status'] = 'failed'
        manifest['error'] = str(error)
        print('PROBE FAILED: ' + str(error), flush=True)
        raise
    finally:
        write_json(run / 'stop.json', {'utc': time.time()})
        cleanup_failures = []
        # Only these exact Popen handles may be terminated. Existing user editors are untouched.
        for role, process in children.items():
            graceful = True
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                graceful = False
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=10)
            manifest['processes'][role]['graceful_quit'] = graceful
            manifest['processes'][role]['exit_code'] = process.returncode
            if not graceful or process.returncode != 0:
                cleanup_failures.append(role)
        manifest['finished_utc'] = time.time()
        after = preserved_files()
        changed = sorted(path for path in before.keys() | after.keys() if before.get(path) != after.get(path))
        write_json(run / 'preservation-after.json', after)
        manifest['preservation'] = {'files_before': len(before), 'files_after': len(after), 'changed': changed}
        manifest['cleanup_failures'] = cleanup_failures
        if cleanup_failures:
            manifest['status'] = 'failed'
        if changed:
            manifest['status'] = 'failed'
            manifest['preservation_error'] = 'Maps or saves changed; original hashes are retained, no restoration attempted'
        save()
        if changed:
            raise RuntimeError(manifest['preservation_error'])
        if cleanup_failures:
            raise RuntimeError('Probe children did not exit normally: ' + ', '.join(cleanup_failures))


if __name__ == '__main__':
    main()
