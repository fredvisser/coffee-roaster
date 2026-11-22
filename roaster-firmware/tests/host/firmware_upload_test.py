"""Run with python3 tests/host/firmware_upload_test.py. No device is contacted."""

import os
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory

TOOLS = Path(__file__).resolve().parents[2] / "tools"

with TemporaryDirectory() as directory:
    root = Path(directory)
    (root / "tools").mkdir()
    for name in ("firmware.sh", "workflow-runner.sh", "tests.sh"):
        shutil.copy2(TOOLS / name, root / "tools" / name)
    (root / "roaster-firmware.ino").touch()
    suite_sketches = {
        "pid": "test_pid", "profiles": "test_profiles", "state": "test_state_machine",
        "safety": "test_safety", "unit": "unit_tests", "hardware": "hardware_validation",
        "step-response": "test_step_response",
    }
    for sketch in suite_sketches.values():
        path = root / "tests" / sketch / f"{sketch}.ino"
        path.parent.mkdir(parents=True)
        path.touch()
    (root / "tools/roaster-pair.py").write_text(
        "import os, sys\n"
        "assert sys.argv[1] == f\"http://{os.environ['TEST_HOST']}:80\"\n"
    )
    esp = root / "Library/Arduino15/packages/esp32"

    def stub(path, script):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/bash\nset -eu\n" + script)
        path.chmod(0o755)

    stub(root / "bin/arduino-cli", """
echo "compile $*" >> "$HOME/calls"
if [[ "${@: -1}" != */roaster-firmware.ino ]]; then exit 0; fi
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-path) dir=$2; shift ;;
    --fqbn) fqbn=$2; shift ;;
    --build-property)
      if [[ "$2" == compiler.cpp.* ]]; then props=${2%% -DVERSION=*}; fi
      shift ;;
  esac
  shift
done
mkdir -p "$dir"
printf '{"fqbn": "%s", "customBuildProperties": "%s -DVERSION=x"}\\n' "$fqbn" "$props" > "$dir/build.options.json"
printf '%s\\n' "$props" > "$dir/roaster-firmware.ino.bin"
echo boot > "$dir/roaster-firmware.ino.bootloader.bin"
echo partitions > "$dir/partitions.csv"
""")
    stub(root / "bin/curl", """
[[ "$1" == -q && "$2" == --config && "$3" == - ]]
IFS= read -r config
[[ "$config" == "header = \\"Authorization: Bearer $TEST_TOKEN\\"" ]]
url="${@: -1}"
[[ "$url" == "http://$TEST_HOST:80/ota/"* ]]
ipv4=0
for arg in "$@"; do
  if [[ "$arg" == -4 ]]; then ipv4=1; fi
done
if [[ "$TEST_HOST" == *.local && "$ipv4" == 0 ]]; then
  echo "Resolving timed out" >&2
  exit 28
fi
if [[ "$TEST_HOST" != *.local ]]; then [[ "$ipv4" == 0 ]]; fi
for arg in "$@"; do
  if [[ "$arg" == file=@* ]]; then
    path=${arg#file=@}
    printf 'ota %s\\n' "$(cat "${path%;*}")" >> "$HOME/calls"
  fi
done
echo OK
""")
    stub(root / "bin/security", """
[[ "$*" == "find-generic-password -s coffee-roaster -a http://$TEST_HOST:80 -w" ]]
[[ -n "$TEST_TOKEN" ]] || exit 1
echo "$TEST_TOKEN"
""")
    stub(esp / "tools/esptool_py/1/esptool", 'printf \'usb %s\\n\' "$(cat "${@: -1}")" >> "$HOME/calls"\n')
    (esp / "hardware/esp32/1/tools").mkdir(parents=True)
    (esp / "hardware/esp32/1/tools/gen_esp32part.py").write_text("pass\n")
    stub(root / "bin/sleep", ":\n")
    env = dict(os.environ, HOME=directory, PATH=f"{root}/bin:{os.environ['PATH']}",
               OTA_HOST="ignored.local", TEST_HOST="roaster-dev.local", OTA_SCHEME="http", OTA_PORT="80",
               OTA_USER="admin", OTA_PASS="unused-legacy-password", OTA_TOKEN="", TEST_TOKEN="",
               SERIAL_PORT="/dev/test", ROASTER_FORCE_REBUILD="0",
               ROASTER_DEFAULT_BOARD="jc4827w543c", ROASTER_BUILD_JOBS="1", BOARD_FQBN="")

    def run(*args, error=None, entrypoint="firmware.sh"):
        calls = root / "calls"
        calls.write_text("")
        result = subprocess.run([root / "tools" / entrypoint, *args], env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if error is None:
            assert result.returncode == 0, result.stdout
        else:
            assert result.returncode != 0 and error in result.stdout, result.stdout
        return calls.read_text().splitlines()

    for args in (("--host",), ("--host", ""), ("--host", "--board", "jc4827w543c")):
        assert not run("pair", *args, error="--host requires a hostname or IP address")
    assert not run("pair")
    assert not run("ota-upload", error="Run ./tools/firmware.sh pair --host roaster-dev.local")
    env["TEST_TOKEN"] = "a" * 64
    assert [line.split()[0] for line in run("build")] == ["compile"]
    upload, = run("upload")
    assert upload.startswith("usb ") and "BOARD_JC4827W543C" in upload
    upload, = run("ota-upload")
    assert upload.startswith("ota ") and "BOARD_JC4827W543C" in upload
    env["TEST_HOST"] = "roaster-test.local"
    assert not run("pair", "--host", env["TEST_HOST"])
    upload, = run("ota-upload", "--host", env["TEST_HOST"])
    assert upload.startswith("ota ") and "BOARD_JC4827W543C" in upload
    env["TEST_HOST"] = "127.0.0.1"
    assert not run("pair", "--host", env["TEST_HOST"])
    compile_call, upload = run("ota", "--board", "jc4827w543r", "--host", env["TEST_HOST"])
    assert compile_call.startswith("compile ") and "BOARD_JC4827W543R" in upload
    compile_call, upload = run("upload", "--rebuild", "--board", "jc4827w543r")
    assert "--clean" in compile_call.split() and upload.startswith("usb ")
    (root / "build/roaster-firmware.bootloader.bin").unlink()
    assert len(run("ota-upload", "--board", "jc4827w543r", "--host", env["TEST_HOST"])) == 1
    assert [line.split()[0] for line in run("upload", "--board", "jc4827w543r")] == ["compile", "usb"]
    env["OTA_TOKEN"] = "invalid"
    assert not run("ota-upload", error="Invalid OTA session token")

    for suite, sketch in suite_sketches.items():
        compile_call, = run("compile", suite, entrypoint="tests.sh")
        assert compile_call.split()[-1] == str(root / "tests" / sketch / f"{sketch}.ino")
    assert not run(entrypoint="workflow-runner.sh", error="Use ./tools/firmware.sh or ./tools/tests.sh.")
    assert not run("main", entrypoint="workflow-runner.sh",
                   error="Use ./tools/firmware.sh or ./tools/tests.sh.")
    assert not run("1", "compile", entrypoint="workflow-runner.sh", error="Unknown target: 1")
    assert not run("main", "all", entrypoint="workflow-runner.sh", error="Invalid action: all")
    assert not run("state_machine", "compile", entrypoint="workflow-runner.sh",
                   error="Unknown target: state_machine")

    stub(root / "tools/workflow-runner.sh", 'echo "dispatch $*" >> "$HOME/calls"\n')
    assert run("run") == ["dispatch main run"]
    assert run("run", "safety", entrypoint="tests.sh") == ["dispatch safety run"]

print("Firmware upload and named-target workflows passed")
