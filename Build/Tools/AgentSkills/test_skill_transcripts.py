"""Regression tests for confirmed skill loads, using the CLIs' completed-result formats."""

import json
import unittest

import skill_evals as s


NAMES = ["alpha-skill", "beta-skill"]
ALPHA = ".agents/skills/alpha-skill/SKILL.md"
BETA = ".claude/skills/beta-skill/SKILL.md"


def content(skill="alpha-skill"):
    return f"---\nname: {skill}\ndescription: Guides the task.\n---\n# Instructions\nRead the relevant project files.\n"


def parse_codex(command, output=None, code=0, kind="item.completed"):
    events = [{"type": kind, "item": {"id": "read", "type": "command_execution", "command": command,
                                        "aggregated_output": content() if output is None else output,
                                        "exit_code": code, "status": "completed" if code == 0 else "failed"}},
              {"type": "item.completed", "item": {"id": "answer", "type": "agent_message", "text": "Plan."}},
              {"type": "turn.completed", "usage": {}}]
    return s.parse_codex([json.dumps(event) for event in events], NAMES)


def parse_claude(name, arguments, output=None, error=False, metadata=None, completed=True):
    events = [{"type": "assistant", "message": {"content": [
        {"type": "tool_use", "id": "read", "name": name, "input": arguments}]}}]
    if completed:
        events.append({"type": "user", "message": {"content": [
            {"type": "tool_result", "tool_use_id": "read", "is_error": error,
             "content": content() if output is None else output}]}, "tool_use_result": metadata})
    events.append({"type": "result", "subtype": "success", "result": "Plan."})
    return s.parse_claude([json.dumps(event) for event in events], NAMES)


class ConfirmedSkillTests(unittest.TestCase):
    def test_literal_full_reads_and_real_shell_wrappers(self):
        commands = [f"cat {ALPHA}", f"type {ALPHA}", f"Get-Content -Raw -Encoding utf8 -LiteralPath '{ALPHA}'",
                    f'''"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -Command 'Get-Content {ALPHA}' ''',
                    f'''powershell.exe -NoProfile -Command "Get-Content '{ALPHA}'"''',
                    f"/bin/bash -lc 'cat {ALPHA}'"]
        for command in commands:
            with self.subTest(command=command):
                self.assertEqual(parse_codex(command).loaded, ["alpha-skill"])

    def test_failed_empty_or_unfinished_reads_do_not_load(self):
        for kwargs in ({"output": "Access denied", "code": 1}, {"output": ""}, {"kind": "item.started"}):
            with self.subTest(kwargs=kwargs):
                result = parse_codex(f"cat {ALPHA}", **kwargs)
                self.assertEqual(result.loaded, [])
                self.assertEqual(len(result.tools), 1)

    def test_existence_checks_searches_and_path_mentions_do_not_load(self):
        for command in (f"Test-Path {ALPHA}", f"rg --files {ALPHA}", f"rg -n name {ALPHA}",
                        f"echo Get-Content {ALPHA}", f"ls {ALPHA}"):
            with self.subTest(command=command):
                self.assertEqual(parse_codex(command).loaded, [])

    def test_partial_or_dynamic_reads_do_not_prove_a_full_load(self):
        for command in (f"Get-Content -TotalCount 2 {ALPHA}", f"Get-Content {ALPHA} -Tail 2",
                        f"Get-Content {ALPHA} | Select-Object -First 2", f"cat {ALPHA} | head -2",
                        f"sed -n '1,2p' {ALPHA}", f"cat {ALPHA} > output.txt",
                        f"Get-Content $path # {ALPHA}"):
            with self.subTest(command=command):
                self.assertEqual(parse_codex(command).loaded, [])

    def test_compound_reads_require_each_skills_own_returned_content(self):
        command = f"Get-Content {ALPHA}; Get-Content {BETA}"
        self.assertEqual(parse_codex(command).loaded, ["alpha-skill"])
        self.assertEqual(parse_codex(command, content() + content("beta-skill")).loaded, NAMES)
        # A failed trailing search must not discard the successful preceding skill read.
        self.assertEqual(parse_codex(f"Get-Content {ALPHA}; rg absent missing-file", code=1).loaded, ["alpha-skill"])
        self.assertEqual(parse_codex(f"cat {ALPHA}; Test-Path {BETA}", content() + content("beta-skill")).loaded,
                         ["alpha-skill"])

    def test_returned_path_header_or_truncated_output_is_insufficient(self):
        for output in (ALPHA, "---\nname: alpha-skill\n---\n", content("beta-skill"),
                       content() + "\n... 1000 tokens truncated ...",
                       "Warning: truncated output (original token count: 4000)\n" + content()):
            with self.subTest(output=output):
                self.assertEqual(parse_codex(f"cat {ALPHA}", output).loaded, [])

    def test_claude_skill_requires_successful_matching_result(self):
        arguments = {"skill": "alpha-skill"}
        self.assertEqual(parse_claude("Skill", arguments, "Launching skill: alpha-skill").loaded, ["alpha-skill"])
        self.assertEqual(parse_claude("Skill", arguments, "", metadata={"success": True}).loaded, ["alpha-skill"])
        for kwargs in ({"completed": False}, {"error": True}, {"metadata": {"success": False}},
                       {"output": "Unknown skill: alpha-skill"}):
            with self.subTest(kwargs=kwargs):
                self.assertEqual(parse_claude("Skill", arguments, **kwargs).loaded, [])

    def test_claude_only_completed_full_reads_count(self):
        arguments = {"file_path": ALPHA}
        self.assertEqual(parse_claude("Read", arguments).loaded, ["alpha-skill"])
        for kwargs in ({"completed": False}, {"error": True}, {"output": ""}, {"output": "Access denied"}):
            with self.subTest(kwargs=kwargs):
                result = parse_claude("Read", arguments, **kwargs)
                self.assertEqual(result.loaded, [])
                self.assertEqual(len(result.tools), 1)
        self.assertEqual(parse_claude("Read", dict(arguments, offset=2)).loaded, [])
        self.assertEqual(parse_claude("Read", dict(arguments, limit=2)).loaded, [])
        self.assertEqual(parse_claude("Glob", {"pattern": ALPHA}).loaded, [])
        self.assertEqual(parse_claude("Grep", {"path": ALPHA, "pattern": "name"}).loaded, [])

    def test_claude_read_metadata_verifies_complete_coverage(self):
        metadata = {"type": "text", "file": {"content": content(), "startLine": 1,
                                               "numLines": 7, "totalLines": 7}}
        self.assertEqual(parse_claude("Read", {"file_path": ALPHA, "limit": 200}, metadata=metadata).loaded,
                         ["alpha-skill"])
        metadata["file"]["totalLines"] = 20
        self.assertEqual(parse_claude("Read", {"file_path": ALPHA}, metadata=metadata).loaded, [])

    def test_claude_text_blocks_numbered_reads_and_bash_results(self):
        numbered = "\n".join(f"{i}\t{line}" for i, line in enumerate(content().splitlines(), 1))
        result = parse_claude("Read", {"file_path": ALPHA.replace("/", "\\")},
                              [{"type": "text", "text": numbered}])
        self.assertEqual(result.loaded, ["alpha-skill"])
        self.assertEqual(parse_claude("Bash", {"command": f"cat {ALPHA}"}).loaded, ["alpha-skill"])
        self.assertEqual(parse_claude("Bash", {"command": f"cat {ALPHA}"}, error=True).loaded, [])


if __name__ == "__main__":
    unittest.main()
