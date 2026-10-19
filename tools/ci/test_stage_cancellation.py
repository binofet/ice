#!/usr/bin/env python3
"""Real owned Python process fixtures; no compiler, network or host-process cleanup."""
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time
import unittest

if os.name != 'nt':
    import fcntl

HELPER = Path(__file__).with_name('run_stage.py')
CHILD = r'''
import json, os, pathlib, signal, subprocess, sys, time
root = pathlib.Path(sys.argv[1]); mode = sys.argv[2]
def stop(number, frame):
    (root/'handled').write_text(str(number))
    print('child handled '+str(number), flush=True)
    sys.exit(0)
signal.signal(signal.SIGTERM, stop)
signal.signal(signal.SIGINT, stop)
if mode == 'stubborn':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGINT, signal.SIG_IGN)
if mode in ('descendant','late-descendant'):
    code = """import os, pathlib, signal, sys, time
signal.signal(signal.SIGTERM, signal.SIG_IGN)
signal.signal(signal.SIGINT, signal.SIG_IGN)
fd=os.open(sys.argv[1],os.O_WRONLY)
os.write(fd,b'R')
pathlib.Path(sys.argv[2]).write_text(str(os.getpid()))
while True: time.sleep(10)
"""
    subprocess.Popen([sys.executable,'-c',code,str(root/'alive.fifo'),str(root/'descendant')],
                     stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    until = time.monotonic()+5
    while not (root/'descendant').exists() and time.monotonic()<until: time.sleep(.01)
    if not (root/'descendant').exists(): raise RuntimeError('descendant not ready')
if mode == 'noisy':
    sys.stdout.write('x'*(9*1024*1024)); sys.stdout.flush()
pending=root/'ready.pending'
pending.write_text(json.dumps({'pid':os.getpid(),'pgid':os.getpgrp(),'owner':os.getppid()}))
pending.replace(root/'ready')
print('child ready',flush=True)
if mode=='late-descendant': sys.exit(0)
while True: time.sleep(10)
'''
DRIVER = r'''
import importlib.util, json, pathlib, signal, sys
helper, restored, grace = sys.argv[1:4]
spec=importlib.util.spec_from_file_location('stage_fixture',helper)
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
if grace=='late':
    original=module.write_json
    fired=False
    def write(path,record):
        global fired
        if not fired:
            fired=True
            root=path.parent
            (root/'late-ready').write_text('ready')
            import time
            until=time.monotonic()+4
            while not (root/'late-go').exists() and time.monotonic()<until: time.sleep(.01)
            if not (root/'late-go').exists(): raise RuntimeError('late control not released')
            os.kill(os.getpid(),signal.SIGTERM)
        original(path,record)
    import os
    module.write_json=write
elif grace!='default': module.CANCEL_GRACE_SECONDS=float(grace)
calls=[]
def previous(number, frame): calls.append(number)
signal.signal(signal.SIGINT,previous); signal.signal(signal.SIGTERM,previous)
sys.argv=[helper,*sys.argv[4:]]
result=module.main()
pathlib.Path(restored).write_text(json.dumps({'restored':all(signal.getsignal(n) is previous for n in (signal.SIGINT,signal.SIGTERM)),'old_handler_calls':calls}))
sys.exit(result)
'''
RENDER_DRIVER = r'''
import pathlib, sys
sys.path.insert(0,sys.argv[1]); root=pathlib.Path(sys.argv[2])
import render_checks as render
render.xvfb.mesa_descriptor=lambda record:{'sha256':'fixture-only'}
render.cases=lambda build:(('cancel-fixture',[sys.executable,str(root/'child.py'),str(root),'descendant']),('must-not-start',[sys.executable,'-c',"import pathlib; pathlib.Path("+repr(str(root/'forbidden'))+").write_text('bad')"]))
sys.argv=['render_checks.py']
sys.exit(render.main())
'''


