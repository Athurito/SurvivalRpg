"""Start a packaged SurvivalRpg session for GASP-VAL-02 and keep its evidence.

One process hosts a listen server (or plays alone with --clients 0 --solo), the
others join over the IP driver on this machine. Every process logs CMC
corrections (p.NetShowCorrections) and NetworkPrediction reconciles
(np.PrintReconciles) and applies the same emulated network profile. The session
ends when every game window is closed, or after --minutes. See README.md.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid
import zlib

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(REPO / "Build/Tools/Unreal"))
import ue  # noqa: E402  (map resolution shared with ue.py)

RUNS = REPO / "Saved/GaspVal02"
PACKAGES = REPO / "Saved/Packaged"

# Round-trip latency, round-trip jitter (ms) and loss per direction (%). Every process
# delays its outgoing packets by half the latency, so the round trip carries the full value.
PROFILES = {
    "none": (0, 0, 0),
    "good": (50, 10, 0),
    "average": (120, 30, 1),
    "bad": (250, 60, 5),
}
LOGGING = ["p.NetShowCorrections 1", "p.NetCorrectionLifetime 0.01", "np.PrintReconciles 1"]


def newest_game() -> Path:
    games = sorted(PACKAGES.glob("*/Windows/SurvivalRpg.exe"), key=lambda path: path.stat().st_mtime)
    if not games:
        raise SystemExit("No packaged game under Saved/Packaged; run: python Build/Tools/Unreal/ue.py package ...")
    return games[-1]


def game_binary(game: Path) -> Path:
    """The staged SurvivalRpg.exe only bootstraps SurvivalRpg/Binaries/Win64/SurvivalRpg.exe as a child process.
    Start the real binary so the session owns the game window and its exit code."""
    real = game.parent / "SurvivalRpg/Binaries/Win64/SurvivalRpg.exe"
    return real if game.name == "SurvivalRpg.exe" and real.is_file() else game


def profile_commands(name: str) -> list[str]:
    latency, jitter, loss = PROFILES[name]
    return [f"NetEmulation.PktLag {latency // 2}", f"NetEmulation.PktLagVariance {jitter // 2}",
            f"NetEmulation.PktLoss {loss}"]


def exec_cmds(fps: int, profile: str) -> str:
    commands = [f"t.MaxFPS {fps}", *LOGGING, *profile_commands(profile)]
    if any("," in command for command in commands):
        raise SystemExit("Console commands may not contain commas")
    return ",".join(commands)


def role_command(game: Path, role: str, index: int, url: str, port: int, fps: int, profile: str, run: Path) -> list[str]:
    width, height = 960, 540
    x, y = (index % 2) * width, (index // 2) * height
    return [str(game), url, "-nosteam", "-windowed", f"-ResX={width}", f"-ResY={height}", f"-WinX={x}", f"-WinY={y}",
            f"-port={port}", "-NoSplash", f"-abslog={run / (role + '.log')}", f"-ExecCmds={exec_cmds(fps, profile)}"]


# --- Window capture (only the game's own window, never the desktop) ------------------------------

def _windows_of(pid: int) -> list[int]:
    user32 = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(visit, 0)
    return found


def _png(width: int, height: int, bgra: bytes) -> bytes:
    """Encode top-down BGRA pixels as an RGB PNG (filter 0 per row)."""
    raw = bytearray()
    for y in range(height):
        row = bgra[y * width * 4:(y + 1) * width * 4]
        raw.append(0)
        for x in range(0, len(row), 4):
            raw += bytes((row[x + 2], row[x + 1], row[x]))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b""))


def capture_window(pid: int, path: Path) -> bool:
    """PrintWindow with PW_RENDERFULLCONTENT into a PNG; returns False when the process has no window."""
    user32, gdi32 = ctypes.windll.user32, ctypes.windll.gdi32
    windows = _windows_of(pid)
    if not windows:
        return False
    hwnd = windows[0]
    rect = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    width, height = rect.right - rect.left, rect.bottom - rect.top
    if width <= 0 or height <= 0:
        return False
    window_dc = user32.GetDC(hwnd)
    memory_dc = gdi32.CreateCompatibleDC(window_dc)
    bitmap = gdi32.CreateCompatibleBitmap(window_dc, width, height)
    gdi32.SelectObject(memory_dc, bitmap)
    user32.PrintWindow(hwnd, memory_dc, 3)  # PW_CLIENTONLY | PW_RENDERFULLCONTENT

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG), ("biHeight", wintypes.LONG),
                    ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                    ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                    ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD), ("biClrImportant", wintypes.DWORD)]

    header = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), width, -height, 1, 32, 0, 0, 0, 0, 0, 0)
    buffer = ctypes.create_string_buffer(width * height * 4)
    gdi32.GetDIBits(memory_dc, bitmap, 0, height, buffer, ctypes.byref(header), 0)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(hwnd, window_dc)
    path.write_bytes(_png(width, height, buffer.raw))
    return True


# --- Session -----------------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("label", help="alphanumeric run label; evidence goes to Saved/GaspVal02/session_<label>")
    parser.add_argument("--map", required=True, help="/Game/... package path or unique map name")
    parser.add_argument("--profile", choices=sorted(PROFILES), default="none")
    parser.add_argument("--clients", type=int, default=1, help="client processes (0..3)")
    parser.add_argument("--solo", action="store_true", help="with --clients 0: play the map without a listen server")
    parser.add_argument("--late-join", type=float, default=0, metavar="SECONDS",
                        help="start the last client this many seconds after the others")
    parser.add_argument("--fps", default="60", help="comma list of frame caps: host first, then clients (last value repeats)")
    parser.add_argument("--game", type=Path, help="packaged SurvivalRpg.exe (default: newest under Saved/Packaged)")
    parser.add_argument("--port", type=int, default=17777)
    parser.add_argument("--minutes", type=float, default=30, help="stop every process after this many minutes")
    parser.add_argument("--capture", type=float, default=0, metavar="SECONDS",
                        help="capture each game window once after this many seconds (0: never)")
    args = parser.parse_args(argv)
    if os.name != "nt":
        parser.error("Windows only")
    if not args.label.replace("-", "").replace("_", "").isalnum():
        parser.error("Use an alphanumeric label")
    if not 0 <= args.clients <= 3 or (args.solo and args.clients):
        parser.error("--clients must be 0..3, and --solo needs --clients 0")
    caps = [int(value) for value in args.fps.split(",")]
    if any(cap < 1 or cap > 240 for cap in caps):
        parser.error("Frame caps must be 1..240")
    game = game_binary((args.game or newest_game()).resolve())
    map_path = ue.resolve_map(args.map)
    run = RUNS / f"session_{args.label}"
    run.mkdir(parents=True, exist_ok=False)

    roles = ["host"] + [f"client{index}" for index in range(1, args.clients + 1)]
    fps = {role: caps[min(index, len(caps) - 1)] for index, role in enumerate(roles)}
    manifest = {"game": str(game), "map": map_path, "profile": args.profile, "profile_values": PROFILES[args.profile],
                "profile_meaning": "round-trip latency ms, round-trip jitter ms, loss % per direction",
                "fps": fps, "late_join_seconds": args.late_join, "solo": args.solo, "started_utc": time.time(),
                "processes": {}}
    children: dict[str, subprocess.Popen] = {}

    def save():
        (run / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    def launch(role: str, index: int):
        # Without Steam the server only admits an offline player with a canonical UUID-v4 profile id.
        url = map_path if args.solo else (map_path + "?listen" if role == "host"
                                          else f"127.0.0.1:{args.port}?PlayerProfileId={uuid.uuid4()}")
        command = role_command(game, role, index, url, args.port, fps[role], args.profile, run)
        children[role] = subprocess.Popen(command, cwd=str(game.parent))
        manifest["processes"][role] = {"pid": children[role].pid, "command": command, "started_utc": time.time()}
        save()
        print(f"started {role} (pid {children[role].pid}, {fps[role]} FPS)", flush=True)

    deadline = time.monotonic() + args.minutes * 60
    captured = False
    try:
        launch("host", 0)
        time.sleep(8)
        for index, role in enumerate(roles[1:], start=1):
            if role == roles[-1] and args.late_join > 0:
                print(f"{role} joins late in {args.late_join:.0f}s", flush=True)
                time.sleep(args.late_join)
            launch(role, index)
            time.sleep(2)
        started = time.monotonic()
        while any(process.poll() is None for process in children.values()) and time.monotonic() < deadline:
            if args.capture and not captured and time.monotonic() - started >= args.capture:
                for role, process in children.items():
                    if process.poll() is None:
                        manifest["processes"][role]["capture"] = capture_window(process.pid, run / f"{role}.png")
                captured = True
                save()
            time.sleep(1)
    except KeyboardInterrupt:
        print("interrupted; stopping the session", flush=True)
    finally:
        for role, process in children.items():
            stopped = process.poll() is None
            if stopped:
                subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)], capture_output=True)
                process.wait()
            manifest["processes"][role].update({"exit_code": process.returncode, "stopped_by_session": stopped})
        manifest["finished_utc"] = time.time()
        save()
    print(f"evidence: {ue.shown(run)}", flush=True)
    print(f"analyse: python Build/Tools/GaspNetSession/analyze.py {ue.shown(run)}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
