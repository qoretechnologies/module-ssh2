#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Verify develop reuse against real Git histories and simulated GitHub statuses."""

from contextlib import redirect_stdout
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("platform_ci", ROOT / "test/docker_test/platform-ci.py")
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)

UBUNTU = "a1" * 32
ALPINE = "b2" * 32


class PlatformCI(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.origin = os.getcwd()
        os.chdir(self.directory.name)
        self.addCleanup(os.chdir, self.origin)
        ci.git("init", "-q", "-b", "develop")
        ci.git("config", "user.email", "ci@example.invalid")
        ci.git("config", "user.name", "CI")
        ci.git("config", "commit.gpgsign", "false")
        ci.git("commit", "-qm", "base", "--allow-empty")
        self.base = ci.git("rev-parse", "HEAD")
        ci.git("checkout", "-qb", "feature")
        Path("module").write_text("change\n")
        ci.git("add", "module")
        ci.git("commit", "-qm", "feature")
        self.tip = ci.git("rev-parse", "HEAD")
        self.tree = ci.git("rev-parse", "HEAD^{tree}")
        ci.git("checkout", "-q", "develop")
        ci.git("merge", "--squash", "feature")
        ci.git("commit", "-qm", "squashed feature")
        self.head = ci.git("rev-parse", "HEAD")
        self.api = Mock(repo="qoretechnologies/module-ssh2")
        self.env = dict(CI_COMMIT_SHA=self.head, CI_JOB_NAME="test-ubuntu", CI_PIPELINE_ID="101",
                        CI_JOB_URL="https://git.example/mirror/module-ssh2/-/jobs/999",
                        CI_PROJECT_URL="https://git.example/mirror/module-ssh2", REPO_NAME="module-ssh2",
                        CI_PIPELINE_SOURCE="push", CI_COMMIT_BRANCH="develop")
        self.api.request.return_value = {"id": 7}
        self.pr = {"merge_commit_sha": self.head, "merged_at": "today", "state": "closed",
                   "base": {"ref": "develop", "repo": {"full_name": self.api.repo}},
                   "head": {"sha": self.tip}}
        self.commit = {"sha": self.tip, "tree": {"sha": self.tree}}
        self.jobs = [self.status("test-ubuntu"), self.status("test-alpine")]
        self.routes = {f"commits/{self.head}/statuses?per_page=100": [],
                       f"commits/{self.head}/pulls?per_page=100": [self.pr],
                       f"git/commits/{self.tip}": self.commit,
                       f"commits/{self.tip}/statuses?per_page=100": self.jobs}
        self.api.read.side_effect = self.routes.__getitem__

    def status(self, platform, pipeline=100, **changes):
        index, value = (1, UBUNTU) if platform == "test-ubuntu" else (2, ALPINE)
        return dict({"context": "module-ssh2", "creator": {"id": 7}, "state": "success",
                     "description": f"{platform} #{pipeline} {value}",
                     "target_url": f"{self.env['CI_PROJECT_URL']}/-/jobs/{index}"}, **changes)

    def evidence(self, value=UBUNTU, **env):
        return ci.evidence(self.api, dict(self.env, **env), value)

    def test_squash_merge_reuses_each_verified_platform(self):
        self.assertNotEqual(self.head, self.tip)
        self.assertEqual(self.base, ci.git("rev-parse", "HEAD^"))
        self.assertEqual(self.jobs[0]["target_url"], self.evidence())
        self.assertEqual(self.jobs[1]["target_url"], self.evidence(ALPINE, CI_JOB_NAME="test-alpine",
                                                                  CI_PIPELINE_SOURCE="pipeline"))

    def test_same_commit_and_regular_merge_need_no_pr_lookup(self):
        ci.git("checkout", "-q", "feature")
        self.assertEqual(self.jobs[0]["target_url"], self.evidence(CI_COMMIT_SHA=self.tip))
        ci.git("checkout", "-qb", "merge-test", self.base)
        ci.git("merge", "--no-ff", "-m", "merge feature", "feature")
        merge = ci.git("rev-parse", "HEAD")
        self.routes[f"commits/{merge}/statuses?per_page=100"] = []
        self.assertEqual(self.jobs[0]["target_url"], self.evidence(CI_COMMIT_SHA=merge))
        self.assertFalse(any("pulls" in call.args[0] for call in self.api.read.call_args_list))

    def test_tree_and_linked_pr_must_match(self):
        for obj, key, invalid in ((self.commit["tree"], "sha", self.base), (self.commit, "sha", self.head),
                                  (self.pr, "merge_commit_sha", self.base), (self.pr, "state", "open"),
                                  (self.pr, "merged_at", None), (self.pr["head"], "sha", "../invalid"),
                                  (self.pr["base"], "ref", "other"), (self.pr["base"]["repo"], "full_name", "other/repo")):
            with self.subTest(key=key, invalid=invalid), patch.dict(obj, {key: invalid}):
                self.assertIsNone(self.evidence())

    def test_mismatched_checkout_cannot_reuse_or_record_evidence(self):
        env = dict(self.env, CI_COMMIT_SHA=self.base)
        self.assertEqual(0, self.exercise(env, codes=[0, 0]))
        self.api.read.assert_not_called()
        self.api.status.assert_called_with(env, "success", "test-ubuntu #101", env["CI_JOB_URL"])

    def test_inputs_that_always_run_full(self):
        for env in ({"CI_COMMIT_BRANCH": "feature"}, {"CI_PIPELINE_SOURCE": "web"},
                    {"CI_PIPELINE_SOURCE": "schedule"}, {"QORE_CI_FORCE_FULL": "1"}):
            self.assertIsNone(self.evidence(**env))
        self.assertIsNone(self.evidence(None))
        self.api.request.assert_not_called()

    def test_partial_failed_stale_or_foreign_evidence(self):
        for changes in ({"state": "pending"}, {"state": "failure"}, {"context": "other"}, {"creator": {"id": 9}},
                        {"description": "test-ubuntu #100"}, {"description": f"test-ubuntu #99 {UBUNTU}"},
                        {"description": f"test-ubuntu #101 {UBUNTU}"}, {"description": "Gitlab CI"},
                        {"target_url": self.env["CI_JOB_URL"] + "?elsewhere"},
                        {"target_url": self.env["CI_JOB_URL"].replace("module-ssh2", "module-ssh")}):
            with self.subTest(changes=changes), patch.dict(self.jobs[0], changes):
                self.assertIsNone(self.evidence())
        self.jobs.insert(0, self.status("test-ubuntu", state="failure", description="test-ubuntu #100"))
        self.assertIsNone(self.evidence())
        self.jobs[:] = [self.status("test-ubuntu"), self.status("test-alpine", pipeline=99)]
        self.assertIsNone(self.evidence())
        self.assertIsNone(self.evidence("ff" * 32))

    def test_changed_image_reruns_only_affected_platform(self):
        self.assertIsNone(self.evidence("ff" * 32))
        self.assertEqual(self.jobs[1]["target_url"], self.evidence(ALPINE, CI_JOB_NAME="test-alpine"))

    def exercise(self, env, fingerprint=UBUNTU, codes=()):
        results = [subprocess.CompletedProcess([], code) for code in codes]
        # Git queries still use real Git. Only the full build and test subprocesses are simulated.
        original_run = subprocess.run
        def commands(*args, **kwargs):
            if args[0][0] == "git":
                return original_run(*args, **kwargs)
            return results.pop(0)
        with patch.object(ci, "image_fingerprint", return_value=fingerprint), \
                patch.object(ci.subprocess, "run", side_effect=commands), redirect_stdout(io.StringIO()):
            result = ci.run(self.api, env)
        self.assertEqual([], results)
        return result

    def test_reuse_reports_source_job_without_recording_another_full_run(self):
        self.assertEqual(0, self.exercise(self.env))
        self.api.status.assert_called_once_with(self.env, "success", "test-ubuntu reused a tested tree",
                                                self.jobs[0]["target_url"])

    def test_full_suite_reports_only_its_own_result(self):
        for fingerprint, codes, description in ((UBUNTU, [0, 0], "test-ubuntu #101 " + UBUNTU),
                                               (None, [0, 0], "test-ubuntu #101"),
                                               (UBUNTU, [1], "test-ubuntu #101"),
                                               (UBUNTU, [0, 2], "test-ubuntu #101")):
            env = dict(self.env, CI_COMMIT_BRANCH="feature")
            self.assertEqual(codes[-1], self.exercise(env, fingerprint, codes))
            self.api.status.assert_any_call(env, "pending", "test-ubuntu #101", env["CI_JOB_URL"])
            self.api.status.assert_called_with(env, "failure" if codes[-1] else "success", description,
                                               env["CI_JOB_URL"])

    def test_unavailable_api_and_failed_reuse_publication_run_full(self):
        for broken in (OSError(), {"bad": "response"}):
            self.api.read.side_effect = broken if isinstance(broken, Exception) else lambda path: broken
            self.assertEqual(0, self.exercise(self.env, codes=[0, 0]))
        self.api.read.side_effect = self.routes.__getitem__
        self.api.status.side_effect = [OSError(), None, None]
        self.assertEqual(0, self.exercise(self.env, codes=[0, 0]))

    def test_missing_status_credential_preserves_full_job_result(self):
        self.api = ci.GitHub(dict(self.env, GITHUB_ORG="qoretechnologies"))
        with patch.object(ci, "urlopen") as network:
            self.assertEqual(0, self.exercise(self.env, codes=[0, 0]))
            self.assertEqual(2, self.exercise(self.env, codes=[0, 2]))
            network.assert_not_called()

    def test_status_request_preserves_context_and_scopes_auth_to_initial_host(self):
        api = ci.GitHub(dict(self.env, GITHUB_ORG="qoretechnologies", GITHUB_ACCESS_TOKEN="test-only"))
        with patch.object(ci, "urlopen", return_value=io.BytesIO(b"{}")) as send:
            api.status(self.env, "pending", "test-ubuntu #101", self.env["CI_JOB_URL"])
        request = send.call_args.args[0]
        self.assertEqual(f"https://api.github.com/repos/{api.repo}/statuses/{self.head}", request.full_url)
        self.assertEqual("Bearer test-only", request.unredirected_hdrs["Authorization"])
        self.assertNotIn("Authorization", request.headers)
        self.assertEqual(dict(state="pending", description="test-ubuntu #101", context="module-ssh2",
                              target_url=self.env["CI_JOB_URL"]), ci.json.loads(request.data))

    def test_fingerprint_requires_complete_valid_output(self):
        for code, output, expected in ((0, UBUNTU + "\n", UBUNTU), (1, UBUNTU, None), (0, "bad", None)):
            with patch.object(ci.subprocess, "run", return_value=subprocess.CompletedProcess([], code, output)):
                self.assertEqual(expected, ci.image_fingerprint())


if __name__ == "__main__":
    unittest.main()
