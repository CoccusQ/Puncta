#include "puncta.h"

Coc_Log_Config coc_log_config;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        coc_log(COC_ERROR, "Usage: %s <file name> [-options]", argv[0]);
        return 1;
    }
    Coc_ArgParser argparser = {0};
    coc_argparser_parse(&argparser, argc, argv);
    Coc_Log_Level log_level = coc_log_level_from_cstr(coc_argparser_get(&argparser, "--log-level"));
    char *log_file = coc_argparser_get(&argparser, "--log-file");
    coc_log_init(log_level, true, false, log_file);
    char *src_file = coc_argparser_get(&argparser, "1");
    if (src_file == NULL) {
        coc_log(COC_ERROR, "Usage: %s <file name> [-options]", argv[0]);
        return 1;
    }
    VM *vm = run_file(src_file, NULL);
    vm_free(vm);
    coc_argparser_free(&argparser);
    coc_log_close();
    return 0;
}