#define _GNU_SOURCE

#include "b8g_xnu_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_power_of_two(uint32_t v) { return v != 0U && (v & (v - 1U)) == 0U; }

static struct b8g_xnu_descriptor *sq_ring(struct b8g_arena_v1_header *hdr) {
    uint8_t *base = (uint8_t *)hdr;
    return (struct b8g_xnu_descriptor *)(base +
                                         sizeof(struct b8g_arena_v1_header));
}

static struct b8g_xnu_descriptor *cq_ring(struct b8g_arena_v1_header *hdr) {
    uint8_t *base = (uint8_t *)hdr;
    return (struct b8g_xnu_descriptor
                *)(base + sizeof(struct b8g_arena_v1_header) +
                   (size_t)hdr->ring_capacity *
                       sizeof(struct b8g_xnu_descriptor));
}

int b8g_xnu_arena_init(void *base, size_t total_bytes, uint32_t guest_arch) {
    if (base == NULL || ((uintptr_t)base % B8G_ARENA_ALIGNMENT) != 0U) {
        return B8G_XNU_ERR_ALIGN;
    }

    size_t min_bytes = b8g_xnu_arena_min_bytes(B8G_XNU_RING_CAPACITY);
    if (total_bytes < min_bytes) {
        return B8G_XNU_ERR_BOUNDS;
    }

    memset(base, 0, min_bytes);

    struct b8g_arena_v1_header *hdr = (struct b8g_arena_v1_header *)base;
    hdr->abi_magic = B8G_ABI_MAGIC_V3;
    hdr->arena_magic = B8G_ARENA_MAGIC_V1;
    hdr->version = B8G_XNU_BRIDGE_VERSION;
    hdr->host_arch = B8G_HOST_ARCH;
    hdr->guest_arch = guest_arch;
    hdr->ring_capacity = B8G_XNU_RING_CAPACITY;
    hdr->total_bytes = (uint64_t)total_bytes;
    hdr->payload_base_offset = (uint64_t)min_bytes;
    hdr->payload_capacity = (uint64_t)(total_bytes - min_bytes);

    atomic_init(&hdr->sq_head, 0U);
    atomic_init(&hdr->sq_tail, 0U);
    atomic_init(&hdr->cq_head, 0U);
    atomic_init(&hdr->cq_tail, 0U);

    return B8G_XNU_OK;
}

int b8g_xnu_arena_validate(const void *base, size_t total_bytes) {
    if (base == NULL || ((uintptr_t)base % B8G_ARENA_ALIGNMENT) != 0U) {
        return B8G_XNU_ERR_ALIGN;
    }
    if (total_bytes < sizeof(struct b8g_arena_v1_header)) {
        return B8G_XNU_ERR_BOUNDS;
    }

    const struct b8g_arena_v1_header *hdr =
        (const struct b8g_arena_v1_header *)base;
    if (hdr->abi_magic != B8G_ABI_MAGIC_V3 ||
        hdr->arena_magic != B8G_ARENA_MAGIC_V1) {
        return B8G_XNU_ERR_MAGIC;
    }
    if (hdr->version != B8G_XNU_BRIDGE_VERSION ||
        !is_power_of_two(hdr->ring_capacity)) {
        return B8G_XNU_ERR_UNSUPPORTED;
    }

    size_t min_bytes = b8g_xnu_arena_min_bytes(hdr->ring_capacity);
    if (hdr->total_bytes > (uint64_t)total_bytes ||
        hdr->payload_base_offset < (uint64_t)min_bytes ||
        (hdr->payload_base_offset % B8G_ARENA_ALIGNMENT) != 0U ||
        hdr->payload_base_offset + hdr->payload_capacity > hdr->total_bytes) {
        return B8G_XNU_ERR_BOUNDS;
    }

    return B8G_XNU_OK;
}

void *b8g_xnu_payload_ptr(struct b8g_arena_v1_header *hdr, uint64_t offset,
                          uint32_t len) {
    if (hdr == NULL || offset > hdr->payload_capacity ||
        (uint64_t)len > hdr->payload_capacity - offset) {
        return NULL;
    }
    return (uint8_t *)hdr + hdr->payload_base_offset + offset;
}

int b8g_xnu_submit(struct b8g_arena_v1_header *hdr,
                   const struct b8g_xnu_descriptor *desc) {
    if (hdr == NULL || desc == NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }
    if (hdr->abi_magic != B8G_ABI_MAGIC_V3 ||
        desc->abi_magic != B8G_ABI_MAGIC_V3) {
        return B8G_XNU_ERR_MAGIC;
    }
    if (desc->payload_len > 0U &&
        b8g_xnu_payload_ptr(hdr, desc->payload_offset, desc->payload_len) ==
            NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }

    uint32_t cap = hdr->ring_capacity;
    uint32_t mask = cap - 1U;
    uint32_t tail = atomic_load_explicit(&hdr->sq_tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&hdr->sq_head, memory_order_acquire);

    if ((uint32_t)(tail - head) >= cap) {
        return B8G_XNU_ERR_RING_FULL;
    }

    sq_ring(hdr)[tail & mask] = *desc;
    atomic_store_explicit(&hdr->sq_tail, tail + 1U, memory_order_release);
    return B8G_XNU_OK;
}

