#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/../.." && pwd)"
source_file="$repo_dir/src/config/HEARTBEAT.md"
serial="${ADB_SERIAL:-emulator-5554}"

if [[ ! -f "$source_file" ]]; then
  echo "Heartbeat template not found: $source_file" >&2
  exit 1
fi

adb -s "$serial" wait-for-device
adb -s "$serial" shell "mkdir -p /data/ai_agent/config"
adb -s "$serial" push "$source_file" /data/ai_agent/HEARTBEAT.md
adb -s "$serial" push "$source_file" /data/ai_agent/config/HEARTBEAT.md
echo "Study heartbeat installed on $serial."
