"""Summarise a GASP-VAL-02 session folder: per role, Experience load, joins, CMC corrections,
NetworkPrediction reconciles, network failures, errors, ensures and crashes.

Writes summary.json next to the logs and prints one line per role. Exit code 0 when no
role crashed or logged an unexplained error, 1 otherwise. Error lines matching KNOWN_NOISE
are counted separately and do not fail the run.
"""
from __future__ import annotations

import json
from pathlib import Path
import re
import statistics
import sys

STAMP = re.compile(r"^\[(?P<stamp>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
EXPERIENCE = re.compile(r"OnExperienceLoadComplete\((?:RpgExperienceDefinition:)?(?P<name>[^)]+)\)")
SERVER_CORRECTION = re.compile(r"\*\*\* Server: Error for (?P<pawn>\S+) at Time=\S+ is (?P<distance>[\d.]+)")
CLIENT_CORRECTION = re.compile(r"\*\*\* Client: Error for (?P<pawn>\S+) at Time=\S+ is (?P<distance>[\d.]+)")
RECONCILE = re.compile(r"Reconcile required due to Sync/Aux mismatch")
JOIN = re.compile(r"LogNet: Join succeeded")
# A real loss shows a timeout first; the host closing the session only produces FailureReceived and ConnectionLost.
NETWORK_FAILURE = re.compile(r"NetworkFailure: (?!FailureReceived, Error: 'Host closed the connection|ConnectionLost)|ConnectionTimeout")
ERROR = re.compile(r"(?:^|[\s:)])Error:")
ENSURE = re.compile(r"Ensure condition failed:")
CRASH = re.compile(r"=== Critical error: ===|Fatal error!|Unhandled Exception: ")
# Errors seen in every packaged run that do not concern GASP (startup and shutdown noise).
KNOWN_NOISE = [
    re.compile(r"LogOnline: Error: STEAM:"),
    re.compile(r"LogSteamShared: Error:"),
    # Uncooked -game probes only: the engine's experimental toolset plugins fail to import under -game.
    re.compile(r"LogPython: Error:"),
    # Session end: the host quits first, so clients report the closed connection. Timeouts stay real failures.
    re.compile(r"Host closed the connection"),
    re.compile(r"Your connection to the host has been lost"),
]


def distances(values: list[float]) -> dict:
    if not values:
        return {"count": 0}
    return {"count": len(values), "max_cm": round(max(values), 2), "median_cm": round(statistics.median(values), 2)}


def analyse_log(path: Path) -> dict:
    text = path.read_bytes().decode("utf-8", errors="replace")
    lines = text.splitlines()
    stamps = [match["stamp"] for line in lines if (match := STAMP.match(line))]
    errors, noise = [], 0
    for line in lines:
        if ERROR.search(line) and "Display:" not in line.split("Error:")[0]:
            if any(pattern.search(line) for pattern in KNOWN_NOISE):
                noise += 1
            else:
                errors.append(STAMP.sub("", line).strip()[:300])
    server = [float(match["distance"]) for line in lines if (match := SERVER_CORRECTION.search(line))]
    client = [float(match["distance"]) for line in lines if (match := CLIENT_CORRECTION.search(line))]
    return {
        "first": stamps[0] if stamps else None,
        "last": stamps[-1] if stamps else None,
        "experiences": sorted({match["name"] for line in lines if (match := EXPERIENCE.search(line))}),
        "joins": sum(1 for line in lines if JOIN.search(line)),
        "server_corrections": distances(server),
        "client_corrections": distances(client),
        "reconciles": sum(1 for line in lines if RECONCILE.search(line)),
        "network_failures": sum(1 for line in lines if NETWORK_FAILURE.search(line)),
        "ensures": sum(1 for line in lines if ENSURE.search(line)),
        "crashed": bool(CRASH.search(text)),
        "errors": list(dict.fromkeys(errors))[:40],
        "error_count": len(errors),
        "known_noise_errors": noise,
    }


def analyse(run: Path) -> dict:
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.is_file() else {}
    roles = {log.stem: analyse_log(log) for log in sorted(run.glob("*.log"))}
    return {"run": str(run), "map": manifest.get("map"), "profile": manifest.get("profile"),
            "fps": manifest.get("fps"), "roles": roles,
            "passed": bool(roles) and not any(role["crashed"] or role["error_count"] for role in roles.values())}


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 1:
        print("usage: analyze.py <session folder>", file=sys.stderr)
        return 2
    run = Path(argv[0]).resolve()
    summary = analyse(run)
    (run / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"{run.name}: map {summary['map']}, profile {summary['profile']}, fps {summary['fps']}")
    for role, data in summary["roles"].items():
        print(f"  {role}: experiences {data['experiences']}, joins {data['joins']}, "
              f"server corrections {data['server_corrections']}, client corrections {data['client_corrections']}, "
              f"reconciles {data['reconciles']}, network failures {data['network_failures']}, ensures {data['ensures']}, "
              f"errors {data['error_count']} (+{data['known_noise_errors']} known noise), crashed {data['crashed']}")
        for error in data["errors"][:5]:
            print(f"    {error}")
    print("PASSED" if summary["passed"] else "FAILED")
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
