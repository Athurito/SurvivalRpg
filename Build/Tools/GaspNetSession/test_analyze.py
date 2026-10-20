"""Unreal-free tests for the GASP-VAL-02 session tools."""
from pathlib import Path
import tempfile
import unittest

import analyze
import session

HOST_LOG = """[2026.10.19-20.00.00:000][  0]LogInit: Display: Starting
[2026.10.19-20.00.01:000][  1]LogRpgExperience: EXPERIENCE: OnExperienceLoadComplete(RpgExperienceDefinition:RpgGaspMantleExperience)
[2026.10.19-20.00.05:000][ 50]LogNetPlayerMovement: Warning: *** Server: Error for BP_RpgGasp_MantleCharacter_C_1 at Time=3.210 is 12.500 LocDiff(X=12.5)
[2026.10.19-20.00.06:000][ 60]LogNetPlayerMovement: Warning: *** Server: Error for BP_RpgGasp_MantleCharacter_C_1 at Time=4.210 is 2.500 LocDiff(X=2.5)
[2026.10.19-20.00.07:000][ 70]LogOnline: Error: STEAM: Steam API disabled!
[2026.10.19-20.00.08:000][ 80]LogNetworkPrediction: Warning: Reconcile required due to Sync/Aux mismatch. LocalFrame: 5.
[2026.10.19-20.00.09:000][ 90]LogTemp: Error: Something real broke
"""
CLIENT_LOG = """[2026.10.19-20.00.02:000][  0]LogNet: Join succeeded: Player
[2026.10.19-20.00.05:000][ 50]LogNetPlayerMovement: Warning: *** Client: Error for BP_RpgGasp_MantleCharacter_C_1 at Time=3.210 is 12.500 LocDiff
"""


class Analyse(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.run_dir = Path(folder.name)
        (self.run_dir / "host.log").write_text(HOST_LOG, encoding="utf-8")
        (self.run_dir / "client1.log").write_text(CLIENT_LOG, encoding="utf-8")

    def test_counts_corrections_reconciles_and_errors(self):
        summary = analyze.analyse(self.run_dir)
        host, client = summary["roles"]["host"], summary["roles"]["client1"]
        self.assertEqual(host["experiences"], ["RpgGaspMantleExperience"])
        self.assertEqual(host["server_corrections"], {"count": 2, "max_cm": 12.5, "median_cm": 7.5})
        self.assertEqual((host["reconciles"], host["known_noise_errors"], host["error_count"]), (1, 1, 1))
        self.assertEqual((client["joins"], client["client_corrections"]["count"]), (1, 1))
        self.assertFalse(summary["passed"])

    def test_clean_run_passes(self):
        (self.run_dir / "host.log").write_text(HOST_LOG.replace("LogTemp: Error: Something real broke", "LogTemp: fine"),
                                               encoding="utf-8")
        self.assertTrue(analyze.analyse(self.run_dir)["passed"])


class Session(unittest.TestCase):
    def test_profiles_split_the_round_trip(self):
        self.assertEqual(session.profile_commands("average"),
                         ["NetEmulation.PktLag 60", "NetEmulation.PktLagVariance 15", "NetEmulation.PktLoss 1"])
        self.assertIn("p.NetShowCorrections 1", session.exec_cmds(30, "none"))

    def test_role_command_line(self):
        command = session.role_command(Path("G/SurvivalRpg.exe"), "client1", 1, "127.0.0.1:17777", 17777, 120, "bad",
                                       Path("run"))
        self.assertEqual(command[1], "127.0.0.1:17777")
        for part in ("-nosteam", "-WinX=960", "-port=17777", f"-abslog={Path('run/client1.log')}"):
            self.assertIn(part, command)
        self.assertTrue(command[-1].startswith("-ExecCmds=t.MaxFPS 120,"))


if __name__ == "__main__":
    unittest.main()
