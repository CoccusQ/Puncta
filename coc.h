// coc.h - version 1.4.0 (2026-01-10)
#ifndef COC_H_
#define COC_H_

#define COC_VERSION_MAJOR 1
#define COC_VERSION_MINOR 4
#define COC_VERSION_PATCH 0

#ifndef COCDEF
   #define COCDEF static inline
#endif // COCDEF

#ifndef COC_MALLOC
   #include <stdlib.h>
   #define COC_MALLOC(...)  malloc(__VA_ARGS__)
   #define COC_REALLOC(...) realloc(__VA_ARGS__)
   #define COC_CALLOC(...)  calloc(__VA_ARGS__)
   #define COC_FREE(...)    free(__VA_ARGS__)
#endif // COC_MALLOC

#ifndef COC_ASSERT
   #include <assert.h>
   #define COC_ASSERT(x) assert(x)
#endif // COC_ASSERT

#include <time.h>
#ifdef _WIN32
   #ifndef localtime_r
      #define localtime_r(timer, tm_buf) localtime_s(tm_buf, timer) 
   #endif // localtime_r
#endif // _WIN32

#include <stdio.h>
#include <string.h>
#include <errno.h>
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
    Coc_Log_Level min_level;
    bool          use_date;
    bool          use_time;
    bool          use_ms;
    bool          use_color;
    FILE         *out;
} Coc_Log_Config;

extern Coc_Log_Config coc_global_log_config;

#define COC_LOG_MIN_LEVEL    COC_INFO
#define COC_LOG_OUT          stderr
#define COC_LOG_RESET        "\033[0m"
#define COC_LOG_RED          "\033[31m"
#define COC_LOG_GREEN        "\033[32m"
#define COC_LOG_YELLOW       "\033[33m"
#define COC_LOG_CYAN         "\033[36m"

#define COC_VEC_INIT_CAP     64

#define COC_HT_INIT_CAP      16
#define COC_HT_LOAD_FACTOR   0.75f

#ifndef COC_UNUSED
   #define COC_UNUSED(x) (void)(x)
#endif // COC_UNUSED

#define coc_defer(value) do { result = (value); goto defer; } while (0)

COCDEF void coc_log_time() {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    struct tm tm_info;
    localtime_r(&now.tv_sec, &tm_info);
    char buf_date[32] = "";
    char buf_time[32] = "";
    char buf_ms[32] = "";
    if (coc_global_log_config.use_date) {
	strftime(buf_date, sizeof(buf_date), " %Y-%m-%d", &tm_info);
    }
    if (coc_global_log_config.use_time) {
	strftime(buf_time, sizeof(buf_time), " %H:%M:%S", &tm_info);
	if (coc_global_log_config.use_ms)
	    snprintf(buf_ms, sizeof(buf_ms), ".%03ld", now.tv_nsec / 1000000);
    }
    fprintf(coc_global_log_config.out, "[%s%s%s ] ", buf_date, buf_time, buf_ms);
}

COCDEF void coc_log_core(int level, const char *fmt, va_list args) {
    COC_ASSERT(level >= COC_DEBUG && level <= COC_NONE);
    if ((int)coc_global_log_config.min_level > level) return;
    if (coc_global_log_config.use_time ||
	coc_global_log_config.use_date )
	coc_log_time();
    const char *tag = "";
    const char *color = "";
    switch (level) {
    case COC_DEBUG  : tag = "DEBUG"  ; color = COC_LOG_GREEN ; break;
    case COC_INFO   : tag = "INFO"   ; color = COC_LOG_CYAN  ; break;
    case COC_WARNING: tag = "WARNING"; color = COC_LOG_YELLOW; break;
    case COC_ERROR  : tag = "ERROR"  ; color = COC_LOG_RED   ; break;
    case COC_FATAL  : tag = "FATAL"  ; color = COC_LOG_RED   ; break;
    default: break;
    }
    if (coc_global_log_config.use_color && coc_global_log_config.out == COC_LOG_OUT)
        fprintf(COC_LOG_OUT, "[%s%s" COC_LOG_RESET "] ", color, tag);
    else
        fprintf(coc_global_log_config.out, "[%s] ", tag);
    vfprintf(coc_global_log_config.out, fmt, args);
    fprintf(coc_global_log_config.out, "\n");
    if (level >= COC_WARNING) fflush(coc_global_log_config.out);
}

