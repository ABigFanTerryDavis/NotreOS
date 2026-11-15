# NotreOS Ancient 0.0.1

NotreOS is a small x86_64 Linux system. Linux is the kernel; NotreOS supplies its own UEFI bootloader (**NotreBoot**), first-process init (**NotreInit**) and interactive shell (**NotreShell**). The default graphical session is **P4DE — Platform 4 Desktop Environment**. Dillo is the browser in the first edition, **Ancient**.

The release is built from source in GitHub Actions with a pinned Buildroot release used as the build orchestrator. Buildroot is not used as NotreOS's bootloader, init system or user shell. BusyBox shell applets are disabled; BusyBox remains only as a compact set of command-line utilities. The Linux kernel, X.org, Dillo and their required libraries are compiled for NotreOS during the build.

NotreOS uses **Linux 7.1.13**, with its source checksum verified from the pinned Buildroot release. The other upstream pins and their source references are documented in [`docs/upstream-pins.md`](docs/upstream-pins.md).

## Build

On a recent Ubuntu or Debian host, install the packages listed in `.github/workflows/build.yml`, then run:

```sh
./scripts/build.sh
```

The build is x86_64 and UEFI-first. It produces a live ISO with a NotreOS-written EFI loader and a kernel containing the live root filesystem. It also builds a 192 MiB ext4 root-filesystem image for installation experiments.

## 0.0.1 outputs

After a successful build, files are in `output/images/`: `notreos-ancient-0.0.1-x86_64-uefi.iso` is the UEFI live ISO; `rootfs.ext4` is a 192 MiB ext4 system image labelled `NOTREOS`; `bzImage` is the Linux kernel with the live initramfs embedded; `NotreBootX64.efi` is NotreOS's UEFI loader; and `notreos-ancient-0.0.1-manifest.txt` records versions, sizes and SHA-256 checksums.

The ISO target is approximately 50 MiB; GitHub Actions reports its actual size rather than hiding an overage. The ext4 image is the installed root-filesystem size target. This first release is UEFI-only; legacy BIOS boot is not included.

## Using the live ISO

Boot the ISO from a UEFI virtual machine or x86_64 computer. NotreBoot loads the Linux kernel, then NotreInit starts the system. **P4DE** is the name of the graphical session. In Ancient 0.0.1 it is intentionally minimal: NotreInit starts X.org and opens Dillo on the graphical console; a separate window manager, panel and desktop settings app are not included yet. NotreShell is available on virtual terminal 2. The local browser page is `/usr/share/notreos/welcome.html`.

## Install experiment

The ext4 image is a root-filesystem image, not a partitioned whole-disk image. Write it to an ext4 root partition of at least 192 MiB, create a FAT EFI System Partition, and copy `NotreBootX64.efi` as `EFI/BOOT/BOOTX64.EFI` and `bzImage` as `vmlinuz.efi`. Put a `notreos.cfg` beside the kernel with a root-device option, for example:

```text
console=tty0 root=LABEL=NOTREOS rootfstype=ext4 rootwait rw
```

NotreInit can resolve the `NOTREOS` filesystem label and switch from the kernel's embedded initramfs to the ext4 root. Back up any target disk before experimenting; the repository does not yet include a graphical installer or a whole-disk image.

## NotreShell 0.0.1

NotreShell supports command execution through `PATH`, quoted arguments, and the built-ins `cd`, `clear`, `echo`, `exit`, `help`, `pwd`, and `version`. Pipes, redirection, shell variables, globbing and scripting are not part of this first version. It is the only interactive shell compiled into the target image.

## Build transparency

The workflow validates NotreShell, builds the source tree, inspects the EFI/ISO outputs, and boots the actual NotreBoot ISO under QEMU/OVMF. It then runs NotreShell commands over the emulated serial console. The automated test does not capture or judge P4DE's graphical appearance.

## License

NotreOS project source is licensed under [GPL-3.0](LICENSE). Third-party packages retain their upstream licenses.