@unittest.skipIf(os.name == 'nt', 'POSIX process-group cancellation fixtures')
class StageCancellationTests(unittest.TestCase):
    def audit(self, **value):
        target = os.environ.get('CI_CANCELLATION_FIXTURE_AUDIT')
        if target:
            with open(target, 'a') as output:
                output.write(json.dumps(dict(test=self.id(), helper=str(HELPER), **value))+'\n')

    def setUp(self):
        def interrupted(_number, _frame):
            raise KeyboardInterrupt
        previous = signal.signal(signal.SIGTERM, interrupted)
        self.addCleanup(signal.signal, signal.SIGTERM, previous)

    def bash_fixture(self, number, rendered=False, exec_entry=True, late=False):
        launcher = """import fcntl,os,sys
fd=os.open(sys.argv[1],os.O_CREAT|os.O_RDWR,0o600)
os.dup2(fd,9,inheritable=True)
if fd!=9: os.close(fd)
fcntl.flock(9,fcntl.LOCK_EX)
os.execv('/bin/bash',['/bin/bash','--noprofile','--norc','-e','-o','pipefail',*sys.argv[2:]])
"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'entry.sh').write_text('source /dev/null\n'+('exec ' if exec_entry else '')+'"$@"\n')
            (root/'child.py').write_text(CHILD)
            os.mkfifo(root/'alive.fifo')
            fifo = os.open(root/'alive.fifo', os.O_RDONLY | os.O_NONBLOCK)
            child_group = None
            descendant_eof = False
            with (root/'console').open('wb') as console, (root/'lock').open('a+b') as lock:
                target = [sys.executable, '-B', str(HELPER), '--timeout-seconds', '10',
                          'cancel-fixture', '--', sys.executable, str(root/'child.py'), str(root),
                          'late-descendant' if late else 'descendant']
                if late:
                    target = [sys.executable, '-B', '-c', DRIVER, str(HELPER), str(root/'restored.json'),
                              'late', *target[3:]]
                if rendered:
                    target = [sys.executable, '-B', '-c', RENDER_DRIVER, str(HELPER.parent), str(root)]
                argv = [sys.executable, '-B', '-c', launcher, str(root/'lock'), str(root/'entry.sh'), *target]
                owner = subprocess.Popen(argv, env=dict(os.environ, CI_EVIDENCE_DIR=str(root)),
                                         stdout=console, stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    until = time.monotonic()+5
                    while not (root/'ready').exists() and owner.poll() is None and time.monotonic()<until:
                        time.sleep(.01)
                    self.assertTrue((root/'ready').exists())
                    ready = json.loads((root/'ready').read_text())
                    child_group = ready['pgid']
                    self.assertEqual(child_group, ready['pid'])
                    self.assertNotEqual(child_group, os.getpgrp())
                    self.assertEqual(os.getpgid(ready['owner']), owner.pid,
                                     'helper belongs to the owned entry group')
                    self.audit(event='owned', pids=[owner.pid, ready['owner'], ready['pid'],
                                                   int((root/'descendant').read_text())],
                               groups=[owner.pid, child_group], route='entry-pid',
                               rendered=rendered, exec_entry=exec_entry, late=late)
                    self.assertEqual(os.read(fifo, 1), b'R')
                    with self.assertRaises(BlockingIOError):
                        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    if exec_entry and not rendered:
                        self.assertEqual(ready['owner'], owner.pid, 'entry PID is the stage owner')
                    else:
                        self.assertNotEqual(ready['owner'], owner.pid)
                    if late:
                        until=time.monotonic()+3
                        while not (root/'late-ready').exists() and time.monotonic()<until: time.sleep(.01)
                        self.assertTrue((root/'late-ready').exists())
                        (root/'late-go').write_text('go')
                    else:
                        os.kill(owner.pid, number)  # Runner signals the entry PID, not a PGID.
                    if not exec_entry:
                        time.sleep(1.2)
                        self.assertFalse((root/'handled').exists(), 'Bash forwarded the PID signal')
                        with self.assertRaises(BlockingIOError):
                            os.read(fifo, 1)
                        with self.assertRaises(BlockingIOError):
                            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                        self.assertFalse((root/'cancel-fixture.json').exists())
                        return
                    # If the entry Bash exits first, the helper must retain fd9
                    # until its private descendant closes the liveness pipe.
                    try:
                        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    except BlockingIOError:
                        pass
                    else:
                        try:
                            closed = os.read(fifo, 1) == b''
                        except BlockingIOError:
                            closed = False
                        self.assertTrue(closed, 'lock released before descendant cleanup')
                    self.assertTrue(select.select([fifo], [], [], 4)[0])
                    self.assertEqual(os.read(fifo, 1), b'', 'canceled descendant still live')
                    descendant_eof = True
                    until = time.monotonic()+2
                    while True:
                        try:
                            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                            break
                        except BlockingIOError:
                            self.assertLess(time.monotonic(), until, 'helper did not release fd9')
                            time.sleep(.01)
                    self.assertNotEqual(owner.wait(timeout=3), 0)
                    record = json.loads((root/'cancel-fixture.json').read_text())
                    self.assertEqual(record['exit_code'], 128+number)
                    self.assertEqual(record['process_exit_code'], 0)
                    self.assertTrue(record['cleanup_escalated'])
                    self.assertFalse((root/'forbidden').exists(), 'driver started work after cancellation')
                    self.audit(event='result', record=record, fd9_released=True, descendant_eof=True)
                finally:
                    if (root/'console').exists():
                        self.audit(event='console-tail', text=(root/'console').read_bytes()[-4096:].decode(errors='replace'))
                    if child_group is not None:
                        try: os.killpg(child_group, signal.SIGKILL)
                        except ProcessLookupError: pass
                    if owner.poll() is None:
                        try: os.killpg(owner.pid, signal.SIGKILL)
                        except ProcessLookupError: pass
                    owner.wait(timeout=3)
                    if child_group is not None:
                        if not descendant_eof:
                            self.assertTrue(select.select([fifo], [], [], 4)[0],
                                            'teardown descendant did not exit')
                            self.assertEqual(os.read(fifo, 1), b'', 'teardown descendant still live')
                        # Bash may already be gone while its helper finalizes
                        # evidence. fd9 remains held until every such writer
                        # exits; wait for that observation before rmtree.
                        until = time.monotonic()+4
                        while True:
                            try:
                                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                                break
                            except BlockingIOError:
                                self.assertLess(time.monotonic(), until,
                                                'teardown helper still holds fd9')
                                time.sleep(.01)
                        self.audit(event='teardown', helper_pid=ready['owner'],
                                   descendant_eof=True, fd9_released=True)
                    os.close(fifo)

    def test_bash_exec_entry_pid_keeps_fd9_until_cleanup(self):
        self.bash_fixture(signal.SIGTERM)

    def test_bash_exec_entry_pid_forwards_int(self):
        self.bash_fixture(signal.SIGINT)

    def test_late_cancel_retains_group_and_fd9_through_finalization(self):
        self.bash_fixture(signal.SIGTERM, late=True)

    def test_render_entry_pid_forwards_to_active_helper_and_stops(self):
        for number in (signal.SIGTERM, signal.SIGINT):
            with self.subTest(number=number):
                self.bash_fixture(number, rendered=True)

    def test_nonexec_bash_entry_pid_does_not_deliver_to_helper(self):
        self.bash_fixture(signal.SIGTERM, exec_entry=False)

    def test_workflow_cancellable_entries_replace_entry_bash(self):
        text = (Path(__file__).resolve().parents[2]/'.github/workflows/engine-quality.yml').read_text()
        entries = [line.strip() for line in text.splitlines() if line.startswith('          ')
                   and not line.startswith('           ') and any(name in line for name in
                   ['tools/ci/run_stage.py', 'tools/ci/render_checks.py', 'tools/ci/xvfb_readiness.py'])]
        self.assertGreater(len(entries), 0)
        self.assertTrue(all(line.startswith('exec ') for line in entries))

    def fixture(self, number, mode='cooperative', noisy=False, timeout=10,
                driver=False, grace='default', repeats=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fifo = None
            if mode == 'descendant':
                os.mkfifo(root/'alive.fifo')
                fifo = os.open(root/'alive.fifo', os.O_RDONLY | os.O_NONBLOCK)
            argv = ['--timeout-seconds', str(timeout), 'cancel-fixture', '--',
                    sys.executable, '-c', CHILD, str(root), 'noisy' if noisy else mode]
            if driver:
                command = [sys.executable, '-B', '-c', DRIVER, str(HELPER),
                           str(root/'restored.json'), str(grace), *argv]
            else:
                command = [sys.executable, '-B', str(HELPER), *argv]
            child_group = None
            with (root/'console').open('wb') as console:
                owner = subprocess.Popen(command, env=dict(os.environ, CI_EVIDENCE_DIR=str(root)),
                                         stdout=console, stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    deadline = time.monotonic()+5
                    while not (root/'ready').exists() and owner.poll() is None and time.monotonic()<deadline:
                        time.sleep(.01)
                    self.assertTrue((root/'ready').exists(), 'fixture command reached readiness')
                    info = json.loads((root/'ready').read_text())
                    child_group = info['pgid']
                    self.assertEqual(child_group, info['pid'])
                    self.assertNotEqual(child_group, os.getpgrp())
                    self.audit(event='owned', pids=[owner.pid, info['pid']],
                               groups=[owner.pid, child_group], route='direct-owner')
                    if fifo is not None:
                        self.assertEqual(os.read(fifo, 1), b'R')
                    canceled_at = time.monotonic()
                    os.kill(owner.pid, number)
                    if repeats:
                        for _ in range(4):
                            time.sleep(.05)
                            if owner.poll() is None:
                                os.kill(owner.pid, signal.SIGTERM)
                    result = owner.wait(timeout=6)
                    self.assertEqual(result, 128+number)
                    self.assertLess(time.monotonic()-canceled_at, 4)
                    record = json.loads((root/'cancel-fixture.json').read_text())
                    self.assertTrue(record['cancelled'])
                    self.assertEqual(record['cancellation_signal'], number)
                    self.assertEqual(record['exit_code'], 128+number)
                    if mode == 'stubborn':
                        self.assertEqual(record['process_exit_code'], -signal.SIGKILL)
                        self.assertTrue(record['cleanup_escalated'])
                    else:
                        self.assertEqual(record['process_exit_code'], 0)
                    if fifo is not None:
                        self.assertTrue(select.select([fifo], [], [], 1)[0], 'descendant writer closed')
                        self.assertEqual(os.read(fifo, 1), b'', 'no live descendant retains its private FD')
                    if driver:
                        restored = json.loads((root/'restored.json').read_text())
                        self.assertTrue(restored['restored'])
                        self.assertEqual(restored['old_handler_calls'], [])
                    log = (root/'cancel-fixture.log').read_bytes()
                    self.assertIn(b'child ready', log)
                    if noisy:
                        self.assertTrue(record['log_truncated'])
                        self.assertGreater(record['output_bytes'], 9*1024*1024)
                        self.assertLess(len(log), 8*1024*1024+100)
                        header = len(('+ '+' '.join(argv[argv.index('--')+1:])+'\n').encode())
                        self.assertLess((root/'console').stat().st_size, 1024*1024+header+5000)
                    self.audit(event='result', record=record, descendant_eof=fifo is not None)
                    return record
                finally:
                    # Clean only fixture-owned identifiers, including on baseline failures.
                    if child_group is not None:
                        try: os.killpg(child_group, signal.SIGKILL)
                        except ProcessLookupError: pass
                    if owner.poll() is None:
                        os.killpg(owner.pid, signal.SIGKILL)
                    owner.wait(timeout=3)
                    if fifo is not None:
                        os.close(fifo)

    def test_term_forwards_and_child_zero_cannot_pass_cancellation(self):
        record = self.fixture(signal.SIGTERM)
        self.assertFalse(record['timed_out'])

    def test_int_forwards_and_child_zero_cannot_pass_cancellation(self):
        record = self.fixture(signal.SIGINT)
        self.assertFalse(record['timed_out'])

    def test_stubborn_child_is_killed_and_reaped(self):
        record = self.fixture(signal.SIGTERM, mode='stubborn')
        with self.assertRaises(ProcessLookupError):
            os.kill(record['process_id'], 0)

    def test_zero_exit_parent_cannot_leave_closed_stdout_descendant(self):
        for number in (signal.SIGTERM, signal.SIGINT):
            with self.subTest(number=number):
                record = self.fixture(number, mode='descendant')
                self.assertTrue(record['cleanup_escalated'])

    def test_cancellation_does_not_extend_existing_deadline(self):
        record = self.fixture(signal.SIGTERM, mode='stubborn', driver=True,
                              grace=3, timeout=2)
        self.assertTrue(record['timed_out'])
        self.assertLess(record['duration_seconds'], 3)

    def test_previous_handlers_are_restored_after_cancellation(self):
        self.fixture(signal.SIGTERM, driver=True)

    def test_repeated_signals_keep_first_reason_and_bounded_grace(self):
        self.fixture(signal.SIGINT, mode='stubborn', repeats=True)

    def test_cancellation_preserves_bounded_noisy_log_and_console(self):
        self.fixture(signal.SIGTERM, noisy=True)

    def test_closed_runner_console_cannot_prevent_cancellation_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = subprocess.Popen([sys.executable, '-B', str(HELPER), '--timeout-seconds', '10',
                'closed-console', '--', sys.executable, '-c', CHILD, str(root), 'cooperative'],
                env=dict(os.environ, CI_EVIDENCE_DIR=str(root)), stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL, start_new_session=True)
            owner.stdout.close()
            group = None
            try:
                until = time.monotonic()+5
                while not (root/'ready').exists() and owner.poll() is None and time.monotonic()<until:
                    time.sleep(.01)
                self.assertTrue((root/'ready').exists())
                group = json.loads((root/'ready').read_text())['pgid']
                self.assertNotEqual(group, os.getpgrp())
                self.audit(event='owned', pids=[owner.pid], groups=[owner.pid, group], route='closed-console')
                os.kill(owner.pid, signal.SIGTERM)
                self.assertEqual(owner.wait(timeout=4), 143)
                record = json.loads((root/'closed-console.json').read_text())
                self.assertTrue(record['console_output_failed'])
                self.assertEqual(record['process_exit_code'], 0)
                self.assertIn(b'child ready', (root/'closed-console.log').read_bytes())
            finally:
                if group is not None:
                    try: os.killpg(group, signal.SIGKILL)
                    except ProcessLookupError: pass
                if owner.poll() is None: os.killpg(owner.pid, signal.SIGKILL)
                owner.wait(timeout=3)

    def test_previous_handlers_are_restored_on_normal_and_spawn_failure(self):
        for command, expected in (([sys.executable, '-c', 'raise SystemExit(7)'], 7),
                                  (['/nonexistent/fixture-command'], 127)):
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                argv = [sys.executable, '-B', '-c', DRIVER, str(HELPER), str(root/'restored.json'),
                        'default', '--timeout-seconds', '3', 'normal', '--', *command]
                result = subprocess.run(argv, env=dict(os.environ, CI_EVIDENCE_DIR=str(root)),
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
                self.assertEqual(result.returncode, expected)
                self.assertTrue(json.loads((root/'restored.json').read_text())['restored'])
                record = json.loads((root/'normal.json').read_text())
                self.assertEqual(record['exit_code'], expected)
                self.assertEqual(record['process_exit_code'], expected)
                self.assertNotIn('cancelled', record)

    def test_signal_during_evidence_write_cannot_leave_success_record(self):
        driver = r'''
import importlib.util,os,pathlib,signal,sys
spec=importlib.util.spec_from_file_location('stage_fixture',sys.argv[1])
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
original=module.write_json
fired=False
def write(path,record):
    global fired
    if not fired:
        fired=True
        os.kill(os.getpid(),signal.SIGTERM)
    original(path,record)
module.write_json=write
sys.exit(module.run('late',[sys.executable,'-c','pass'],pathlib.Path(sys.argv[2]),3))
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = subprocess.run([sys.executable,'-B','-c',driver,str(HELPER),str(root)],
                                    stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=5)
            self.assertEqual(result.returncode,143)
            record=json.loads((root/'late.json').read_text())
            self.assertEqual(record['exit_code'],143)
            self.assertEqual(record['process_exit_code'],0)
            self.assertTrue(record['cancelled'])


if __name__ == '__main__':
    unittest.main()
