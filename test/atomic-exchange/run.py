#!/usr/bin/env python3
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Run debug-enabled exchange tests against a fresh loopback-only OpenSSH endpoint."""
import argparse
import base64
import hashlib
import importlib.util
import os
from pathlib import Path
import pwd
import signal
import shutil
import socket
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
loader = importlib.util.spec_from_file_location("ssh_fixture", REPO / "rpm/run-tests.py")
fixture = importlib.util.module_from_spec(loader)
loader.loader.exec_module(fixture)


def run(args):
    with tempfile.TemporaryDirectory(prefix=".ssh2-exchange-", dir=pwd.getpwuid(os.getuid()).pw_dir) as tmp:
        root = Path(tmp)
        for directory in ("stage", "in", "local"):
            (root / directory).mkdir(mode=0o700)
        os.mkfifo(root / "gate-ready", 0o600)
        os.mkfifo(root / "gate-release", 0o600)
        for name in ("host", "client"):
            options = ["-t", "ed25519"] if name == "host" else ["-t", "rsa", "-b", "2048", "-m", "PEM"]
            subprocess.run(["ssh-keygen", "-q", *options, "-N", "", "-f", root / name], check=True)
        sftp = next(path for path in (Path("/usr/libexec/openssh/sftp-server"),
                                     Path("/usr/lib/openssh/sftp-server"), Path("/usr/lib/ssh/sftp-server"),
                                     Path("/usr/libexec/sftp-server"))
                    if path.is_file())
        user = pwd.getpwuid(os.getuid()).pw_name
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        config = root / "sshd.conf"
        config.write_text(f"""ListenAddress 127.0.0.1
Port {port}
HostKey {root}/host
PidFile {root}/pid
AuthorizedKeysFile {root}/client.pub
VersionAddendum QoreStagedSFTP_1
StrictModes yes
UsePAM yes
PasswordAuthentication no
KbdInteractiveAuthentication no
AllowUsers {user}
Subsystem sftp "{sys.executable}" "{HERE}/subsystem.py" "{root}" "{sftp}"
""")
        sshd = shutil.which("sshd") or "/usr/sbin/sshd"
        pub = (root / "host.pub").read_text().split()
        (root / "known_hosts").write_text(f"[127.0.0.1]:{port} {pub[0]} {pub[1]}\n")
        env = dict(os.environ, QORE_MODULE_DIR=os.pathsep.join((str(REPO / "qlib"), str(args.build.resolve()),
                       os.getenv("QORE_MODULE_DIR", ""))).rstrip(os.pathsep),
                   EXCHANGE_ROOT=tmp, EXCHANGE_URI=f"{user}@127.0.0.1:{port}",
                   EXCHANGE_PIN=hashlib.sha256(base64.b64decode(pub[1])).hexdigest())
        api = subprocess.check_output(["qore", "--latest-module-api"], text=True).strip()
        native = args.build.resolve() / f"ssh2-api-{api}.qmod"
        with fixture.running_server([sshd, "-D", "-e", "-f", str(config)], env):
            for suite in args.suites or sorted(HERE.glob("*.qtest")):
                command = ["qore", "-b", "--enable-debug", "-l", str(native), str(suite), "-v"]
                if args.valgrind:
                    command.insert(1, "--exec-mode=ast")
                    env["QORE_PCRE2_NO_JIT"] = "1"
                    command = ["valgrind", "--leak-check=full", "--errors-for-leak-kinds=definite",
                               "--error-exitcode=97", *command]
                if suite.name == "disk-full.qtest":
                    # prlimit applies the limit to the Qore process only. The host can still serve the entire file.
                    command = ["prlimit", "--fsize=65536:65536", "--", *command]
                    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
                subprocess.run(command, env=env, check=True, restore_signals=suite.name != "disk-full.qtest",
                               timeout=600 if args.valgrind else 180)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=REPO / "build-debug")
    parser.add_argument("--valgrind", action="store_true")
    parser.add_argument("suites", nargs="*", type=Path)
    run(parser.parse_args())