COCDEF void coc_log(Coc_Log_Level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    coc_log_core((int)level, fmt, args);
    va_end(args);
}

#define coc_log_raw(level, ...) do {           \
    if (level < (int)COC_LOG_MIN_LEVEL) break; \
    fprintf(COC_LOG_OUT, __VA_ARGS__);         \
    fprintf(COC_LOG_OUT, "\n");                \
} while (0)

COCDEF void coc_log_init(Coc_Log_Config config, const char *filename) {
    coc_global_log_config = config;
    coc_global_log_config.out = COC_LOG_OUT;
    if (filename) {
        FILE *fp = fopen(filename, "a+");
        if (fp == NULL) {
            coc_log(COC_FATAL, "cannot write file %s: %s", filename, strerror(errno));
            exit(errno);
        }
        coc_global_log_config.out = fp;
    } 
}

COCDEF void coc_log_close() {
    FILE *out = coc_global_log_config.out;
    if (out && out != COC_LOG_OUT) fclose(out);
    coc_global_log_config.out = COC_LOG_OUT;
}

COCDEF void coc_log_reopen(const char *filename) {
    COC_ASSERT(filename != NULL);
    coc_log_close();
    FILE *fp = fopen(filename, "a+");
    if (fp == NULL) {
        coc_log(COC_FATAL, "cannot write file %s: %s", filename, strerror(errno));
        exit(errno);
    }
    coc_global_log_config.out = fp;
}

COCDEF Coc_Log_Level coc_log_level_from_cstr(const char *cstr) {
    if (cstr == NULL) return COC_LOG_MIN_LEVEL;
    if (strcmp("DEBUG"  , cstr) == 0) return COC_DEBUG;
    if (strcmp("INFO"   , cstr) == 0) return COC_INFO;
    if (strcmp("WARNING", cstr) == 0) return COC_WARNING;
    if (strcmp("ERROR"  , cstr) == 0) return COC_ERROR;
    if (strcmp("FATAL"  , cstr) == 0) return COC_FATAL;
    if (strcmp("NONE"   , cstr) == 0) return COC_NONE;
    return COC_LOG_MIN_LEVEL;
}

// Vec format:
// typedef struct Vec {
//     T     *items;
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
    (vec)->items    = NULL;    \
    (vec)->size     = 0;       \
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
    (dst)->size     = (src)->size;                                           \
    (dst)->capacity = (src)->capacity;                                       \
    (dst)->items    = COC_MALLOC((src)->capacity * sizeof(*(src)->items));   \
    COC_ASSERT((dst)->items != NULL && "Malloc failed");                     \
    memcpy((dst)->items, (src)->items, (src)->size * sizeof(*(src)->items)); \
} while (0)

#define coc_vec_move(dst, src) do {    \
    COC_ASSERT((dst) != NULL);         \
    COC_ASSERT((src) != NULL);         \
    (dst)->size     = (src)->size;     \
    (dst)->capacity = (src)->capacity; \
    (dst)->items    = (src)->items;    \
    (src)->size     = 0;               \
    (src)->capacity = 0;               \
    (src)->items    = NULL;            \
} while (0)

#define COC_SSO_CAP (sizeof(char *) + sizeof(size_t) * 2 - sizeof(uint8_t) - 1)

typedef struct Coc_String {
    union {
        struct {
            char   *items;
            size_t  size;
            size_t  capacity;
        };
        struct {
            uint8_t size;
            char    items[COC_SSO_CAP + 1];
        } sso;
    };
    uint32_t hash;
    bool     is_hash;
    bool     not_sso;
} Coc_String;

COCDEF char *coc_str_data(Coc_String *str) {
    COC_ASSERT(str != NULL);
    return str->not_sso ? str->items : str->sso.items;
}

