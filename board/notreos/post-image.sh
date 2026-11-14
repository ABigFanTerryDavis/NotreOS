#!/usr/bin/env bash
set -euo pipefail

: "${BINARIES_DIR:?Buildroot must provide BINARIES_DIR}"
PROJECT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
ISO_NAME="notreos-ancient-0.0.1-x86_64-uefi.iso"
STAGE_DIR="$BINARIES_DIR/notreos-iso-root"
EFI_IMAGE="$STAGE_DIR/efiboot.img"
KERNEL="$BINARIES_DIR/bzImage"
LOADER="$BINARIES_DIR/NotreBootX64.efi"
ROOTFS="$BINARIES_DIR/rootfs.ext4"

for required in "$KERNEL" "$LOADER" "$ROOTFS" "$PROJECT_DIR/board/notreos/notreos.cfg"; do
    if [[ ! -s "$required" ]]; then
        echo "NotreOS post-image: required output is missing or empty: $required" >&2
        exit 1
    fi
done

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR/EFI/BOOT" "$STAGE_DIR/NOTREOS"
install -m 0644 "$KERNEL" "$STAGE_DIR/vmlinuz.efi"
install -m 0644 "$PROJECT_DIR/board/notreos/notreos.cfg" "$STAGE_DIR/notreos.cfg"
cat > "$STAGE_DIR/NOTREOS/README.txt" <<'EOF'
NotreOS Ancient 0.0.1 x86_64 UEFI live media.
NotreBoot loads vmlinuz.efi. The kernel contains the live initramfs.
The default kernel command line is in /notreos.cfg on the EFI volume.
EOF

kernel_bytes=$(stat -c '%s' "$KERNEL")
minimum_bytes=$((16 * 1024 * 1024))
requested_bytes=$((kernel_bytes + 12 * 1024 * 1024))
image_bytes=$(( ((requested_bytes + 1024 * 1024 - 1) / (1024 * 1024)) * 1024 * 1024 ))
if (( image_bytes < minimum_bytes )); then
    image_bytes=$minimum_bytes
fi
image_mib=$((image_bytes / (1024 * 1024)))
truncate -s "$image_bytes" "$EFI_IMAGE"
if (( image_mib < 64 )); then
    mkfs.vfat -F 16 -n NOTREOS_EFI "$EFI_IMAGE" >/dev/null
else
    mkfs.vfat -F 32 -n NOTREOS_EFI "$EFI_IMAGE" >/dev/null
fi
mmd -i "$EFI_IMAGE" ::/EFI
mmd -i "$EFI_IMAGE" ::/EFI/BOOT
mcopy -i "$EFI_IMAGE" "$LOADER" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$EFI_IMAGE" "$KERNEL" ::/vmlinuz.efi
mcopy -i "$EFI_IMAGE" "$PROJECT_DIR/board/notreos/notreos.cfg" ::/notreos.cfg

xorriso -as mkisofs \
    -iso-level 3 -R -J -V NOTREOS_0_0_1 \
    -e efiboot.img -no-emul-boot -isohybrid-gpt-basdat \
    -o "$BINARIES_DIR/$ISO_NAME" "$STAGE_DIR"

iso_bytes=$(stat -c '%s' "$BINARIES_DIR/$ISO_NAME")
rootfs_bytes=$(stat -c '%s' "$ROOTFS")
{
    printf 'NotreOS Ancient 0.0.1\n'
    printf 'Architecture: x86_64\n'
    printf 'Boot: NotreBoot UEFI; Linux EFI stub\n'
    printf 'Kernel: Linux 7.1.13 with embedded xz-compressed live initramfs\n'
    printf 'Build system: Buildroot 2026.08.1 (source archive SHA-256 verified)\n'
    printf 'Interactive shell: NotreShell 0.0.1 (BusyBox shells disabled)\n'
    printf 'Init: NotreInit 0.0.1\n'
    printf 'ISO: %s bytes (%.2f MiB); target approximately 50 MiB\n' \
        "$iso_bytes" "$(awk -v n="$iso_bytes" 'BEGIN { printf "%.2f", n / 1048576 }')"
    printf 'Installed root filesystem image: %s bytes (%.2f MiB); target 192 MiB\n' \
        "$rootfs_bytes" "$(awk -v n="$rootfs_bytes" 'BEGIN { printf "%.2f", n / 1048576 }')"
    printf '\nSHA-256 checksums\n'
    sha256sum "$BINARIES_DIR/$ISO_NAME" "$ROOTFS" "$KERNEL" "$LOADER"
} > "$BINARIES_DIR/notreos-ancient-0.0.1-manifest.txt"

if (( iso_bytes > 50 * 1024 * 1024 )); then
    echo "NotreOS: ISO size is $((iso_bytes / 1024 / 1024)) MiB, above the approximate 50 MiB target; retaining artifact for measurement." >&2
fi
printf 'NotreOS ISO: %s (%s bytes)\n' "$BINARIES_DIR/$ISO_NAME" "$iso_bytes"
printf 'NotreOS installed rootfs: %s (%s bytes)\n' "$ROOTFS" "$rootfs_bytes"
