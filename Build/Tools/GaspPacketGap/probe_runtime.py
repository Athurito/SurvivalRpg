"""Vault observation and input driver, loaded inside one scoped -game process.

Input uses public Enhanced Input injection for IA_Move/IA_Jump. Positions, capsule
bounds, montages and Mover state are read-only. Real rollback delegates are counted;
no artificial corrections or engine interpolation resets are introduced.
"""
import json
import math
import os
from pathlib import Path
import re
import time
import traceback
import unreal


def argument(name):
    match = re.search(r'-' + name + r'=(?:"([^"]+)"|(\S+))', unreal.SystemLibrary.get_command_line())
    if not match:
        raise RuntimeError('Missing scoped probe argument ' + name)
    return match.group(1) or match.group(2)


ROLE = argument('RpgVaultProbeRole')
RUN = Path(os.environ['SURVIVALRPG_VAULT_PROBE_RUN']).resolve()
BASE = Path(__file__).resolve().parent
SAVED = (BASE.parents[2] / 'Saved').resolve()
LANE_INDEX = int(argument('RpgVaultProbeLane'))
DRIVER = argument('RpgVaultProbeDriver')
GAIT = argument('RpgVaultProbeGait')
try:
    GAP_MS = int(argument('RpgVaultProbeGapMS'))
except RuntimeError:
    GAP_MS = 0
if DRIVER not in ('host', 'owner') or GAIT not in ('stand', 'run'):
    raise RuntimeError('Invalid scoped driver or gait')
if not 0 <= GAP_MS <= 1000 or (GAP_MS and DRIVER != 'host'):
    raise RuntimeError('A bounded packet gap is supported only for a host-driven traversal')
if ROLE not in ('host', 'owner', 'observer') or SAVED not in RUN.parents or LANE_INDEX not in (0, 1):
    raise RuntimeError('Invalid scoped probe role, lane or output directory')
LOG = (RUN / (ROLE + '-samples.jsonl')).open('x', encoding='utf-8', buffering=1)
STATE = {'world': None, 'controller': None, 'pawns': {}, 'input': None, 'ready': False,
         'trigger': None, 'last_discover': 0, 'last_sample': 0, 'started': time.time(),
         'finished': False, 'quitting': False, 'stage': 'ready', 'waypoint': 0,
         'contact_since': None, 'pressed_at': None, 'route_done': False, 'last_drive': None}
STATE['gap_started'] = None
STATE['gap_finished'] = False


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2), encoding='utf-8')
    temporary.replace(path)


def record(kind, **values):
    LOG.write(json.dumps({'kind': kind, 'role': ROLE, 'pid': os.getpid(), 'utc': time.time(),
                          'frame': unreal.SystemLibrary.get_frame_count(), **values}, default=str) + '\n')


def vec(value):
    return [value.x, value.y, value.z]


def step_value(value):
    return {'server_frame': value.server_frame, 'base_ms': value.base_sim_time_ms,
            'step_ms': value.step_ms, 'resimulating': value.is_resimulating}


