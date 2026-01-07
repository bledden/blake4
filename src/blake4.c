/*
 * BLAKE4 Cryptographic Hash Function
 * Reference Implementation (Portable C)
 *
 * Based on BLAKE3 algorithm. This is a clean-room implementation
 * focused on correctness over performance.
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <string.h>

/* ============== Internal Constants ============== */

static const uint32_t IV[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

static const uint8_t MSG_SCHEDULE[7][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8},
    {3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1},
    {10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6},
    {12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
    {9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7},
    {11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13}
};

#define CHUNK_START         (1 << 0)
#define CHUNK_END           (1 << 1)
#define PARENT              (1 << 2)
#define ROOT                (1 << 3)
#define KEYED_HASH          (1 << 4)
#define DERIVE_KEY_CONTEXT  (1 << 5)
#define DERIVE_KEY_MATERIAL (1 << 6)

/* ============== Utility Functions ============== */

static inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t load32_le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline void store32_le(uint8_t *p, uint32_t x) {
    p[0] = (uint8_t)(x);
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
}

static inline void words_from_bytes(const uint8_t bytes[32], uint32_t words[8]) {
    for (int i = 0; i < 8; i++) {
        words[i] = load32_le(bytes + i * 4);
    }
}

static inline void words_to_bytes(const uint32_t words[8], uint8_t bytes[32]) {
    for (int i = 0; i < 8; i++) {
        store32_le(bytes + i * 4, words[i]);
    }
}

/* ============== Compression Function ============== */

static inline void g(uint32_t *state, int a, int b, int c, int d,
                     uint32_t mx, uint32_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = rotr32(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + my;
    state[d] = rotr32(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 7);
}

static inline void round_fn(uint32_t *state, const uint32_t *m, int round) {
    const uint8_t *s = MSG_SCHEDULE[round];
    g(state, 0, 4, 8, 12, m[s[0]], m[s[1]]);
    g(state, 1, 5, 9, 13, m[s[2]], m[s[3]]);
    g(state, 2, 6, 10, 14, m[s[4]], m[s[5]]);
    g(state, 3, 7, 11, 15, m[s[6]], m[s[7]]);
    g(state, 0, 5, 10, 15, m[s[8]], m[s[9]]);
    g(state, 1, 6, 11, 12, m[s[10]], m[s[11]]);
    g(state, 2, 7, 8, 13, m[s[12]], m[s[13]]);
    g(state, 3, 4, 9, 14, m[s[14]], m[s[15]]);
}

/*
 * Core compression function.
 * cv: 8-word chaining value
 * block: 64-byte block
 * block_len: actual bytes in block (for padding)
 * counter: block counter within chunk
 * flags: domain separation flags
 * out: 16-word output (first 8 for chaining, all 16 for root output)
 */
static void compress(const uint32_t cv[8], const uint8_t block[64],
                     uint32_t block_len, uint64_t counter, uint32_t flags,
                     uint32_t out[16]) {
    uint32_t state[16];
    uint32_t m[16];

    /* Initialize state */
    for (int i = 0; i < 8; i++) state[i] = cv[i];
    for (int i = 0; i < 4; i++) state[8 + i] = IV[i];
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32);
    state[14] = block_len;
    state[15] = flags;

    /* Load message */
    for (int i = 0; i < 16; i++) {
        m[i] = load32_le(block + i * 4);
    }

    /* 7 rounds */
    for (int r = 0; r < 7; r++) {
        round_fn(state, m, r);
    }

    /* Output: XOR halves */
    for (int i = 0; i < 8; i++) {
        out[i] = state[i] ^ state[i + 8];
    }
    for (int i = 8; i < 16; i++) {
        out[i] = state[i] ^ cv[i - 8];
    }
}

/* ============== Output Structure ============== */

/*
 * Represents the output of hashing - either a chaining value or final output.
 * This captures the state needed to produce arbitrary-length XOF output.
 */
typedef struct {
    uint32_t input_cv[8];
    uint8_t block[64];
    uint32_t block_len;
    uint64_t counter;
    uint32_t flags;
} output_t;

static void output_chaining_value(const output_t *o, uint32_t cv[8]) {
    uint32_t out[16];
    compress(o->input_cv, o->block, o->block_len, o->counter, o->flags, out);
    memcpy(cv, out, 8 * sizeof(uint32_t));
}

static void output_root_bytes(const output_t *o, uint64_t seek, uint8_t *out,
                              size_t out_len) {
    uint64_t output_block_counter = seek / 64;
    size_t offset_within_block = seek % 64;
    uint32_t wide[16];

    while (out_len > 0) {
        compress(o->input_cv, o->block, o->block_len, output_block_counter,
                 o->flags | ROOT, wide);

        size_t available = 64 - offset_within_block;
        size_t take = (available < out_len) ? available : out_len;

        for (size_t i = 0; i < take; i++) {
            size_t word_idx = (offset_within_block + i) / 4;
            size_t byte_idx = (offset_within_block + i) % 4;
            out[i] = (uint8_t)(wide[word_idx] >> (8 * byte_idx));
        }

        out += take;
        out_len -= take;
        output_block_counter++;
        offset_within_block = 0;
    }
}

/* ============== Chunk State ============== */

typedef struct {
    uint32_t cv[8];
    uint64_t chunk_counter;
    uint8_t buf[BLAKE4_BLOCK_LEN];
    uint8_t buf_len;
    uint8_t blocks_compressed;
    uint8_t flags;
} chunk_state_t;

static void chunk_state_init(chunk_state_t *s, const uint32_t key[8],
                             uint64_t chunk_counter, uint8_t flags) {
    memcpy(s->cv, key, 8 * sizeof(uint32_t));
    s->chunk_counter = chunk_counter;
    memset(s->buf, 0, sizeof(s->buf));
    s->buf_len = 0;
    s->blocks_compressed = 0;
    s->flags = flags;
}

static size_t chunk_state_len(const chunk_state_t *s) {
    return (size_t)s->blocks_compressed * BLAKE4_BLOCK_LEN + s->buf_len;
}

static uint32_t chunk_state_start_flag(const chunk_state_t *s) {
    return (s->blocks_compressed == 0) ? CHUNK_START : 0;
}

static void chunk_state_update(chunk_state_t *s, const uint8_t *input,
                               size_t input_len) {
    while (input_len > 0) {
        /* If buffer is full, compress it */
        if (s->buf_len == BLAKE4_BLOCK_LEN) {
            uint32_t block_flags = s->flags | chunk_state_start_flag(s);
            uint32_t out[16];
            compress(s->cv, s->buf, BLAKE4_BLOCK_LEN, s->chunk_counter,
                     block_flags, out);
            memcpy(s->cv, out, 8 * sizeof(uint32_t));
            s->blocks_compressed++;
            s->buf_len = 0;
            memset(s->buf, 0, sizeof(s->buf));
        }

        /* Fill buffer */
        size_t want = BLAKE4_BLOCK_LEN - s->buf_len;
        size_t take = (want < input_len) ? want : input_len;
        memcpy(s->buf + s->buf_len, input, take);
        s->buf_len += (uint8_t)take;
        input += take;
        input_len -= take;
    }
}

static output_t chunk_state_output(const chunk_state_t *s) {
    output_t o;
    memcpy(o.input_cv, s->cv, 8 * sizeof(uint32_t));
    memcpy(o.block, s->buf, sizeof(o.block));
    o.block_len = s->buf_len;
    o.counter = s->chunk_counter;
    o.flags = s->flags | chunk_state_start_flag(s) | CHUNK_END;
    return o;
}

/* ============== Parent Node ============== */

static output_t parent_output(const uint32_t left_cv[8],
                              const uint32_t right_cv[8],
                              const uint32_t key[8], uint8_t flags) {
    output_t o;
    memcpy(o.input_cv, key, 8 * sizeof(uint32_t));
    words_to_bytes(left_cv, o.block);
    words_to_bytes(right_cv, o.block + 32);
    o.block_len = BLAKE4_BLOCK_LEN;
    o.counter = 0;
    o.flags = flags | PARENT;
    return o;
}

static void parent_cv(const uint32_t left_cv[8], const uint32_t right_cv[8],
                      const uint32_t key[8], uint8_t flags, uint32_t out[8]) {
    output_t o = parent_output(left_cv, right_cv, key, flags);
    output_chaining_value(&o, out);
}

/* ============== Hasher Implementation ============== */

/*
 * The hasher maintains:
 * - key: key words for keyed/derive modes (or IV for standard)
 * - chunk_state: current chunk being processed
 * - cv_stack: stack of subtree chaining values
 * - cv_stack_len: number of CVs on stack
 */

void blake4_hasher_init(blake4_hasher *self) {
    memset(self, 0, sizeof(*self));
    memcpy(self->key_words, IV, 8 * sizeof(uint32_t));
    memcpy(self->cv, IV, 8 * sizeof(uint32_t));
    self->flags = 0;
    self->chunk_counter = 0;
    self->buf_len = 0;
    self->chunk_buf_len = 0;
    self->cv_stack_len = 0;
}

void blake4_hasher_init_keyed(blake4_hasher *self,
                              const uint8_t key[BLAKE4_KEY_LEN]) {
    memset(self, 0, sizeof(*self));
    words_from_bytes(key, self->key_words);
    memcpy(self->cv, self->key_words, 8 * sizeof(uint32_t));
    self->flags = KEYED_HASH;
    self->chunk_counter = 0;
    self->buf_len = 0;
    self->chunk_buf_len = 0;
    self->cv_stack_len = 0;
}

void blake4_hasher_init_derive_key(blake4_hasher *self, const char *context) {
    blake4_hasher_init_derive_key_raw(self, context, strlen(context));
}

void blake4_hasher_init_derive_key_raw(blake4_hasher *self,
                                       const void *context,
                                       size_t context_len) {
    /* Hash context to get key */
    blake4_hasher ctx_hasher;
    memset(&ctx_hasher, 0, sizeof(ctx_hasher));
    memcpy(ctx_hasher.key_words, IV, 8 * sizeof(uint32_t));
    memcpy(ctx_hasher.cv, IV, 8 * sizeof(uint32_t));
    ctx_hasher.flags = DERIVE_KEY_CONTEXT;

    blake4_hasher_update(&ctx_hasher, context, context_len);

    uint8_t context_key[32];
    blake4_hasher_finalize(&ctx_hasher, context_key, 32);

    /* Initialize with derived key */
    memset(self, 0, sizeof(*self));
    words_from_bytes(context_key, self->key_words);
    memcpy(self->cv, self->key_words, 8 * sizeof(uint32_t));
    self->flags = DERIVE_KEY_MATERIAL;
}

/*
 * Push a CV onto the stack, merging as needed based on chunk counter.
 */
static void hasher_push_cv(blake4_hasher *self, const uint32_t new_cv[8],
                           uint64_t chunk_counter) {
    /*
     * The stack merge logic: for each trailing 1 bit in chunk_counter,
     * we merge with the CV on top of the stack.
     */
    uint32_t cv[8];
    memcpy(cv, new_cv, 8 * sizeof(uint32_t));

    while ((chunk_counter & 1) == 1) {
        /* Pop and merge */
        self->cv_stack_len--;
        uint32_t left_cv[8];
        words_from_bytes(self->cv_stack[self->cv_stack_len], left_cv);
        parent_cv(left_cv, cv, self->key_words, self->flags, cv);
        chunk_counter >>= 1;
    }

    /* Push result */
    words_to_bytes(cv, self->cv_stack[self->cv_stack_len]);
    self->cv_stack_len++;
}

void blake4_hasher_update(blake4_hasher *self, const void *input,
                          size_t input_len) {
    const uint8_t *in = (const uint8_t *)input;

    /* Process input */
    while (input_len > 0) {
        /* If we have a full chunk, finalize it and push CV */
        if (self->chunk_buf_len == BLAKE4_CHUNK_LEN) {
            chunk_state_t chunk;
            chunk_state_init(&chunk, self->key_words, self->chunk_counter,
                             self->flags);
            chunk_state_update(&chunk, self->chunk_buf, BLAKE4_CHUNK_LEN);
            output_t o = chunk_state_output(&chunk);
            uint32_t chunk_cv[8];
            output_chaining_value(&o, chunk_cv);

            uint64_t total_chunks = self->chunk_counter + 1;
            hasher_push_cv(self, chunk_cv, total_chunks);
            self->chunk_counter = total_chunks;
            self->chunk_buf_len = 0;
        }

        /* Add to chunk buffer */
        size_t want = BLAKE4_CHUNK_LEN - self->chunk_buf_len;
        size_t take = (want < input_len) ? want : input_len;
        memcpy(self->chunk_buf + self->chunk_buf_len, in, take);
        self->chunk_buf_len += (uint16_t)take;
        in += take;
        input_len -= take;
    }
}

void blake4_hasher_finalize(const blake4_hasher *self, uint8_t *out,
                            size_t out_len) {
    blake4_hasher_finalize_seek(self, 0, out, out_len);
}

void blake4_hasher_finalize_seek(const blake4_hasher *self, uint64_t seek,
                                 uint8_t *out, size_t out_len) {
    /* Finalize current chunk */
    chunk_state_t chunk;
    chunk_state_init(&chunk, self->key_words, self->chunk_counter, self->flags);
    chunk_state_update(&chunk, self->chunk_buf, self->chunk_buf_len);
    output_t o = chunk_state_output(&chunk);

    /* If we have CVs on stack, we need to merge them */
    uint8_t cv_stack_len = self->cv_stack_len;
    if (cv_stack_len == 0) {
        /* Single chunk - output directly with ROOT flag */
        o.flags |= ROOT;
        output_root_bytes(&o, seek, out, out_len);
        return;
    }

    /* Multiple chunks - merge with stack */
    uint32_t cv[8];
    output_chaining_value(&o, cv);

    /* Copy stack (we need to modify it) */
    uint8_t stack[BLAKE4_MAX_DEPTH][32];
    memcpy(stack, self->cv_stack, cv_stack_len * 32);

    /* Merge from right to left */
    while (cv_stack_len > 0) {
        cv_stack_len--;
        uint32_t left_cv[8];
        words_from_bytes(stack[cv_stack_len], left_cv);
        if (cv_stack_len == 0) {
            /* Last merge - this is the root */
            o = parent_output(left_cv, cv, self->key_words, self->flags);
            o.flags |= ROOT;
            output_root_bytes(&o, seek, out, out_len);
            return;
        }
        parent_cv(left_cv, cv, self->key_words, self->flags, cv);
    }
}

void blake4_hasher_reset(blake4_hasher *self) {
    uint32_t key_words[8];
    uint8_t flags = self->flags;
    memcpy(key_words, self->key_words, sizeof(key_words));

    memset(self, 0, sizeof(*self));
    memcpy(self->key_words, key_words, sizeof(key_words));
    memcpy(self->cv, key_words, sizeof(self->cv));
    self->flags = flags;
}

/* ============== Convenience Functions ============== */

void blake4_hash(const void *input, size_t input_len,
                 uint8_t out[BLAKE4_OUT_LEN]) {
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, input, input_len);
    blake4_hasher_finalize(&h, out, BLAKE4_OUT_LEN);
}

