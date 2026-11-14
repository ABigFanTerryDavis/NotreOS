#!/usr/bin/env python3
"""Boot the public UEFI ISO under OVMF and exercise NotreShell over serial."""
import os
import select
import subprocess
import sys
import time


def fail(proc, output, reason):
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
        proc.wait()
    text = output.decode("utf-8", errors="replace")
    print(text[-6000:], file=sys.stderr)
    raise SystemExit(reason)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: qemu-smoke.py <notreos.iso> <OVMF_CODE.fd>")
    iso, firmware = sys.argv[1:]
    if not os.path.isfile(iso) or not os.path.isfile(firmware):
        raise SystemExit("ISO or OVMF firmware file is missing")

    command = [
        "qemu-system-x86_64", "-m", "1024", "-bios", firmware,
        "-cdrom", iso, "-boot", "d", "-display", "none", "-monitor", "none",
        "-serial", "stdio", "-no-reboot",
    ]
    proc = subprocess.Popen(
        command,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )
    output = bytearray()
    sent_version = False
    sent_marker = False
    deadline = time.monotonic() + 180

    while time.monotonic() < deadline:
        if proc.poll() is not None:
            fail(proc, output, f"QEMU exited early with status {proc.returncode}")
        ready, _, _ = select.select([proc.stdout], [], [], 1.0)
        if not ready:
            continue
        chunk = os.read(proc.stdout.fileno(), 4096)
        if not chunk:
            continue
        output.extend(chunk)
        if b"notre> " in output and not sent_version:
            proc.stdin.write(b"version\n")
            proc.stdin.flush()
            sent_version = True
        if b"NotreShell 0.0.1" in output and sent_version and not sent_marker:
            proc.stdin.write(b"echo NOTREOS_E2E_OK\n")
            proc.stdin.flush()
            sent_marker = True
        if b"NOTREOS_E2E_OK" in output:
            proc.terminate()
            proc.wait(timeout=10)
            print("NotreOS QEMU/OVMF boot and shell smoke test passed.")
            return

    fail(proc, output, "Timed out waiting for the NotreOS shell in QEMU")


if __name__ == "__main__":
    main()
