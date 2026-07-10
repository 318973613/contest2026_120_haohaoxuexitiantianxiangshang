#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
session="openvela-ai-agent"
base_url="https://token-plan-cn.xiaomimimo.com/v1"
host="token-plan-cn.xiaomimimo.com"
model="mimo-v2.5-pro"
buffer_name="mimo-key-$$"
api_key=""
masked_key=""
buffer_loaded=0
sensitive_sent=0

capture_pane() {
  tmux capture-pane -p -t "$session" -S -160 2>/dev/null | tr -d '\r'
}

clear_sensitive_terminal() {
  if ! tmux has-session -t "$session" 2>/dev/null; then
    return 0
  fi

  tmux send-keys -t "$session" C-u 2>/dev/null || true
  pane_height="$(tmux display-message -p -t "$session" '#{pane_height}' 2>/dev/null || true)"
  if [[ ! "$pane_height" =~ ^[0-9]+$ ]]; then
    pane_height=24
  fi

  for ((line = 0; line < pane_height + 8; line++)); do
    tmux send-keys -t "$session" Enter 2>/dev/null || break
  done
  sleep 1
  tmux clear-history -t "$session" 2>/dev/null || true
}

cleanup() {
  status=$?
  trap - EXIT HUP INT TERM
  set +e
  if [[ -t 0 ]]; then
    stty echo 2>/dev/null
  fi
  if [[ "$buffer_loaded" -eq 1 ]]; then
    tmux delete-buffer -b "$buffer_name" 2>/dev/null
  fi
  if [[ "$sensitive_sent" -eq 1 ]]; then
    clear_sensitive_terminal
  fi
  unset api_key masked_key after cleared config_output router_output
  exit "$status"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 129' HUP
trap 'exit 143' TERM

"$script_dir/start-ai-simulator.sh"
clear_sensitive_terminal

printf 'Enter a NEW Xiaomi MiMo API key (input is hidden): '
if ! IFS= read -r -s api_key; then
  printf '\n'
  echo "Key input was interrupted; configuration cancelled." >&2
  exit 1
fi
printf '\n'

if [[ -z "$api_key" ]]; then
  echo "No key entered; configuration cancelled." >&2
  exit 1
fi

if [[ "$api_key" =~ [[:space:]] ]]; then
  unset api_key
  echo "The key must not contain spaces; configuration cancelled." >&2
  exit 1
fi

if [[ "${#api_key}" -le 6 ]]; then
  unset api_key
  echo "The key is too short to verify safely as masked output." >&2
  exit 1
fi

masked_key="${api_key:0:4}****"
tmux send-keys -t "$session" -l -- "set_llm $base_url $model "
printf '%s' "$api_key" | tmux load-buffer -b "$buffer_name" -
buffer_loaded=1
sensitive_sent=1
tmux paste-buffer -d -b "$buffer_name" -t "$session"
buffer_loaded=0
tmux send-keys -t "$session" Enter

configured=0
for _ in $(seq 1 30); do
  after="$(capture_pane)"
  if [[ "$after" == *"API key saved."* ]]; then
    configured=1
    break
  fi
  sleep 1
done

clear_sensitive_terminal
cleared="$(capture_pane)"
if [[ "$cleared" == *"$api_key"* ]]; then
  clear_sensitive_terminal
  cleared="$(capture_pane)"
  if [[ "$cleared" == *"$api_key"* ]]; then
    echo "The terminal still contains the unmasked key after security cleanup." >&2
    exit 1
  fi
fi
unset after cleared

if [[ "$configured" -ne 1 ]]; then
  echo "The simulator did not confirm that the key was saved." >&2
  echo "Attach with: tmux attach -t $session" >&2
  exit 1
fi

tmux send-keys -t "$session" -l -- "config_show"
tmux send-keys -t "$session" Enter
config_verified=0
for _ in $(seq 1 15); do
  config_output="$(capture_pane)"
  if [[ "$config_output" == *"API Key"*"$masked_key"* \
    && "$config_output" == *"$host"* \
    && "$config_output" == *"$model"* ]]; then
    config_verified=1
    break
  fi
  sleep 1
done

if [[ "$config_output" == *"$api_key"* ]]; then
  echo "config_show exposed the unmasked key; the terminal will be cleared." >&2
  exit 1
fi

if [[ "$config_verified" -ne 1 ]]; then
  echo "config_show did not return the expected masked MiMo configuration." >&2
  exit 1
fi

tmux send-keys -t "$session" -l -- "router_status"
tmux send-keys -t "$session" Enter
router_verified=0
for _ in $(seq 1 15); do
  router_output="$(capture_pane)"
  if [[ "$router_output" == *"=== LLM Router Status ==="* \
    && "$router_output" == *"$host"* \
    && "$router_output" == *"$model"* ]]; then
    router_verified=1
    break
  fi
  sleep 1
done

if [[ "$router_output" == *"$api_key"* ]]; then
  echo "router_status exposed the unmasked key; the terminal will be cleared." >&2
  exit 1
fi

if [[ "$router_verified" -ne 1 ]]; then
  echo "router_status did not report the expected MiMo backend." >&2
  exit 1
fi

unset api_key config_output router_output
sensitive_sent=0

echo "MiMo configuration saved successfully."
echo "Base URL: $base_url"
echo "Model: $model"
echo "config_show verification: API key is masked."
echo "router_status verification: MiMo backend is active."
echo "The API key was not written to the repository, shell history, or a console log file."
