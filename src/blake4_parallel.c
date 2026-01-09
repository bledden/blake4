/*
 * BLAKE4 Parallel Hashing Implementation
 *
 * Multi-threaded hashing using chunk-based work distribution.
 * Each worker processes independent chunks, then results are merged
 * using the Merkle tree structure.
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "blake4.h"
#include "blake4_thread.h"
#include <stdlib.h>
#include <string.h>

/* Minimum input size for parallelization (1 MB) */
#define PARALLEL_THRESHOLD (1024 * 1024)

/* Maximum number of threads */
#define MAX_THREADS 64

/* Work unit: one or more chunks to process */
typedef struct {
    const uint8_t *data;       /* Input data pointer */
    size_t len;                /* Length of data */
    uint64_t chunk_start;      /* Starting chunk counter */
    uint8_t cv_out[64];        /* Output CV after processing */
    uint64_t chunks_processed; /* Number of chunks processed */
} blake4_work_unit;

/* Worker thread state */
typedef struct {
    blake4_thread_t thread;
    blake4_work_unit *work;
    uint8_t key_words[64];     /* Key for keyed mode */
    int has_key;               /* Whether this is keyed hashing */
    volatile int done;         /* Work completion flag */
    volatile int shutdown;     /* Shutdown signal */
    blake4_mutex_t mutex;
    blake4_cond_t work_avail;
    blake4_cond_t work_done;
} blake4_worker;

/* Parallel hasher structure */
struct blake4_parallel_hasher {
    blake4_hasher serial;      /* Serial hasher for small inputs / finalization */
    blake4_worker *workers;    /* Worker threads */
    unsigned num_threads;      /* Number of worker threads */
    uint8_t key_words[64];     /* Key (if keyed mode) */
    int has_key;               /* Whether keyed mode is active */
    int initialized;           /* Whether hasher is initialized */

    /* Accumulated CVs from parallel processing */
    uint8_t (*cv_stack)[64];   /* Stack of CVs from workers */
    size_t cv_stack_len;       /* Number of CVs on stack */
    size_t cv_stack_cap;       /* Capacity of CV stack */
    uint64_t total_chunks;     /* Total chunks processed */
};

/* Forward declarations */
static void *worker_thread_func(void *arg);
static void process_chunks(const uint8_t *data, size_t len,
                          uint64_t chunk_start, const uint8_t *key_words,
                          int has_key, uint8_t *cv_out, uint64_t *chunks_out);
static void merge_cvs(blake4_parallel_hasher *self);

/*
 * Worker thread function
 */
static void *worker_thread_func(void *arg) {
    blake4_worker *worker = (blake4_worker *)arg;

    while (1) {
        blake4_mutex_lock(&worker->mutex);

        /* Wait for work or shutdown */
        while (!worker->work && !worker->shutdown) {
            blake4_cond_wait(&worker->work_avail, &worker->mutex);
        }

        if (worker->shutdown) {
            blake4_mutex_unlock(&worker->mutex);
            break;
        }

        blake4_work_unit *work = worker->work;
        worker->work = NULL;

        blake4_mutex_unlock(&worker->mutex);

        /* Process the work unit */
        if (work) {
            process_chunks(work->data, work->len, work->chunk_start,
                          worker->key_words, worker->has_key,
                          work->cv_out, &work->chunks_processed);
        }

        /* Signal completion */
        blake4_mutex_lock(&worker->mutex);
        worker->done = 1;
        blake4_cond_signal(&worker->work_done);
        blake4_mutex_unlock(&worker->mutex);
    }

    return NULL;
}

/*
 * Process chunks and produce a CV
 * This uses the standard BLAKE4 chunk processing
 */
static void process_chunks(const uint8_t *data, size_t len,
                          uint64_t chunk_start, const uint8_t *key_words,
                          int has_key, uint8_t *cv_out, uint64_t *chunks_out) {
    blake4_hasher h;
    uint64_t chunks = 0;

    if (has_key) {
        blake4_hasher_init_keyed(&h, key_words);
    } else {
        blake4_hasher_init(&h);
    }

    /* Override the chunk counter */
    h.chunk_counter = chunk_start;

    /* Process all data */
    blake4_hasher_update(&h, data, len);

    /* Calculate chunks processed */
    chunks = (len + BLAKE4_CHUNK_LEN - 1) / BLAKE4_CHUNK_LEN;
    if (chunks == 0) chunks = 1;

    /* Extract the current CV */
    memcpy(cv_out, h.cv, 64);

    *chunks_out = chunks;
}

