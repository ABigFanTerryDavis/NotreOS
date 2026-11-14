#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE_DIR="${1:-$ROOT_DIR/output/images}"
ISO="$IMAGE_DIR/notreos-ancient-0.0.1-x86_64-uefi.iso"
for item in "$ISO" "$IMAGE_DIR/rootfs.ext4" "$IMAGE_DIR/bzImage" \
            "$IMAGE_DIR/NotreBootX64.efi" "$IMAGE_DIR/notreos-ancient-0.0.1-manifest.txt"; do
    [[ -s "$item" ]] || { echo "Missing/empty build artifact: $item" >&2; exit 1; }
done

file "$IMAGE_DIR/NotreBootX64.efi" | grep -qi 'EFI application' || {
    echo 'NotreBootX64.efi is not recognized as an EFI application.' >&2
    file "$IMAGE_DIR/NotreBootX64.efi" >&2
    exit 1
}

report="$(xorriso -indev "$ISO" -report_el_torito plain 2>&1)"
printf '%s\n' "$report" | grep -qi 'UEFI' || {
    echo 'ISO does not advertise a UEFI El Torito boot entry.' >&2
    printf '%s\n' "$report" >&2
    exit 1
}

[[ -x "$ROOT_DIR/output/target/init" ]] || { echo 'NotreInit is missing from the initramfs root.' >&2; exit 1; }
[[ -x "$ROOT_DIR/output/target/usr/bin/notre-shell" ]] || { echo 'NotreShell is missing from the target root.' >&2; exit 1; }
[[ -x "$ROOT_DIR/output/target/usr/bin/Xorg" ]] || { echo 'The modular X.org server is missing from the target root.' >&2; exit 1; }
[[ -x "$ROOT_DIR/output/target/usr/bin/dillo" ]] || { echo 'Dillo is missing from the target root.' >&2; exit 1; }
if grep -q '^CONFIG_ASH=y\|^CONFIG_HUSH=y' "$ROOT_DIR"/output/build/busybox-*/.config; then
    echo 'A BusyBox shell applet is enabled; NotreShell must be the only interactive shell.' >&2
    exit 1
fi

printf '%s\n' 'NotreOS artifact checks passed.'
printf '%s\n' "$report" | grep -E 'El Torito|UEFI|image[0-9]' || true
