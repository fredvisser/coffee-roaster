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
    for name in ("firmware.sh", "roaster-cli.sh"):
        shutil.copy2(TOOLS / name, root / "tools" / name)
    (root / "roaster-firmware.ino").touch()
    esp = root / "Library/Arduino15/packages/esp32"

    def stub(path, script):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("#!/bin/bash\nset -eu\n" + script)
        path.chmod(0o755)

    stub(root / "bin/arduino-cli", """
echo "compile $*" >> "$HOME/calls"
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
[[ "$url" == "http://$OTA_HOST:80/ota/"* ]]
ipv4=0
for arg in "$@"; do
  if [[ "$arg" == -4 ]]; then ipv4=1; fi
done
if [[ "$OTA_HOST" == *.local && "$ipv4" == 0 ]]; then
  echo "Resolving timed out" >&2
  exit 28
fi
if [[ "$OTA_HOST" != *.local ]]; then [[ "$ipv4" == 0 ]]; fi
for arg in "$@"; do
  if [[ "$arg" == file=@* ]]; then
    path=${arg#file=@}
    printf 'ota %s\\n' "$(cat "${path%;*}")" >> "$HOME/calls"
  fi
done
echo OK
""")
    stub(root / "bin/security", """
[[ "$*" == "find-generic-password -s coffee-roaster -a http://$OTA_HOST:80 -w" ]]
[[ -n "$TEST_TOKEN" ]] || exit 1
echo "$TEST_TOKEN"
""")
    stub(esp / "tools/esptool_py/1/esptool", 'printf \'usb %s\\n\' "$(cat "${@: -1}")" >> "$HOME/calls"\n')
    (esp / "hardware/esp32/1/tools").mkdir(parents=True)
    (esp / "hardware/esp32/1/tools/gen_esp32part.py").write_text("pass\n")
    stub(root / "bin/sleep", ":\n")
    env = dict(os.environ, HOME=directory, PATH=f"{root}/bin:{os.environ['PATH']}",
               OTA_HOST="127.0.0.1", OTA_SCHEME="http", OTA_PORT="80",
               OTA_USER="admin", OTA_PASS="unused-legacy-password", OTA_TOKEN="", TEST_TOKEN="",
               SERIAL_PORT="/dev/test", ROASTER_FORCE_REBUILD="0",
               ROASTER_DEFAULT_BOARD="jc4827w543c", ROASTER_BUILD_JOBS="1", BOARD_FQBN="")

    def run(*args, error=None):
        calls = root / "calls"
        calls.write_text("")
        result = subprocess.run([root / "tools/firmware.sh", *args], env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if error is None:
            assert result.returncode == 0, result.stdout
        else:
            assert result.returncode != 0 and error in result.stdout, result.stdout
        return calls.read_text().splitlines()

    assert not run("ota-upload", error="Run ./tools/firmware.sh pair")
    env["TEST_TOKEN"] = "a" * 64
    assert [line.split()[0] for line in run("build")] == ["compile"]
    upload, = run("upload")
    assert upload.startswith("usb ") and "BOARD_JC4827W543C" in upload
    upload, = run("ota-upload")
    assert upload.startswith("ota ") and "BOARD_JC4827W543C" in upload
    env["OTA_HOST"] = "roaster-test.local"
    upload, = run("ota-upload")
    assert upload.startswith("ota ") and "BOARD_JC4827W543C" in upload
    env["OTA_HOST"] = "127.0.0.1"
    compile_call, upload = run("ota", "--board", "jc4827w543r")
    assert compile_call.startswith("compile ") and "BOARD_JC4827W543R" in upload
    compile_call, upload = run("upload", "--rebuild", "--board", "jc4827w543r")
    assert "--clean" in compile_call.split() and upload.startswith("usb ")
    (root / "build/roaster-firmware.bootloader.bin").unlink()
    assert len(run("ota-upload", "--board", "jc4827w543r")) == 1
    assert [line.split()[0] for line in run("upload", "--board", "jc4827w543r")] == ["compile", "usb"]
    env["OTA_TOKEN"] = "invalid"
    assert not run("ota-upload", error="Invalid OTA session token")

print("Firmware upload workflow passed")