int b8g_xnu_poll_submission(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *out) {
    if (hdr == NULL || out == NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }

    uint32_t mask = hdr->ring_capacity - 1U;
    uint32_t head = atomic_load_explicit(&hdr->sq_head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&hdr->sq_tail, memory_order_acquire);

    if (head == tail) {
        return B8G_XNU_ERR_RING_EMPTY;
    }

    *out = sq_ring(hdr)[head & mask];
    atomic_store_explicit(&hdr->sq_head, head + 1U, memory_order_release);
    return B8G_XNU_OK;
}

int b8g_xnu_complete(struct b8g_arena_v1_header *hdr,
                     const struct b8g_xnu_descriptor *desc) {
    if (hdr == NULL || desc == NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }

    uint32_t cap = hdr->ring_capacity;
    uint32_t mask = cap - 1U;
    uint32_t tail = atomic_load_explicit(&hdr->cq_tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&hdr->cq_head, memory_order_acquire);

    if ((uint32_t)(tail - head) >= cap) {
        return B8G_XNU_ERR_RING_FULL;
    }

    cq_ring(hdr)[tail & mask] = *desc;
    atomic_store_explicit(&hdr->cq_tail, tail + 1U, memory_order_release);
    return B8G_XNU_OK;
}

int b8g_xnu_poll_completion(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *out) {
    if (hdr == NULL || out == NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }

    uint32_t mask = hdr->ring_capacity - 1U;
    uint32_t head = atomic_load_explicit(&hdr->cq_head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&hdr->cq_tail, memory_order_acquire);

    if (head == tail) {
        return B8G_XNU_ERR_RING_EMPTY;
    }

    *out = cq_ring(hdr)[head & mask];
    atomic_store_explicit(&hdr->cq_head, head + 1U, memory_order_release);
    return B8G_XNU_OK;
}

int b8g_xnu_default_handler(struct b8g_arena_v1_header *hdr,
                            struct b8g_xnu_descriptor *desc, void *payload,
                            void *ctx) {
    (void)ctx;
    if (hdr == NULL || desc == NULL) {
        return B8G_XNU_ERR_BOUNDS;
    }

    switch (desc->opcode) {
    case B8G_XNU_OP_NOP:
        desc->status = B8G_XNU_OK;
        desc->response_len = 0U;
        return B8G_XNU_OK;

    case B8G_XNU_OP_HANDSHAKE:
        desc->arg0 = hdr->host_arch;
        desc->arg1 = hdr->guest_arch;
        desc->arg2 = hdr->payload_capacity;
        desc->status = B8G_XNU_OK;
        desc->response_len = 0U;
        return B8G_XNU_OK;

    case B8G_XNU_OP_DISPLAY_BLIT:
    case B8G_XNU_OP_DISPLAY_VSYNC:
        /* Zero-copy framebuffer blit acknowledged in-place within B8G_ARENA_V1 */
        if (desc->opcode == B8G_XNU_OP_DISPLAY_BLIT && desc->payload_len > 0U &&
            payload == NULL) {
            desc->status = B8G_XNU_ERR_BOUNDS;
            return B8G_XNU_ERR_BOUNDS;
        }
        desc->response_len = desc->payload_len;
        desc->status = B8G_XNU_OK;
        return B8G_XNU_OK;

    case B8G_XNU_OP_COMPRESS_ZSTD:
    case B8G_XNU_OP_DECOMPRESS_ZSTD:
        /* Host shard dispatch hook point; pass-through acknowledgment by default */
        if (desc->payload_len > 0U && payload == NULL) {
            desc->status = B8G_XNU_ERR_BOUNDS;
            return B8G_XNU_ERR_BOUNDS;
        }
        desc->response_len = desc->payload_len;
        desc->status = B8G_XNU_OK;
        return B8G_XNU_OK;

    case B8G_XNU_OP_CAS_LOOKUP:
    case B8G_XNU_OP_CAS_MAP:
    case B8G_XNU_OP_FKI_SIGNAL:
        desc->response_len = desc->payload_len;
        desc->status = B8G_XNU_OK;
        return B8G_XNU_OK;

    default:
        desc->status = B8G_XNU_ERR_UNSUPPORTED;
        desc->response_len = 0U;
        return B8G_XNU_ERR_UNSUPPORTED;
    }
}

