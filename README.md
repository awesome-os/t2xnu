# `awesome-os/t2xnu`

> **T2Linux (ARM64 Linux `7.0+`) + [`AFK-surf/xnulinux`](https://github.com/AFK-surf/xnulinux) (`fki.ko` + `xnu.ko`) — native zero-hypervisor Linux + XNU/Darwin dual-personality OS powered by `b8g-ghost @ cas-v1`.**

`t2xnu` pairs our `21.5 MiB` [`awesome-os/t2linux`](https://github.com/awesome-os/t2linux) ARM64 kernel with the open-source XNU loadable kernel module (`insmod fki.ko && insmod xnu.ko`), allowing Linux processes and XNU/Darwin processes to share the exact same CPUs, MMU, network, and `b8g-ghost` CAS storage with **zero VM / hypervisor overhead**.

---

## Architecture

```text
  ARM64 Hardware (Apple Silicon / DGX Spark / QEMU aarch64)
    └── T2Linux Kernel (Linux 7.0+, persistent B8G_CAS)
          ├── Linux Userspace (/bin/b8g-ghost, t2linux, code-oss, sectium-browser)
          └── fki.ko + xnu.ko (AFK-surf/xnulinux, zero hypervisor)
                └── User-supplied Darwin / macOS System Disk (BYO disk)
```

---

## Quick Start (ARM64 T2Linux Host)

```sh
# 1. Inspect host compatibility & cas-v1 status
node src/index.mts --status

# 2. Load fki.ko + xnu.ko against your own legally obtained Darwin/macOS disk image
sh scripts/enable-xnulinux.sh /path/to/darwin-system.img
```

---

## Licensing & Trademark Boundary
- `awesome-os/t2xnu` ships **only open-source orchestration, CAS hydration, and loader glue** (`MIT` / `APSL-2.0` upstream XNU via `awesome-os/upstream-mirrors @ mirror/xnulinux`).
- `t2xnu` **never** redistributes proprietary Apple macOS userspace files — users bring their own legally obtained system disk image.
