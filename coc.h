#ifndef COC_H_
#define COC_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

typedef enum Coc_Log_Level {
    COC_NONE,
    COC_DEBUG,
    COC_INFO,
    COC_WARNING,
    COC_ERROR,
    COC_FATAL
} Coc_Log_Level;

#define COC_LOG_LEVEL_GLOBAL COC_DEBUG
#define COC_LOG_TIME_ENABLE  1
#define COC_LOG_FILE_ENABLE  0
#define COC_LOG_OUTPUT       stderr

#define COC_MALLOC           malloc
#define COC_REALLOC          realloc
#define COC_FREE             free
#define COC_ASSERT           assert
#define COC_VEC_INIT_CAP     64

#define coc_log(level, ...) do {                                            \
    if (COC_LOG_LEVEL_GLOBAL > level) break;                                \
    time_t now = time(NULL);                                                \
    char __coc_time_buf[32];                                                \
    if (COC_LOG_TIME_ENABLE) {                                              \
        strftime(__coc_time_buf, sizeof(__coc_time_buf),                    \
                 "%Y-%m-%d %H:%M:%S", localtime(&now));                     \
        fprintf(COC_LOG_OUTPUT, "[%s] ", __coc_time_buf);                   \
    }                                                                       \
    switch (level) {                                                        \
    case COC_DEBUG:                                                         \
        fprintf(COC_LOG_OUTPUT, "[DEBUG] ");                                \
        break;                                                              \
    case COC_INFO:                                                          \
        fprintf(COC_LOG_OUTPUT, "[INFO] ");                                 \
        break;                                                              \
    case COC_WARNING:                                                       \
        fprintf(COC_LOG_OUTPUT, "[WARNING] ");                              \
        break;                                                              \
    case COC_ERROR:                                                         \
        fprintf(COC_LOG_OUTPUT, "[ERROR] ");                                \
        break;                                                              \
    case COC_FATAL:                                                         \
        fprintf(COC_LOG_OUTPUT, "[FATAL] ");                                \
        break;                                                              \
    default:                                                                \
        break;                                                              \
    }                                                                       \
    if (COC_LOG_FILE_ENABLE)                                                \
        fprintf(stderr, "(%s:%d) ", __FILE__, __LINE__);                    \
    fprintf(COC_LOG_OUTPUT, __VA_ARGS__);                                   \
    fprintf(COC_LOG_OUTPUT, "\n");                                          \
} while(0)

#define coc_log_raw(level, ...) do { \
    if (COC_LOG_LEVEL_GLOBAL > level) break;                                \
    fprintf(COC_LOG_OUTPUT, __VA_ARGS__);                                   \
}  while (0)

// Vec format:
// typedef struct Vec {
//     T *items;
//     size_t size;
//     size_t capacity;
// } Vec;

#define coc_vec_append(vec, item) do {                                                     \
    COC_ASSERT((vec) != NULL);                                                             \
    if ((vec)->size >= (vec)->capacity) {                                                  \
        (vec)->capacity = (vec)->capacity == 0 ? COC_VEC_INIT_CAP : (vec)->capacity * 2;   \
        (vec)->items = COC_REALLOC((vec)->items, (vec)->capacity * sizeof(*(vec)->items)); \
        COC_ASSERT((vec)->items != NULL && "Realloc failed");                              \
    }                                                                                      \
    (vec)->items[(vec)->size++] = (item);                                                  \
} while (0) 

#define coc_vec_free(vec) do { \
    COC_ASSERT((vec) != NULL); \
    COC_FREE((vec)->items);    \
} while (0)

#define coc_vec_append_many(vec, new_items, new_size) do {                                 \
    if ((vec)->size + new_size >= (vec)->capacity) {                                       \
        if ((vec)->capacity == 0) (vec)->capacity = COC_VEC_INIT_CAP;                      \
        while ((vec)->size + new_size > (vec)->capacity) (vec)->capacity *= 2;             \
        (vec)->items = COC_REALLOC((vec)->items, (vec)->capacity * sizeof(*(vec)->items)); \
        COC_ASSERT((vec)->items != NULL && "Realloc failed");                              \
    }                                                                                      \
    if ((new_items) != NULL)                                                               \
        memcpy((vec)->items + (vec)->size, new_items, new_size * sizeof(*(vec)->items));   \
    (vec)->size += new_size;                                                               \
} while (0)

typedef struct Coc_String {
    char *items;
    size_t size;
    size_t capacity;
} Coc_String;

#define coc_str_reserve(str, n) coc_vec_append_many(str, NULL, n)

#define coc_str_append(str, cstr) do {                                            \
    COC_ASSERT((str) != NULL);                                                   \
    COC_ASSERT((cstr) != NULL);                                                  \
    const char *__coc_tmp_cstr = (cstr);                                         \
    size_t __coc_tmp_len = strlen(__coc_tmp_cstr);                              \
    coc_vec_append_many((str), __coc_tmp_cstr, __coc_tmp_len);                   \
} while (0)

