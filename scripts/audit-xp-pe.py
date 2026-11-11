#!/usr/bin/env python3
"""Reject known XP-incompatible PE properties; not a complete XP runtime proof."""
import argparse
import json
import re
import struct
import subprocess
from pathlib import Path
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('exe', type=Path)
p.add_argument('--objdump', default='i686-w64-mingw32-objdump')
p.add_argument('--report', type=Path)
a = p.parse_args()
data = a.exe.read_bytes()
assert data[:2] == b'MZ', 'Not a DOS/PE image'
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe+4] == b'PE\0\0', 'Missing PE signature'
machine = struct.unpack_from('<H', data, pe+4)[0]
optional = pe+24
magic = struct.unpack_from('<H', data, optional)[0]
os_version = struct.unpack_from('<HH', data, optional+40)
subsystem_version = struct.unpack_from('<HH', data, optional+48)
text = subprocess.check_output([a.objdump, '-p', str(a.exe)], text=True)
dlls = re.findall(r'DLL Name:\s*(\S+)', text)
allowed_dlls = {'kernel32.dll', 'msvcrt.dll', 'ws2_32.dll', 'winmm.dll'}
blocked = {'CancelIoEx', 'GetTickCount64', 'WSAPoll', 'InitOnceExecuteOnce',
           'InitializeCriticalSectionEx', 'GetOverlappedResultEx', 'InetPtonA',
           'InetPtonW', 'inet_pton', 'inet_ntop', 'GetSystemTimePreciseAsFileTime',
           'GetFileInformationByHandleEx', 'SetThreadDescription',
           'AcquireSRWLockExclusive', 'CreateEventExW', 'GetTickCount64',
           'InitializeConditionVariable', 'SleepConditionVariableCS'}
found = sorted(name for name in blocked if re.search(r'\b'+re.escape(name)+r'\b', text))
problems = []
if machine != 0x14c or magic != 0x10b: problems.append('not an x86 PE32 image')
if os_version != (5, 1) or subsystem_version != (5, 1): problems.append('PE target is not 5.1')
if not dlls: problems.append('no import table decoded')
if any(d.lower() not in allowed_dlls for d in dlls): problems.append('unexpected runtime DLL')
if 'msvcrt.dll' not in {d.lower() for d in dlls}: problems.append('expected XP system MSVCRT')
if found: problems.append('known unavailable API imports')
report = {'image': a.exe.name, 'machine': hex(machine), 'pe32': magic == 0x10b,
          'os_version': list(os_version), 'subsystem_version': list(subsystem_version),
          'dlls': dlls, 'known_incompatible_imports': found, 'problems': problems,
          'scope': 'static PE/import checks only; XP execution and all API behaviors remain unverified'}
if a.report:
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
if problems: raise SystemExit(1)
