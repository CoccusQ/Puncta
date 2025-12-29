#include "puncta.h"

Coc_Log_Config coc_log_config;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        coc_log_raw(COC_FATAL, "Usage: %s <file name> [-option]", argv[0]);
        return 1;
    }
    const char *filename = NULL;
    Coc_Log_Level log_level = COC_LOG_LEVEL_GLOBAL;
    const char *log_file = NULL;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;
        size_t len = coc_kv_split(arg, &value);
        if (value) {
            if (coc_kv_match(arg, len, "--log-level")) {
                log_level = coc_log_level_from_cstr(value);
            } else if (coc_kv_match(arg, len, "--log-file")) {
                log_file = value;
            } else {
                coc_log_raw(COC_ERROR, "%s: unknown option %.*s", argv[0], (int)len, arg);
                return 1;
            }
        } else {
            if (!filename) {
                filename = arg;
            } else {
                coc_log_raw(COC_ERROR, "%s: extra argument: %s", argv[0],  arg);
                return 1;
            }
        }
    }
    coc_log_init(log_level, true, false, log_file);
    if (filename == NULL) {
        coc_log_raw(COC_FATAL, "%s: no input file", argv[0]);
        return 1;
    }
    VM *vm = run_file(filename, NULL);
    vm_free(vm);
    coc_log_close();
    return 0;
}