#define coc_str_append_null(str) coc_vec_append(str, '\0')

#define coc_str_remove_null(str) do { (str)->size--; } while (0)

#define coc_str_free(str) coc_vec_free(str)

static inline int coc_read_entire_file(const char *path, Coc_String *buf) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) goto fail;
    if (fseek(fp, 0, SEEK_END) < 0) goto fail;
    long file_size = ftell(fp);
    if (file_size < 0) goto fail;
    if (fseek(fp, 0, SEEK_SET) < 0) goto fail;

    size_t new_size = buf->size + file_size;
    if (new_size > buf->capacity) {
        buf->items = realloc(buf->items, new_size);
        COC_ASSERT(buf->items != NULL);
        buf->capacity = new_size;
    }
    fread(buf->items + buf->size, 1, file_size, fp);
    if (ferror(fp)) goto fail;
    buf->size = new_size;
    if (fp) fclose(fp);
    return 0;
fail:
    coc_log(COC_FATAL, "Cannot read file %s: %s", path, strerror(errno));
    if (fp) fclose(fp);
    return errno;
}

// typedef struct Entry {
//     Coc_String key;
//     long long value;
//     bool is_used;
// } Entry;

// typedef struct HashTable {
//     Entry *items;
//     Entry *new_items;
//     size_t size;
//     size_t capacity;
// } HashTable;

#define COC_HT_INIT_CAP    64
#define COC_HT_LOAD_FACTOR 0.75f

static inline uint32_t coc_hash_fnv1a(Coc_String *key) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < key->size; i++) {
        hash ^= (unsigned char)key->items[i];
        hash *= 16777619u;
    }
    return hash;
}

#define coc_hash_value coc_hash_fnv1a

#define coc_ht_insert(ht, k, v) do {                                               \
    if ((ht)->size + 1 > (ht)->capacity * 0.75f) {                                 \
        size_t new_capacity = (ht)->capacity == 0 ? 8 : (ht)->capacity * 2;        \
        (ht)->new_items = calloc(new_capacity, sizeof(*(ht)->items));              \
        COC_ASSERT((ht)->new_items != NULL);                                       \
        for (size_t i = 0; i < (ht)->capacity; i++) {                              \
            if ((ht)->items[i].is_used) {                                          \
                uint32_t idx = coc_hash_value(&(ht)->items[i].key) % new_capacity; \
                while ((ht)->new_items[idx].is_used)                               \
                    idx = (idx + 1) % new_capacity;                                \
                (ht)->new_items[idx] = (ht)->items[i];                             \
            }                                                                      \
        }                                                                          \
        free((ht)->items);                                                         \
        (ht)->items = (ht)->new_items;                                             \
        (ht)->capacity = new_capacity;                                             \
    }                                                                              \
    uint32_t idx = coc_hash_value(&(k)) % (ht)->capacity;                          \
    bool found = false;                                                            \
    while ((ht)->items[idx].is_used) {                                             \
        if (((ht)->items[idx].key.size == (k).size) &&                             \
            memcmp((ht)->items[idx].key.items, (k).items, (k).size) == 0) {        \
            (ht)->items[idx].value = (v);                                          \
            found = true;                                                          \
            break;                                                                 \
        }                                                                          \
        idx = (idx + 1) % (ht)->capacity;                                          \
    }                                                                              \
    if (found) break;                                                              \
    (ht)->items[idx].key = (k);                                                    \
    (ht)->items[idx].value = (v);                                                  \
    (ht)->items[idx].is_used = true;                                               \
    (ht)->size++;                                                                  \
} while (0)

#define coc_ht_insert_cstr(ht, cstr, v) do { \
    Coc_String new_key = {0};                \
    coc_str_append(&new_key, cstr);          \
    coc_ht_insert(ht, new_key, v);           \
} while (0)

#define coc_ht_find(ht, k, v_ptr) do {                                      \
    uint32_t idx = coc_hash_value(&(k)) % (ht)->capacity;                   \
    for (size_t i = 0; i < (ht)->capacity; i++) {                           \
        if (!(ht)->items[idx].is_used) {                                    \
            (v_ptr) = NULL;                                                 \
            break;                                                          \
        }                                                                   \
        if ((ht)->items[idx].key.size == (k).size &&                        \
            memcmp((ht)->items[idx].key.items, (k).items, (k).size) == 0) { \
            (v_ptr) = &(ht)->items[idx].value;                              \
            break;                                                          \
        }                                                                   \
        idx = (idx + 1) % (ht)->capacity;                                   \
    }                                                                       \
} while (0)

#define coc_ht_find_cstr(ht, cstr, v_ptr) do { \
    Coc_String target_key = {0};               \
    coc_str_append(&target_key, cstr);         \
    coc_ht_find(ht, target_key, v_ptr);        \
} while (0)

#endif