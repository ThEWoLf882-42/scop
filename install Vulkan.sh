#!/usr/bin/env bash
set -euo pipefail

VER="1.3.296.0"
GO="/goinfre/$USER"
ZIP="$GO/vulkansdk-macos-${VER}.zip"
EXTRACT="$GO/vulkansdk-macos-${VER}-extract"
ROOT="$GO/VulkanSDK/$VER"

# 1) sanity check
if [ ! -f "$ZIP" ]; then
  echo "ZIP not found: $ZIP"
  echo "Available zips:"
  ls -lh "$GO"/*.zip 2>/dev/null || true
  exit 1
fi

# 2) fresh extract
rm -rf "$EXTRACT"
mkdir -p "$EXTRACT" "$ROOT"
unzip -q -o "$ZIP" -d "$EXTRACT"

# 3) remove quarantine
xattr -dr com.apple.quarantine "$EXTRACT" || true

# 4) locate installer executable (robust)
echo "Looking for installer binary..."
APP_BIN="$(find "$EXTRACT" -type f -path "*/Contents/MacOS/InstallVulkan*" | head -n1 || true)"
if [ -z "${APP_BIN:-}" ]; then
  APP_BIN="$(find "$EXTRACT" -type f -path "*/Contents/MacOS/*" -perm -111 | head -n1 || true)"
fi

if [ -z "${APP_BIN:-}" ]; then
  echo "No installer executable found. Candidates were:"
  find "$EXTRACT" -type f -path "*/Contents/MacOS/*" -print
  exit 1
fi

echo "Installer: $APP_BIN"

# 5) install to goinfre only (no sudo)
"$APP_BIN" \
  --root "$ROOT" \
  --accept-licenses \
  --default-answer \
  --confirm-command install \
  copy_only=1

# 6) verify setup file exists
if [ ! -f "$ROOT/setup-env.sh" ]; then
  echo "setup-env.sh not found at expected path: $ROOT/setup-env.sh"
  echo "Searching for it..."
  find "$GO/VulkanSDK" -name setup-env.sh -print || true
  exit 1
fi

# 7) load env + test
source "$ROOT/setup-env.sh"
echo "VULKAN_SDK=$VULKAN_SDK"
command -v vulkaninfo || true
vulkaninfo | head -n 20 || true
