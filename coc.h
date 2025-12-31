// coc.h - version 1.3.7 (2025-12-31)
#ifndef COC_H_
#define COC_H_

#define COC_VERSION_MAJOR 1
#define COC_VERSION_MINOR 3
#define COC_VERSION_PATCH 7

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>

typedef enum Coc_Log_Level {
    COC_DEBUG,
    COC_INFO,
    COC_WARNING,
    COC_ERROR,
    COC_FATAL,
    COC_NONE
} Coc_Log_Level;

typedef struct Coc_Log_Config {
    Coc_Log_Level g_coc_log_level;
    bool enable_time;
    bool enable_file_line;
    FILE *output;
} Coc_Log_Config;

extern Coc_Log_Config coc_log_config;

#define COC_LOG_LEVEL_GLOBAL COC_INFO
#define COC_LOG_OUTPUT       stderr

#define COC_MALLOC(...)      malloc(__VA_ARGS__)
#define COC_REALLOC(...)     realloc(__VA_ARGS__)
#define COC_CALLOC(...)      calloc(__VA_ARGS__)
#define COC_FREE(...)        free(__VA_ARGS__)
#define COC_ASSERT           assert
#define COC_VEC_INIT_CAP     64
#define COC_HT_INIT_CAP      16
#define COC_HT_LOAD_FACTOR   0.75f

static inline void coc_log_core(int level, const char *fmt, va_list args) {
    if ((int)coc_log_config.g_coc_log_level > level) return;
    time_t now = time(NULL);
    char buf[32];
    if (coc_log_config.enable_time) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(coc_log_config.output, "[%s] ", buf);
    }
    const char *tag = "";
    switch (level) {
    case COC_DEBUG  : tag = "DEBUG"  ; break;
    case COC_INFO   : tag = "INFO"   ; break;
    case COC_WARNING: tag = "WARNING"; break;
    case COC_ERROR  : tag = "ERROR"  ; break;
    case COC_FATAL  : tag = "FATAL"  ; break;
    default: break;
    }
    fprintf(coc_log_config.output, "[%s] ", tag);
    if (coc_log_config.enable_file_line)
        fprintf(coc_log_config.output, "(%s:%d) ", __FILE__, __LINE__);
    vfprintf(coc_log_config.output, fmt, args);
    fprintf(coc_log_config.output, "\n");
    fflush(coc_log_config.output);
}

static inline void coc_log(Coc_Log_Level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    coc_log_core((int)level, fmt, args);
    va_end(args);
}

#define coc_log_raw(level, ...) do {                   \
    if (COC_LOG_LEVEL_GLOBAL > level) break;           \
    fprintf(COC_LOG_OUTPUT, __VA_ARGS__);              \
    fprintf(COC_LOG_OUTPUT, "\n");                     \
    fflush(COC_LOG_OUTPUT);                            \
}  while (0)

static inline void coc_log_init(
    Coc_Log_Level global_level,
    bool enable_time,
    bool enable_file_line,
    const char *logfilename
) {
    coc_log_config.g_coc_log_level = global_level;
    coc_log_config.enable_time = enable_time;
    coc_log_config.enable_file_line = enable_file_line;
    coc_log_config.output = COC_LOG_OUTPUT;
    if (logfilename) {
        FILE *fp = fopen(logfilename, "a+");
        if (fp == NULL) {
            coc_log(COC_FATAL, "cannot write file %s: %s", logfilename, strerror(errno));
            exit(errno);
        }
        coc_log_config.output = fp;
    } 
}

static inline void coc_log_close() {
    FILE *output = coc_log_config.output;
    if (output && output != COC_LOG_OUTPUT) fclose(output);
    coc_log_config.output = COC_LOG_OUTPUT;
}

static inline void coc_log_reopen(const char *logfilename) {
    COC_ASSERT(logfilename != NULL);
    coc_log_close();
    FILE *fp = fopen(logfilename, "a+");
    if (fp == NULL) {
        coc_log(COC_FATAL, "cannot write file %s: %s", logfilename, strerror(errno));
        exit(errno);
    }
    coc_log_config.output = fp;
}

static inline Coc_Log_Level coc_log_level_from_cstr(const char *cstr) {
    if (cstr == NULL) return COC_LOG_LEVEL_GLOBAL;
    if (strcmp("DEBUG"  , cstr) == 0) return COC_DEBUG;
    if (strcmp("INFO"   , cstr) == 0) return COC_INFO;
    if (strcmp("WARNING", cstr) == 0) return COC_WARNING;
    if (strcmp("ERROR"  , cstr) == 0) return COC_ERROR;
    if (strcmp("FATAL"  , cstr) == 0) return COC_FATAL;
    if (strcmp("NONE"   , cstr) == 0) return COC_NONE;
    return COC_LOG_LEVEL_GLOBAL;
}

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
    (vec)->items = NULL;       \
    (vec)->size = 0;           \
    (vec)->capacity = 0;       \
} while (0)