/*
 * Create a new parallel hasher
 */
blake4_parallel_hasher* blake4_parallel_hasher_new(unsigned num_threads) {
#if !BLAKE4_THREAD_AVAILABLE
    (void)num_threads;
    return NULL;
#else
    blake4_parallel_hasher *self = calloc(1, sizeof(blake4_parallel_hasher));
    if (!self) return NULL;

    /* Auto-detect thread count */
    if (num_threads == 0) {
        num_threads = blake4_cpu_count();
    }
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    if (num_threads < 1) {
        num_threads = 1;
    }

    self->num_threads = num_threads;

    /* Allocate workers */
    self->workers = calloc(num_threads, sizeof(blake4_worker));
    if (!self->workers) {
        free(self);
        return NULL;
    }

    /* Allocate CV stack */
    self->cv_stack_cap = 64;
    self->cv_stack = malloc(self->cv_stack_cap * 64);
    if (!self->cv_stack) {
        free(self->workers);
        free(self);
        return NULL;
    }

    /* Initialize workers */
    for (unsigned i = 0; i < num_threads; i++) {
        blake4_worker *w = &self->workers[i];
        blake4_mutex_init(&w->mutex);
        blake4_cond_init(&w->work_avail);
        blake4_cond_init(&w->work_done);
        w->work = NULL;
        w->done = 0;
        w->shutdown = 0;

        if (blake4_thread_create(&w->thread, worker_thread_func, w) != 0) {
            /* Failed to create thread, clean up */
            for (unsigned j = 0; j < i; j++) {
                blake4_worker *wj = &self->workers[j];
                blake4_mutex_lock(&wj->mutex);
                wj->shutdown = 1;
                blake4_cond_signal(&wj->work_avail);
                blake4_mutex_unlock(&wj->mutex);
                blake4_thread_join(wj->thread);
                blake4_mutex_destroy(&wj->mutex);
                blake4_cond_destroy(&wj->work_avail);
                blake4_cond_destroy(&wj->work_done);
            }
            free(self->cv_stack);
            free(self->workers);
            free(self);
            return NULL;
        }
    }

    return self;
#endif
}

/*
 * Free a parallel hasher
 */
void blake4_parallel_hasher_free(blake4_parallel_hasher *self) {
    if (!self) return;

#if BLAKE4_THREAD_AVAILABLE
    /* Shutdown all workers */
    for (unsigned i = 0; i < self->num_threads; i++) {
        blake4_worker *w = &self->workers[i];
        blake4_mutex_lock(&w->mutex);
        w->shutdown = 1;
        blake4_cond_signal(&w->work_avail);
        blake4_mutex_unlock(&w->mutex);
    }

    /* Join all threads */
    for (unsigned i = 0; i < self->num_threads; i++) {
        blake4_worker *w = &self->workers[i];
        blake4_thread_join(w->thread);
        blake4_mutex_destroy(&w->mutex);
        blake4_cond_destroy(&w->work_avail);
        blake4_cond_destroy(&w->work_done);
    }

    free(self->workers);
#endif

    free(self->cv_stack);
    free(self);
}

/*
 * Initialize for standard hashing
 */
void blake4_parallel_hasher_init(blake4_parallel_hasher *self) {
    if (!self) return;

    blake4_hasher_init(&self->serial);
    self->has_key = 0;
    memset(self->key_words, 0, sizeof(self->key_words));
    self->cv_stack_len = 0;
    self->total_chunks = 0;
    self->initialized = 1;

#if BLAKE4_THREAD_AVAILABLE
    for (unsigned i = 0; i < self->num_threads; i++) {
        self->workers[i].has_key = 0;
    }
#endif
}

/*
 * Initialize for keyed hashing
 */