def bounds(component):
    origin, extent, _radius = unreal.SystemLibrary.get_component_bounds(component)
    return {'min': [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
            'max': [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
            'center': vec(origin)}


def find_lane(world):
    barriers = sorted(unreal.GameplayStatics.get_all_actors_with_tag(world, 'Rpg.TraversalTest.Vault'),
                      key=lambda actor: actor.get_actor_location().y)
    if len(barriers) <= LANE_INDEX:
        raise RuntimeError('Saved Mover map is missing the requested tagged Vault lane')
    barrier = barriers[LANE_INDEX]
    physical = barrier.get_component_by_class(unreal.StaticMeshComponent)
    result = {'actor': barrier.get_path_name(), 'barrier': bounds(physical)}
    for actor in unreal.GameplayStatics.get_all_actors_with_tag(world, 'Rpg.TraversalTest.Vault.Approach'):
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        box = bounds(component)
        if abs(box['center'][1] - result['barrier']['center'][1]) < 10:
            result['deck'] = box
            break
    if 'deck' not in result or abs(result['barrier']['max'][2] - result['deck']['max'][2] - 100) > 2:
        raise RuntimeError('The original one-metre Vault deck/barrier contract changed: ' + str(result))
    return result


def warp_targets(row):
    warping = row['warping']
    if not warping or not hasattr(warping, 'find_warp_target'):
        return None
    result = {}
    try:
        for name in ('FrontLedge', 'BackLedge'):
            target, found = warping.find_warp_target(name)
            result[name] = {'found': bool(found), 'location': vec(target.location) if found else None}
        return result
    except Exception as error:
        if not row.get('warp_unavailable'):
            row['warp_unavailable'] = str(error)
            record('optional_capability_unavailable', pawn=row['pawn'].get_name(),
                   capability='read-only MotionWarping target query', error=str(error))
        return None


def observe(row):
    pawn, mover = row['pawn'], row['mover']
    player_state = pawn.get_editor_property('player_state')
    visual = mover.get_primary_visual_component()
    animation = visual.get_anim_instance() if visual else None
    montage = animation.get_current_active_montage() if animation else None
    position = pawn.get_actor_location()
    capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
    result = {'pawn': pawn.get_name(), 'player_id': player_state.get_editor_property('player_id') if player_state else None,
              'local': pawn.is_locally_controlled(), 'net_role': str(pawn.get_local_role()),
              'position': vec(position), 'velocity': vec(mover.get_velocity()),
              'visual_position': vec(visual.get_world_location()) if visual else None,
              'feet_z': position.z - capsule.get_scaled_capsule_half_height() if capsule else None,
              'mode': str(mover.get_movement_mode_name()), 'grounded': mover.is_on_ground(), 'falling': mover.is_falling(),
              'montage': montage.get_path_name() if montage else None,
              'montage_position': animation.montage_get_position(montage) if montage else None,
              'montage_playing': animation.montage_is_playing(montage) if montage else False,
              'steps': row['steps'], 'resim_steps': row['resim_steps'], 'rollbacks': row['rollbacks'], 'last_step': row['last_step']}
    signature = (result['mode'], result['grounded'], result['montage'])
    if signature != row['last_signature']:
        previous = row['last_observation']
        record('transition', pawn=result, previous=previous, warp_targets=warp_targets(row))
        row['last_signature'] = signature
    row['last_observation'] = result
    if result['montage'] and '/Vault/' in result['montage']:
        row['seen_vault'] = True
    row['seen_traversing'] |= result['mode'] == 'Traversing'
    trigger = STATE['trigger']
    if trigger and result['player_id'] == trigger['owner_player_id']:
        lane = trigger['lane']
        crossed = position.x > lane['barrier']['max'][0]
        row['crossed'] |= crossed and result['mode'] == 'Traversing'
        row['fallen_after'] |= row['seen_traversing'] and crossed and result['falling']
        row['landed_beyond'] |= row['fallen_after'] and result['grounded'] and crossed and result['feet_z'] < lane['deck']['max'][2] - 50
    return result


def bind(row):
    def on_step(step):
        row['steps'] += 1
        row['resim_steps'] += int(step.is_resimulating)
        row['last_step'] = step_value(step)

    def on_rollback(current, expunged):
        row['rollbacks'] += 1
        record('rollback', pawn=row['pawn'].get_name(), current=step_value(current), expunged=step_value(expunged),
               position=vec(row['pawn'].get_actor_location()))

    def on_finalize(_sync, _aux):
        if not STATE['finished']:
            observe(row)

    row['mover'].on_post_simulation_tick.add_callable(on_step)
    row['mover'].on_post_simulation_rollback.add_callable(on_rollback)
    row['mover'].on_post_finalize.add_callable(on_finalize)
    row['delegates'] = (on_step, on_rollback, on_finalize)


def discover(now):
    if now - STATE['last_discover'] < 0.5:
        return
    STATE['last_discover'] = now
    if STATE['world'] is None:
        for world in unreal.ObjectIterator(unreal.World):
            if 'Lvl_RpgGaspMover' not in world.get_path_name():
                continue
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            if controller and controller.is_local_controller() and isinstance(controller.get_controlled_pawn(), unreal.RpgMoverPawn):
                STATE['world'], STATE['controller'] = world, controller
                break
    world = STATE['world']
    if world is None:
        return
    if ROLE == 'host' and not STATE['ready']:
        mode = unreal.GameplayStatics.get_game_mode(world)
        if not mode or mode.get_editor_property('bEnableDiskPersistence'):
            raise RuntimeError('Probe requires the saved isolated Mover map with disk persistence disabled')
    for pawn in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RpgMoverPawn):
        key = pawn.get_path_name()
        if key in STATE['pawns']:
            continue
        mover = pawn.get_component_by_class(unreal.MoverComponent)
        if not mover or not mover.get_primary_visual_component():
            continue
        row = {'pawn': pawn, 'mover': mover, 'warping': pawn.get_component_by_class(unreal.MotionWarpingComponent),
               'steps': 0, 'resim_steps': 0, 'rollbacks': 0, 'last_step': None, 'last_signature': None,
               'last_observation': None, 'seen_vault': False, 'seen_traversing': False,
               'crossed': False, 'fallen_after': False, 'landed_beyond': False}
        STATE['pawns'][key] = row
        bind(row)
        record('pawn', path=key, local=pawn.is_locally_controlled(), net_role=str(pawn.get_local_role()))
    controller = STATE['controller']
    pawn = controller.get_controlled_pawn()
    if STATE['ready'] or pawn.get_path_name() not in STATE['pawns']:
        return
    lane = find_lane(world)
    if ROLE == DRIVER:
        subsystems = [item for item in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                      if 'Default__' not in item.get_path_name()]
        if len(subsystems) != 1:
            raise RuntimeError('Expected exactly one runtime local input subsystem')
        STATE['input'] = subsystems[0]
        STATE['move_action'] = unreal.load_asset('/Game/SurvivalRpg/Characters/GASP/Shared/Input/IA_Move')
        STATE['jump_action'] = unreal.load_asset('/Game/SurvivalRpg/Characters/GASP/Shared/Input/IA_Jump')
        if not STATE['move_action'] or not STATE['jump_action']:
            raise RuntimeError('Existing mapped movement/Space actions are missing')
        STATE['input'].inject_input_vector_for_action(STATE['move_action'], unreal.Vector(), [], [])
        STATE['input'].inject_input_vector_for_action(STATE['jump_action'], unreal.Vector(), [], [])
        # The authored steps extend 1,000 cm west of the deck. Stay another 150 cm
        # west while changing lane to avoid other players and the existing Mantle blocks.
        west = lane['deck']['min'][0] - 1150
        start = pawn.get_actor_location()
        STATE['waypoints'] = [(west, start.y), (west, lane['barrier']['center'][1]),
                              (lane['deck']['min'][0] + 200, lane['barrier']['center'][1])]
        STATE['lane'] = lane
    STATE['ready'] = True
    info = {'pid': os.getpid(), 'role': ROLE, 'utc': now, 'world': world.get_path_name(), 'pawn': pawn.get_name(),
            'player_id': pawn.get_editor_property('player_state').get_editor_property('player_id'), 'lane': lane}
    write_json(RUN / (ROLE + '-ready.json'), info)
    record('ready', info=info, waypoints=STATE.get('waypoints'))


def drive_world(dx, dy, jump=False):
    if ROLE != DRIVER or not STATE['input']:
        return
    yaw = math.radians(STATE['controller'].get_control_rotation().yaw)
    # IA_Move is a conventional Axis2D action: X right, Y forward. Transform
    # desired world movement into the current camera basis without changing view yaw.
    right = -math.sin(yaw) * dx + math.cos(yaw) * dy
    forward = math.cos(yaw) * dx + math.sin(yaw) * dy
    STATE['input'].inject_input_vector_for_action(STATE['move_action'], unreal.Vector(right, forward, 0), [], [])
    STATE['input'].inject_input_vector_for_action(STATE['jump_action'], unreal.Vector(1 if jump else 0, 0, 0), [], [])
    changed = (STATE['stage'], bool(jump))
    if STATE['last_drive'] != changed:
        record('input', stage=STATE['stage'], world_direction=[dx, dy], action_value=[right, forward], jump=jump)
        STATE['last_drive'] = changed


def drive(now, elapsed):
    if ROLE != DRIVER or elapsed < 0:
        return
    pawn = STATE['controller'].get_controlled_pawn()
    row = STATE['pawns'][pawn.get_path_name()]
    position = pawn.get_actor_location()
    observation = row['last_observation'] or observe(row)
    if STATE['route_done']:
        drive_world(0, 0)
        return
    if STATE['pressed_at'] is not None:
        STATE['stage'] = 'vault'
        # One ordinary press, then release; forward intent preserves source handoff.
        drive_world(1, 0, now - STATE['pressed_at'] < 0.075)
        if row['landed_beyond']:
            STATE['route_done'] = True
            STATE['stage'] = 'complete'
            drive_world(0, 0)
            result = {'utc': now, 'elapsed': elapsed, 'seen_vault': row['seen_vault'], 'crossed_while_traversing': row['crossed'],
                      'seen_falling': row['fallen_after'], 'landed_beyond': row['landed_beyond'], 'observation': observation}
            write_json(RUN / (DRIVER + '-route-done.json'), result)
            record('route_done', **result)
        elif now - STATE['pressed_at'] > 10:
            raise RuntimeError('Vault did not cross/fall/land after the real Space action; inspect recorded montage and mode transitions')
        return
    if STATE['waypoint'] < len(STATE['waypoints']):
        target = STATE['waypoints'][STATE['waypoint']]
        dx, dy = target[0] - position.x, target[1] - position.y
        length = math.hypot(dx, dy)
        if length < 65:
            STATE['waypoint'] += 1
            record('waypoint', index=STATE['waypoint'], position=vec(position))
            return
        STATE['stage'] = 'waypoint_' + str(STATE['waypoint'])
        drive_world(dx / length, dy / length)
    else:
        STATE['stage'] = 'standing_contact' if GAIT == 'stand' else 'running_approach'
        target_y = STATE['lane']['barrier']['center'][1]
        direction_y = max(-0.3, min(0.3, (target_y - position.y) / 250))
        drive_world(1, direction_y)
        speed = math.hypot(*observation['velocity'][:2])
        capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
        distance = STATE['lane']['barrier']['min'][0] - position.x
        on_deck = abs(observation['feet_z'] - STATE['lane']['deck']['max'][2]) < 10
        if GAIT == 'run' and observation['grounded'] and on_deck and distance <= 170 and speed > 250:
            STATE['pressed_at'] = now
            record('space_pressed', elapsed=elapsed, gait=GAIT, speed=speed, distance=distance, observation=observation)
            drive_world(1, 0, True)
        elif GAIT == 'stand' and observation['grounded'] and on_deck and distance <= capsule.get_scaled_capsule_radius() + 6 and speed < 5:
            if STATE['contact_since'] is None:
                STATE['contact_since'] = now
            if now - STATE['contact_since'] > 0.25:
                STATE['pressed_at'] = now
                record('space_pressed', elapsed=elapsed, speed=speed, distance=distance, observation=observation)
                drive_world(1, 0, True)
        else:
            STATE['contact_since'] = None
    if elapsed > 50:
        raise RuntimeError('Real-input navigation did not reach the prepared standing Vault within 50 seconds')


def sample(now):
    if now - STATE['last_sample'] < 0.012 or STATE['world'] is None:
        return
    STATE['last_sample'] = now
    record('sample', delta_seconds=unreal.GameplayStatics.get_world_delta_seconds(STATE['world']),
           world_seconds=unreal.SystemLibrary.get_game_time_in_seconds(STATE['world']),
           pawns=[observe(row) for row in STATE['pawns'].values()])


def observer_gap(now):
    if not GAP_MS or ROLE == DRIVER or STATE['gap_finished']:
        return
    if STATE['gap_started'] is not None:
        if now - STATE['gap_started'] >= GAP_MS / 1000:
            unreal.SystemLibrary.execute_console_command(STATE['world'], 'NetEmulation.PktIncomingLoss 0', STATE['controller'])
            STATE['gap_finished'] = True
            record('packet_gap_end', actual_ms=(now - STATE['gap_started']) * 1000)
        return
    trigger = STATE['trigger']
    if not trigger:
        return
    for row in STATE['pawns'].values():
        observed = row['last_observation']
        if (observed and observed['player_id'] == trigger['owner_player_id']
                and observed['montage'] and '/Vault/' in observed['montage']
                and observed['mode'] == 'Traversing' and .30 <= observed['montage_position'] <= .60):
            unreal.SystemLibrary.execute_console_command(STATE['world'], 'NetEmulation.PktIncomingLoss 100', STATE['controller'])
            STATE['gap_started'] = now
            record('packet_gap_start', requested_ms=GAP_MS, observed=observed)
            break


def restore_gap(reason):
    """Undo this process's temporary emulation even when the measurement aborts."""
    if STATE['gap_started'] is not None and not STATE['gap_finished']:
        unreal.SystemLibrary.execute_console_command(STATE['world'], 'NetEmulation.PktIncomingLoss 0', STATE['controller'])
        STATE['gap_finished'] = True
        record('packet_gap_aborted', reason=reason, actual_ms=(time.time() - STATE['gap_started']) * 1000)


def tick(_delta_seconds):
    try:
        if (RUN / 'stop.json').exists() and not STATE['quitting']:
            STATE['quitting'] = True
            restore_gap('scoped quit')
            drive_world(0, 0)
            record('quit_requested')
            unreal.SystemLibrary.execute_console_command(STATE['world'], 'quit', STATE['controller'])
            return
        if STATE['finished'] or STATE['quitting']:
            return
        now = time.time()
        discover(now)
        trigger_path = RUN / 'trigger.json'
        if STATE['trigger'] is None and trigger_path.exists():
            STATE['trigger'] = json.loads(trigger_path.read_text(encoding='utf-8'))
            record('trigger', trigger=STATE['trigger'])
        trigger = STATE['trigger']
        if trigger and STATE['ready']:
            elapsed = now - trigger['start_utc']
            drive(now, elapsed)
            finish = RUN / 'finish.json'
            finish_utc = json.loads(finish.read_text(encoding='utf-8'))['finish_utc'] if finish.exists() else None
            if (finish_utc and now >= finish_utc) or elapsed >= trigger['duration_seconds']:
                restore_gap('measurement finished')
                drive_world(0, 0)
                sample(now)
                write_json(RUN / (ROLE + '-finished.json'), {'utc': now, 'pid': os.getpid(), 'elapsed': elapsed})
                record('finished', elapsed=elapsed)
                STATE['finished'] = True
        sample(now)
        observer_gap(now)
        if now - STATE['started'] > 600:
            raise RuntimeError('Scoped runtime exceeded its total 600-second lifetime')
    except Exception:
        restore_gap('measurement exception')
        drive_world(0, 0)
        error = traceback.format_exc()
        record('error', error=error)
        (RUN / (ROLE + '-error.txt')).write_text(error, encoding='utf-8')
        STATE['finished'] = True
        unreal.log_error(error)


STATE['callback'] = unreal.register_slate_post_tick_callback(tick)
settings_class = unreal.load_class(None, '/Script/NetworkPrediction.NetworkPredictionSettingsObject')
settings = unreal.get_default_object(settings_class).get_editor_property('Settings')
engine_class = unreal.load_class(None, '/Script/Engine.Engine')
engines = [engine for engine in unreal.ObjectIterator(engine_class) if 'Default__' not in engine.get_path_name()]
engine_settings = [{'path': engine.get_path_name(),
                    'use_fixed_frame_rate': engine.get_editor_property('bUseFixedFrameRate'),
                    'fixed_frame_rate': engine.get_editor_property('FixedFrameRate'),
                    'smooth_frame_rate': engine.get_editor_property('bSmoothFrameRate')} for engine in engines]
record('boot', command_line=unreal.SystemLibrary.get_command_line(), run=str(RUN), network_prediction_settings=settings.export_text(),
       engine_settings=engine_settings,
       limits='Reads displayed montage, movement mode, transform and live rollback delegates; does not inspect exact historical NP traversal payload or GAS completion delegate')
if len(engine_settings) != 1 or engine_settings[0]['use_fixed_frame_rate']:
    message = 'Probe requires ordinary variable-rate engine ticking: ' + str(engine_settings)
    (RUN / (ROLE + '-error.txt')).write_text(message, encoding='utf-8')
    STATE['finished'] = True
    raise RuntimeError(message)
unreal.log('RPG_VAULT_PROBE_BOOT ' + ROLE)
