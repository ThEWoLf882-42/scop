#!/usr/bin/env bash
set -Eeuo pipefail

VERSION="${VERSION:-1.3.296.0}"
USER_NAME="${USER:-$(id -un)}"
GOINFRE_ROOT="${GOINFRE_ROOT:-/goinfre/${USER_NAME}}"
SDK_ARCHIVE="${SDK_ARCHIVE:-${GOINFRE_ROOT}/vulkansdk-macos-${VERSION}.zip}"
SDK_BASE_DIR="${SDK_BASE_DIR:-${GOINFRE_ROOT}/VulkanSDK}"
SDK_ROOT="${SDK_ROOT:-${SDK_BASE_DIR}/${VERSION}}"
EXTRACT_DIR=""
KEEP_ARCHIVE="${KEEP_ARCHIVE:-1}"
FORCE_INSTALL="${FORCE_INSTALL:-0}"

cleanup() {
  if [[ -n "${EXTRACT_DIR}" && -d "${EXTRACT_DIR}" ]]; then
    rm -rf "${EXTRACT_DIR}"
  fi
}
trap cleanup EXIT

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This installer is intended for macOS." >&2
  exit 1
fi

mkdir -p "${GOINFRE_ROOT}" "${SDK_BASE_DIR}"

if [[ -f "${SDK_ROOT}/setup-env.sh" && "${FORCE_INSTALL}" != "1" ]]; then
  echo "Vulkan SDK already installed at: ${SDK_ROOT}"
  echo "Use: source \"${SDK_ROOT}/setup-env.sh\""
  exit 0
fi

if [[ ! -f "${SDK_ARCHIVE}" ]]; then
  echo "Downloading Vulkan SDK ${VERSION}..."
  curl -fL --retry 3 --retry-delay 2 -o "${SDK_ARCHIVE}" \
    "https://sdk.lunarg.com/sdk/download/${VERSION}/mac/vulkansdk-macos-${VERSION}.zip"
else
  echo "Using cached archive: ${SDK_ARCHIVE}"
fi

EXTRACT_DIR="$(mktemp -d "${GOINFRE_ROOT}/vulkansdk-${VERSION}.XXXXXX")"
rm -rf "${SDK_ROOT}"
mkdir -p "${SDK_ROOT}"

echo "Extracting archive..."
unzip -q -o "${SDK_ARCHIVE}" -d "${EXTRACT_DIR}"

xattr -dr com.apple.quarantine "${EXTRACT_DIR}" 2>/dev/null || true

echo "Looking for installer binary..."
INSTALLER_BIN="$(find "${EXTRACT_DIR}" -type f -path '*/Contents/MacOS/InstallVulkan*' | head -n1 || true)"
if [[ -z "${INSTALLER_BIN}" ]]; then
  INSTALLER_BIN="$(find "${EXTRACT_DIR}" -type f -path '*/Contents/MacOS/*' -perm -111 | head -n1 || true)"
fi

if [[ -z "${INSTALLER_BIN}" ]]; then
  echo "No installer executable found inside the extracted SDK." >&2
  find "${EXTRACT_DIR}" -type f -path '*/Contents/MacOS/*' -print >&2 || true
  exit 1
fi

echo "Installer: ${INSTALLER_BIN}"
"${INSTALLER_BIN}" \
  --root "${SDK_ROOT}" \
  --accept-licenses \
  --default-answer \
  --confirm-command install \
  copy_only=1

SETUP_ENV="$(find "${SDK_ROOT}" -name setup-env.sh -type f | head -n1 || true)"
if [[ -z "${SETUP_ENV}" ]]; then
  echo "setup-env.sh was not found under ${SDK_ROOT}" >&2
  find "${SDK_BASE_DIR}" -name setup-env.sh -print >&2 || true
  exit 1
fi

if [[ "${KEEP_ARCHIVE}" != "1" ]]; then
  rm -f "${SDK_ARCHIVE}"
fi

echo
printf 'VULKAN_SDK installed at: %s\n' "${SDK_ROOT}"
printf 'Load it with: source "%s"\n' "${SETUP_ENV}"

echo "Quick check:"
source "${SETUP_ENV}"
printf '  VULKAN_SDK=%s\n' "${VULKAN_SDK:-}"
command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo | head -n 20 || true
