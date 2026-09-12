#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/../.." && pwd)"
serial="${ADB_SERIAL:-emulator-5554}"

skills=(
  study-assistant.md
  guided-study.md
  device-diagnosis.md
  daily-study-review.md
)

adb -s "$serial" wait-for-device

for skill in "${skills[@]}"; do
  source_file="$repo_dir/src/skills/$skill"
  if [[ ! -f "$source_file" ]]; then
    echo "Skill not found: $source_file" >&2
    exit 1
  fi
  adb -s "$serial" push "$source_file" "/data/ai_agent/skills/$skill"
done

echo "Study companion skills installed on $serial. Restart ai_agent to load them."
