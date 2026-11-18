#!/usr/bin/env python3
"""Download a CEF minimal binary tarball from Spotify CDN and extract it.

Usage:
  python scripts/ci_fetch_cef.py <url> <parent_dir> [sha1]

If sha1 is given, the downloaded archive must have exactly this SHA-1 (from index.json of the CDN),
otherwise nothing is extracted and the script fails.

Prints the absolute path to the extracted cef_binary_* root (single line on stdout).
See https://cef-builds.spotifycdn.com/index.html
"""
from __future__ import annotations

import hashlib
import shutil
import sys
import tarfile
import urllib.request
from pathlib import Path


def main() -> int:
	if len(sys.argv) not in (3, 4):
		print("usage: ci_fetch_cef.py <tar.bz2_url> <extract_parent_dir> [sha1]", file=sys.stderr)
		return 2

	url = sys.argv[1]
	parent = Path(sys.argv[2]).resolve()
	expected_sha1 = sys.argv[3].strip().lower() if len(sys.argv) == 4 else ""
	parent.mkdir(parents=True, exist_ok=True)

	archive = parent / "cef_dist.tar.bz2"
	print(f"Downloading CEF from {url}", file=sys.stderr)
	req = urllib.request.Request(
		url,
		headers={"User-Agent": "cs16-client-ci/1.0"},
	)
	with urllib.request.urlopen(req, timeout=600) as resp:
		with archive.open("wb") as out:
			shutil.copyfileobj(resp, out)

	if expected_sha1:
		h = hashlib.sha1()
		with archive.open("rb") as f:
			for block in iter(lambda: f.read(1 << 20), b""):
				h.update(block)
		actual = h.hexdigest()
		if actual != expected_sha1:
			print(f"SHA-1 mismatch: expected {expected_sha1}, got {actual}", file=sys.stderr)
			archive.unlink(missing_ok=True)
			return 1
		print(f"SHA-1 OK: {actual}", file=sys.stderr)

	print(f"Extracting to {parent}", file=sys.stderr)
	with tarfile.open(archive, "r:bz2") as tf:
		# filter= exists in Python 3.12+
		kw = {}
		if sys.version_info >= (3, 12):
			kw["filter"] = "data"
		tf.extractall(parent, **kw)

	archive.unlink(missing_ok=True)

	for p in sorted(parent.iterdir()):
		if p.is_dir() and p.name.startswith("cef_binary_"):
			print(p.resolve().as_posix() if sys.platform != "win32" else str(p.resolve()))
			return 0

	print("No cef_binary_* directory found after extract", file=sys.stderr)
	return 1


if __name__ == "__main__":
	sys.exit(main())
