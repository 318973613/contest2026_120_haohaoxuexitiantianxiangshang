#!/usr/bin/env bash
set -euo pipefail

workspace="$HOME/openvela"
build_dir="cmake_out/demo_goldfish-arm64-v8a-ap-ai-agent"
session="openvela-ai-agent"
lib_dir="$HOME/.local/libcxxabi/usr/lib/x86_64-linux-gnu"

last_terminal_line() {
  tmux capture-pane -p -t "$session" -S -200 2>/dev/null \
    | tr -d '\r' \
    | awk 'NF { line = $0 } END { print line }'
}

last_prompt_token() {
  tmux capture-pane -p -t "$session" -S -200 2>/dev/null \
    | tr -d '\r' \
    | grep -Eo 'goldfish-armv8a-ap>|vela>' \
    | tail -1
}

if tmux has-session -t "$session" 2>/dev/null; then
  echo "Reusing ai_agent simulator tmux session: $session"
else
  if pgrep -f 'qemu-system-aarch64-headless' >/dev/null 2>&1; then
    echo "Another ai_agent emulator is already running outside tmux."
    echo "Stop that test process before starting this managed session."
    exit 1
  fi

  if [[ ! -d "$workspace/$build_dir" ]]; then
    echo "Build output not found: $workspace/$build_dir" >&2
    exit 1
  fi

  tmux new-session -d -s "$session" \
    "cd '$workspace' && export LD_LIBRARY_PATH='$lib_dir'\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH} && exec ./emulator.sh '$build_dir/' -no-window"

  echo "Starting ai_agent simulator. This normally takes about two minutes..."
fi

ai_agent_sent=0
outer_prompt_probed=0
for attempt in $(seq 1 180); do
  if ! tmux has-session -t "$session" 2>/dev/null; then
    echo "Simulator exited before reaching the vela prompt." >&2
    exit 1
  fi

  prompt="$(last_prompt_token || true)"
  if [[ "$prompt" == "vela>" ]]; then
    echo "ai_agent simulator is ready: $session"
    exit 0
  fi

  line="$(last_terminal_line || true)"
  if [[ "$prompt" == "goldfish-armv8a-ap>" && "$line" =~ goldfish-armv8a-ap\>[[:space:]]*$ ]]; then
    if [[ "$ai_agent_sent" -eq 1 ]]; then
      echo "ai_agent returned to the outer NSH prompt before reaching vela>." >&2
      echo "Attach with: tmux attach -t $session" >&2
      exit 1
    fi

    echo "Outer NSH prompt detected; starting ai_agent..."
    tmux send-keys -t "$session" -l -- "ai_agent"
    tmux send-keys -t "$session" Enter
    ai_agent_sent=1
  elif [[ "$prompt" == "goldfish-armv8a-ap>" && "$ai_agent_sent" -eq 0 && "$outer_prompt_probed" -eq 0 ]]; then
    # Boot-time emulator logs can be appended to the NSH prompt line. One empty
    # command refreshes the prompt so the next iteration can verify it is idle.
    tmux send-keys -t "$session" Enter
    outer_prompt_probed=1
  fi

  if [[ "$ai_agent_sent" -eq 0 && "$attempt" -eq 30 ]]; then
    echo "Still waiting for the simulator prompt..."
  elif [[ "$ai_agent_sent" -eq 1 && "$attempt" -eq 120 ]]; then
    echo "Still waiting for ai_agent to reach vela>..."
  fi

  sleep 1
done

echo "Timed out waiting for the vela prompt." >&2
exit 1
