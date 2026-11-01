#!/usr/bin/env bash
# Registers this devcontainer as a self-hosted runner for THIS repository only.
#
# What registering means: the runner accepts workflow jobs from
# 1984ozone1984/esp-to-mx320-printserver and executes them here, and those jobs
# flash and drive the DUT on the testbench. Only tag-triggered release
# verification targets it (labels self-hosted,testbench). Keep "Require approval
# for all outside collaborators" enabled in Settings -> Actions.
#
# Run once, by the repository owner:  bash .devcontainer/runner/register.sh
set -euo pipefail
REPO=1984ozone1984/esp-to-mx320-printserver
touch /opt/actions-runner/.registration-granted
echo "Registration granted. start.sh now loops: fetch a token, take one job, exit."
bash "$(dirname "$0")/start.sh"
