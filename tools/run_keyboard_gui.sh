#!/usr/bin/env bash
# One-click launcher for hp48_keyboard_gui.py, modeled on soynut's own
# sim/run_with_gui.sh - see CLAUDE.md's "Interactive keyboard bridge"
# section for the full picture.
#
# Simpler than soynut's version: that script starts a host-native
# simulator process and auto-discovers its virtual serial port before
# launching the GUI. Cassini has no host-native simulator yet (sim/ is
# still empty) - the emulator runs on the real physical board, and
# hp48_keyboard_gui.py's own find_port() already auto-detects its USB
# serial port directly, so there's no process to start or port-file to
# wait on here. Any arguments (e.g. --port /dev/cu.usbmodemXXXX, if
# auto-detection ever needs overriding) are passed straight through.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 not found on PATH" >&2
    exit 1
fi

exec python3 hp48_keyboard_gui.py "$@"