#define coc_vec_grow(vec, new_size) do {                                                   \
    COC_ASSERT((vec) != NULL);                                                             \
    if ((vec)->size + new_size >= (vec)->capacity) {                                       \
        if ((vec)->capacity == 0) (vec)->capacity = COC_VEC_INIT_CAP;                      \
        while ((vec)->size + new_size > (vec)->capacity) (vec)->capacity *= 2;             \
        (vec)->items = COC_REALLOC((vec)->items, (vec)->capacity * sizeof(*(vec)->items)); \
        COC_ASSERT((vec)->items != NULL && "Realloc failed");                              \
    }                                                                                      \
} while (0)

#define coc_vec_append_many(vec, new_items, new_size) do {                           \
    COC_ASSERT((vec) != NULL);                                                       \
    COC_ASSERT((new_items) != NULL);                                                 \
    coc_vec_grow(vec, new_size);                                                     \
    memcpy((vec)->items + (vec)->size, new_items, new_size * sizeof(*(vec)->items)); \
    (vec)->size += new_size;                                                         \
} while (0)

#define coc_vec_copy(dst, src) do {                                          \
    COC_ASSERT((dst) != NULL);                                               \
    COC_ASSERT((src) != NULL);                                               \
    (dst)->size = (src)->size;                                               \
    (dst)->capacity = (src)->capacity;                                       \
    (dst)->items = COC_MALLOC((src)->capacity * sizeof(*(src)->items));      \
    COC_ASSERT((dst)->items != NULL && "Malloc failed");                     \
    memcpy((dst)->items, (src)->items, (src)->size * sizeof(*(src)->items)); \
} while (0)

#define coc_vec_move(dst, src) do {    \
    COC_ASSERT((dst) != NULL);         \
    COC_ASSERT((src) != NULL);         \
    (dst)->size = (src)->size;         \
    (dst)->capacity = (src)->capacity; \
    (dst)->items = (src)->items;       \
    (src)->size = 0;                   \
    (src)->capacity = 0;               \
    (src)->items = NULL;               \
} while (0)

typedef struct Coc_String {
    char *items;
    size_t size;
    size_t capacity;
} Coc_String;

static inline void coc_str_reserve(Coc_String *str, size_t n) {
    COC_ASSERT((str) != NULL);
    coc_vec_grow(str, n);
}

static inline void coc_str_append(Coc_String *str, const char *cstr) {
    COC_ASSERT((str) != NULL);
    COC_ASSERT((cstr) != NULL);
    const char *__coc_tmp_cstr = cstr;
    size_t __coc_tmp_len = strlen(__coc_tmp_cstr);
    coc_vec_append_many(str, __coc_tmp_cstr, __coc_tmp_len);
}

static inline void coc_str_append_null(Coc_String *str) {
    COC_ASSERT((str) != NULL);
    coc_vec_append(str, '\0');
}

static inline void coc_str_remove_null(Coc_String *str) {
    COC_ASSERT((str) != NULL);
    COC_ASSERT((str)->size > 0);
    str->size--;
}

static inline void coc_str_free(Coc_String *str) {
    COC_ASSERT((str) != NULL);
    coc_vec_free(str);
}

static inline Coc_String coc_str_copy(Coc_String *src) {
    COC_ASSERT((src) != NULL);
    Coc_String dst = {0};
    coc_vec_copy(&dst, src);
    return dst;
}

static inline Coc_String coc_str_move(Coc_String *src) {
    COC_ASSERT((src) != NULL);
    Coc_String dst = {0};
    coc_vec_move(&dst, src);
    return dst;
}

static inline int coc_read_entire_file(const char *path, Coc_String *buf) {
    COC_ASSERT((buf) != NULL);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) goto fail;
    if (fseek(fp, 0, SEEK_END) < 0) goto fail;
    long file_size = ftell(fp);
    if (file_size < 0) goto fail;
    if (fseek(fp, 0, SEEK_SET) < 0) goto fail;

    size_t new_size = buf->size + file_size;
    if (new_size > buf->capacity) {
        buf->items = COC_REALLOC(buf->items, new_size);
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

static inline uint32_t coc_hash_fnv1a(Coc_String *key) {
    COC_ASSERT((key) != NULL);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < key->size; i++) {
        hash ^= (unsigned char)key->items[i];
        hash *= 16777619u;
    }
    return hash;
}

#define coc_hash_value coc_hash_fnv1a

