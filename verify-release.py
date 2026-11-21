"""Verify downloaded release assets against this repository's checksum list."""
from pathlib import Path
import hashlib
import sys

checksums = Path(__file__).resolve().parent / 'SHA256SUMS.txt'
downloads = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd()
verified = 0
for line in checksums.read_text().splitlines():
    expected, name = line.split('  ', 1)
    path = downloads / name
    if not path.exists():
        continue
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(f'CHECK FAILED: {name}')
    verified += 1
    print(f'OK: {name}')
if not verified:
    raise SystemExit('No release assets found. Pass their download directory as the first argument.')
print(f'Verified {verified} downloaded release assets.')
