#!/usr/bin/env bash
# Ephemeral runner loop: each registration takes exactly one job, then a fresh
# one is made. Does nothing until the owner has run register.sh.
# Needs GH_RUNNER_PAT in the container environment: a fine-grained token for
# this repository with Administration: read/write. Kept in the owner's host
# environment and passed through, never committed.
set -uo pipefail
REPO=1984ozone1984/esp-to-mx320-printserver
cd /opt/actions-runner || exit 0
[ -f .registration-granted ] || { echo "runner: not registered (run .devcontainer/runner/register.sh)"; exit 0; }
pgrep -f 'runner-loop' >/dev/null && exit 0
nohup bash -c '
  exec -a runner-loop bash -c "
    while true; do
      TOKEN=\$(curl -sf -X POST -H \"Authorization: Bearer \$GH_RUNNER_PAT\" https://api.github.com/repos/'"$REPO"'/actions/runners/registration-token | jq -r .token) || { sleep 60; continue; }
      ./config.sh --unattended --ephemeral --replace --url https://github.com/'"$REPO"' \
        --token \"\$TOKEN\" --name \"devcontainer-\$(hostname)\" --labels self-hosted,testbench
      ./run.sh
      sleep 5
    done"' > /opt/actions-runner/loop.log 2>&1 &
echo "runner: loop started (log /opt/actions-runner/loop.log)"
