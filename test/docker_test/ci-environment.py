#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Fingerprint the test image inputs that this repository's Git tree does not record.

The platform jobs test this module against the Qore installation and the system
packages of their CI image. Successful-tree reuse is valid only when those inputs
are unchanged, so each job records this fingerprint before it builds anything and a
later job may reuse its result only when its own fingerprint is identical.

The fingerprint covers the machine architecture, /etc/os-release, the installed
system packages (dpkg, apk or rpm), the output of "qore -V" and the content of the
qore binary, its libqore and every Qore module directory (binary, user and
QORE_MODULE_DIR), plus libssh2's headers, library and the SSH test server.
Any inspection failure is an error: without a fingerprint, the
full job runs.

Usage: ci-environment.py [--manifest]
Prints the SHA-256 fingerprint, or with --manifest the manifest that it hashes.
"""

import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


class EnvironmentError_(Exception):
    """The environment cannot be inspected completely; no fingerprint is valid."""


def run(*args):
    try:
        return subprocess.run(list(args), check=True, capture_output=True, text=True,
                              timeout=120).stdout
    except (OSError, subprocess.SubprocessError) as error:
        raise EnvironmentError_(f"{args[0]}: {error}") from error


PACKAGE_MANAGERS = (
    (("debian", "ubuntu"), "dpkg-query", ["-W", "-f", "${Package} ${Version} ${Architecture}\n"]),
    (("alpine",), "apk", ["info", "-v"]),
    (("fedora", "rhel", "centos", "suse", "opensuse"), "rpm",
     ["-qa", "--qf", "%{NAME} %{EPOCH}:%{VERSION}-%{RELEASE} %{ARCH}\n"]),
)


def os_release(text):
    values = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if sep:
            values[key.strip()] = value.strip().strip("\"'")
    return values


def packages(release):
    """Return a sorted manifest of the system packages from the distribution's own package manager.

    Another distribution's tool may be installed without owning the system packages
    (dpkg on Fedora, for example), so the tool is chosen from ID and ID_LIKE.
    """
    ids = [release.get("ID", "")] + release.get("ID_LIKE", "").split()
    for families, tool, args in PACKAGE_MANAGERS:
        if not any(family in ids for family in families):
            continue
        path = shutil.which(tool)
        if not path:
            raise EnvironmentError_(f"{tool} not found")
        lines = sorted(line for line in run(path, *args).splitlines() if line.strip())
        if not lines:
            raise EnvironmentError_(f"{tool} reports no installed packages")
        return [f"packages {tool}"] + [f"package {line}" for line in lines]
    raise EnvironmentError_(f"no supported package manager for distribution {ids}")


def file_digest(path):
    digest = hashlib.sha256()
    try:
        with open(path, "rb") as file:
            for block in iter(lambda: file.read(1 << 20), b""):
                digest.update(block)
    except OSError as error:
        raise EnvironmentError_(f"cannot read {path}: {error}") from error
    return digest.hexdigest()


def tree(root):
    """Describe every entry below root; symbolic links are recorded, not followed."""
    lines = []
    def fail(error):
        raise EnvironmentError_(f"cannot list {error.filename}: {error}")
    for directory, dirs, files in os.walk(root, onerror=fail):
        dirs.sort()
        base = Path(directory)
        for name in sorted(dirs + files):
            path = base / name
            relative = path.relative_to(root).as_posix()
            if path.is_symlink():
                lines.append(f"link {root}/{relative} -> {os.readlink(path)}")
            elif name in files and path.is_file():
                lines.append(f"file {root}/{relative} {file_digest(path)}")
            elif name in files:
                raise EnvironmentError_(f"unsupported file type: {path}")
    return lines


def libqore(qore):
    """Return the libqore paths that the dynamic linker resolves for the qore binary."""
    ldd = shutil.which("ldd")
    if not ldd:
        raise EnvironmentError_("ldd not found")
    paths = sorted({match.group(1) for match in re.finditer(r"\blibqore[^\s]*\s+=>\s+(\S+)", run(ldd, qore))})
    if not paths:
        raise EnvironmentError_("cannot resolve libqore for the qore binary")
    return paths


def qore_inputs():
    qore = shutil.which("qore")
    if not qore:
        raise EnvironmentError_("qore not found")
    qore = os.path.realpath(qore)
    lines = [f"qore-version {line}" for line in run(qore, "-V").splitlines()]
    files = [qore] + [os.path.realpath(path) for path in libqore(qore)]
    lines += [f"file {path} {file_digest(path)}" for path in files]
    roots = [run(qore, "--module-dir").strip(), run(qore, "--user-module-dir").strip()]
    roots += [entry for entry in os.environ.get("QORE_MODULE_DIR", "").split(":") if entry]
    seen = set()
    for root in roots:
        if not root:
            raise EnvironmentError_("qore reported an empty module directory")
        resolved = os.path.realpath(root)
        if resolved in seen:
            continue
        seen.add(resolved)
        if os.path.isdir(resolved):
            lines.append(f"module-dir {resolved}")
            lines += tree(resolved)
        else:
            lines.append(f"module-dir {resolved} missing")
    return lines


def manifest():
    lines = [f"machine {platform.machine()}"]
    try:
        release = Path("/etc/os-release").read_text()
    except OSError as error:
        raise EnvironmentError_(f"cannot read /etc/os-release: {error}") from error
    lines += [f"os-release {line}" for line in release.splitlines()]
    lines += packages(os_release(release)) + qore_inputs()
    libdir = Path(run("pkg-config", "--variable=libdir", "libssh2").strip())
    include = Path(run("pkg-config", "--variable=includedir", "libssh2").strip())
    if not libdir.is_absolute() or not include.is_absolute():
        raise EnvironmentError_("pkg-config returned incomplete libssh2 paths")
    sshd = shutil.which("sshd") or "/usr/sbin/sshd"
    for path in (libdir / "libssh2.so", include / "libssh2.h", include / "libssh2_sftp.h",
                 include / "libssh2_publickey.h", sshd):
        lines.append(f"file {path} {file_digest(path)}")
    return lines


def main(argv):
    if argv not in ([], ["--manifest"]):
        print(__doc__.strip().splitlines()[-2], file=sys.stderr)
        return 2
    try:
        lines = manifest()
    except EnvironmentError_ as error:
        print(f"cannot fingerprint the CI environment: {error}", file=sys.stderr)
        return 1
    text = "".join(line + "\n" for line in lines)
    print(text, end="") if argv else print(hashlib.sha256(text.encode()).hexdigest())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
