// coc.h - version 1.1.1 (2025-12-20)
#ifndef COC_H_
#define COC_H_

#define COC_VERSION_MAJOR 1
#define COC_VERSION_MINOR 1
#define COC_VERSION_PATCH 1

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

typedef struct Coc_Log_Config {
    Coc_Log_Level g_coc_log_level;
    bool enable_time;
    bool enable_file_line;
    FILE *output;
} Coc_Log_Config;

extern Coc_Log_Config coc_log_config;

#define COC_LOG_LEVEL_GLOBAL COC_DEBUG
#define COC_LOG_OUTPUT       stderr

#define COC_MALLOC           malloc
#define COC_REALLOC          realloc
#define COC_CALLOC           calloc
#define COC_FREE             free
#define COC_ASSERT           assert
#define COC_VEC_INIT_CAP     64
#define COC_HT_INIT_CAP      16
#define COC_HT_LOAD_FACTOR   0.75f

#define coc_log(level, ...) do {                                            \
    if (coc_log_config.g_coc_log_level > level) break;                    \
    time_t now = time(NULL);                                                \
    char __coc_time_buf[32];                                                \
    if (coc_log_config.enable_time) {                                     \
        strftime(__coc_time_buf, sizeof(__coc_time_buf),                    \
                 "%Y-%m-%d %H:%M:%S", localtime(&now));                     \
        fprintf(coc_log_config.output, "[%s] ", __coc_time_buf);          \
    }                                                                       \
    switch (level) {                                                        \
    case COC_DEBUG:                                                         \
        fprintf(coc_log_config.output, "[DEBUG] ");                       \
        break;                                                              \
    case COC_INFO:                                                          \
        fprintf(coc_log_config.output, "[INFO] ");                        \
        break;                                                              \
    case COC_WARNING:                                                       \
        fprintf(coc_log_config.output, "[WARNING] ");                     \
        break;                                                              \
    case COC_ERROR:                                                         \
        fprintf(coc_log_config.output, "[ERROR] ");                       \
        break;                                                              \
    case COC_FATAL:                                                         \
        fprintf(coc_log_config.output, "[FATAL] ");                       \
        break;                                                              \
    default:                                                                \
        break;                                                              \
    }                                                                       \
    if (coc_log_config.enable_file_line)                                  \
        fprintf(coc_log_config.output, "(%s:%d) ", __FILE__, __LINE__);   \
    fprintf(coc_log_config.output, __VA_ARGS__);                          \
    fprintf(coc_log_config.output, "\n");                                 \
} while(0)

#define coc_log_raw(level, ...) do {                   \
    if (COC_LOG_LEVEL_GLOBAL > level) break;           \
    fprintf(COC_LOG_OUTPUT, __VA_ARGS__);              \
    fprintf(COC_LOG_OUTPUT, "\n");                     \
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

static inline Coc_Log_Level coc_log_level_from_cstr(const char *s) {
    if (s == NULL) return COC_LOG_LEVEL_GLOBAL;
    if (strcmp("COC_DEBUG", s) == 0) {
        return COC_DEBUG;
    } else if (strcmp("COC_INFO", s) == 0) {
        return COC_INFO;
    } else if (strcmp("COC_WARNING", s) == 0) {
        return COC_WARNING;
    } else if (strcmp("COC_ERROR", s) == 0) {
        return COC_ERROR;
    } else if (strcmp("COC_FATAL", s) == 0) {
        return COC_FATAL;
    } else {
        coc_log_raw(COC_ERROR, "Arg Parser: invalid log level %s", s);
        exit(1);
    }
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

#define coc_vec_append_many(vec, new_items, new_size) do {                                 \
    COC_ASSERT((vec) != NULL);                                                             \
    if ((vec)->size + new_size >= (vec)->capacity) {                                       \
        if ((vec)->capacity == 0) (vec)->capacity = COC_VEC_INIT_CAP;                      \
        while ((vec)->size + new_size > (vec)->capacity) (vec)->capacity *= 2;             \
        (vec)->items = COC_REALLOC((vec)->items, (vec)->capacity * sizeof(*(vec)->items)); \
        COC_ASSERT((vec)->items != NULL && "Realloc failed");                              \
    }                                                                                      \
    if ((new_items) != NULL) {                                                             \
        memcpy((vec)->items + (vec)->size, new_items, new_size * sizeof(*(vec)->items));   \
        (vec)->size += new_size;                                                           \
    }                                                                                      \
} while (0)

#define coc_vec_copy(dst, src) do {                                          \
    COC_ASSERT((dst) != NULL);                                               \
    COC_ASSERT((src) != NULL);                                               \
    (dst)->size = (src)->size;                                               \
    (dst)->capacity = (src)->capacity;                                       \
    (dst)->items = COC_MALLOC((src)->capacity * sizeof(*(src)->items));      \
    COC_ASSERT((dst)->items != NULL && "Realloc failed");                    \
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
    coc_vec_append_many(str, NULL, n);
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

typedef struct Coc_ArgEntry {
    Coc_String key;
    Coc_String value;
    bool is_used;
} Coc_ArgEntry;

