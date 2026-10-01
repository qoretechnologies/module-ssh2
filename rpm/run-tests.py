#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Exercise compiled SSH2 modules against a private unprivileged SSH server."""
import argparse
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import pwd
import re
import select
import shutil
import signal
import socket
import subprocess
import tempfile
import time

MODULES = ('SftpPollerUtil', 'SftpPoller', 'Ssh2Connections', 'SftpClientDataProvider')


def await_startup(stream, timeout=20):
    deadline = time.monotonic() + timeout
    output = bytearray()
    while not re.search(rb'Server listening on 127\.0\.0\.1 port [0-9]+\.', output):
        ready, _, _ = select.select([stream], [], [], max(0, deadline - time.monotonic()))
        if not ready:
            raise RuntimeError('SSH startup deadline exceeded: ' + output.decode(errors='replace'))
        data = os.read(stream.fileno(), 4096)
        if not data:
            raise RuntimeError('SSH server exited before startup: ' + output.decode(errors='replace'))
        output.extend(data)
    return output.decode(errors='replace')


@contextmanager
def running_server(command, env):
    server = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              start_new_session=True)
    # Drain the pipe throughout the suites, so negative authentication tests
    # cannot block sshd by filling its logging pipe.
    with ThreadPoolExecutor(max_workers=1) as reader:
        output = None
        try:
            print(await_startup(server.stdout), end='', flush=True)
            output = reader.submit(server.stdout.read)
            yield server
        finally:
            try:
                os.killpg(server.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                server.wait(timeout=20)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(server.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                server.wait()
                raise
            finally:
                try:
                    text = output.result() if output else server.stdout.read()
                finally:
                    server.stdout.close()
                print(text.decode(errors='replace'), end='', flush=True)


def module_paths(build, env):
    paths = subprocess.check_output(['/usr/bin/qore', '--module-path'], env=env, text=True).strip().split(':')
    if build:
        native_dir, aot_dir = build.resolve(), build.resolve() / 'qlib-qmod'
    else:
        candidates = [Path(path) for path in paths
                      if (Path(path) / 'SftpClientDataProvider/SftpClientDataProvider.qmod').is_file()]
        if len(candidates) != 1:
            raise RuntimeError('Expected one installed SSH2 compiled module directory: ' + repr(candidates))
        native_dir = aot_dir = candidates[0]
    native, = native_dir.glob('ssh2-api-*.qmod')
    modules = []
    for name in MODULES:
        candidates = [path for path in (aot_dir / (name + '.qmod'), aot_dir / name / (name + '.qmod'))
                      if path.is_file()]
        if len(candidates) != 1:
            raise RuntimeError('Expected one compiled artifact for ' + name)
        modules.append(candidates[0])
    env.update(QORE_MODULE_DIR=':'.join(dict.fromkeys([str(native_dir), str(aot_dir), *paths])),
               QORE_MODULE_DIR_ONLY='1')
    return [native, *modules]


def run(build=None):
    if os.getuid() == 0:
        raise RuntimeError('SSH package tests must run unprivileged')
    source = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in ('QORE_MODULE_DIR', 'QORE_MODULE_DIR_ONLY', 'QORE_INCLUDE_DIR',
                'LD_LIBRARY_PATH', 'LD_PRELOAD', 'NSS_WRAPPER_PASSWD', 'NSS_WRAPPER_GROUP'):
        env.pop(key, None)
    env.update(LC_ALL='C.UTF-8', TZ='UTC')
    modules = module_paths(build, env)
    qore = ['/usr/bin/qore', '-b', '--enable-debug']
    for module in modules:
        qore += ['-l', str(module)]
    libraries = {path.resolve() for directory in ('/usr/lib64', '/usr/lib')
                 for path in Path(directory).glob('libnss_wrapper.so') if path.is_file()}
    if len(libraries) != 1:
        raise RuntimeError('Expected one native nss_wrapper library: ' + repr(libraries))
    with tempfile.TemporaryDirectory(prefix='qore-ssh2-rpm-') as directory:
        root = Path(directory)
        home = root / 'home'; home.mkdir(mode=0o700)
        (home / '.ssh').mkdir(mode=0o700)
        user = pwd.getpwuid(os.getuid()).pw_name
        rows = subprocess.check_output(['getent', 'passwd'], text=True).splitlines()
        rewritten = []
        for row in rows:
            fields = row.split(':')
            if fields[2] == str(os.getuid()):
                fields[1], fields[5] = 'x', str(home)
            rewritten.append(':'.join(fields))
        (root / 'passwd').write_text('\n'.join(rewritten) + '\n')
        (root / 'group').write_text(subprocess.check_output(['getent', 'group'], text=True))
        env.update(NSS_WRAPPER_PASSWD=str(root / 'passwd'), NSS_WRAPPER_GROUP=str(root / 'group'),
                   LD_PRELOAD=str(next(iter(libraries))))
        for name, options in (('host_key', ['-t', 'ed25519']), ('client_key', ['-t', 'rsa', '-b', '2048', '-m', 'PEM'])):
            subprocess.run(['ssh-keygen', '-q', *options, '-N', '', '-f', str(root / name)], check=True, env=env)
        shutil.copyfile(root / 'client_key.pub', home / '.ssh/authorized_keys')
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            port = listener.getsockname()[1]
        config = root / 'sshd.conf'
        config.write_text('\n'.join([
            f'Port {port}', 'ListenAddress 127.0.0.1', f'HostKey {root}/host_key',
            f'PidFile {root}/sshd.pid', f'AuthorizedKeysFile {home}/.ssh/authorized_keys',
            'StrictModes yes', 'UsePAM yes', 'PasswordAuthentication no', 'KbdInteractiveAuthentication no',
            'PubkeyAuthentication yes', f'AllowUsers {user}', 'PrintMotd no',
            'Subsystem sftp internal-sftp', 'LogLevel INFO', '']))
        settings = subprocess.check_output(['/usr/sbin/sshd', '-T', '-f', str(config)], env=env, text=True)
        if re.search(r'^persourcepenalties ', settings, re.M):
            with config.open('a') as stream:
                stream.write('PerSourcePenalties no\n')
        env.update(QORE_SSH2_TEST_URI=f'{user}@127.0.0.1:{port}', QORE_SSH2_TEST_DIR=str(home),
                   QORE_SSH2_TEST_KEY=str(root / 'client_key'))
        suites = sorted((source / 'test').glob('*.qtest'))
        if len(suites) != 8:
            raise RuntimeError('Review the SSH2 suite inventory before qualification')
        tests = root / 'test'; tests.mkdir()
        for suite in suites:
            text = re.sub(r'^%prepend-module-path .*\n', '', suite.read_text(), flags=re.M)
            (tests / suite.name).write_text(text)
        with running_server(['/usr/sbin/sshd', '-D', '-e', '-f', str(config)], env):
            subprocess.run([*qore, '-e', 'SSH2Client client(ENV.QORE_SSH2_TEST_URI); '
                            'client.setKeys(ENV.QORE_SSH2_TEST_KEY); client.connect(); client.disconnect();'],
                           env=env, cwd=root, check=True, timeout=30)
            for suite in suites:
                args = []
                if suite.name != 'NegativeTests.qtest':
                    endpoint = env['QORE_SSH2_TEST_URI']
                    if suite.name == 'SftpClientDataProvider.qtest':
                        endpoint = 'sftp://' + endpoint + str(home)
                    args = ['-k', env['QORE_SSH2_TEST_KEY'], '--uri=' + endpoint]
                print('=== ' + suite.name + ' ===', flush=True)
                subprocess.run([*qore, str(tests / suite.name), '-v', *args], env=env, cwd=root,
                               check=True, timeout=300)
        print('All eight SSH2 suites passed against compiled modules.', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--build-dir', type=Path)
    mode.add_argument('--installed', action='store_true')
    arguments = parser.parse_args()
    run(arguments.build_dir)
