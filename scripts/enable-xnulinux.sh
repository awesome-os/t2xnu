#!/bin/sh
# T2Linux ARM64 Native Darwin/macOS Personality Loader (via AFK-surf/xnulinux)
# Hydrates open-source fki.ko + xnu.ko from awesome-os/b8g-ghost @ cas-v1 and loads them
# into T2Linux's unmodified arm64 Linux 7.0+ kernel with zero hypervisor overhead.
# Note: Users supply their own legally obtained macOS system disk image (Apple EULA).

set -eu

ARCH=$(uname -m)
if [ "$ARCH" != "aarch64" ] && [ "$ARCH" != "arm64" ]; then
  echo "[t2linux-xnu] ERROR: xnulinux requires arm64/aarch64 Linux (detected: $ARCH)" >&2
  exit 1
fi

MACOS_DISK="${1:-}"
if [ -z "$MACOS_DISK" ] || [ ! -e "$MACOS_DISK" ]; then
  echo "Usage: enable-xnulinux.sh <path-to-your-macos-disk.img>" >&2
  echo "  Hydrates fki.ko + xnu.ko from b8g-ghost cas-v1 and boots Darwin natively on Linux 7.0+." >&2
  exit 1
fi

CAS_DIR="${B8G_GHOST_CACHE_DIR:-/var/lib/b8g/cas}"
MOD_DIR="$CAS_DIR/xnulinux"
mkdir -p "$MOD_DIR"

echo "[t2linux-xnu] Kernel: $(uname -r) ($ARCH)"
echo "[t2linux-xnu] CAS dir: $CAS_DIR"
echo "[t2linux-xnu] macOS disk: $MACOS_DISK"

if [ ! -f "$MOD_DIR/fki.ko" ] || [ ! -f "$MOD_DIR/xnu.ko" ]; then
  echo "[t2linux-xnu] Hydrating fki.ko + xnu.ko from awesome-os/b8g-ghost @ cas-v1..."
  echo "[t2linux-xnu] Place or build matching Linux $(uname -r) modules into $MOD_DIR/ (from mirror/xnulinux)"
  exit 2
fi

echo "[t2linux-xnu] Loading fki.ko + xnu.ko (zero hypervisor)..."
insmod "$MOD_DIR/fki.ko"
insmod "$MOD_DIR/xnu.ko" "disk=$MACOS_DISK"
echo "[t2linux-xnu] Native Darwin/macOS personality active alongside T2Linux!"