COCDEF size_t coc_str_size(Coc_String *str) {
    COC_ASSERT(str != NULL);
    return str->not_sso ? str->size : str->sso.size;
}

COCDEF size_t coc_str_capacity(Coc_String *str) {
    COC_ASSERT(str != NULL);
    return str->not_sso ? str->capacity : COC_SSO_CAP; 
}

COCDEF bool coc_str_eq(Coc_String *a, Coc_String *b) {
    COC_ASSERT(a != NULL);
    COC_ASSERT(b != NULL);
    size_t size = coc_str_size(a); 
    return size == coc_str_size(b) &&
           memcmp(coc_str_data(a), coc_str_data(b), size) == 0;
}

COCDEF void coc_str_to_heap(Coc_String *str, size_t n) {
    COC_ASSERT(!str->not_sso);
    size_t size = str->sso.size;
    size_t cap = n;
    if (cap < COC_SSO_CAP * 2) cap = COC_SSO_CAP * 2;
    char *buf = COC_MALLOC(cap);
    COC_ASSERT(buf != NULL && "Malloc failed");
    memcpy(buf, str->sso.items, str->sso.size);
    str->items    = buf;
    str->size     = size;
    str->capacity = cap;
    str->not_sso  = true;
}

COCDEF void coc_str_ensure(Coc_String *str, size_t add) {
    COC_ASSERT(str != NULL);
    size_t need = coc_str_size(str) + add;
    if (need <= coc_str_capacity(str)) return;
    if (str->not_sso) coc_vec_grow(str, need);
    else coc_str_to_heap(str, need);
}

COCDEF void coc_str_reserve(Coc_String *str, size_t n) {
    COC_ASSERT(str != NULL);
    if (str->not_sso) {
        coc_vec_grow(str, n);
    } else {
        if (n > COC_SSO_CAP) coc_str_to_heap(str, n);
    }
}

COCDEF void coc_str_push(Coc_String *str, char c) {
    COC_ASSERT(str != NULL);
    coc_str_ensure(str, 1);
    if (str->not_sso) str->items[str->size++] = c;
    else str->sso.items[str->sso.size++] = c;
    str->is_hash = false;
}
    
COCDEF void coc_str_pop(Coc_String *str) {
    COC_ASSERT(str != NULL);
    COC_ASSERT(coc_str_size(str) > 0);
    if (str->not_sso) --str->size;
    else --str->sso.size;
    str->is_hash = false;
}

COCDEF char coc_str_back(Coc_String *str) {
    COC_ASSERT(str != NULL);
    COC_ASSERT(coc_str_size(str) > 0);
    if (str->not_sso) return str->items[str->size - 1];
    else return str->sso.items[str->sso.size - 1];
}

COCDEF void coc_str_append_many(Coc_String *str, const char *cstr, size_t len) {
    COC_ASSERT(str != NULL);
    COC_ASSERT(cstr != NULL);
    coc_str_ensure(str, len);
    char *dst = coc_str_data(str);
    memcpy(dst + coc_str_size(str), cstr, len);
    if (str->not_sso) str->size += len;
    else str->sso.size += len;
    str->is_hash = false;
}

COCDEF void coc_str_append(Coc_String *str, const char *cstr) {
    COC_ASSERT(str != NULL);
    COC_ASSERT(cstr != NULL);
    size_t len = strlen(cstr);
    coc_str_append_many(str, cstr, len);
}

COCDEF void coc_str_append_null(Coc_String *str) {
    COC_ASSERT(str != NULL);
    if (str->not_sso) coc_vec_append(str, '\0');
    else str->sso.items[str->sso.size] = '\0';
}

COCDEF void coc_str_remove_null(Coc_String *str) {
    COC_ASSERT(str != NULL);
    if (str->not_sso) {
        while (str->size > 0 && str->items[str->size - 1] == '\0')
            str->size--;
    } else {
        while (str->sso.size > 0 && str->sso.items[str->sso.size - 1] == '\0')
            str->sso.size--;
    }
}

COCDEF void coc_str_free(Coc_String *str) {
    COC_ASSERT(str != NULL);
    if (str->not_sso) coc_vec_free(str);
    *str = (Coc_String){0};
}

