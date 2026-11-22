#!/usr/bin/env python3
"""Pair a CLI session and store its token in the macOS Keychain."""

import json
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def main(base):
    if sys.platform != "darwin":
        raise ValueError("firmware.sh pair requires macOS Keychain")
    if not re.fullmatch(r"https?://[A-Za-z0-9.-]+:[0-9]+", base):
        raise ValueError("Invalid OTA host or port")
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

    def post(path, headers=None):
        request = urllib.request.Request(base + path, headers=headers or {}, method="POST")
        try:
            with opener.open(request, timeout=6) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            raise ValueError(error.read(512).decode("utf-8", errors="replace")) from None

    offer = post("/api/pair-request?cli=1")
    if not re.fullmatch(r"[a-f0-9]{64}", offer["ticket"]) or not re.fullmatch(r"[0-9]{4}", offer["code"]):
        raise ValueError("Invalid pairing response")
    print(f"Check that the roaster shows {offer['code']}, then tap Approve.", flush=True)
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        time.sleep(1)
        result = post("/api/pair-claim", {
            "X-Roaster-Pair": offer["ticket"],
            "X-Roaster-Paired-At": str(int(time.time())),
        })
        if result.get("status") == "pending":
            continue
        token = result.get("token", "")
        if not re.fullmatch(r"[a-f0-9]{64}", token):
            raise ValueError("Invalid session token")
        command = f'add-generic-password -U -s coffee-roaster -a "{base}" -w "{token}"\n'
        if subprocess.run(["security", "-i"], input=command, text=True, capture_output=True).returncode:
            raise ValueError("Could not save paired session in Keychain")
        saved = subprocess.run(["security", "find-generic-password", "-s", "coffee-roaster", "-a", base, "-w"],
                               text=True, capture_output=True, check=True).stdout.strip()
        if saved != token:
            raise ValueError("Could not verify Keychain session; revoke it on the Server page and retry")
        print("Paired session saved in Keychain.")
        return
    raise ValueError("Pairing timed out")


if __name__ == "__main__":
    try:
        main(sys.argv[1])
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(f"Pairing failed: {error}")
