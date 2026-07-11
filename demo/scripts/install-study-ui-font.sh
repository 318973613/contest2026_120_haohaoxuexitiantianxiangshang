#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/../.." && pwd)"
font="$repo_dir/assets/fonts/NotoSansSC-Regular.ttf"
serial="${ADB_SERIAL:-emulator-5554}"

if [[ ! -f "$font" ]]; then
  echo "Study UI font not found: $font" >&2
  exit 1
fi

if ! command -v adb >/dev/null 2>&1; then
  echo "adb is required to prepare the QEMU data partition." >&2
  exit 1
fi

adb -s "$serial" wait-for-device
adb -s "$serial" push "$font" /data/NotoSansSC-Regular.ttf
echo "Study UI font installed on $serial."
