#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILDROOT_VERSION="2026.08.1"
BUILDROOT_SHA256="34fb7bea8a213ffde39cc02e61703391560f26e0850f03138c722d0b09880f2b"
WORK_DIR="${NOTREOS_BUILD_WORK_DIR:-${TMPDIR:-/tmp}/notreos-build}"
SOURCE_DIR="$WORK_DIR/buildroot-$BUILDROOT_VERSION"
ARCHIVE="$WORK_DIR/buildroot-$BUILDROOT_VERSION.tar.xz"
DOWNLOAD_DIR="$ROOT_DIR/dl"
OUTPUT_DIR="$ROOT_DIR/output"
JOBS="${NOTREOS_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"

mkdir -p "$WORK_DIR" "$DOWNLOAD_DIR"
if [[ ! -s "$ARCHIVE" ]]; then
    curl --fail --location --retry 3 \
        "https://buildroot.org/downloads/buildroot-$BUILDROOT_VERSION.tar.xz" \
        --output "$ARCHIVE"
fi
printf '%s  %s\n' "$BUILDROOT_SHA256" "$ARCHIVE" | sha256sum --check --status || {
    echo "Buildroot archive checksum mismatch; refusing to build." >&2
    exit 1
}
if [[ ! -f "$SOURCE_DIR/Makefile" ]]; then
    tar -xf "$ARCHIVE" -C "$WORK_DIR"
fi

if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
    if git -C "$ROOT_DIR" rev-parse --verify HEAD >/dev/null 2>&1; then
        SOURCE_DATE_EPOCH="$(git -C "$ROOT_DIR" log -1 --format=%ct)"
    else
        SOURCE_DATE_EPOCH="1794614400"
    fi
fi
export SOURCE_DATE_EPOCH
export LC_ALL=C

make -C "$SOURCE_DIR" O="$OUTPUT_DIR" \
    BR2_EXTERNAL="$ROOT_DIR" \
    BR2_DL_DIR="$DOWNLOAD_DIR" \
    BR2_DEFCONFIG="$ROOT_DIR/configs/notreos_x86_64_defconfig" \
    defconfig

for required in \
    BR2_x86_64=y \
    BR2_USE_WCHAR=y \
    BR2_KERNEL_HEADERS_7_1=y \
    BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=\"7.1.13\" \
    BR2_INIT_NONE=y \
    BR2_SYSTEM_BIN_SH_NONE=y \
    BR2_TARGET_ROOTFS_INITRAMFS=y \
    BR2_PACKAGE_NOTRE_BOOT=y \
    BR2_PACKAGE_NOTRE_INIT=y \
    BR2_PACKAGE_NOTRE_SHELL=y \
    BR2_PACKAGE_XORG7=y \
    BR2_PACKAGE_XSERVER_XORG_SERVER=y \
    BR2_PACKAGE_XSERVER_XORG_SERVER_MODULAR=y \
    BR2_PACKAGE_DILLO=y \
    BR2_PACKAGE_DHCPCD=y \
    BR2_TARGET_ROOTFS_EXT2_SIZE=\"192M\"; do
    if ! grep -Fqx "$required" "$OUTPUT_DIR/.config"; then
        echo "NotreOS defconfig did not enable required setting: $required" >&2
        exit 1
    fi
done

make -C "$SOURCE_DIR" O="$OUTPUT_DIR" \
    BR2_EXTERNAL="$ROOT_DIR" \
    BR2_DL_DIR="$DOWNLOAD_DIR" \
    -j"$JOBS"

"$ROOT_DIR/scripts/check-artifacts.sh" "$OUTPUT_DIR/images"
