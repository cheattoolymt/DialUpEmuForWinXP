#!/usr/bin/env python3
"""Verify the recorded baseline Git objects, optionally the working source too."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--worktree', action='store_true', help='also compare all copied files to the worktree')
args = parser.parse_args()
manifest = json.loads((root / 'docs/xp/baseline-manifest.json').read_text())

def git(*args):
    return subprocess.check_output(['git', *args], cwd=root)

baseline = manifest['baseline_commit']
assert git('rev-parse', baseline + '^{tree}').decode().strip() == manifest['baseline_tree']
paths = git('ls-tree', '-rz', '--name-only', baseline).decode().strip('\0').split('\0')
assert sorted(paths) == sorted(manifest['files']), 'Baseline path set differs'
for path, expected in manifest['files'].items():
    content = git('show', baseline + ':' + path)
    assert len(content) == expected['bytes'], path
    assert hashlib.sha256(content).hexdigest() == expected['sha256'], path
    if args.worktree:
        assert (root / path).read_bytes() == content, 'Worktree differs: ' + path
print(f"PASS: {len(paths)} baseline files; tree={manifest['baseline_tree']}; worktree={args.worktree}")