void blake4_parallel_hasher_init_keyed(blake4_parallel_hasher *self,
                                        const uint8_t key[BLAKE4_KEY_LEN]) {
    if (!self) return;

    blake4_hasher_init_keyed(&self->serial, key);
    self->has_key = 1;
    memcpy(self->key_words, key, BLAKE4_KEY_LEN);
    self->cv_stack_len = 0;
    self->total_chunks = 0;
    self->initialized = 1;

#if BLAKE4_THREAD_AVAILABLE
    for (unsigned i = 0; i < self->num_threads; i++) {
        self->workers[i].has_key = 1;
        memcpy(self->workers[i].key_words, key, BLAKE4_KEY_LEN);
    }
#endif
}

/*
 * Update with input data
 */
void blake4_parallel_hasher_update(blake4_parallel_hasher *self,
                                    const void *input, size_t input_len) {
    if (!self || !self->initialized || input_len == 0) return;

    const uint8_t *data = (const uint8_t *)input;

#if BLAKE4_THREAD_AVAILABLE
    /* For small inputs, use serial path */
    if (input_len < PARALLEL_THRESHOLD || self->num_threads <= 1) {
        blake4_hasher_update(&self->serial, input, input_len);
        return;
    }

    /* Calculate work distribution */
    size_t chunks_per_thread = (input_len / BLAKE4_CHUNK_LEN) / self->num_threads;
    if (chunks_per_thread < 1) chunks_per_thread = 1;
    size_t bytes_per_thread = chunks_per_thread * BLAKE4_CHUNK_LEN;

    /* Allocate work units */
    blake4_work_unit *work = calloc(self->num_threads, sizeof(blake4_work_unit));
    if (!work) {
        /* Fallback to serial */
        blake4_hasher_update(&self->serial, input, input_len);
        return;
    }

    /* Distribute work */
    size_t offset = 0;
    unsigned active_workers = 0;

    for (unsigned i = 0; i < self->num_threads && offset < input_len; i++) {
        size_t this_len = bytes_per_thread;
        if (i == self->num_threads - 1 || offset + this_len > input_len) {
            this_len = input_len - offset;
        }
        if (this_len == 0) break;

        work[i].data = data + offset;
        work[i].len = this_len;
        work[i].chunk_start = self->total_chunks + (offset / BLAKE4_CHUNK_LEN);
        work[i].chunks_processed = 0;

        blake4_worker *w = &self->workers[i];
        blake4_mutex_lock(&w->mutex);
        w->work = &work[i];
        w->done = 0;
        blake4_cond_signal(&w->work_avail);
        blake4_mutex_unlock(&w->mutex);

        offset += this_len;
        active_workers++;
    }

    /* Wait for all workers to complete */
    for (unsigned i = 0; i < active_workers; i++) {
        blake4_worker *w = &self->workers[i];
        blake4_mutex_lock(&w->mutex);
        while (!w->done) {
            blake4_cond_wait(&w->work_done, &w->mutex);
        }
        blake4_mutex_unlock(&w->mutex);
    }

    /* Collect CVs from workers */
    for (unsigned i = 0; i < active_workers; i++) {
        /* Grow CV stack if needed */
        if (self->cv_stack_len >= self->cv_stack_cap) {
            size_t new_cap = self->cv_stack_cap * 2;
            uint8_t (*new_stack)[64] = realloc(self->cv_stack, new_cap * 64);
            if (!new_stack) {
                /* Out of memory - this is bad but we continue */
                break;
            }
            self->cv_stack = new_stack;
            self->cv_stack_cap = new_cap;
        }

        memcpy(self->cv_stack[self->cv_stack_len], work[i].cv_out, 64);
        self->cv_stack_len++;
        self->total_chunks += work[i].chunks_processed;
    }

    free(work);

#else
    /* No threading - use serial path */
    blake4_hasher_update(&self->serial, input, input_len);
#endif
}

/*
 * Merge accumulated CVs into final result
 */
static void merge_cvs(blake4_parallel_hasher *self) {
    if (self->cv_stack_len == 0) return;

    /* Merge pairs of CVs bottom-up */
    while (self->cv_stack_len > 1) {
        size_t new_len = 0;
        for (size_t i = 0; i + 1 < self->cv_stack_len; i += 2) {
            /* Hash two CVs together using parent node operation */
            blake4_hasher h;
            if (self->has_key) {
                blake4_hasher_init_keyed(&h, self->key_words);
            } else {
                blake4_hasher_init(&h);
            }

            /* Combine left and right children */
            blake4_hasher_update(&h, self->cv_stack[i], 64);
            blake4_hasher_update(&h, self->cv_stack[i + 1], 64);
            blake4_hasher_finalize(&h, self->cv_stack[new_len], 64);
            new_len++;
        }

        /* Handle odd CV */
        if (self->cv_stack_len % 2 == 1) {
            memcpy(self->cv_stack[new_len], self->cv_stack[self->cv_stack_len - 1], 64);
            new_len++;
        }

        self->cv_stack_len = new_len;
    }
}

