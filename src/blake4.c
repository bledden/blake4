/*
 * BLAKE4 Cryptographic Hash Function
 * 512-bit internal state implementation
 *
 * Key parameters:
 * - 64-bit words
 * - 512-bit internal state
 * - 128-byte blocks
 * - 2048-byte chunks
 * - 10 rounds
 * - 64-byte default output
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include <string.h>

/* ============== Internal Constants ============== */

/* IV: Same as SHA-512 (fractional parts of sqrt of first 8 primes) */
static const uint64_t IV[8] = {
    0x6A09E667F3BCC908ULL,
    0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL,
    0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL,
    0x5BE0CD19137E2179ULL
};

/* Message schedule permutations (BLAKE2b style, 10 rounds) */
static const uint8_t SIGMA[10][16] = {
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15},
    {14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3},
    {11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4},
    { 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8},
    { 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13},
    { 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9},
    {12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11},
    {13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10},
    { 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5},
    {10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0},
};

/* Domain separation flags */
#define CHUNK_START         (1 << 0)
#define CHUNK_END           (1 << 1)
#define PARENT              (1 << 2)
#define ROOT                (1 << 3)
#define KEYED_HASH          (1 << 4)
#define DERIVE_KEY_CONTEXT  (1 << 5)
#define DERIVE_KEY_MATERIAL (1 << 6)

/* Rotation constants for 64-bit words (same as BLAKE2b) */
#define R1 32
#define R2 24
#define R3 16
#define R4 63

/* ============== Utility Functions ============== */

static inline uint64_t rotr64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