#define coc_ht_resize(ht, required_size) do {                                      \
    COC_ASSERT((ht) != NULL);                                                      \
    if (required_size > (ht)->capacity * COC_HT_LOAD_FACTOR) {                     \
        size_t new_capacity = (ht)->capacity == 0 ?                                \
                                  COC_HT_INIT_CAP : (ht)->capacity * 2;            \
        while (required_size > new_capacity * COC_HT_LOAD_FACTOR)                  \
            new_capacity *= 2;                                                     \
        (ht)->new_items = COC_CALLOC(new_capacity, sizeof(*(ht)->items));          \
        COC_ASSERT((ht)->new_items != NULL);                                       \
        for (size_t i = 0; i < (ht)->capacity; i++) {                              \
            if ((ht)->items[i].is_used) {                                          \
                uint32_t idx = coc_hash_value(&(ht)->items[i].key) % new_capacity; \
                while ((ht)->new_items[idx].is_used)                               \
                    idx = (idx + 1) % new_capacity;                                \
                (ht)->new_items[idx] = (ht)->items[i];                             \
            }                                                                      \
        }                                                                          \
        COC_FREE((ht)->items);                                                     \
        (ht)->items = (ht)->new_items;                                             \
        (ht)->capacity = new_capacity;                                             \
    }                                                                              \
} while (0)

#define coc_ht_insert_move(ht, k, v) do {                                          \
    COC_ASSERT((ht) != NULL);                                                      \
    COC_ASSERT((k) != NULL);                                                       \
    coc_ht_resize(ht, (ht)->size + 1);                                             \
    uint32_t idx = coc_hash_value(k) % (ht)->capacity;                             \
    bool found = false;                                                            \
    while ((ht)->items[idx].is_used) {                                             \
        if (((ht)->items[idx].key.size == (k)->size) &&                            \
            memcmp((ht)->items[idx].key.items, (k)->items, (k)->size) == 0) {      \
            (ht)->items[idx].value = (v);                                          \
            coc_str_free(k);                                                       \
            found = true;                                                          \
            break;                                                                 \
        }                                                                          \
        idx = (idx + 1) % (ht)->capacity;                                          \
    }                                                                              \
    if (found) break;                                                              \
    (ht)->items[idx].key = coc_str_move(k);                                        \
    (ht)->items[idx].value = (v);                                                  \
    (ht)->items[idx].is_used = true;                                               \
    (ht)->size++;                                                                  \
} while (0)

#define coc_ht_insert_copy(ht, k, v) do { \
    COC_ASSERT((ht) != NULL);             \
    COC_ASSERT((k) != NULL);              \
    Coc_String tmp = coc_str_copy(k);     \
    coc_ht_insert_move(ht, &tmp, v);      \
} while (0)

#define coc_ht_insert_cstr(ht, cstr, v) do {  \
    COC_ASSERT((ht) != NULL);                 \
    COC_ASSERT((cstr) != NULL);               \
    Coc_String new_key = {0};                 \
    coc_str_append(&new_key, cstr);           \
    coc_ht_insert_move(ht, &new_key, v);      \
} while (0)

#define coc_ht_find(ht, k, v_ptr) do {                                        \
    COC_ASSERT((ht) != NULL);                                                 \
    COC_ASSERT((k) != NULL);                                                  \
    if ((ht)->size == 0) {                                                    \
        (v_ptr) = NULL;                                                       \
        break;                                                                \
    }                                                                         \
    uint32_t idx = coc_hash_value(k) % (ht)->capacity;                        \
    for (size_t i = 0; i < (ht)->capacity; i++) {                             \
        if (!(ht)->items[idx].is_used) {                                      \
            (v_ptr) = NULL;                                                   \
            break;                                                            \
        }                                                                     \
        if ((ht)->items[idx].key.size == (k)->size &&                         \
            memcmp((ht)->items[idx].key.items, (k)->items, (k)->size) == 0) { \
            (v_ptr) = &(ht)->items[idx].value;                                \
            break;                                                            \
        }                                                                     \
        idx = (idx + 1) % (ht)->capacity;                                     \
    }                                                                         \
} while (0)

#define coc_ht_find_cstr(ht, cstr, v_ptr) do {  \
    COC_ASSERT((ht) != NULL);                   \
    COC_ASSERT((cstr) != NULL);                 \
    Coc_String target_key = {0};                \
    coc_str_append(&target_key, cstr);          \
    coc_ht_find(ht, &target_key, v_ptr);        \
    coc_str_free(&target_key);                  \
} while (0)

#define coc_ht_free(ht) do {                      \
    COC_ASSERT((ht) != NULL);                     \
    for (size_t i = 0; i < (ht)->capacity; i++) { \
        if ((ht)->items[i].is_used) {             \
            coc_str_free(&((ht)->items[i].key));  \
        }                                         \
    }                                             \
    COC_FREE((ht)->items);                        \
    (ht)->items = NULL;                           \
    (ht)->size = 0;                               \
    (ht)->capacity = 0;                           \
} while (0)

static inline size_t coc_kv_split(const char *arg, const char **value) {
    COC_ASSERT(arg != NULL);
    COC_ASSERT(value != NULL);
    const char *eq = strchr(arg, '=');
    if (eq && eq != arg) {
        *value = eq + 1;
        return (size_t)(eq - arg);
    } else {
        *value = NULL;
        return strlen(arg);
    }
}

static inline bool coc_kv_match(const char *arg, size_t arg_len, const char *option) {
    size_t opt_len = strlen(option);
    return arg_len == opt_len && strncmp(arg, option, opt_len) == 0;
}

#endif