void blake4_hash_xof(const void *input, size_t input_len, uint8_t *out,
                     size_t out_len) {
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, input, input_len);
    blake4_hasher_finalize(&h, out, out_len);
}

void blake4_hash_keyed(const uint8_t key[BLAKE4_KEY_LEN], const void *input,
                       size_t input_len, uint8_t out[BLAKE4_OUT_LEN]) {
    blake4_hasher h;
    blake4_hasher_init_keyed(&h, key);
    blake4_hasher_update(&h, input, input_len);
    blake4_hasher_finalize(&h, out, BLAKE4_OUT_LEN);
}

void blake4_derive_key(const char *context, const void *key_material,
                       size_t key_material_len, uint8_t *out, size_t out_len) {
    blake4_hasher h;
    blake4_hasher_init_derive_key(&h, context);
    blake4_hasher_update(&h, key_material, key_material_len);
    blake4_hasher_finalize(&h, out, out_len);
}

/* ============== Version ============== */

const char* blake4_version_string(void) {
    return BLAKE4_VERSION_STRING;
}

int blake4_version_major(void) { return BLAKE4_VERSION_MAJOR; }
int blake4_version_minor(void) { return BLAKE4_VERSION_MINOR; }
int blake4_version_patch(void) { return BLAKE4_VERSION_PATCH; }

/* ============== Serialization (TODO) ============== */

size_t blake4_hasher_state_size(const blake4_hasher *self) {
    (void)self;
    return 0;
}

size_t blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf,
                               size_t buf_len) {
    (void)self; (void)buf; (void)buf_len;
    return 0;
}

int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf,
                              size_t buf_len) {
    (void)self; (void)buf; (void)buf_len;
    return -1;
}
