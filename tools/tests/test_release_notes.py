# AI prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""Release boundaries: exercise real Git tags with a mocked GitHub API."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


generate = load("generate_release_notes")
prepare = load("prepare_release")


class ReleaseNotesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "VERSION").write_text("1.2.0\n")
        (self.root / ".github/release-notes").mkdir(parents=True)
        self.published = [{"tag_name": "v1.2.0", "draft": False, "prerelease": False}]
        self.api_calls = []
        self.real_run = generate.run
        self.scope = contextlib.ExitStack()
        self.addCleanup(self.scope.close)
        self.scope.enter_context(patch.object(generate, "ROOT", self.root))
        self.scope.enter_context(patch.object(prepare, "ROOT", self.root))
        self.scope.enter_context(patch.object(generate, "run", self.command))
        self.git("init", "-q")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "--allow-empty", "-qm", "Baseline")
        self.git("tag", "v1.2.0")

    def git(self, *args):
        return self.real_run("git", *args)

    def command(self, *args):
        if args[0] != "gh":
            return self.real_run(*args)
        self.api_calls.append(args)
        if "--paginate" in args:
            return json.dumps([self.published])
        return json.dumps({"body": "## What's Changed\n### Opravy\n* New fix (#7)"})

    def test_next_release_has_explicit_previous_tag(self):
        generate.generate("1.2.1", "owner/repo", "HEAD")
        self.assertIn("previous_tag_name=v1.2.0", self.api_calls[-1])
        self.assertIn("target_commitish=HEAD", self.api_calls[-1])

    def test_prepared_but_unpublished_version_stops_before_generation(self):
        self.published = []
        (self.root / ".github/release-notes/v1.2.0.md").write_text("Prepared")
        with self.assertRaisesRegex(ValueError, "not published"):
            generate.generate("1.2.1", "owner/repo", "HEAD")
        self.assertEqual(len(self.api_calls), 1)

    def test_old_release_is_not_silently_used(self):
        self.published[0]["tag_name"] = "v1.1.0"
        with self.assertRaisesRegex(ValueError, "not published"):
            generate.generate("1.2.1", "owner/repo", "HEAD")

    def test_draft_and_prerelease_are_not_stable_baselines(self):
        for field in ("draft", "prerelease"):
            with self.subTest(field=field):
                self.published[0][field] = True
                with self.assertRaisesRegex(ValueError, "not published"):
                    generate.generate("1.2.1", "owner/repo", "HEAD")
                self.published[0][field] = False

    def test_first_template_release_without_history(self):
        self.published = []
        self.git("tag", "-d", "v1.2.0")
        generate.generate("1.2.1", "owner/repo", "HEAD")
        self.assertFalse(any(a.startswith("previous_tag_name=") for a in self.api_calls[-1]))

    def test_missing_local_tag_stops_generation(self):
        self.git("tag", "-d", "v1.2.0")
        with self.assertRaises(subprocess.CalledProcessError):
            generate.generate("1.2.1", "owner/repo", "HEAD")
        self.assertEqual(len(self.api_calls), 1)

    def test_tag_outside_target_history_stops_generation(self):
        before = self.git("rev-parse", "HEAD").strip()
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "--allow-empty", "-qm", "Later commit")
        self.git("tag", "-f", "v1.2.0")
        with self.assertRaises(subprocess.CalledProcessError):
            generate.generate("1.2.1", "owner/repo", before)
        self.assertEqual(len(self.api_calls), 1)

    def test_invalid_or_old_version_has_no_api_calls(self):
        for version in ("v1.2.1", "1.2.0", "1.1.9", "1.2.1-rc1"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                generate.generate(version, "owner/repo", "HEAD")
        self.assertEqual(self.api_calls, [])

    def test_api_failure_does_not_fall_back_to_first_release(self):
        with patch.object(generate, "run", side_effect=subprocess.CalledProcessError(1, "gh")):
            with self.assertRaises(subprocess.CalledProcessError):
                generate.generate("1.2.1", "owner/repo", "HEAD")

    def test_cumulative_history_is_preserved_and_headings_nested(self):
        old = "## [1.2.0] - 2026-09-20\n\nOld entry\n"
        path = self.root / "CHANGELOG.md"
        path.write_text("# Changelog\n\n## [Unreleased]\n\n### Opraveno\n- New fix.\n\n" + old)
        with contextlib.redirect_stdout(io.StringIO()):
            prepare.prepare("1.2.1", "## What's Changed\n### Opravy\n* New fix (#7)")
        updated = path.read_text()
        self.assertTrue(updated.endswith(old))
        self.assertIn("#### What's Changed\n##### Opravy", updated)
        self.assertEqual(updated.count("New fix."), 1)
        self.assertEqual((self.root / "VERSION").read_text().strip(), "1.2.1")
        self.assertIn("New fix (#7)", (self.root / ".github/release-notes/v1.2.1.md").read_text())


if __name__ == "__main__":
    unittest.main()