int b8g_xnu_dispatch_one(struct b8g_arena_v1_header *hdr,
                         b8g_xnu_handler_fn handler, void *ctx) {
    struct b8g_xnu_descriptor desc;
    int rc = b8g_xnu_poll_submission(hdr, &desc);
    if (rc != B8G_XNU_OK) {
        return rc;
    }

    if (desc.abi_magic != B8G_ABI_MAGIC_V3) {
        desc.status = B8G_XNU_ERR_MAGIC;
        desc.response_len = 0U;
        return b8g_xnu_complete(hdr, &desc);
    }

    void *payload = NULL;
    if (desc.payload_len > 0U) {
        payload =
            b8g_xnu_payload_ptr(hdr, desc.payload_offset, desc.payload_len);
        if (payload == NULL) {
            desc.status = B8G_XNU_ERR_BOUNDS;
            desc.response_len = 0U;
            return b8g_xnu_complete(hdr, &desc);
        }
    }

    b8g_xnu_handler_fn fn = handler != NULL ? handler : b8g_xnu_default_handler;
    int hrc = fn(hdr, &desc, payload, ctx);
    if (desc.status == 0 && hrc != B8G_XNU_OK) {
        desc.status = hrc;
    }

    return b8g_xnu_complete(hdr, &desc);
}

int b8g_xnu_selftest(void) {
    const size_t arena_size = 64U * 1024U;
    void *mem = NULL;
    if (posix_memalign(&mem, B8G_ARENA_ALIGNMENT, arena_size) != 0 ||
        mem == NULL) {
        return B8G_XNU_ERR_ALIGN;
    }

    int rc = b8g_xnu_arena_init(mem, arena_size, B8G_ARCH_AARCH64);
    if (rc != B8G_XNU_OK) {
        free(mem);
        return rc;
    }
    rc = b8g_xnu_arena_validate(mem, arena_size);
    if (rc != B8G_XNU_OK) {
        free(mem);
        return rc;
    }

    struct b8g_arena_v1_header *hdr = (struct b8g_arena_v1_header *)mem;

    /* 1. Handshake round-trip */
    struct b8g_xnu_descriptor req;
    memset(&req, 0, sizeof(req));
    req.abi_magic = B8G_ABI_MAGIC_V3;
    req.opcode = B8G_XNU_OP_HANDSHAKE;
    req.seq_id = 1U;

    rc = b8g_xnu_submit(hdr, &req);
    if (rc != B8G_XNU_OK) {
        free(mem);
        return rc;
    }
    rc = b8g_xnu_dispatch_one(hdr, NULL, NULL);
    if (rc != B8G_XNU_OK) {
        free(mem);
        return rc;
    }

    struct b8g_xnu_descriptor cpl;
    rc = b8g_xnu_poll_completion(hdr, &cpl);
    if (rc != B8G_XNU_OK || cpl.status != B8G_XNU_OK || cpl.seq_id != 1U ||
        cpl.arg0 != B8G_HOST_ARCH || cpl.arg1 != B8G_ARCH_AARCH64) {
        free(mem);
        return B8G_XNU_ERR_MAGIC;
    }

    /* 2. Zero-copy display blit + CAS lookup + FKI signal */
    const uint16_t ops[3] = {B8G_XNU_OP_DISPLAY_BLIT, B8G_XNU_OP_CAS_LOOKUP,
                             B8G_XNU_OP_FKI_SIGNAL};
    uint8_t *slot = (uint8_t *)b8g_xnu_payload_ptr(hdr, 0U, 256U);
    if (slot == NULL) {
        free(mem);
        return B8G_XNU_ERR_BOUNDS;
    }
    memset(slot, 0xA5, 256U);

    for (uint32_t i = 0U; i < 3U; i++) {
        memset(&req, 0, sizeof(req));
        req.abi_magic = B8G_ABI_MAGIC_V3;
        req.opcode = ops[i];
        req.seq_id = (uint64_t)(i + 2U);
        req.payload_offset = 0U;
        req.payload_len = 256U;
        req.arg0 = 1920U;
        req.arg1 = 1080U;

        rc = b8g_xnu_submit(hdr, &req);
        if (rc != B8G_XNU_OK) {
            free(mem);
            return rc;
        }
        rc = b8g_xnu_dispatch_one(hdr, NULL, NULL);
        if (rc != B8G_XNU_OK) {
            free(mem);
            return rc;
        }
        rc = b8g_xnu_poll_completion(hdr, &cpl);
        if (rc != B8G_XNU_OK || cpl.status != B8G_XNU_OK ||
            cpl.response_len != 256U) {
            free(mem);
            return B8G_XNU_ERR_BOUNDS;
        }
    }

    free(mem);
    return B8G_XNU_OK;
}

#ifdef B8G_XNU_BRIDGE_MAIN
int main(void) {
    int rc = b8g_xnu_selftest();
    if (rc != B8G_XNU_OK) {
        fprintf(stderr, "[B8G-XNU-BRIDGE] SELFTEST FAIL (rc=%d)\n", rc);
        return 1;
    }
    printf("[B8G-XNU-BRIDGE] SELFTEST PASS (B8G_ABI_MAGIC_V3=0x%08X | "
           "B8G_ARENA_V1=64B-aligned | host_arch=%u)\n",
           B8G_ABI_MAGIC_V3, (unsigned)B8G_HOST_ARCH);
    return 0;
}
#endif