COCDEF Coc_String coc_str_copy(Coc_String *src) {
    COC_ASSERT(src != NULL);
    Coc_String dst = {0};
    dst.not_sso  = src->not_sso;
    dst.is_hash = src->is_hash;
    dst.hash    = src->hash;
    if (src->not_sso) {
        coc_vec_copy(&dst, src);
    } else {
        memcpy(dst.sso.items, src->sso.items, src->sso.size);
        dst.sso.size = src->sso.size;
    }
    return dst;
}

COCDEF Coc_String coc_str_move(Coc_String *src) {
    COC_ASSERT(src != NULL);
    Coc_String dst = *src;
    *src = (Coc_String){0};
    return dst;
}

COCDEF int coc_read_entire_file(const char *path, Coc_String *buf) {
    COC_ASSERT(buf != NULL);
    int result = 0;
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) coc_defer(errno);
    if (fseek(fp, 0, SEEK_END) < 0) coc_defer(errno);
    long file_size = ftell(fp);
    if (file_size < 0) coc_defer(errno);
    if (fseek(fp, 0, SEEK_SET) < 0) coc_defer(errno);

    size_t need = (size_t)file_size + coc_str_size(buf);
    if (!buf->not_sso) coc_str_to_heap(buf, need);
    size_t new_size = buf->size + file_size;
    if (new_size > buf->capacity) {
        buf->items = COC_REALLOC(buf->items, new_size);
        COC_ASSERT(buf->items != NULL);
        buf->capacity = new_size;
    }
    fread(buf->items + buf->size, 1, file_size, fp);
    if (ferror(fp)) coc_defer(errno);
    buf->size = new_size;
defer:
    if (result != 0) coc_log(COC_FATAL, "Cannot read file %s: %s", path, strerror(result));
    if (fp) fclose(fp);
    return result;
}

// typedef struct Entry {
//     Coc_String key;
//     long long  value;
//     bool       is_used;
// } Entry;

// typedef struct HashTable {
//     Entry  *items;
//     Entry  *new_items;
//     size_t  size;
//     size_t  capacity;
// } HashTable;

COCDEF uint32_t coc_hash_fnv1a(Coc_String *key) {
    COC_ASSERT((key) != NULL);
    if (key->is_hash) return key->hash;
    uint32_t hash = 2166136261u;
    char  *data = coc_str_data(key);
    size_t size = coc_str_size(key);
    for (size_t i = 0; i < size; i++) {
        hash ^= (unsigned char)data[i];
        hash *= 16777619u;
    }
    key->hash    = hash;
    key->is_hash = true;
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

#define coc_ht_insert_move(ht, k, v) do {                    \
    COC_ASSERT((ht) != NULL);                                \
    COC_ASSERT((k) != NULL);                                 \
    coc_ht_resize(ht, (ht)->size + 1);                       \
    uint32_t __coc_idx = coc_hash_value(k) % (ht)->capacity; \
    bool found = false;                                      \
    while ((ht)->items[__coc_idx].is_used) {                 \
        if (coc_str_eq(&(ht)->items[__coc_idx].key, k)) {    \
            (ht)->items[__coc_idx].value = (v);              \
            coc_str_free(k);                                 \
            found = true;                                    \
            break;                                           \
        }                                                    \
        __coc_idx = (__coc_idx + 1) % (ht)->capacity;        \
    }                                                        \
    if (found) break;                                        \
    (ht)->items[__coc_idx].key     = coc_str_move(k);        \
    (ht)->items[__coc_idx].value   = (v);                    \
    (ht)->items[__coc_idx].is_used = true;                   \
    (ht)->size++;                                            \
} while (0)

#define coc_ht_insert_copy(ht, k, v) do {       \
    COC_ASSERT((ht) != NULL);                   \
    COC_ASSERT((k) != NULL);                    \
    Coc_String __coc_tmp_key = coc_str_copy(k); \
    coc_ht_insert_move(ht, &__coc_tmp_key, v);  \
} while (0)