static inline uint64_t load64_le(const uint8_t *p) {
    return ((uint64_t)p[0]) |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline void store64_le(uint8_t *p, uint64_t x) {
    p[0] = (uint8_t)(x);
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
    p[4] = (uint8_t)(x >> 32);
    p[5] = (uint8_t)(x >> 40);
    p[6] = (uint8_t)(x >> 48);
    p[7] = (uint8_t)(x >> 56);
}

static inline void words_from_bytes(const uint8_t bytes[64], uint64_t words[8]) {
    for (int i = 0; i < 8; i++) {
        words[i] = load64_le(bytes + i * 8);
    }
}

static inline void words_to_bytes(const uint64_t words[8], uint8_t bytes[64]) {
    for (int i = 0; i < 8; i++) {
        store64_le(bytes + i * 8, words[i]);
    }
}

/* ============== Compression Function ============== */

static inline void g(uint64_t *state, int a, int b, int c, int d,
                     uint64_t mx, uint64_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = rotr64(state[d] ^ state[a], R1);
    state[c] = state[c] + state[d];
    state[b] = rotr64(state[b] ^ state[c], R2);
    state[a] = state[a] + state[b] + my;
    state[d] = rotr64(state[d] ^ state[a], R3);
    state[c] = state[c] + state[d];
    state[b] = rotr64(state[b] ^ state[c], R4);
}

static inline void round_fn(uint64_t *state, const uint64_t *m, int round) {
    const uint8_t *s = SIGMA[round];

    /* Column step */
    g(state, 0, 4,  8, 12, m[s[ 0]], m[s[ 1]]);
    g(state, 1, 5,  9, 13, m[s[ 2]], m[s[ 3]]);
    g(state, 2, 6, 10, 14, m[s[ 4]], m[s[ 5]]);
    g(state, 3, 7, 11, 15, m[s[ 6]], m[s[ 7]]);

    /* Diagonal step */
    g(state, 0, 5, 10, 15, m[s[ 8]], m[s[ 9]]);
    g(state, 1, 6, 11, 12, m[s[10]], m[s[11]]);
    g(state, 2, 7,  8, 13, m[s[12]], m[s[13]]);
    g(state, 3, 4,  9, 14, m[s[14]], m[s[15]]);
}

/*
 * Core compression function.
 * cv: 8-word (512-bit) chaining value
 * block: 128-byte block
 * block_len: actual bytes in block
 * counter: block counter within chunk
 * flags: domain separation flags
 * out: 16-word output (first 8 for chaining, all 16 for root)
 */
static void compress(const uint64_t cv[8], const uint8_t block[128],
                     uint32_t block_len, uint64_t counter, uint32_t flags,
                     uint64_t out[16]) {
    uint64_t state[16];
    uint64_t m[16];

    /* Initialize state */
    for (int i = 0; i < 8; i++) state[i] = cv[i];
    state[8]  = IV[0];
    state[9]  = IV[1];
    state[10] = IV[2];
    state[11] = IV[3];
    state[12] = IV[4] ^ counter;
    state[13] = IV[5];  /* High bits of counter if needed */
    state[14] = IV[6] ^ block_len;
    state[15] = IV[7] ^ flags;

    /* Load message (16 x 64-bit words from 128 bytes) */
    for (int i = 0; i < 16; i++) {
        m[i] = load64_le(block + i * 8);
    }

    /* 10 rounds */
    for (int r = 0; r < 10; r++) {
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

typedef struct {
    uint64_t input_cv[8];
    uint8_t block[128];
    uint32_t block_len;
    uint64_t counter;
    uint32_t flags;
} output_t;

static void output_chaining_value(const output_t *o, uint64_t cv[8]) {
    uint64_t out[16];
    compress(o->input_cv, o->block, o->block_len, o->counter, o->flags, out);
    memcpy(cv, out, 8 * sizeof(uint64_t));
}

static void output_root_bytes(const output_t *o, uint64_t seek, uint8_t *out,
                              size_t out_len) {
    uint64_t output_block_counter = seek / 128;
    size_t offset_within_block = seek % 128;
    uint64_t wide[16];

    while (out_len > 0) {
        compress(o->input_cv, o->block, o->block_len, output_block_counter,
                 o->flags | ROOT, wide);

        size_t available = 128 - offset_within_block;
        size_t take = (available < out_len) ? available : out_len;

        for (size_t i = 0; i < take; i++) {
            size_t word_idx = (offset_within_block + i) / 8;
            size_t byte_idx = (offset_within_block + i) % 8;
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
    uint64_t cv[8];
    uint64_t chunk_counter;
    uint8_t buf[BLAKE4_BLOCK_LEN];
    uint8_t buf_len;
    uint8_t blocks_compressed;
    uint8_t flags;
} chunk_state_t;

static void chunk_state_init(chunk_state_t *s, const uint64_t key[8],
                             uint64_t chunk_counter, uint8_t flags) {
    memcpy(s->cv, key, 8 * sizeof(uint64_t));
    s->chunk_counter = chunk_counter;
    memset(s->buf, 0, sizeof(s->buf));
    s->buf_len = 0;
    s->blocks_compressed = 0;
    s->flags = flags;
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
            uint64_t out[16];
            compress(s->cv, s->buf, BLAKE4_BLOCK_LEN, s->chunk_counter,
                     block_flags, out);
            memcpy(s->cv, out, 8 * sizeof(uint64_t));
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
    memcpy(o.input_cv, s->cv, 8 * sizeof(uint64_t));
    memcpy(o.block, s->buf, sizeof(o.block));
    o.block_len = s->buf_len;
    o.counter = s->chunk_counter;
    o.flags = s->flags | chunk_state_start_flag(s) | CHUNK_END;
    return o;
}

/* ============== Parent Node ============== */

static output_t parent_output(const uint64_t left_cv[8],
                              const uint64_t right_cv[8],
                              const uint64_t key[8], uint8_t flags) {
    output_t o;
    memcpy(o.input_cv, key, 8 * sizeof(uint64_t));
    words_to_bytes(left_cv, o.block);
    words_to_bytes(right_cv, o.block + 64);
    o.block_len = BLAKE4_BLOCK_LEN;
    o.counter = 0;
    o.flags = flags | PARENT;
    return o;
}

static void parent_cv(const uint64_t left_cv[8], const uint64_t right_cv[8],
                      const uint64_t key[8], uint8_t flags, uint64_t out[8]) {
    output_t o = parent_output(left_cv, right_cv, key, flags);
    output_chaining_value(&o, out);
}

/* ============== Hasher Implementation ============== */

void blake4_hasher_init(blake4_hasher *self) {
    memset(self, 0, sizeof(*self));
    memcpy(self->key_words, IV, 8 * sizeof(uint64_t));
    memcpy(self->cv, IV, 8 * sizeof(uint64_t));
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
    memcpy(self->cv, self->key_words, 8 * sizeof(uint64_t));
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
    memcpy(ctx_hasher.key_words, IV, 8 * sizeof(uint64_t));
    memcpy(ctx_hasher.cv, IV, 8 * sizeof(uint64_t));
    ctx_hasher.flags = DERIVE_KEY_CONTEXT;

    blake4_hasher_update(&ctx_hasher, context, context_len);

    uint8_t context_key[64];
    blake4_hasher_finalize(&ctx_hasher, context_key, 64);

    /* Initialize with derived key */
    memset(self, 0, sizeof(*self));
    words_from_bytes(context_key, self->key_words);
    memcpy(self->cv, self->key_words, 8 * sizeof(uint64_t));
    self->flags = DERIVE_KEY_MATERIAL;
}

/*
 * Push a CV onto the stack, merging as needed based on total chunk count.
 */
static void hasher_push_cv(blake4_hasher *self, const uint64_t new_cv[8],
                           uint64_t total_chunks) {
    uint64_t cv[8];
    memcpy(cv, new_cv, 8 * sizeof(uint64_t));

    /* Merge while total_chunks is even (trailing bit is 0) */
    while ((total_chunks & 1) == 0) {
        /* Pop and merge */
        self->cv_stack_len--;
        uint64_t left_cv[8];
        words_from_bytes(self->cv_stack[self->cv_stack_len], left_cv);
        parent_cv(left_cv, cv, self->key_words, self->flags, cv);
        total_chunks >>= 1;
    }

    /* Push result */
    words_to_bytes(cv, self->cv_stack[self->cv_stack_len]);
    self->cv_stack_len++;
}

void blake4_hasher_update(blake4_hasher *self, const void *input,
                          size_t input_len) {
    const uint8_t *in = (const uint8_t *)input;

    while (input_len > 0) {
        /* If we have a full chunk, finalize it and push CV */
        if (self->chunk_buf_len == BLAKE4_CHUNK_LEN) {
            chunk_state_t chunk;
            chunk_state_init(&chunk, self->key_words, self->chunk_counter,
                             self->flags);
            chunk_state_update(&chunk, self->chunk_buf, BLAKE4_CHUNK_LEN);
            output_t o = chunk_state_output(&chunk);
            uint64_t chunk_cv[8];
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
    uint64_t cv[8];
    output_chaining_value(&o, cv);

    /* Copy stack (we need to modify it) */
    uint8_t stack[BLAKE4_MAX_DEPTH][64];
    memcpy(stack, self->cv_stack, cv_stack_len * 64);

    /* Merge from right to left */
    while (cv_stack_len > 0) {
        cv_stack_len--;
        uint64_t left_cv[8];
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
    uint64_t key_words[8];
    uint8_t flags = self->flags;
    memcpy(key_words, self->key_words, sizeof(key_words));

    memset(self, 0, sizeof(*self));
    memcpy(self->key_words, key_words, sizeof(key_words));
    memcpy(self->cv, key_words, sizeof(self->cv));
    self->flags = flags;
}

/* ============== State Serialization ============== */

/*
 * Serialization format (little-endian):
 *   [8 bytes: magic "BLAKE4S\0"]
 *   [64 bytes: cv (8 x uint64_t)]
 *   [64 bytes: key_words (8 x uint64_t)]
 *   [8 bytes: chunk_counter (uint64_t)]
 *   [128 bytes: buf]
 *   [1 byte: buf_len]
 *   [2048 bytes: chunk_buf]
 *   [2 bytes: chunk_buf_len (uint16_t)]
 *   [54*64 bytes: cv_stack]
 *   [1 byte: cv_stack_len]
 *   [1 byte: flags]
 *   Total: 5780 bytes
 */

static const uint8_t SERIALIZE_MAGIC[8] = {'B', 'L', 'A', 'K', 'E', '4', 'S', '\0'};

size_t blake4_hasher_serialize(const blake4_hasher *self, uint8_t *buf,
                               size_t buf_len) {
    if (!self || !buf || buf_len < BLAKE4_SERIALIZED_SIZE) {
        return 0;
    }

    size_t pos = 0;

    /* Magic */
    memcpy(buf + pos, SERIALIZE_MAGIC, 8);
    pos += 8;

    /* cv: 8 x 64-bit words */
    for (int i = 0; i < 8; i++) {
        store64_le(buf + pos, self->cv[i]);
        pos += 8;
    }

    /* key_words: 8 x 64-bit words */
    for (int i = 0; i < 8; i++) {
        store64_le(buf + pos, self->key_words[i]);
        pos += 8;
    }

    /* chunk_counter */
    store64_le(buf + pos, self->chunk_counter);
    pos += 8;

    /* buf */
    memcpy(buf + pos, self->buf, BLAKE4_BLOCK_LEN);
    pos += BLAKE4_BLOCK_LEN;

    /* buf_len */
    buf[pos++] = self->buf_len;

    /* chunk_buf */
    memcpy(buf + pos, self->chunk_buf, BLAKE4_CHUNK_LEN);
    pos += BLAKE4_CHUNK_LEN;

    /* chunk_buf_len */
    buf[pos++] = (uint8_t)(self->chunk_buf_len & 0xFF);
    buf[pos++] = (uint8_t)((self->chunk_buf_len >> 8) & 0xFF);

    /* cv_stack */
    memcpy(buf + pos, self->cv_stack, BLAKE4_MAX_DEPTH * 64);
    pos += BLAKE4_MAX_DEPTH * 64;

    /* cv_stack_len */
    buf[pos++] = self->cv_stack_len;

    /* flags */
    buf[pos++] = self->flags;

    return pos;
}

int blake4_hasher_deserialize(blake4_hasher *self, const uint8_t *buf,
                              size_t buf_len) {
    if (!self || !buf || buf_len < BLAKE4_SERIALIZED_SIZE) {
        return 0;
    }

    size_t pos = 0;

    /* Verify magic */
    if (memcmp(buf + pos, SERIALIZE_MAGIC, 8) != 0) {
        return 0;
    }
    pos += 8;

    /* cv: 8 x 64-bit words */
    for (int i = 0; i < 8; i++) {
        self->cv[i] = load64_le(buf + pos);
        pos += 8;
    }

    /* key_words: 8 x 64-bit words */
    for (int i = 0; i < 8; i++) {
        self->key_words[i] = load64_le(buf + pos);
        pos += 8;
    }

    /* chunk_counter */
    self->chunk_counter = load64_le(buf + pos);
    pos += 8;

    /* buf */
    memcpy(self->buf, buf + pos, BLAKE4_BLOCK_LEN);
    pos += BLAKE4_BLOCK_LEN;

    /* buf_len */
    self->buf_len = buf[pos++];
    if (self->buf_len > BLAKE4_BLOCK_LEN) {
        return 0;  /* Invalid buf_len */
    }

    /* chunk_buf */
    memcpy(self->chunk_buf, buf + pos, BLAKE4_CHUNK_LEN);
    pos += BLAKE4_CHUNK_LEN;

    /* chunk_buf_len */
    self->chunk_buf_len = (uint16_t)buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    pos += 2;
    if (self->chunk_buf_len > BLAKE4_CHUNK_LEN) {
        return 0;  /* Invalid chunk_buf_len */
    }

    /* cv_stack */
    memcpy(self->cv_stack, buf + pos, BLAKE4_MAX_DEPTH * 64);
    pos += BLAKE4_MAX_DEPTH * 64;

    /* cv_stack_len */
    self->cv_stack_len = buf[pos++];
    if (self->cv_stack_len > BLAKE4_MAX_DEPTH) {
        return 0;  /* Invalid cv_stack_len */
    }

    /* flags */
    self->flags = buf[pos++];

    return 1;
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

/* ============== Hash-Based Signature Support ============== */

/*
 * Domain separation context for HBS functions.
 * Each function uses a unique context prefix to ensure domain separation.
 */
#define HBS_CONTEXT_PRF     "BLAKE4-HBS-PRF"
#define HBS_CONTEXT_PRF_MSG "BLAKE4-HBS-PRFmsg"
#define HBS_CONTEXT_H       "BLAKE4-HBS-H"
#define HBS_CONTEXT_F       "BLAKE4-HBS-F"
#define HBS_CONTEXT_T       "BLAKE4-HBS-T"
#define HBS_CONTEXT_H_MSG   "BLAKE4-HBS-Hmsg"

void blake4_hbs_prf(const uint8_t key[BLAKE4_KEY_LEN],
                    const uint8_t addr[32],
                    uint8_t *out, size_t out_len) {
    /*
     * PRF(SK.PRF, ADRS) = BLAKE4-KDF(HBS_CONTEXT_PRF, SK.PRF || ADRS)
     * Using keyed mode with the secret key, then hashing the address.
     */
    blake4_hasher h;
    blake4_hasher_init_keyed(&h, key);
    blake4_hasher_update(&h, HBS_CONTEXT_PRF, sizeof(HBS_CONTEXT_PRF) - 1);
    blake4_hasher_update(&h, addr, 32);
    blake4_hasher_finalize(&h, out, out_len);
}

void blake4_hbs_prf_msg(const uint8_t key[BLAKE4_KEY_LEN],
                        const uint8_t opt_rand[BLAKE4_OUT_LEN],
                        const void *message, size_t message_len,
                        uint8_t *out, size_t out_len) {
    /*
     * PRFmsg(SK.PRF, OptRand, M) = BLAKE4-keyed(SK.PRF, context || OptRand || M)
     */
    blake4_hasher h;
    blake4_hasher_init_keyed(&h, key);
    blake4_hasher_update(&h, HBS_CONTEXT_PRF_MSG, sizeof(HBS_CONTEXT_PRF_MSG) - 1);
    blake4_hasher_update(&h, opt_rand, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, message, message_len);
    blake4_hasher_finalize(&h, out, out_len);
}

void blake4_hbs_h(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t left[BLAKE4_OUT_LEN],
                  const uint8_t right[BLAKE4_OUT_LEN],
                  uint8_t out[BLAKE4_OUT_LEN]) {
    /*
     * H(PK.seed, ADRS, M1 || M2) = BLAKE4(context || PK.seed || ADRS || M1 || M2)
     * The public seed provides domain separation for the hash tree.
     */
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, HBS_CONTEXT_H, sizeof(HBS_CONTEXT_H) - 1);
    blake4_hasher_update(&h, pub_seed, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, addr, 32);
    blake4_hasher_update(&h, left, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, right, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h, out, BLAKE4_OUT_LEN);
}

void blake4_hbs_f(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t input[BLAKE4_OUT_LEN],
                  uint8_t out[BLAKE4_OUT_LEN]) {
    /*
     * F(PK.seed, ADRS, M) = BLAKE4(context || PK.seed || ADRS || M)
     * Used for WOTS+ chain function.
     */
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, HBS_CONTEXT_F, sizeof(HBS_CONTEXT_F) - 1);
    blake4_hasher_update(&h, pub_seed, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, addr, 32);
    blake4_hasher_update(&h, input, BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h, out, BLAKE4_OUT_LEN);
}

void blake4_hbs_t(const uint8_t pub_seed[BLAKE4_OUT_LEN],
                  const uint8_t addr[32],
                  const uint8_t *input, size_t num_blocks,
                  uint8_t out[BLAKE4_OUT_LEN]) {
    /*
     * T_l(PK.seed, ADRS, M) = BLAKE4(context || PK.seed || ADRS || M)
     * Tweakable hash for l input blocks.
     */
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, HBS_CONTEXT_T, sizeof(HBS_CONTEXT_T) - 1);
    blake4_hasher_update(&h, pub_seed, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, addr, 32);
    blake4_hasher_update(&h, input, num_blocks * BLAKE4_OUT_LEN);
    blake4_hasher_finalize(&h, out, BLAKE4_OUT_LEN);
}

void blake4_hbs_h_msg(const uint8_t r[BLAKE4_OUT_LEN],
                      const uint8_t pub_seed[BLAKE4_OUT_LEN],
                      const uint8_t pub_root[BLAKE4_OUT_LEN],
                      const void *message, size_t message_len,
                      uint8_t *out, size_t out_len) {
    /*
     * H_msg(R, PK.seed, PK.root, M) = BLAKE4(context || R || PK.seed || PK.root || M)
     * Message hash for SPHINCS+.
     */
    blake4_hasher h;
    blake4_hasher_init(&h);
    blake4_hasher_update(&h, HBS_CONTEXT_H_MSG, sizeof(HBS_CONTEXT_H_MSG) - 1);
    blake4_hasher_update(&h, r, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, pub_seed, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, pub_root, BLAKE4_OUT_LEN);
    blake4_hasher_update(&h, message, message_len);
    blake4_hasher_finalize(&h, out, out_len);
}

/* ============== Configurable Output Modes ============== */

void blake4_256_hash(const void *input, size_t input_len, uint8_t out[32]) {
    uint8_t full[BLAKE4_OUT_LEN];
    blake4_hash(input, input_len, full);
    memcpy(out, full, 32);
}

void blake4_384_hash(const void *input, size_t input_len, uint8_t out[48]) {
    uint8_t full[BLAKE4_OUT_LEN];
    blake4_hash(input, input_len, full);
    memcpy(out, full, 48);
}

void blake4_512_hash(const void *input, size_t input_len, uint8_t out[64]) {
    blake4_hash(input, input_len, out);
}

/* ============== Version ============== */

const char* blake4_version_string(void) {
    return BLAKE4_VERSION_STRING;
}

int blake4_version_major(void) { return BLAKE4_VERSION_MAJOR; }
int blake4_version_minor(void) { return BLAKE4_VERSION_MINOR; }
int blake4_version_patch(void) { return BLAKE4_VERSION_PATCH; }
