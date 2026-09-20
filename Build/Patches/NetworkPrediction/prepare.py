"""Stage/verify/remove a pinned local UE plugin override; never edit the installed Engine."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
MANIFEST = HERE / "manifest.json"
MARKER = ".survival-rpg-network-prediction-patch.json"
GENERATED = {"Binaries", "Intermediate"}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def files(root: Path) -> dict[str, Path]:
    """Inspect links before traversal/read, including generated trees and markers."""
    anchor = root.absolute()
    found: dict[str, Path] = {}
    pending = [root]
    while pending:
        path = pending.pop()
        info = path.lstat()
        require(not stat.S_ISLNK(info.st_mode) and
                not (getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)),
                f"Links/junctions/reparse points are not permitted in plugin copies: {path}")
        require(path.resolve().is_relative_to(anchor), f"Plugin path escapes its root: {path}")
        if stat.S_ISDIR(info.st_mode):
            with os.scandir(path) as entries:
                pending.extend(Path(entry.path) for entry in entries)
        else:
            relative = path.relative_to(root)
            if relative.parts[0] not in GENERATED:
                # A hard link resolves normally, but patching it could still write
                # into the Engine. Generated binary hardlinks are only unlinked.
                require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1,
                        f"Source/content must be an ordinary unshared file: {path}")
                if relative.as_posix() != MARKER:
                    found[relative.as_posix()] = path
    return found


def check_hashes(root: Path, hashes: dict[str, str]) -> None:
    found = files(root)
    require(set(found) == set(hashes), f"Unexpected source/content file set: {root}")
    for name, expected in hashes.items():
        require(digest(found[name]) == expected, f"Hash mismatch: {found[name]}")


def apply_exact_patch(source: str, patch: str) -> str:
    """Apply one file with exact context, no fuzzy matching or arbitrary target paths."""
    original = source.splitlines(keepends=True)
    lines = patch.splitlines(keepends=True)
    output: list[str] = []
    cursor = 0
    line = 2
    require(lines[0].startswith("--- a/") and lines[1].startswith("+++ b/"), "Invalid patch header")
    while line < len(lines):
        match = re.match(r"@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", lines[line])
        require(match is not None, "Invalid patch hunk")
        start = int(match.group(1)) - 1
        require(start >= cursor, "Overlapping patch hunks")
        output.extend(original[cursor:start])
        cursor = start
        line += 1
        while line < len(lines) and not lines[line].startswith("@@"):
            entry = lines[line]
            require(entry[:1] in {" ", "+", "-"}, "Unsupported patch line")
            if entry[0] in " -":
                require(cursor < len(original) and original[cursor] == entry[1:], "Patch context mismatch")
                cursor += 1
            if entry[0] in " +":
                output.append(entry[1:])
            line += 1
    output.extend(original[cursor:])
    return "".join(output)


def destination(name: str) -> Path:
    require(name in {"NetworkPrediction", "Mover", "ChaosMover", "MoverExamples"}, "Unexpected plugin name")
    plugins = (REPO / "Plugins").resolve()
    require(plugins == REPO.resolve() / "Plugins", f"Plugins directory must stay at its repository path: {plugins}")
    target = plugins / name
    require(target.resolve().parent == plugins and not target.is_symlink(), f"Unsafe target: {target}")
    return target


def expected_files(manifest: dict, name: str) -> dict[str, str]:
    result = dict(manifest["plugins"][name]["baseline"])
    if name == "NetworkPrediction":
        result.update(manifest["patched"])
        result.update(manifest["overlay"])
    return result


def verify_staged(manifest: dict) -> None:
    for name in manifest["plugins"]:
        target = destination(name)
        check_hashes(target, expected_files(manifest, name))
        marker = target / MARKER
        require(marker.is_file(), f"Missing staging marker: {marker}. Run prepare.py stage.")
        require(json.loads(marker.read_text())["manifest_sha256"] == digest(MANIFEST), f"Stale staging manifest: {target}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check", "stage", "verify", "remove"))
    parser.add_argument("--engine", type=Path, default=Path("D:/Programme/UE_5.8/Engine"))
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    if args.action in {"verify", "remove"}:
        verify_staged(manifest)
        if args.action == "remove":
            # Verification includes user-modified assets and unknown source files.
            # Resolved exact targets are checked again immediately before deletion.
            for name in manifest["plugins"]:
                target = destination(name)
                files(target)
                shutil.rmtree(target)
        print(f"{args.action}: {len(manifest['plugins'])} plugin overrides verified")
        return

    engine = args.engine.resolve()
    version = json.loads((engine / "Build/Build.version").read_text())
    require(tuple(version[k] for k in ("MajorVersion", "MinorVersion", "PatchVersion")) == (5, 8, 2), "This patch requires exactly UE 5.8.2")
    for name, plugin in manifest["plugins"].items():
        check_hashes(engine / "Plugins" / plugin["source"], plugin["baseline"])
    for name, expected in manifest["overlay"].items():
        require(digest(HERE / "Overlay" / name) == expected, f"Unrecorded overlay edit: {name}")
    require(digest(HERE / "fixed-interpolation-recovery.patch") == manifest["patch_sha256"], "Unrecorded patch edit")
    patched_relative = next(iter(manifest["patched"]))
    original = engine / "Plugins" / manifest["plugins"]["NetworkPrediction"]["source"] / patched_relative
    patched = apply_exact_patch(original.read_text(encoding="utf-8-sig"), (HERE / "fixed-interpolation-recovery.patch").read_text())
    require(hashlib.sha256(patched.encode()).hexdigest() == manifest["patched"][patched_relative], "Patched output hash mismatch")
    if args.action == "check":
        print("check: exact UE 5.8.2 plugin source/content baselines and patch hashes match")
        return

    existing = [destination(name).exists() for name in manifest["plugins"]]
    if any(existing):
        require(all(existing), "Partial/existing plugin override found; preserve it and inspect manually")
        markers = [(destination(name) / MARKER).exists() for name in manifest["plugins"]]
        if all(markers):
            verify_staged(manifest)
            print("stage: exact current patch already staged")
            return
        require(not any(markers), "Partial staged markers found; preserve overrides and inspect manually")
        # A previous interruption may have finished copying all four pristine
        # baselines. Resume only after checking every file in every destination.
        for name, plugin in manifest["plugins"].items():
            check_hashes(destination(name), plugin["baseline"])
        print("stage: all four existing copies match pristine baselines; resuming patch application")
    else:
        for name, plugin in manifest["plugins"].items():
            target = destination(name)
            for relative in plugin["baseline"]:
                output = target / relative
                output.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(engine / "Plugins" / plugin["source"] / relative, output)
    np = destination("NetworkPrediction")
    target = np / patched_relative
    target.write_text(patched, encoding="utf-8", newline="\n")
    for relative in manifest["overlay"]:
        target = np / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(HERE / "Overlay" / relative, target)
    for name in manifest["plugins"]:
        (destination(name) / MARKER).write_text(json.dumps({"manifest_sha256": digest(MANIFEST), "engine": str(engine)}, indent=2) + "\n")
    verify_staged(manifest)
    print("stage: four local plugin overrides ready; run normal Editor/Game build next")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
