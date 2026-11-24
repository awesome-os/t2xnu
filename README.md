# `awesome-os/t2xnu`

> **T2XNU (ARM64 Linux + [`AFK-surf/xnulinux`](https://github.com/AFK-surf/xnulinux) `fki.ko` + `xnu.ko`) — sovereign bootable ARM64 UEFI ISO & QEMU QCOW2 powered by `b8g-ghost @ cas-v1`.**

`t2xnu` boots offline straight into a zero-dependency static musl ARM64 C11 PID 1 (`/init`) verified against `B8G_ABI_MAGIC_V3` (`0x33564742`), mounts a persistent `LABEL=B8G_CAS` `ext4` volume (or `tmpfs` fallback), hydrates packages from `awesome-os/b8g-ghost @ cas-v1`, and bundles `/scripts/enable-xnulinux.sh` to load `fki.ko` + `xnu.ko` with **zero VM / hypervisor overhead**.

---

## Downloads (`v0.1.0`)

Download the latest verified ARM64 release artifacts from [GitHub Releases (`v0.1.0`)](https://github.com/awesome-os/t2xnu/releases/tag/v0.1.0):

| Artifact | Description | Direct Link |
| :--- | :--- | :--- |
| `t2xnu-aarch64.iso` | Bootable ARM64 UEFI ISO (`Limine v8` `BOOTAA64.EFI`) | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/t2xnu-aarch64.iso) |
| `t2xnu-aarch64.qcow2` | GPT disk image (64 MiB FAT32 ESP + persistent `ext4` `B8G_CAS`) | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/t2xnu-aarch64.qcow2) |
| `vmlinuz-aarch64` | ARM64 Linux kernel for direct QEMU boot | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/vmlinuz-aarch64) |
| `initramfs-aarch64.cpio.zst` | zstd-compressed initramfs (`/init`, `busybox`, `b8g-ghost`, `enable-xnulinux.sh`) | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/initramfs-aarch64.cpio.zst) |
| `init-aarch64` | Static musl ARM64 PID 1 binary | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/init-aarch64) |
| `SHA256SUMS.txt` | SHA-256 checksums for all release artifacts | [Download](https://github.com/awesome-os/t2xnu/releases/download/v0.1.0/SHA256SUMS.txt) |

---

## Architecture

```text
  ARM64 Hardware (Apple Silicon / DGX Spark / QEMU aarch64)
    └── Limine v8 ARM64 UEFI (EFI/BOOT/BOOTAA64.EFI)
          └── ARM64 Linux Kernel + initramfs-aarch64.cpio.zst
                ├── /init (Static musl ARM64 PID 1, B8G_ABI_MAGIC_V3=0x33564742)
                ├── Linux Userspace (/bin/b8g-ghost, /bin/busybox, /var/lib/b8g/cas)
                └── /scripts/enable-xnulinux.sh -> fki.ko + xnu.ko (zero hypervisor)
                      └── User-supplied Darwin / macOS System Disk (BYO disk)
```

---

## Boot in QEMU (`aarch64`)

### 1. Direct Kernel + Initramfs (Interactive Shell)

```bash
qemu-system-aarch64 \
  -M virt -cpu cortex-a57 -m 512 -nographic \
  -kernel vmlinuz-aarch64 \
  -initrd initramfs-aarch64.cpio.zst \
  -append "console=ttyAMA0,115200"
```

### 2. Direct Kernel + Initramfs (CI Self-Test)

```bash
qemu-system-aarch64 \
  -M virt -cpu cortex-a57 -m 512 -nographic -no-reboot \
  -kernel vmlinuz-aarch64 \
  -initrd initramfs-aarch64.cpio.zst \
  -append "console=ttyAMA0,115200 t2.selftest=1"
```

### 3. Bootable ARM64 UEFI ISO (`qemu-efi-aarch64`)

```bash
qemu-system-aarch64 \
  -M virt -cpu cortex-a57 -m 1024 -nographic \
  -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
  -cdrom t2xnu-aarch64.iso
```

### 4. GPT UEFI QCOW2 with Persistent `B8G_CAS` Partition

```bash
qemu-system-aarch64 \
  -M virt -cpu cortex-a57 -m 1024 -nographic \
  -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
  -drive file=t2xnu-aarch64.qcow2,format=qcow2,if=virtio
```

---

## Quick Start (ARM64 Host)

```sh
# 1. Inspect host compatibility & cas-v1 status
node src/index.mts --status

# 2. Load fki.ko + xnu.ko against your own legally obtained Darwin/macOS disk image
sh /scripts/enable-xnulinux.sh /path/to/darwin-system.img
```

---

## Cross-Compiling `/init` Locally

```powershell
zig cc -target aarch64-linux-musl -static -Os -s -Wall -Wextra src/init.c -o dist/init-aarch64
```

---

## Licensing & Trademark Boundary
- `awesome-os/t2xnu` ships **only open-source orchestration, CAS hydration, and loader glue** (`MIT` / `APSL-2.0` upstream XNU via `awesome-os/upstream-mirrors @ mirror/xnulinux`).
- `t2xnu` **never** redistributes proprietary Apple macOS userspace files — users bring their own legally obtained system disk image.

## Quick Install & Run (Windows / Linux / macOS)

```sh
# Install globally via awesome-os/pnpm (all platforms)
pnpm add -g github:awesome-os/t2xnu

# Run directly (all platforms)
t2xnu --help
```
