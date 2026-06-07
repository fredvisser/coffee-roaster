#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_DIR="$ROOT_DIR/simulator"
BUILD_DIR="$ROOT_DIR/build/lvgl-sim"
BIN="$BUILD_DIR/roaster-lvgl-sim"
DEFAULT_SCREEN_DIR="$ROOT_DIR/build/simulator-screens"

SCREENS=(
  start
  edit-setpoint
  profile-list
  profile-graph
  roasting
  network
  cooling
  error
)

build_simulator() {
  cmake -S "$SIM_DIR" -B "$BUILD_DIR"
  cmake --build "$BUILD_DIR" --target roaster-lvgl-sim
}

render_bmp() {
  local screen="$1"
  local bmp_path="$2"
  "$BIN" --screen "$screen" --hidden --output-bmp "$bmp_path"
}

convert_bmp_to_png() {
  local bmp_path="$1"
  local png_path="$2"

  if command -v sips >/dev/null 2>&1; then
    if sips -s format png "$bmp_path" --out "$png_path" >/dev/null; then
      rm -f "$bmp_path"
      echo "$png_path"
      return 0
    fi

    echo "Warning: PNG conversion failed for $bmp_path; keeping BMP output" >&2
  fi

  echo "$bmp_path"
}

usage() {
  cat <<EOF
Usage:
  ./tools/lvgl-sim.sh build
  ./tools/lvgl-sim.sh run [screen]
  ./tools/lvgl-sim.sh screenshot <screen|all> [output-dir]

Screens:
  ${SCREENS[*]}
EOF
}

command_name="${1:-run}"
case "$command_name" in
  build)
    build_simulator
    ;;
  run)
    screen="${2:-start}"
    build_simulator
    "$BIN" --screen "$screen"
    ;;
  screenshot)
    screen="${2:-all}"
    output_dir="${3:-$DEFAULT_SCREEN_DIR}"
    mkdir -p "$output_dir"
    build_simulator

    if [[ "$screen" == "all" ]]; then
      for item in "${SCREENS[@]}"; do
        bmp_path="$output_dir/$item.bmp"
        final_path="$output_dir/$item.png"
        render_bmp "$item" "$bmp_path"
        saved_path="$(convert_bmp_to_png "$bmp_path" "$final_path")"
        echo "Saved $saved_path"
      done
    else
      bmp_path="$output_dir/$screen.bmp"
      final_path="$output_dir/$screen.png"
      render_bmp "$screen" "$bmp_path"
      saved_path="$(convert_bmp_to_png "$bmp_path" "$final_path")"
      echo "Saved $saved_path"
    fi
    ;;
  *)
    usage
    exit 1
    ;;
esac