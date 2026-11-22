#!/usr/bin/env python3
"""Read roaster settings/logs and write settings using an existing paired session."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import urlencode, urlsplit


def request(base, token, path, method="GET", body=None):
    config = f'header = "Authorization: Bearer {token}"\n'
    if body is not None:
        config += 'header = "Content-Type: application/json"\n'
        config += f"data-binary = {json.dumps(body)}\n"
    command = ["curl", "-q", "--config", "-", "--noproxy", "*", "--silent",
               "--show-error", "--connect-timeout", "5", "--max-time", "30",
               "--request", method, "--write-out", "\n%{http_code}"]
    if urlsplit(base).hostname.lower().rstrip(".").endswith(".local"):
        command.append("-4")
    result = subprocess.run(command + [base + path], input=config, text=True, capture_output=True)
    if result.returncode:
        raise ValueError(f"Request failed: {result.stderr.strip()}")
    text, status = result.stdout.rsplit("\n", 1)
    if not 200 <= int(status) < 300:
        if status == "401":
            raise ValueError("Pairing required or session expired; run firmware.sh pair with the same --host.")
        detail = text.replace(token, "<REDACTED>")
        api_key = json.loads(body).get("apiKey") if body is not None else None
        if isinstance(api_key, str) and api_key:
            detail = detail.replace(json.dumps(api_key)[1:-1], "<REDACTED>").replace(api_key, "<REDACTED>")
        raise ValueError(f"HTTP {status}: {detail[:512]}")
    return json.loads(text)


def card_logs(fetch, selected_file):
    status = fetch("/api/logs/card/status")
    if status.get("busy") or not status.get("available"):
        raise ValueError(f"Card logs unavailable: {status.get('error', 'card busy or not mounted')}")
    files = [item["id"] for item in status["files"]]
    if selected_file is not None:
        if selected_file not in files:
            raise ValueError(f"Card log file {selected_file} not found")
        files = [selected_file]
    entries = []
    for file_id in sorted(files, reverse=True):
        pages = []
        cursor = None
        file_size = None
        while True:
            params = {"file": file_id, "limit": 100}
            if cursor is not None:
                params["cursor"] = cursor
            page = fetch("/api/logs/card?" + urlencode(params))
            if page.get("busy") or not page.get("available") or not page.get("file_found"):
                raise ValueError(f"Card log file {file_id} unavailable during download")
            if file_size is not None and page["file_size"] < file_size:
                raise ValueError("Card logs rotated during download; retry")
            file_size = page["file_size"]
            pages.append(page["logs"])
            if not page["has_more"]:
                break
            next_cursor = page["next_cursor"]
            if not 0 < next_cursor < (cursor if cursor is not None else file_size):
                raise ValueError("Card log pagination did not advance")
            cursor = next_cursor
        entries.extend(entry for page_entries in reversed(pages) for entry in page_entries)
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Roaster hostname or IPv4 address")
    commands = parser.add_subparsers(dest="command", required=True)
    get = commands.add_parser("get", help="Read settings or current state as JSON")
    get.add_argument("resource", choices=("state", "pid", "server", "systemlink", "profiles", "profile"))
    get.add_argument("--id", help="Profile ID (required for get profile)")
    put = commands.add_parser("set", help="Write settings from a JSON object")
    put.add_argument("resource", choices=("pid", "server", "systemlink", "profile"))
    put.add_argument("--file", required=True, help="JSON file, or - for stdin")
    put.add_argument("--id", help="Profile ID to update; omit to create a profile")
    logs = commands.add_parser("logs", help="Print log entries as JSONL")
    logs.add_argument("--card", action="store_true", help="Download all retained card logs instead of recent RAM logs")
    logs.add_argument("--file", type=int, choices=range(5), help="Download one card log file (requires --card)")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.-]*", args.host):
        parser.error("--host must be a hostname or IPv4 address, not a URL")
    profile_id = getattr(args, "id", None)
    if profile_id is not None and not re.fullmatch(r"[A-Za-z0-9_-]+", profile_id):
        parser.error("Invalid profile ID")
    if args.command != "logs" and profile_id is not None and args.resource != "profile":
        parser.error("--id is only supported for profile")
    if args.command == "get" and args.resource == "profile" and not profile_id:
        parser.error("get profile requires --id")
    if args.command == "logs" and args.file is not None and not args.card:
        parser.error("--file requires --card")

    body = None
    if args.command == "set":
        text = sys.stdin.read() if args.file == "-" else Path(args.file).read_text()
        settings = json.loads(text)
        if not isinstance(settings, dict) or not settings:
            raise ValueError("Settings must be a nonempty JSON object")
        fields = {
            "pid": {"kp", "ki", "kd", "rampScale"},
            "server": {"hostname"},
            "systemlink": {"enabled", "apiUrl", "workspaceId", "systemId", "apiKey", "clearApiKey"},
            "profile": {"id", "name", "active", "setpoints"},
        }
        unknown = settings.keys() - fields[args.resource]
        if unknown:
            raise ValueError("Unknown settings: " + ", ".join(sorted(unknown)))
        if args.resource == "pid" and any(type(value) not in (int, float) for value in settings.values()):
            raise ValueError("PID settings must be numbers")
        if args.resource in ("server", "systemlink"):
            for key, value in settings.items():
                expected = bool if key in ("enabled", "clearApiKey") else str
                if type(value) is not expected:
                    raise ValueError(f"{key} must be a {expected.__name__}")
        if args.resource == "profile":
            for key in ("id", "active"):
                if key in settings:
                    print(f"Ignoring read-only profile metadata: {key}", file=sys.stderr)
                    del settings[key]
            points = settings.get("setpoints")
            if not isinstance(points, list) or not 1 <= len(points) <= 9:
                raise ValueError("Profiles require 1-9 setpoints (the firmware reserves one slot)")
            for point in points:
                bounds = {"time": 4294967, "temp": 500, "fanSpeed": 100}
                if not isinstance(point, dict) or any(
                    type(point.get(key)) is not int or not 0 <= point[key] <= maximum
                    for key, maximum in bounds.items()
                ):
                    raise ValueError("Setpoints require integer time (seconds), temp (0-500F), and fanSpeed (0-100)")
            if "name" in settings and not isinstance(settings["name"], str):
                raise ValueError("Profile name must be a string")
        body = json.dumps(settings, separators=(",", ":"), allow_nan=False)
        maximum = {"pid": 256, "server": 512, "systemlink": 1024, "profile": 4096}[args.resource]
        if len(body.encode()) > maximum:
            raise ValueError(f"Settings exceed the {maximum}-byte payload limit")

    base = f"http://{args.host}:80"
    token = os.environ.get("OTA_TOKEN", "")
    if not token:
        saved = subprocess.run(
            ["security", "find-generic-password", "-s", "coffee-roaster", "-a", base, "-w"],
            text=True, capture_output=True,
        )
        if saved.returncode:
            raise ValueError(f"No paired session; run ./tools/firmware.sh pair --host {args.host}")
        token = saved.stdout.strip()
    if not re.fullmatch(r"[a-f0-9]{64}", token):
        raise ValueError("Invalid paired session token")

    def fetch(path):
        return request(base, token, path)

    if args.command == "logs":
        entries = card_logs(fetch, args.file) if args.card else fetch("/api/logs?max=100")["logs"]
        for entry in entries:
            print(json.dumps(entry))
    else:
        path = "/api/" + args.resource
        method = "GET" if args.command == "get" else "POST"
        if args.resource == "profile":
            if profile_id:
                path += "/" + profile_id
                if args.command == "set":
                    method = "PUT"
            else:
                path = "/api/profiles"
        print(json.dumps(request(base, token, path, method, body), indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        sys.exit(f"Roaster request failed: {error}")
