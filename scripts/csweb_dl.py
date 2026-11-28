#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Download a prebuilt csweb library from a GitHub release.

Usage:
    csweb_dl.py --repo OWNER/REPO --tag vX.Y.Z --asset NAME --output DIR

Verifies the file against SHA256SUMS from the same release if it exists.
"""
import argparse
import hashlib
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.github.com"


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class _StripAuthOnRedirect(urllib.request.HTTPRedirectHandler):
    """Release assets redirect to a signed storage URL, which rejects
    an extra Authorization header."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        new = super().redirect_request(req, fp, code, msg, headers, newurl)
        if new is not None:
            if urllib.parse.urlparse(newurl).netloc != urllib.parse.urlparse(req.full_url).netloc:
                new.remove_header("Authorization")
        return new


_opener = urllib.request.build_opener(_StripAuthOnRedirect)


def _open(url, token=None, accept=None):
    req = urllib.request.Request(url, headers={"User-Agent": "csweb-dl"})
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    if accept:
        req.add_header("Accept", accept)
    return _opener.open(req, timeout=60)


class Release:
    def __init__(self, repo, tag, token):
        self.repo, self.tag, self.token = repo, tag, token
        self._assets = None  # name -> asset id (API mode only)

    def _api_assets(self):
        if self._assets is None:
            url = f"{API}/repos/{self.repo}/releases/tags/{self.tag}"
            with _open(url, self.token, "application/vnd.github+json") as r:
                data = json.load(r)
            self._assets = {a["name"]: a["id"] for a in data.get("assets", [])}
        return self._assets

    def open_asset(self, name):
        """Return a file-like object. Raises FileNotFoundError if missing."""
        try:
            if self.token:
                ids = self._api_assets()
                if name not in ids:
                    raise FileNotFoundError(name)
                url = f"{API}/repos/{self.repo}/releases/assets/{ids[name]}"
                return _open(url, self.token, "application/octet-stream")
            url = f"https://github.com/{self.repo}/releases/download/{self.tag}/{name}"
            return _open(url)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                raise FileNotFoundError(name) from e
            raise


def expected_hash(release, asset):
    try:
        with release.open_asset("SHA256SUMS") as r:
            text = r.read().decode("utf-8", "replace")
    except FileNotFoundError:
        return None
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].lstrip("*") == asset:
            return parts[0].lower()
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--asset", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()

    token = (os.environ.get("CSWEB_GITHUB_TOKEN")
             or os.environ.get("GITHUB_TOKEN")
             or os.environ.get("GH_TOKEN"))

    os.makedirs(args.output, exist_ok=True)
    dest = os.path.join(args.output, args.asset)
    marker = dest + ".sha256"

    # Already downloaded and verified: no network at all
    if os.path.isfile(dest) and os.path.isfile(marker):
        with open(marker) as f:
            if f.read().strip() == sha256_file(dest):
                print(f"csweb: {args.asset} ({args.tag}) is up to date")
                return 0

    release = Release(args.repo, args.tag, token)
    try:
        want = expected_hash(release, args.asset)
        if want is None:
            print(f"csweb: warning: no checksum for {args.asset} in {args.tag}, "
                  f"download is not verified", file=sys.stderr)

        print(f"csweb: downloading {args.asset} from {args.repo} {args.tag}")
        tmp = dest + ".part"
        with release.open_asset(args.asset) as r, open(tmp, "wb") as f:
            while True:
                chunk = r.read(1 << 20)
                if not chunk:
                    break
                f.write(chunk)
    except FileNotFoundError as e:
        print(f"csweb: error: {e} not found in release {args.tag} of {args.repo} "
              f"(private repo? set CSWEB_GITHUB_TOKEN)", file=sys.stderr)
        return 1
    except (urllib.error.URLError, OSError) as e:
        print(f"csweb: error: download failed: {e}", file=sys.stderr)
        return 1

    got = sha256_file(tmp)
    if want is not None and got != want:
        os.remove(tmp)
        print(f"csweb: error: SHA256 mismatch for {args.asset}: "
              f"expected {want}, got {got}", file=sys.stderr)
        return 1

    os.replace(tmp, dest)
    with open(marker, "w") as f:
        f.write(got)
    print(f"csweb: saved {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())