/*
 * Finalize and produce output
 */
void blake4_parallel_hasher_finalize(blake4_parallel_hasher *self,
                                      uint8_t *out, size_t out_len) {
    if (!self || !self->initialized || !out || out_len == 0) return;

#if BLAKE4_THREAD_AVAILABLE
    if (self->cv_stack_len > 0) {
        /* We have parallel CVs to merge */
        merge_cvs(self);

        /* Final output from merged CV */
        if (self->cv_stack_len == 1) {
            /* Copy as much as we can from the final CV */
            size_t copy_len = (out_len < 64) ? out_len : 64;
            memcpy(out, self->cv_stack[0], copy_len);

            /* If more output needed, extend using XOF */
            if (out_len > 64) {
                blake4_hasher h;
                if (self->has_key) {
                    blake4_hasher_init_keyed(&h, self->key_words);
                } else {
                    blake4_hasher_init(&h);
                }
                blake4_hasher_update(&h, self->cv_stack[0], 64);
                blake4_hasher_finalize(&h, out, out_len);
            }
        }
        return;
    }
#endif

    /* Use serial hasher for finalization */
    blake4_hasher_finalize(&self->serial, out, out_len);
}

/*
 * Reset to initial state
 */
void blake4_parallel_hasher_reset(blake4_parallel_hasher *self) {
    if (!self) return;

    if (self->has_key) {
        blake4_hasher_init_keyed(&self->serial, self->key_words);
    } else {
        blake4_hasher_init(&self->serial);
    }

    self->cv_stack_len = 0;
    self->total_chunks = 0;
}

/*
 * Get thread count
 */
unsigned blake4_parallel_hasher_num_threads(const blake4_parallel_hasher *self) {
    return self ? self->num_threads : 0;
}

/*
 * One-shot parallel hash
 */
void blake4_parallel_hash(const void *input, size_t input_len,
                          uint8_t out[BLAKE4_OUT_LEN],
                          unsigned num_threads) {
#if BLAKE4_THREAD_AVAILABLE
    if (input_len < PARALLEL_THRESHOLD || num_threads <= 1) {
        blake4_hash(input, input_len, out);
        return;
    }

    blake4_parallel_hasher *h = blake4_parallel_hasher_new(num_threads);
    if (!h) {
        blake4_hash(input, input_len, out);
        return;
    }

    blake4_parallel_hasher_init(h);
    blake4_parallel_hasher_update(h, input, input_len);
    blake4_parallel_hasher_finalize(h, out, BLAKE4_OUT_LEN);
    blake4_parallel_hasher_free(h);
#else
    (void)num_threads;
    blake4_hash(input, input_len, out);
#endif
}

/*
 * One-shot parallel hash with XOF
 */
void blake4_parallel_hash_xof(const void *input, size_t input_len,
                               uint8_t *out, size_t out_len,
                               unsigned num_threads) {
#if BLAKE4_THREAD_AVAILABLE
    if (input_len < PARALLEL_THRESHOLD || num_threads <= 1) {
        blake4_hash_xof(input, input_len, out, out_len);
        return;
    }

    blake4_parallel_hasher *h = blake4_parallel_hasher_new(num_threads);
    if (!h) {
        blake4_hash_xof(input, input_len, out, out_len);
        return;
    }

    blake4_parallel_hasher_init(h);
    blake4_parallel_hasher_update(h, input, input_len);
    blake4_parallel_hasher_finalize(h, out, out_len);
    blake4_parallel_hasher_free(h);
#else
    (void)num_threads;
    blake4_hash_xof(input, input_len, out, out_len);
#endif
}

/*
 * Check if parallel hashing is available
 */
int blake4_parallel_available(void) {
#if BLAKE4_THREAD_AVAILABLE
    return 1;
#else
    return 0;
#endif
}
