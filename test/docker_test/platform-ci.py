#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Run a platform suite, or reuse verified results for the same develop tree."""

from http.client import HTTPException
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.request import Request, urlopen


PLATFORMS = {"test-ubuntu", "test-alpine"}
SHA = re.compile(r"[0-9a-f]{40}")
FINGERPRINT = re.compile(r"[0-9a-f]{64}")
TOOLS = Path(__file__).resolve().parent


class GitHub:
    def __init__(self, env):
        self.repo = f"{env['GITHUB_ORG']}/{env['REPO_NAME']}"
        self.token = env.get("GITHUB_ACCESS_TOKEN")

    def request(self, path, data=None):
        if not self.token:
            raise OSError("GitHub status credential unavailable")
        body = None if data is None else json.dumps(data).encode()
        req = Request("https://api.github.com" + path, data=body,
                      headers={"Accept": "application/vnd.github+json", "Content-Type": "application/json"})
        req.add_unredirected_header("Authorization", "Bearer " + self.token)
        with urlopen(req, timeout=15) as response:
            return json.load(response)

    def read(self, path):
        return self.request(f"/repos/{self.repo}/{path}")

    def status(self, env, state, description, url):
        self.request(f"/repos/{self.repo}/statuses/{env['CI_COMMIT_SHA']}", {
            "state": state, "context": env["REPO_NAME"], "description": description, "target_url": url,
        })


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True, timeout=15).stdout.strip()


def candidates(api, head, tree):
    yield head
    parents = git("rev-list", "--parents", "-1", "HEAD").split()[1:]
    for parent in parents:
        if git("rev-parse", parent + "^{tree}") == tree:
            yield parent

    # The tested tip is not a parent of a squash commit. Resolve it through
    # the merged PR and compare the Git trees before reading its statuses.
    for pr in api.read(f"commits/{head}/pulls?per_page=100"):
        if (pr["state"] != "closed" or not pr["merged_at"] or pr["merge_commit_sha"] != head
                or pr["base"]["ref"] != "develop" or pr["base"]["repo"]["full_name"] != api.repo):
            continue
        tip = pr["head"]["sha"]
        if not SHA.fullmatch(tip) or tip in [head, *parents]:
            continue
        commit = api.read(f"git/commits/{tip}")
        if commit["sha"] == tip and commit["tree"]["sha"] == tree:
            yield tip


def prior_job(statuses, env, actor, fingerprint):
    """Require both platform successes from one earlier pipeline of this mirror."""
    project = env["CI_PROJECT_URL"] + "/-/jobs/"
    job_url = re.compile(re.escape(project) + r"[1-9][0-9]*")
    description = re.compile(r"(test-ubuntu|test-alpine) #([1-9][0-9]*)(?: ([0-9a-f]{64}))?")
    pipelines = {}
    # GitHub returns newest first. A retry's pending/failure supersedes older
    # success for the same platform and pipeline.
    for item in statuses:
        if item["context"] != env["REPO_NAME"] or item["creator"]["id"] != actor:
            continue
        match = description.fullmatch(item.get("description") or "")
        url = item.get("target_url") or ""
        if not match or not job_url.fullmatch(url):
            continue
        platform, pipeline, recorded = match.groups()
        if int(pipeline) >= int(env["CI_PIPELINE_ID"]):
            continue
        jobs = pipelines.setdefault(pipeline, {})
        if platform not in jobs:
            jobs[platform] = (recorded, url) if item["state"] == "success" and recorded else None
    if pipelines:
        jobs = pipelines[max(pipelines, key=int)]
        if all(jobs.get(name) for name in PLATFORMS):
            recorded, url = jobs[env["CI_JOB_NAME"]]
            if recorded == fingerprint:
                return url
    return None


def evidence(api, env, fingerprint):
    if (not fingerprint or env.get("CI_COMMIT_BRANCH") != "develop"
            or env.get("CI_PIPELINE_SOURCE") not in ("push", "pipeline")
            or env.get("QORE_CI_FORCE_FULL") == "1"):
        return None
    head = env["CI_COMMIT_SHA"]
    actor = api.request("/user")["id"]
    if type(actor) is not int or actor <= 0:
        return None
    tree = git("rev-parse", "HEAD^{tree}")
    for commit in candidates(api, head, tree):
        job = prior_job(api.read(f"commits/{commit}/statuses?per_page=100"), env, actor, fingerprint)
        if job:
            return job
    return None


def image_fingerprint():
    # Match the environment sourced by the build scripts. Keep it in a subshell.
    result = subprocess.run(["bash", "-ec", '. /tmp/env.sh; exec python3 "$1"', "fingerprint",
                             str(TOOLS / "ci-environment.py")], capture_output=True, text=True, timeout=180)
    value = result.stdout.strip()
    if result.returncode == 0 and FINGERPRINT.fullmatch(value):
        return value
    return None


def run(api, env):
    platform = env["CI_JOB_NAME"]
    if platform not in PLATFORMS:
        raise ValueError("unknown platform job")
    label = f"{platform} #{env['CI_PIPELINE_ID']}"
    fingerprint = None
    try:
        # Only record evidence for a checkout of the SHA receiving the status.
        if git("rev-parse", "HEAD") == env["CI_COMMIT_SHA"]:
            fingerprint = image_fingerprint()
        if not fingerprint:
            print("CI environment fingerprint unavailable. Running the full suite.", flush=True)
        job = evidence(api, env, fingerprint)
        if job:
            api.status(env, "success", f"{platform} reused a tested tree", job)
            print(f"Reused {platform} for an identical Git tree and environment from {job}", flush=True)
            return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, HTTPException) as error:
        print(f"CI reuse evidence unavailable ({type(error).__name__}). Running the full suite.", flush=True)

    try:
        api.status(env, "pending", label, env["CI_JOB_URL"])
    except (OSError, ValueError, HTTPException) as error:
        print(f"Could not publish pending status ({type(error).__name__}).", flush=True)
    print(f"Running the full {platform} suite.", flush=True)
    # Run policy regression tests in the full Ubuntu job, before module installation.
    code = 0
    if platform == "test-ubuntu":
        code = subprocess.run([sys.executable, "-B", "-W", "error", "-m", "unittest", "discover",
                               "-s", "test", "-p", "test_*.py"]).returncode
    if code == 0:
        code = subprocess.run([str(TOOLS / (platform + ".sh"))]).returncode
    if code == 0 and fingerprint:
        label += " " + fingerprint
    try:
        api.status(env, "success" if code == 0 else "failure", label, env["CI_JOB_URL"])
    except (OSError, ValueError, HTTPException) as error:
        print(f"Could not publish final status ({type(error).__name__}).", flush=True)
    return code


if __name__ == "__main__":
    sys.exit(run(GitHub(os.environ), os.environ))
