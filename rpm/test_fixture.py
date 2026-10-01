#!/usr/bin/python3
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
"""Check SSH startup events, process cleanup and compiled module isolation."""
import importlib.util
import io
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

loader = importlib.util.spec_from_file_location('fixture', Path(__file__).with_name('run-tests.py'))
fixture = importlib.util.module_from_spec(loader)
loader.loader.exec_module(fixture)


class FixtureTests(unittest.TestCase):
    def test_startup_event_and_early_exit(self):
        for payload, succeeds in ((b'Server listening on 127.0.0.1 port 18022.\n', True),
                                  (b'invalid host key\n', False)):
            read, write = os.pipe()
            with os.fdopen(read, 'rb') as stream:
                os.write(write, payload); os.close(write)
                if succeeds:
                    self.assertIn('port 18022.', fixture.await_startup(stream))
                else:
                    with self.assertRaisesRegex(RuntimeError, 'exited before startup'):
                        fixture.await_startup(stream)

    def test_startup_deadline(self):
        read, write = os.pipe()
        try:
            with os.fdopen(read, 'rb') as stream, self.assertRaisesRegex(RuntimeError, 'deadline exceeded'):
                fixture.await_startup(stream, timeout=0)
        finally:
            os.close(write)

    def test_real_process_cleanup_on_success_and_test_failure(self):
        command = [sys.executable, '-c',
                   'import signal; print("Server listening on 127.0.0.1 port 18022.", flush=True); signal.pause()']
        for fail in (False, True):
            with self.subTest(fail=fail), patch('sys.stdout', new_callable=io.StringIO):
                process = None
                def exercise():
                    nonlocal process
                    with fixture.running_server(command, os.environ.copy()) as process:
                        if fail:
                            raise ValueError('test failure')
                if fail:
                    with self.assertRaisesRegex(ValueError, 'test failure'):
                        exercise()
                else:
                    exercise()
                self.assertEqual(-signal.SIGTERM, process.returncode)
                self.assertTrue(process.stdout.closed)

    def test_large_server_log_is_drained_without_blocking(self):
        command = [sys.executable, '-c',
                   'print("Server listening on 127.0.0.1 port 18022.", flush=True); print("x" * 1048576, flush=True)']
        with patch('sys.stdout', new_callable=io.StringIO) as output:
            with fixture.running_server(command, os.environ.copy()) as process:
                self.assertEqual(0, process.wait(timeout=10))
            self.assertIn('x' * 1048576, output.getvalue())

    def test_startup_failure_reaps_process(self):
        process = Mock(pid=1234, stdout=io.BytesIO(b'invalid config\n'))
        with patch.object(fixture.subprocess, 'Popen', return_value=process), \
                patch.object(fixture, 'await_startup', side_effect=RuntimeError('startup')), \
                patch.object(fixture.os, 'killpg') as kill, patch('sys.stdout', new_callable=io.StringIO):
            with self.assertRaisesRegex(RuntimeError, 'startup'):
                with fixture.running_server(['sshd'], {}):
                    self.fail('must not start tests')
            kill.assert_called_once_with(process.pid, signal.SIGTERM)
            process.wait.assert_called_once_with(timeout=20)
            self.assertTrue(process.stdout.closed)

    def test_shutdown_timeout_kills_and_reaps(self):
        process = Mock(pid=1234, stdout=io.BytesIO(b''))
        process.wait.side_effect = [subprocess.TimeoutExpired('sshd', 20), 0]
        with patch.object(fixture.subprocess, 'Popen', return_value=process), \
                patch.object(fixture, 'await_startup', return_value=''), \
                patch.object(fixture.os, 'killpg') as kill, patch('sys.stdout', new_callable=io.StringIO):
            with self.assertRaises(subprocess.TimeoutExpired):
                with fixture.running_server(['sshd'], {}):
                    pass
            self.assertEqual([signal.SIGTERM, signal.SIGKILL], [call.args[1] for call in kill.call_args_list])
            self.assertEqual(2, process.wait.call_count)
            self.assertTrue(process.stdout.closed)

    def test_module_artifacts_are_complete_and_isolated(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); build = root / 'build'; aot = build / 'qlib-qmod'; aot.mkdir(parents=True)
            (build / 'ssh2-api-2.0.qmod').touch()
            for name in fixture.MODULES:
                (aot / (name + '.qmod')).touch()
            env = {}
            with patch.object(fixture.subprocess, 'check_output', return_value='/usr/lib64/qore-modules\n'):
                artifacts = fixture.module_paths(build, env)
                self.assertEqual(5, len(artifacts))
                self.assertEqual('1', env['QORE_MODULE_DIR_ONLY'])
                self.assertEqual([str(build), str(aot), '/usr/lib64/qore-modules'], env['QORE_MODULE_DIR'].split(':'))
                (aot / 'SftpPoller.qmod').unlink()
                with self.assertRaisesRegex(RuntimeError, 'SftpPoller'):
                    fixture.module_paths(build, {})


if __name__ == '__main__':
    unittest.main()
