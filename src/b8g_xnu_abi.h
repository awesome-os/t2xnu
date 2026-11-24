#ifndef B8G_XNU_ABI_H
#define B8G_XNU_ABI_H

#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical Awesome-OS B8G ABI v3 magic ('B','G','V','3' little-endian) */
#define B8G_ABI_MAGIC_V3 0x33564742U
/* Zero-copy SharedArrayBuffer / mmap arena magic ('B','A','V','1') */
#define B8G_ARENA_MAGIC_V1 0x31564142U

#define B8G_ARENA_ALIGNMENT 64U
#define B8G_XNU_BRIDGE_VERSION 1U
#define B8G_XNU_RING_CAPACITY 64U

#define B8G_ARCH_UNKNOWN 0U
#define B8G_ARCH_X86_64 1U
#define B8G_ARCH_AARCH64 2U

#if defined(__aarch64__) || defined(_M_ARM64)
#define B8G_HOST_ARCH B8G_ARCH_AARCH64
#elif defined(__x86_64__) || defined(_M_X64)
#define B8G_HOST_ARCH B8G_ARCH_X86_64
#else
#define B8G_HOST_ARCH B8G_ARCH_UNKNOWN
#endif

enum b8g_xnu_opcode {
    B8G_XNU_OP_NOP = 0x0000U,
    B8G_XNU_OP_HANDSHAKE = 0x0001U,

    /* Zero-copy graphics / display escape -> ghost_sectium_display */
    B8G_XNU_OP_DISPLAY_BLIT = 0x0100U,
    B8G_XNU_OP_DISPLAY_VSYNC = 0x0101U,

    /* Native host compression shard offload */
    B8G_XNU_OP_COMPRESS_ZSTD = 0x0200U,
    B8G_XNU_OP_DECOMPRESS_ZSTD = 0x0201U,

    /* Content-addressed b8g-ghost @ cas-v1 zero-copy lookup & mapping */
    B8G_XNU_OP_CAS_LOOKUP = 0x0300U,
    B8G_XNU_OP_CAS_MAP = 0x0301U,

    /* AFK-surf/xnulinux fki.ko + xnu.ko foreign-kernel doorbell */
    B8G_XNU_OP_FKI_SIGNAL = 0x0400U
};

enum b8g_xnu_status {
    B8G_XNU_OK = 0,
    B8G_XNU_ERR_MAGIC = -1,
    B8G_XNU_ERR_ALIGN = -2,
    B8G_XNU_ERR_BOUNDS = -3,
    B8G_XNU_ERR_RING_FULL = -4,
    B8G_XNU_ERR_RING_EMPTY = -5,
    B8G_XNU_ERR_UNSUPPORTED = -6,
    B8G_XNU_ERR_NO_HANDLER = -7
};

/*
 * 64-byte cache-line aligned host-escape descriptor shared between
 * ARM64 XNU/Darwin guest (via fki.ko / xnu.ko) and native host b8g-ghost.
 */
struct b8g_xnu_descriptor {
    _Alignas(64) uint32_t abi_magic; /* Must equal B8G_ABI_MAGIC_V3 */
    uint16_t opcode;         /* enum b8g_xnu_opcode */
    uint16_t flags;          /* Opcode-specific flags */
    uint64_t seq_id;         /* Monotonic request correlation ID */
    uint64_t payload_offset; /* Byte offset from arena payload_base_offset */
    uint32_t payload_len;    /* Input payload length in bytes */
    uint32_t response_len;   /* Output payload length written by host */
    int32_t status;          /* enum b8g_xnu_status */
    uint32_t arg0;           /* e.g. width / compression level / CAS flags */
    uint32_t arg1;           /* e.g. height / stride / target arch */
    uint32_t reserved0;      /* Zero-initialized padding */
    uint64_t arg2;           /* e.g. IOSurface handle / FKI token */
    uint64_t timestamp_ns;   /* Monotonic timestamp */
};

