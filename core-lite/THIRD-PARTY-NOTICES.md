# Core Lite third-party package notices

The repository's GPL-3.0 license covers NotreOS-authored source and configuration; it does not replace the licenses of packages included in the Core Lite live image.

## Zorin OS 18.1 Lite source

Zorin's [technical details](https://zorin.com/os/details/) identify Zorin OS 18.1 as Ubuntu 24.04 LTS (Noble), with XFCE in the Lite edition and support through 1 June 2029. Zorin publishes an [18.1 Lite installed-package list](https://assets.zorincdn.com/zorin.com/downloads/installed-package-lists/18/Zorin-OS-18.1-Lite.zip) and points to its public [Stable Launchpad archive](https://launchpad.net/~zorinos/+archive/ubuntu/stable) as an official package source on its [source-code page](https://zorin.com/about/).

The build uses the archive at `https://ppa.launchpadcontent.net/zorinos/stable/ubuntu`, suite `noble`, component `main`. Before enabling it, the build downloads the public key and checks the Launchpad-published signing fingerprint `B2228B8CCA5C00A2C59D35E530FCF8F64F71B61C`; APT then verifies the signed archive metadata and package hashes. The installed package manifest records the exact versions used for each image.

The profile explicitly installs `zorin-os-lite-desktop-minimal`, `zorin-os-lite-default-settings`, `zorin-os-lite-live-settings`, and `xfce4-zorinmenulite-plugin`. Zorin's package metadata makes the official Lite profile and supporting settings the base; NotreOS overlays its own user-facing identity and P4DE configuration on top. The image remains a custom package-built live system, not a copy of the full Zorin ISO.

## License handling

The Zorin Stable package copyright records inspected for the Lite settings and Zorin Menu Lite plug-in specify GPL-2-or-later. Zorin desktop themes specify GPL-2-or-later; the Zorin icon theme package specifies CC BY-SA 4.0. Zorin's wallpaper package uses per-file notices, including CC0-1.0 and CC BY 2.0. The Zorin artwork and GRUB theme packages also carry their own notices. These packages are distributed as separate upstream packages, not relicensed as NotreOS source.

The builder removes most documentation to keep the image small but deliberately retains every `/usr/share/doc/**/copyright` file in the live filesystem. These installed records are the authoritative license notices for each package version; review them in the built image before making any further redistribution or artwork changes. NotreOS uses its own name, release identity, Welcome page, wallpaper, and GTK/panel configuration; upstream Zorin artwork remains governed by its included notices and is not presented as NotreOS branding.
