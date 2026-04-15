#!/usr/bin/env bash
#
# Build and flash all connected ESP32 boards.
# Usage: ./deploy.sh [--monitor]
#   --monitor   Open serial monitor on the last flashed board after deploy
#
set -euo pipefail

MONITOR=false
[[ "${1:-}" == "--monitor" ]] && MONITOR=true

# Ensure PlatformIO CLI is available
if ! command -v pio &>/dev/null; then
    echo "PlatformIO CLI not found. Install it with:"
    echo "  brew install platformio"
    echo "  # or: pip install platformio"
    exit 1
fi

# Build once
echo "=== Building firmware ==="
pio run -e esp32

# Find all connected ESP32 serial ports (macOS: /dev/cu.usbserial-* or /dev/cu.wchusbserial-*)
PORTS=()
while IFS= read -r port; do
    [[ -n "$port" ]] && PORTS+=("$port")
done < <(ls /dev/cu.usbserial-* /dev/cu.wchusbserial-* /dev/cu.SLAB_USBtoUART* 2>/dev/null || true)

if [[ ${#PORTS[@]} -eq 0 ]]; then
    echo "No ESP32 serial ports detected."
    echo "Make sure your boards are connected and drivers are installed:"
    echo "  - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    echo "  - CH340:  https://github.com/WCHSoftware/ch34xser_macos"
    exit 1
fi

echo "=== Found ${#PORTS[@]} board(s) ==="
printf "  %s\n" "${PORTS[@]}"
echo

# Flash each board
LAST_PORT=""
for port in "${PORTS[@]}"; do
    echo "--- Flashing $port ---"
    pio run -e esp32 -t upload --upload-port "$port"
    LAST_PORT="$port"
    echo
done

echo "=== Done. Flashed ${#PORTS[@]} board(s) ==="

if $MONITOR && [[ -n "$LAST_PORT" ]]; then
    echo "=== Opening monitor on $LAST_PORT ==="
    pio device monitor -p "$LAST_PORT" -b 115200
fi