/*
 * 64-byte aligned B8G_ARENA_V1 control header placed at offset 0 of
 * the shared memory arena (mmap / SharedArrayBuffer / fki.ko SHM).
 *
 * Layout inside the arena:
 *   [0 .. 64)                                      : struct b8g_arena_v1_header
 *   [64 .. 64 + cap*64)                            : Submission Queue (SQ)
 *   [64 + cap*64 .. 64 + 2*cap*64)                 : Completion Queue (CQ)
 *   [payload_base_offset .. total_bytes)           : Zero-copy payload buffer
 */
struct b8g_arena_v1_header {
    _Alignas(64) uint32_t abi_magic; /* B8G_ABI_MAGIC_V3 (0x33564742) */
    uint32_t arena_magic;         /* B8G_ARENA_MAGIC_V1 (0x31564142) */
    uint32_t version;             /* B8G_XNU_BRIDGE_VERSION */
    uint32_t host_arch;           /* B8G_ARCH_X86_64 or B8G_ARCH_AARCH64 */
    uint32_t guest_arch;          /* B8G_ARCH_AARCH64 (XNU/Darwin) */
    uint32_t ring_capacity;       /* Power-of-two ring slots (default 64) */
    uint64_t total_bytes;         /* Total arena size in bytes */
    uint64_t payload_base_offset; /* 64-byte aligned start of payload region */
    uint64_t payload_capacity;    /* Bytes available for zero-copy payloads */
    _Atomic uint32_t sq_head;     /* Host consumer index */
    _Atomic uint32_t sq_tail;     /* Guest producer index */
    _Atomic uint32_t cq_head;     /* Guest consumer index */
    _Atomic uint32_t cq_tail;     /* Host producer index */
};

_Static_assert(sizeof(struct b8g_xnu_descriptor) == 64,
               "b8g_xnu_descriptor must be exactly 64 bytes");
_Static_assert(alignof(struct b8g_xnu_descriptor) == 64,
               "b8g_xnu_descriptor must be 64-byte aligned");
_Static_assert(sizeof(struct b8g_arena_v1_header) == 64,
               "b8g_arena_v1_header must be exactly 64 bytes");
_Static_assert(alignof(struct b8g_arena_v1_header) == 64,
               "b8g_arena_v1_header must be 64-byte aligned");

typedef int (*b8g_xnu_handler_fn)(struct b8g_arena_v1_header *hdr,
                                  struct b8g_xnu_descriptor *desc,
                                  void *payload, void *ctx);

/* Minimum byte size required for a B8G_ARENA_V1 with ring_capacity slots. */
static inline size_t b8g_xnu_arena_min_bytes(uint32_t ring_capacity) {
    return sizeof(struct b8g_arena_v1_header) +
           (size_t)ring_capacity * sizeof(struct b8g_xnu_descriptor) * 2U;
}

int b8g_xnu_arena_init(void *base, size_t total_bytes, uint32_t guest_arch);
int b8g_xnu_arena_validate(const void *base, size_t total_bytes);

void *b8g_xnu_payload_ptr(struct b8g_arena_v1_header *hdr, uint64_t offset,
                          uint32_t len);

int b8g_xnu_submit(struct b8g_arena_v1_header *hdr,
                   const struct b8g_xnu_descriptor *desc);
int b8g_xnu_poll_submission(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *out);
int b8g_xnu_complete(struct b8g_arena_v1_header *hdr,
                     const struct b8g_xnu_descriptor *desc);
int b8g_xnu_poll_completion(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *out);

int b8g_xnu_default_handler(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *desc, void *payload,
                            void *ctx);

int b8g_xnu_dispatch_one(struct b8g_arena_v1_header *hdr,
                         b8g_xnu_handler_fn handler, void *ctx);

int b8g_xnu_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* B8G_XNU_ABI_H */
