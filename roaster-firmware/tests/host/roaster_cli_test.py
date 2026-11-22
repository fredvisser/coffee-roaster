"""Run with python3 tests/host/roaster_cli_test.py. Only a local test server is contacted."""

from contextlib import redirect_stdout, redirect_stderr
from http.server import BaseHTTPRequestHandler, HTTPServer
import importlib.util
from io import StringIO
import json
import os
from pathlib import Path
import sys
import threading
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("roaster", Path(__file__).resolve().parents[2] / "tools/roaster.py")
roaster = importlib.util.module_from_spec(spec)
spec.loader.exec_module(roaster)
token = "a" * 64
received = []


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        received.append((self.path, self.headers["Authorization"]))
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/unexpected")
            self.end_headers()
            self.wfile.write(b'{"error":"redirected"}')
        else:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b'{"kp":8.0}')

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        assert self.headers["Content-Type"] == "application/json"
        received.append((self.path, self.headers["Authorization"], json.loads(body)))
        self.send_response(409 if self.path == "/reject" else 200)
        self.end_headers()
        self.wfile.write(body if self.path == "/reject" else b'{"ok":true}')

    def log_message(self, *args):
        pass


server = HTTPServer(("127.0.0.1", 0), Handler)
thread = threading.Thread(target=server.serve_forever)
thread.start()
try:
    base = f"http://127.0.0.1:{server.server_port}"
    assert roaster.request(base, token, "/api/pid") == {"kp": 8.0}
    settings = {"apiKey": 'test-secret"\\value', "enabled": True}
    with patch.object(roaster.subprocess, "run", wraps=roaster.subprocess.run) as run:
        assert roaster.request(base, token, "/api/systemlink", "POST", json.dumps(settings)) == {"ok": True}
        command = " ".join(run.call_args.args[0])
        assert token not in command and "test-secret" not in command
    assert received[-1] == ("/api/systemlink", f"Bearer {token}", settings)
    try:
        roaster.request(base, token, "/reject", "POST", json.dumps(settings))
        raise AssertionError("HTTP errors must fail")
    except ValueError as error:
        assert "409" in str(error) and "test-secret" not in str(error), str(error)
    try:
        roaster.request(base, token, "/redirect")
        raise AssertionError("Redirects must not be followed")
    except ValueError as error:
        assert "302" in str(error)
    assert not any(item[0] == "/unexpected" for item in received)
finally:
    server.shutdown()
    server.server_close()
    thread.join()

with patch.object(roaster.subprocess, "run", return_value=roaster.subprocess.CompletedProcess(
    [], 0, stdout="{}\n200", stderr="",
)) as run:
    roaster.request("http://roaster-test.LOCAL.:80", token, "/api/state")
    assert "-4" in run.call_args.args[0]


calls = []
responses = {
    "/api/state": {"state": 0},
    "/api/pid": {"kp": 8.0},
    "/api/server": {"hostname": "roaster-test"},
    "/api/systemlink": {"enabled": False},
    "/api/profiles": {"profiles": []},
    "/api/profile/test-id": {"name": "Test"},
    "/api/logs?max=100": {"logs": [{"message": "recent"}]},
    "/api/logs/card/status": {"available": True, "files": [{"id": 0}, {"id": 1}]},
    "/api/logs/card?file=1&limit=100": {
        "available": True, "file_found": True, "file_size": 200,
        "logs": [{"message": "older"}], "has_more": False, "next_cursor": 0,
    },
    "/api/logs/card?file=0&limit=100": {
        "available": True, "file_found": True, "file_size": 200,
        "logs": [{"message": "newest"}], "has_more": True, "next_cursor": 100,
    },
    "/api/logs/card?file=0&limit=100&cursor=100": {
        "available": True, "file_found": True, "file_size": 200,
        "logs": [{"message": "middle"}], "has_more": False, "next_cursor": 0,
    },
}


def fake_request(base, secret, path, method="GET", body=None):
    assert base == "http://roaster-test.local:80" and secret == token
    calls.append((path, method, json.loads(body) if body else None))
    return responses[path] if method == "GET" else {"ok": True}


def invoke(*args, stdin=""):
    output = StringIO()
    with patch.dict(os.environ, {"OTA_TOKEN": token}), \
            patch.object(roaster, "request", fake_request), \
            patch("sys.argv", ["roaster.py", "--host", "roaster-test.local", *args]), \
            patch("sys.stdin", StringIO(stdin)), redirect_stdout(output), redirect_stderr(StringIO()):
        roaster.main()
    return output.getvalue()


for resource in ("state", "pid", "server", "systemlink", "profiles"):
    json.loads(invoke("get", resource))
    assert calls[-1][:2] == ("/api/" + resource, "GET")
invoke("get", "profile", "--id", "test-id")
assert calls[-1][:2] == ("/api/profile/test-id", "GET")
for resource, settings in (("pid", {"kp": 8}), ("server", {"hostname": "test"}),
                           ("systemlink", {"enabled": False})):
    invoke("set", resource, "--file", "-", stdin=json.dumps(settings))
    assert calls[-1] == ("/api/" + resource, "POST", settings)
profile = {"name": "Test", "setpoints": [{"time": 0, "temp": 180, "fanSpeed": 100}]}
invoke("set", "profile", "--id", "test-id", "--file", "-", stdin=json.dumps(profile))
assert calls[-1] == ("/api/profile/test-id", "PUT", profile)
invoke("set", "profile", "--file", "-", stdin=json.dumps(profile))
assert calls[-1] == ("/api/profiles", "POST", profile)
maximum_profile = dict(profile, setpoints=profile["setpoints"] * 9)
invoke("set", "profile", "--file", "-", stdin=json.dumps(maximum_profile))
assert calls[-1][2] == maximum_profile
assert json.loads(invoke("logs"))["message"] == "recent"
assert [json.loads(line)["message"] for line in invoke("logs", "--card").splitlines()] == [
    "older", "middle", "newest",
]
for resource, settings in (("pid", {"kp": True}), ("systemlink", {"enabled": "yes"}),
                           ("pid", {"kp": float("nan")}), ("pid", {"typo": 3}),
                           ("profile", {"setpoints": profile["setpoints"] * 10})):
    count = len(calls)
    try:
        invoke("set", resource, "--file", "-", stdin=json.dumps(settings))
        raise AssertionError("Invalid settings must fail before a request")
    except ValueError:
        assert len(calls) == count
bad_page = dict(responses["/api/logs/card?file=0&limit=100"], next_cursor=200)
with patch.dict(responses, {"/api/logs/card?file=0&limit=100": bad_page}):
    try:
        invoke("logs", "--card")
        raise AssertionError("Non-advancing pagination must fail")
    except ValueError as error:
        assert "pagination" in str(error)

print("Roaster settings, authenticated HTTP, errors and paginated log checks passed")
