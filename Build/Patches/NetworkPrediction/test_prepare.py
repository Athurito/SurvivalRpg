"""Small installer regressions using synthetic files, never copied Epic plugin trees."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class PrepareTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "Project"
        self.patch = self.repo / "Build/Patches/NetworkPrediction"
        self.patch.mkdir(parents=True)
        shutil.copy2(Path(__file__).with_name("prepare.py"), self.patch / "prepare.py")
        self.engine = self.root / "Engine"
        (self.engine / "Build").mkdir(parents=True)
        (self.engine / "Build/Build.version").write_text(json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 2}))
        self.cpp = "Source/NetworkPrediction/Private/NetworkPredictionWorldManager.cpp"
        original, patched = b"alpha\nbeta\ngamma\n", b"alpha\ncorrected\ngamma\n"
        patch = (f"--- a/{self.cpp}\n+++ b/{self.cpp}\n@@ -1,3 +1,3 @@\n alpha\n-beta\n+corrected\n gamma\n").encode()
        (self.patch / "fixed-interpolation-recovery.patch").write_bytes(patch)
        overlay_name = "Source/NetworkPrediction/Private/Recovery.h"
        overlay = self.patch / "Overlay" / overlay_name
        overlay.parent.mkdir(parents=True)
        overlay.write_bytes(b"synthetic helper\n")
        self.manifest = {"patched": {self.cpp: sha(patched)}, "overlay": {overlay_name: sha(overlay.read_bytes())},
                         "patch_sha256": sha(patch), "plugins": {}}
        for name in ("NetworkPrediction", "Mover", "ChaosMover", "MoverExamples"):
            # The last copied file deliberately differs from the patch target.
            relative = self.cpp if name == "NetworkPrediction" else f"Source/{name}/Public/{name}Module.h"
            data = original if name == "NetworkPrediction" else name.encode()
            source = self.engine / "Plugins" / name / relative
            source.parent.mkdir(parents=True)
            source.write_bytes(data)
            self.manifest["plugins"][name] = {"source": name, "baseline": {relative: sha(data)}}
        (self.patch / "manifest.json").write_text(json.dumps(self.manifest))

    def run_prepare(self, action):
        return subprocess.run([sys.executable, str(self.patch / "prepare.py"), action, "--engine", str(self.engine)],
                              capture_output=True, text=True)

    def copy_pristine_baselines(self):
        for name in self.manifest["plugins"]:
            shutil.copytree(self.engine / "Plugins" / name, self.repo / "Plugins" / name)

    def test_patch_destination_is_independent_of_last_copied_file(self):
        result = self.run_prepare("stage")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.repo / "Plugins/NetworkPrediction" / self.cpp).read_bytes(), b"alpha\ncorrected\ngamma\n")
        self.assertEqual(self.run_prepare("verify").returncode, 0)
        self.assertEqual(self.run_prepare("stage").returncode, 0, "Staging must be idempotent")

    def test_complete_verified_pristine_copies_resume_without_deletion(self):
        self.copy_pristine_baselines()
        result = self.run_prepare("stage")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("match pristine baselines", result.stdout)
        self.assertEqual(self.run_prepare("verify").returncode, 0)

    def test_modified_unmarked_copy_is_preserved_and_refused(self):
        self.copy_pristine_baselines()
        target = self.repo / "Plugins/Mover/Source/Mover/Public/MoverModule.h"
        target.write_bytes(b"user edit")
        self.assertNotEqual(self.run_prepare("stage").returncode, 0)
        self.assertEqual(target.read_bytes(), b"user edit")
        self.assertEqual((self.repo / "Plugins/NetworkPrediction" / self.cpp).read_bytes(), b"alpha\nbeta\ngamma\n")

    def make_symlink(self, link, target, is_directory=False):
        try:
            link.symlink_to(target, target_is_directory=is_directory)
        except OSError as error:
            self.skipTest(f"OS does not allow this test to create symbolic links: {error}")

    def test_pristine_source_symlink_cannot_modify_engine_original(self):
        self.copy_pristine_baselines()
        target = self.repo / "Plugins/NetworkPrediction" / self.cpp
        original = self.engine / "Plugins/NetworkPrediction" / self.cpp
        target.unlink()
        self.make_symlink(target, original)
        result = self.run_prepare("stage")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Links/junctions/reparse points", result.stderr)
        self.assertEqual(original.read_bytes(), b"alpha\nbeta\ngamma\n")
        self.assertTrue(target.is_symlink())

    def test_pristine_source_hardlink_cannot_modify_engine_original(self):
        self.copy_pristine_baselines()
        target = self.repo / "Plugins/NetworkPrediction" / self.cpp
        original = self.engine / "Plugins/NetworkPrediction" / self.cpp
        target.unlink()
        os.link(original, target)
        result = self.run_prepare("stage")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(original.read_bytes(), b"alpha\nbeta\ngamma\n")

    def test_symlinked_marker_is_rejected_before_reading_it(self):
        self.assertEqual(self.run_prepare("stage").returncode, 0)
        marker = self.repo / "Plugins/NetworkPrediction/.survival-rpg-network-prediction-patch.json"
        external = self.root / "external-marker.json"
        marker.rename(external)
        self.make_symlink(marker, external)
        result = self.run_prepare("verify")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Links/junctions/reparse points", result.stderr)

    def test_generated_directory_symlink_blocks_removal(self):
        self.assertEqual(self.run_prepare("stage").returncode, 0)
        external = self.root / "external-binaries"
        external.mkdir()
        preserved = external / "keep.txt"
        preserved.write_text("preserve")
        self.make_symlink(self.repo / "Plugins/NetworkPrediction/Binaries", external, True)
        result = self.run_prepare("remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Links/junctions/reparse points", result.stderr)
        self.assertEqual(preserved.read_text(), "preserve")
        self.assertTrue((self.repo / "Plugins/Mover").exists())

    def test_windows_source_junction_cannot_modify_engine_original(self):
        shell = shutil.which("powershell") if os.name == "nt" else None
        if not shell:
            self.skipTest("Windows PowerShell is required for a junction fixture")
        self.copy_pristine_baselines()
        target = self.repo / "Plugins/NetworkPrediction" / self.cpp
        original = self.engine / "Plugins/NetworkPrediction" / self.cpp
        target.unlink()
        target.parent.rmdir()
        environment = dict(os.environ, RPG_TEST_LINK=str(target.parent), RPG_TEST_TARGET=str(original.parent))
        result = subprocess.run([shell, "-NoProfile", "-NonInteractive", "-Command",
                                 "$ErrorActionPreference = 'Stop'\nNew-Item -ItemType Junction -Path $env:RPG_TEST_LINK -Target $env:RPG_TEST_TARGET | Out-Null"],
                                env=environment, capture_output=True, text=True)
        if result.returncode:
            self.skipTest(f"OS denied junction fixture: {result.stderr}")
        result = self.run_prepare("stage")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Links/junctions/reparse points", result.stderr)
        self.assertEqual(original.read_bytes(), b"alpha\nbeta\ngamma\n")


if __name__ == "__main__":
    unittest.main()