typedef struct Coc_ArgHashTable {
    Coc_ArgEntry *items;
    Coc_ArgEntry *new_items;
    size_t size;
    size_t capacity;
} Coc_ArgHashTable;

typedef struct Coc_ArgVec {
    Coc_String *items;
    size_t size;
    size_t capacity;
} Coc_ArgVec;

typedef struct Coc_ArgParser {
    char *short_opts[52];
    Coc_ArgHashTable ht;
    Coc_ArgVec vec;
} Coc_ArgParser;

static inline void coc_argparser_free(Coc_ArgParser *p) {
    COC_ASSERT(p != NULL);
    for (int i = 0; i < 52; i++) {
        if (p->short_opts[i]) COC_FREE(p->short_opts[i]);
        p->short_opts[i] = NULL;
    }
    Coc_ArgHashTable *ht = &p->ht;
    if (ht->items != NULL) {
        for (size_t i = 0; i < ht->capacity; i++) {
            if (ht->items->is_used) {
                coc_str_free(&ht->items[i].key);
                coc_str_free(&ht->items[i].value);
            }
        }
        COC_FREE(ht->items);
    }
    ht->items = NULL;
    ht->new_items = NULL;
    ht->capacity = 0;
    ht->size = 0;
    Coc_ArgVec *vec  = &p->vec;
    if (vec->items != NULL) {
        for (size_t i = 0; i < vec->size; i++)
            coc_str_free(&vec->items[i]);
        COC_FREE(vec->items);
    }
    vec->items = NULL;
    vec->capacity = 0;
    vec->size = 0;
}

static inline int coc_map_short_opt(char opt) {
    if (isupper(opt)) return opt - 'A';
    else if (islower(opt)) return opt - 'a' + 26;
    else {
        coc_log_raw(COC_ERROR,
                "Arg Parser: invalid short option '-%c': short options must be [A-Za-z]",
                opt);
        exit(1);
    }
}

static inline void coc_argparser_set_short_opt(Coc_ArgParser *p, char opt, const char *value) {
    COC_ASSERT(p != NULL);
    if (value) p->short_opts[coc_map_short_opt(opt)] = strdup(value);
}

static inline char *coc_argparser_get_short_opt(Coc_ArgParser *p, char opt) {
    COC_ASSERT(p != NULL);
    return p->short_opts[coc_map_short_opt(opt)];
}

static inline void coc_argparser_parse(Coc_ArgParser *p, int argc, char *argv[]) {
    COC_ASSERT(p != NULL);
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                char *eq_pos = strchr(argv[i], '=');
                Coc_String key = {0};
                Coc_String value = {0};
                if (eq_pos) {
                    *eq_pos = '\0';
                    coc_str_append(&key, argv[i] + 2);
                    coc_str_append(&value, eq_pos + 1);
                } else coc_str_append(&key, argv[i] + 2);
                coc_ht_insert_move(&p->ht, &key, value);
                value.items = NULL;
                value.capacity = 0;
                value.size = 0;
            } else {
                for (int j = 1; argv[i][j] != '\0'; j++) {
                    char opt = argv[i][j];
                    if (i + 1 < argc && argv[i + 1][0] != '-') {
                        coc_argparser_set_short_opt(p, opt, argv[++i]);
                        break;
                    }
                    else coc_argparser_set_short_opt(p, opt, "true");
                }
            }
        } else {
            Coc_String pos_arg = {0};
            coc_str_append(&pos_arg, argv[i]);
            coc_vec_append(&p->vec, pos_arg);
            pos_arg.items = NULL;
            pos_arg.capacity = 0;
            pos_arg.size = 0;
        }
    }
}

static inline char *coc_argparser_get(Coc_ArgParser *p, const char *key) {
    COC_ASSERT(p != NULL);
    COC_ASSERT(key != NULL);
    if (key[0] == '-') {
        if (key[1] == '-') {
            Coc_String *value = NULL;
            coc_ht_find_cstr(&p->ht, key + 2, value);
            if (value == NULL) return NULL;
            if (value->items) coc_str_append_null(value);
            return value->items;
        } else return p->short_opts[coc_map_short_opt(key[1])];
    } else {
        errno = 0;
        char *end_ptr = NULL;
        int pos = strtol(key, &end_ptr, 10) - 1;
        if (end_ptr == key) {
            coc_log_raw(COC_ERROR, "Arg Parser: invalid arg pos");
            exit(1);
        }
        if (*end_ptr != '\0' && *end_ptr != '\r' && *end_ptr != '\n') {
            coc_log_raw(COC_ERROR,
                    "Arg Parser: invalid character '%c' in arg pos",
                    *end_ptr);
            exit(1);
        }
        if (errno == ERANGE) {
            coc_log_raw(COC_ERROR, "Arg Parser: arg pos out of range");
            exit(errno);
        }
        if (pos < 0 || pos >= (int)p->vec.size) return NULL;
        if (p->vec.items[pos].items) coc_str_append_null(&p->vec.items[pos]);
        return p->vec.items[pos].items;
    }
}

#endif