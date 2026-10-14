"""Owned-window lifecycle, stalled captures and bounded resource observation.

Run on an interactive Windows desktop with Python and psutil. The script only
addresses its own preview windows and companions; it never changes Windows
preferences or kills a user application. Not part of the headless build.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import queue
import struct
import subprocess
import threading
import time

import psutil

parser = argparse.ArgumentParser()
parser.add_argument('--bundle', type=Path, required=True)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--cycles', type=int, default=100)
parser.add_argument('--idle-seconds', type=int, default=90)
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
defaults = {s['id']: s['default'] for s in manifest['settings']
    if s['type'] != 'section'}
u = c.WinDLL('user32', use_last_error=True)
k = c.WinDLL('kernel32', use_last_error=True)
u.SetProcessDpiAwarenessContext.argtypes = [c.c_void_p]
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetWindowLongPtrW.argtypes = [w.HWND, c.c_int]
u.GetWindowLongPtrW.restype = c.c_ssize_t
u.GetLayeredWindowAttributes.argtypes = [w.HWND, c.POINTER(w.DWORD),
                                         c.POINTER(c.c_ubyte), c.POINTER(w.DWORD)]
u.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM,
                                  w.UINT, w.UINT, c.POINTER(c.c_size_t)]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.GetGuiResources.argtypes = [c.c_void_p, w.DWORD]
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.OpenProcess.restype = c.c_void_p
k.CloseHandle.argtypes = [c.c_void_p]
callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.EnumWindows.argtypes = [callback, w.LPARAM]


def until(test, message, seconds=5):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        value = test()
        if value:
            return value
        time.sleep(.01)
    raise AssertionError(message)


def windows(pid, cls):
    found = []
    @callback
    def visit(hwnd, _):
        owner = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(owner))
        text = c.create_unicode_buffer(256)
        if owner.value == pid:
            u.GetClassNameW(hwnd, text, 256)
            if text.value == cls:
                found.append(hwnd)
        return True
    u.EnumWindows(visit, 0)
    return found


def command(control, value):
    result = c.c_size_t()
    assert u.SendMessageTimeoutW(control, 0x8014, value, 0, 2, 2500,
                                 c.byref(result)), 'fixture command timed out'


def hidden(hwnd):
    key, alpha, flags = w.DWORD(), c.c_ubyte(), w.DWORD()
    return bool(u.GetLayeredWindowAttributes(hwnd, c.byref(key),
                c.byref(alpha), c.byref(flags)) and flags.value & 2
                and alpha.value == 0)


def send(process, record):
    data = json.dumps(record).encode('utf-8')
    process.stdin.write(struct.pack('<I', len(data)) + data)
    process.stdin.flush()


def start_companion(scratch, values):
    process = subprocess.Popen([str(args.bundle / 'companion.exe')],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        creationflags=0x08000000)
    records = queue.Queue()
    def read():
        try:
            while header := process.stdout.read(4):
                length = struct.unpack('<I', header)[0]
                assert 0 < length <= 1024 * 1024
                records.put(json.loads(process.stdout.read(length)))
        except Exception as error:
            records.put({'type': 'reader_error', 'message': str(error)})
    threading.Thread(target=read, daemon=True).start()
    send(process, {'type': 'init', 'v': 5, 'addonId': 'qa-a07-stress',
        'artifactDigest': 'sha256:' + 'c' * 64, 'version': manifest['version'],
        'targetArchitecture': 'windows-x86_64',
        'scratchDirectory': str(scratch), 'layerSettings': {},
        'deviceSettings': values})
    while True:
        item = records.get(timeout=8)
        assert item['type'] in ['progress', 'ready'], item
        if item['type'] == 'ready':
            return process


scratch = args.report.parent / (args.report.stem + '-scratch')
scratch.mkdir(parents=True, exist_ok=True)
status_path = scratch / 'window-animations-status.json'
def status():
    try:
        return json.loads(status_path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return {}


report = {'passed': False, 'scope': 'owned Win32 windows, interactive desktop',
    'atUtc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
    'companionSha256': hashlib.sha256((args.bundle / 'companion.exe').read_bytes()).hexdigest(),
    'previewSha256': hashlib.sha256((args.bundle / 'window-animations-preview.exe').read_bytes()).hexdigest(),
    'checks': [], 'samples': []}
fixtures, controls, child_ids = [], [], []
process = None
start = time.monotonic()


def sample(label):
    items = []
    for proc in [psutil.Process(process.pid), *psutil.Process(process.pid).children()]:
        handle = k.OpenProcess(0x0400 | 0x0010, False, proc.pid)
        try:
            memory, cpu = proc.memory_info(), proc.cpu_times()
            items.append({'pid': proc.pid, 'privateBytes': memory.private,
                'workingSetBytes': memory.rss, 'handles': proc.num_handles(),
                'gdiObjects': u.GetGuiResources(handle, 0),
                'userObjects': u.GetGuiResources(handle, 1),
                'cpuSeconds': round(cpu.user + cpu.system, 4)})
        finally:
            if handle:
                k.CloseHandle(handle)
    item = {'stage': label, 'seconds': round(time.monotonic() - start, 3),
        'processes': items, 'privateBytes': sum(x['privateBytes'] for x in items),
        'handles': sum(x['handles'] for x in items), 'status': status()}
    report['samples'].append(item)
    print(json.dumps({'stage': label, 'privateBytes': item['privateBytes'],
                     'handles': item['handles']}), flush=True)


try:
    values = defaults | {'applications': 'window-animations-preview.exe',
        'profile': 'custom', 'respectReducedMotion': False,
        'openMode': 'fade', 'openDuration': 500, 'cacheInterval': 100,
        'closeEnabled': False, 'maxSnapshots': 24}
    process = start_companion(scratch, values)
    for i in range(12):
        fixture = subprocess.Popen([str(args.bundle / 'window-animations-preview.exe')],
                                   creationflags=0x08000000)
        fixtures.append(fixture)
        control = until(lambda: windows(fixture.pid, 'MwAnimationPreviewControl'),
                        'preview control absent')[0]
        controls.append(control)
        command(control, 0)
    until(lambda: status().get('tracked') == 12 and status().get('active') == 0,
          '12 windows not captured', seconds=12)
    before = status()['captureFailures']
    for control in controls:
        u.PostMessageW(control, 0x8014, 4, 0)
    until(lambda: status().get('captureFailures', 0) > before,
          'blocked capture not observed')
    sent = time.monotonic()
    send(process, {'type': 'settings', 'v': 5, 'layerSettings': {},
                   'deviceSettings': values | {'enabled': False}})
    until(lambda: status().get('enabled') is False, 'disable stuck behind captures', seconds=1.25)
    latency = time.monotonic() - sent
    assert status()['watchdogAvailable'], 'blocked targets tripped guardian'
    report['checks'].append({'test': '12 stalled targets / disable',
                             'statusLatencySeconds': round(latency, 3)})
    time.sleep(2)
    for control, fixture in zip(controls[1:], fixtures[1:]):
        u.PostMessageW(control, 0x10, 0, 0)
        fixture.wait(timeout=5)
    control, fixture = controls[0], fixtures[0]
    command(control, 3)
    # Repeated interruptions exercise actual live opacity leases, not only
    # configuration parsing or no-op toggles.
    for i in range(args.cycles + 20):
        send(process, {'type': 'settings', 'v': 5, 'layerSettings': {}, 'deviceSettings': values})
        until(lambda: status().get('enabled') is True,
              'resume snapshot not committed')
        command(control, 0)
        hwnd = until(lambda: windows(fixture.pid, 'MwAnimationPreviewCanvas'),
                     'canvas absent')[0]
        original = u.GetWindowLongPtrW(hwnd, -20)
        until(lambda: hidden(hwnd), 'temporary opacity lease absent', seconds=3)
        send(process, {'type': 'settings', 'v': 5, 'layerSettings': {},
                       'deviceSettings': values | {'enabled': False}})
        until(lambda: not hidden(hwnd) and u.GetWindowLongPtrW(hwnd, -20) == original,
              'interrupted animation did not restore original style')
        until(lambda: not windows(process.pid, 'MwWindowAnimationOverlay'),
              'overlay retained after disable')
        command(control, 3)
        until(lambda: status().get('enabled') is False,
              'disable snapshot not committed')
        if i >= 19 and (i - 19) % 20 == 0:
            sample('disabled after warmup' if i == 19 else f'disabled after {i - 19} cycles')
    report['checks'].append({'test': 'interrupted animations restore exactly', 'cycles': args.cycles})
    values['cacheInterval'] = 400
    send(process, {'type': 'settings', 'v': 5, 'layerSettings': {}, 'deviceSettings': values})
    command(control, 0)
    until(lambda: status().get('enabled') and status().get('active') == 0 and status().get('cacheBytes', 0) > 0,
          'idle active capture absent')
    sample('idle active baseline')
    for i in range(3):
        time.sleep(args.idle_seconds / 3)
        sample(f'idle active {(i + 1) * args.idle_seconds / 3:g}s')
    send(process, {'type': 'settings', 'v': 5, 'layerSettings': {},
                   'deviceSettings': values | {'enabled': False}})
    until(lambda: status().get('enabled') is False and status().get('cacheBytes') == 0,
          'cache retained on disable')
    sample('final disabled')
    child_ids = [p.pid for p in psutil.Process(process.pid).children()]
    send(process, {'type': 'shutdown', 'v': 5})
    assert process.wait(timeout=4) == 0, 'shutdown failed'
    until(lambda: not any(psutil.pid_exists(pid) for pid in child_ids), 'helper orphan after stop')
    report['checks'].append({'test': 'shutdown removes companion/helpers/overlays', 'passed': True})
    report['passed'] = True
except Exception as error:
    report['error'] = str(error)
finally:
    if process and process.poll() is None:
        process.stdin.close()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    for control, fixture in zip(controls, fixtures):
        if fixture.poll() is None:
            u.PostMessageW(control, 0x10, 0, 0)
            try:
                fixture.wait(timeout=5)
            except subprocess.TimeoutExpired:
                fixture.kill()
                fixture.wait()
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'passed': report['passed'], 'error': report.get('error')}), flush=True)
    if not report['passed']:
        raise SystemExit(1)
