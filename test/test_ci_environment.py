#!/usr/bin/env python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise the CI image fingerprint with fake qore, ldd and package-manager tools."""

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "test/docker_test/ci-environment.py"


def load_tool():
    # Importing must not leave a __pycache__ directory in the checkout.
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location("ci_environment", TOOL)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class CiEnvironmentTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="ssh2-ci-env-")
        self.addCleanup(directory.cleanup)
        self.dir = Path(directory.name)
        self.bin = self.dir / "bin"
        self.bin.mkdir()
        self.modules = self.dir / "modules"
        self.user_modules = self.dir / "user-modules"
        self.extra = self.dir / "extra"
        for path in (self.modules, self.user_modules / "Sub", self.extra):
            path.mkdir(parents=True)
        (self.modules / "json-api-2.1.qmod").write_bytes(b"\x7fELF json")
        (self.user_modules / "Util.qm").write_text("module Util\n")
        (self.user_modules / "Sub" / "Part.qc").write_text("class Part {}\n")
        os.symlink("Util.qm", self.user_modules / "Alias.qm")
        (self.extra / "Extra.qm").write_text("module Extra\n")
        self.libqore = self.dir / "libqore.so.12"
        self.libqore.write_bytes(b"libqore v1")
        self.packages = self.dir / "packages.txt"
        self.packages.write_text("zlib 1.3\nbash 5.2\n")
        self.version = self.dir / "version.txt"
        self.version.write_text("version 3.0.0\n  git hash: abc\n")

        def tool(name, body):
            path = self.bin / name
            path.write_text(f"#!{sys.executable}\nimport os, sys\n{body}")
            path.chmod(0o755)

        tool("qore", "args = sys.argv[1:]\n"
                     "if args == ['-V']: print(open(os.environ['FAKE_VERSION']).read(), end='')\n"
                     "elif args == ['--module-dir']: print(os.environ['FAKE_MODULE_DIR'])\n"
                     "elif args == ['--user-module-dir']: print(os.environ['FAKE_USER_MODULE_DIR'])\n"
                     "else: sys.exit(9)\n")
        tool("ldd", "print('\\tlinux-vdso.so.1 (0x00007ffd)')\n"
                    "if os.environ.get('FAKE_LIBQORE'): print('\\tlibqore.so.12 => ' + os.environ['FAKE_LIBQORE'] + ' (0x00007f00)')\n"
                    "print('\\tlibc.so.6 => /lib64/libc.so.6 (0x00007f01)')\n")
        # Whichever package manager the real /etc/os-release selects is a fake.
        for name in ("dpkg-query", "apk", "rpm"):
            tool(name, "print(open(os.environ['FAKE_PACKAGES']).read(), end='')\n")
        tool("pkg-config", "print(os.environ['FAKE_SSH2_DIR'])\n")
        tool("sshd", "pass\n")
        for name in ("libssh2.so", "libssh2.h", "libssh2_sftp.h", "libssh2_publickey.h"):
            (self.dir / name).write_text(name)
        self.env = dict(os.environ, PATH=str(self.bin), FAKE_VERSION=str(self.version),
                        FAKE_SSH2_DIR=str(self.dir),
                        FAKE_MODULE_DIR=str(self.modules), FAKE_USER_MODULE_DIR=str(self.user_modules),
                        FAKE_LIBQORE=str(self.libqore), FAKE_PACKAGES=str(self.packages),
                        QORE_MODULE_DIR=f"{self.extra}::{self.modules}")

    def run_tool(self, *args, expect=0, **overrides):
        env = dict(self.env, **overrides)
        result = subprocess.run([sys.executable, str(TOOL), *args], env=env, capture_output=True,
                                text=True, timeout=60)
        self.assertEqual(expect, result.returncode, result.stdout + result.stderr)
        return result

    def fingerprint(self, **overrides):
        output = self.run_tool(**overrides).stdout
        self.assertRegex(output, r"\A[0-9a-f]{64}\n\Z")
        return output.strip()

    def test_manifest_covers_every_input(self):
        manifest = self.run_tool("--manifest").stdout.splitlines()
        self.assertTrue(any(line.startswith("machine ") for line in manifest))
        self.assertTrue(any(line.startswith("os-release ID=") for line in manifest))
        packages = [line for line in manifest if line.startswith("package ")]
        self.assertEqual(["package bash 5.2", "package zlib 1.3"], packages)
        self.assertIn("qore-version   git hash: abc", manifest)
        lines = "\n".join(manifest)
        self.assertRegex(lines, rf"file {self.bin}/qore [0-9a-f]{{64}}")
        self.assertRegex(lines, rf"file {self.libqore} [0-9a-f]{{64}}")
        self.assertRegex(lines, rf"file {self.modules}/json-api-2.1.qmod [0-9a-f]{{64}}")
        self.assertRegex(lines, rf"file {self.user_modules}/Sub/Part.qc [0-9a-f]{{64}}")
        self.assertIn(f"link {self.user_modules}/Alias.qm -> Util.qm", manifest)
        self.assertRegex(lines, rf"file {self.extra}/Extra.qm [0-9a-f]{{64}}")
        # QORE_MODULE_DIR repeats the binary module directory; it is described once.
        self.assertEqual(1, manifest.count(f"module-dir {self.modules}"))
        self.assertEqual(3, len([line for line in manifest if line.startswith("module-dir ")]))

    def test_fingerprint_is_stable_and_ignores_timestamps(self):
        first = self.fingerprint()
        os.utime(self.user_modules / "Util.qm", (1, 1))
        os.utime(self.libqore, (2, 2))
        self.assertEqual(first, self.fingerprint())

    def test_every_input_changes_the_fingerprint(self):
        original = self.fingerprint()
        changes = {
            "module content": lambda: (self.user_modules / "Sub" / "Part.qc").write_text("class Part { x; }\n"),
            "new module": lambda: (self.modules / "xml-api-2.1.qmod").write_bytes(b"xml"),
            "renamed module": lambda: (self.extra / "Extra.qm").rename(self.extra / "Other.qm"),
            "symlink target": lambda: (os.unlink(self.user_modules / "Alias.qm"),
                                       os.symlink("Sub/Part.qc", self.user_modules / "Alias.qm")),
            "libqore": lambda: self.libqore.write_bytes(b"libqore v2"),
            "libssh2": lambda: (self.dir / "libssh2.so").write_bytes(b"libssh2 v2"),
            "libssh2 headers": lambda: (self.dir / "libssh2_sftp.h").write_text("new header"),
            "ssh server": lambda: (self.bin / "sshd").write_bytes(b"new sshd"),
            "qore version": lambda: self.version.write_text("version 3.0.1\n  git hash: def\n"),
            "packages": lambda: self.packages.write_text("zlib 1.3.1\nbash 5.2\n"),
        }
        seen = {original}
        for name, change in changes.items():
            with self.subTest(change=name):
                change()
                current = self.fingerprint()
                self.assertNotIn(current, seen)
                seen.add(current)

    def test_module_path_changes_change_the_fingerprint(self):
        missing = self.dir / "later"
        with_missing = self.fingerprint(QORE_MODULE_DIR=f"{self.extra}:{missing}")
        self.assertIn(f"module-dir {missing} missing",
                      self.run_tool("--manifest", QORE_MODULE_DIR=str(missing)).stdout.splitlines())
        missing.mkdir()
        self.assertNotEqual(with_missing, self.fingerprint(QORE_MODULE_DIR=f"{self.extra}:{missing}"))
        self.assertNotEqual(self.fingerprint(), self.fingerprint(QORE_MODULE_DIR=""))

    def assert_failure(self, message, **overrides):
        result = self.run_tool(expect=1, **overrides)
        self.assertEqual("", result.stdout)
        self.assertIn("cannot fingerprint the CI environment", result.stderr)
        self.assertIn(message, result.stderr)

    def test_missing_qore_fails(self):
        (self.bin / "qore").unlink()
        self.assert_failure("qore not found")

    def test_incomplete_libssh2_install_fails(self):
        self.assert_failure("incomplete libssh2 paths", FAKE_SSH2_DIR="")
        (self.dir / "libssh2.so").unlink()
        self.assert_failure("cannot read")

    def test_unresolved_libqore_fails(self):
        self.assert_failure("cannot resolve libqore", FAKE_LIBQORE="")
        self.assert_failure("cannot read", FAKE_LIBQORE=str(self.dir / "no-libqore.so"))

    def test_missing_ldd_fails(self):
        (self.bin / "ldd").unlink()
        self.assert_failure("ldd not found")

    def test_empty_package_list_fails(self):
        self.packages.write_text("\n")
        self.assert_failure("reports no installed packages")

    def test_failing_qore_query_fails(self):
        (self.bin / "qore").write_text(f"#!{sys.executable}\nimport sys\nsys.exit(3)\n")
        self.assert_failure("qore")

    def test_empty_module_dir_fails(self):
        self.assert_failure("empty module directory", FAKE_USER_MODULE_DIR="")

    def test_unsupported_file_type_fails(self):
        os.mkfifo(self.extra / "pipe")
        self.assert_failure("unsupported file type")

    @unittest.skipIf(os.geteuid() == 0, "root can read every directory")
    def test_unreadable_module_directory_fails(self):
        locked = self.user_modules / "Sub"
        locked.chmod(0)
        self.addCleanup(locked.chmod, 0o755)
        self.assert_failure("cannot list")

    def test_usage(self):
        result = self.run_tool("--other", expect=2)
        self.assertEqual("", result.stdout)
        self.assertIn("Usage: ci-environment.py [--manifest]", result.stderr)

    def test_distribution_selects_its_package_manager(self):
        tool = load_tool()
        self.assertEqual({"ID": "ubuntu", "ID_LIKE": "debian", "NAME": "Ubuntu 24.04"},
                         tool.os_release('ID=ubuntu\nID_LIKE=debian\nNAME="Ubuntu 24.04"\n# comment\n'))
        original = os.environ["PATH"]
        os.environ["PATH"] = str(self.bin)
        os.environ["FAKE_PACKAGES"] = str(self.packages)
        self.addCleanup(os.environ.__setitem__, "PATH", original)
        self.addCleanup(os.environ.pop, "FAKE_PACKAGES")
        for release, expected in (({"ID": "ubuntu"}, "dpkg-query"), ({"ID": "pop", "ID_LIKE": "ubuntu debian"}, "dpkg-query"),
                                  ({"ID": "alpine"}, "apk"), ({"ID": "fedora"}, "rpm"),
                                  ({"ID": "rocky", "ID_LIKE": "rhel centos fedora"}, "rpm")):
            with self.subTest(release=release):
                self.assertEqual(f"packages {expected}", tool.packages(release)[0])
        with self.assertRaisesRegex(tool.EnvironmentError_, "no supported package manager"):
            tool.packages({"ID": "plan9"})
        (self.bin / "apk").unlink()
        with self.assertRaisesRegex(tool.EnvironmentError_, "apk not found"):
            tool.packages({"ID": "alpine"})


if __name__ == "__main__":
    unittest.main()