#define coc_ht_insert_cstr(ht, cstr, v) do {   \
    COC_ASSERT((ht) != NULL);                  \
    COC_ASSERT((cstr) != NULL);                \
    Coc_String __coc_new_key = {0};            \
    coc_str_append(&__coc_new_key, cstr);      \
    coc_ht_insert_move(ht, &__coc_new_key, v); \
} while (0)

#define coc_ht_find(ht, k, v_ptr) do {                       \
    COC_ASSERT((ht) != NULL);                                \
    COC_ASSERT((k) != NULL);                                 \
    if ((ht)->size == 0) {                                   \
        (v_ptr) = NULL;                                      \
        break;                                               \
    }                                                        \
    uint32_t __coc_idx = coc_hash_value(k) % (ht)->capacity; \
    for (size_t i = 0; i < (ht)->capacity; i++) {            \
        if (!(ht)->items[__coc_idx].is_used) {               \
            (v_ptr) = NULL;                                  \
            break;                                           \
        }                                                    \
        if (coc_str_eq(&(ht)->items[__coc_idx].key, k)) {    \
            (v_ptr) = &(ht)->items[__coc_idx].value;         \
            break;                                           \
        }                                                    \
        __coc_idx = (__coc_idx + 1) % (ht)->capacity;        \
    }                                                        \
} while (0)

#define coc_ht_find_cstr(ht, cstr, v_ptr) do { \
    COC_ASSERT((ht) != NULL);                  \
    COC_ASSERT((cstr) != NULL);                \
    size_t __coc_len = strlen(cstr);           \
    Coc_String __coc_target_key = {            \
        .items    = (char *)(cstr),            \
        .size     = __coc_len,                 \
        .capacity = __coc_len + 1,             \
        .hash     = 0,                         \
        .is_hash  = false,                     \
        .not_sso  = true                       \
    };                                         \
    coc_ht_find(ht, &__coc_target_key, v_ptr); \
} while (0)

#define coc_ht_free(ht) do {                      \
    COC_ASSERT((ht) != NULL);                     \
    for (size_t i = 0; i < (ht)->capacity; i++) { \
        if ((ht)->items[i].is_used) {             \
            coc_str_free(&((ht)->items[i].key));  \
        }                                         \
    }                                             \
    COC_FREE((ht)->items);                        \
    (ht)->items    = NULL;                        \
    (ht)->size     = 0;                           \
    (ht)->capacity = 0;                           \
} while (0)

COCDEF size_t coc_kv_split(const char *arg, const char **value) {
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

COCDEF bool coc_kv_match(const char *arg, size_t arg_len, const char *option) {
    COC_ASSERT(arg != NULL);
    COC_ASSERT(option != NULL);
    size_t opt_len = strlen(option);
    return arg_len == opt_len && strncmp(arg, option, opt_len) == 0;
}

#ifdef COC_IMPLEMENTATION

Coc_Log_Config coc_global_log_config = {0};

#endif // COC_IMPLEMENTATION

#endif // COC_H_

/*
Recent Revision History:

1.4.1 (2026-01-16)

Added:
- coc_str_back(), coc_str_append_many()

Changed:
- char coc_str_pop() -> void coc_str_pop()

1.4.0 (2026-01-10)

Fixed:
- invalid log level assert
- hash cache bug

Added:
- COC_SSO_CAP
- not_sso, struct sso in Coc_String
- coc_str_data(), coc_str_size(), coc_str_capacity(), coc_str_eq(), coc_str_ensure()
- coc_str_push(), coc_str_pop()

1.3.8 (2026-01-10)

Added:
- COC_IMPLEMENTATION, COCDEF, COC_UNUSED
- use_ms, use_color
- coc_log_time(), localtime_s(timer, tm_buf), coc_defer(value)

Changed:
- COC_LOG_LEVEL_GLOBAL -> COC_LOG_MIN_LEVEL
- COC_LOG_OUTPUT -> COC_LOG_OUT
- enable_time -> use_date / use_time
- g_coc_log_level -> min_level
- output -> out

1.0.0 (2025-12-01)

Added:
- initial release

 */
