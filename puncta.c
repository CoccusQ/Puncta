#include "puncta.h"

Coc_Log_Config coc_log_config;

int main(int argc, char *argv[]) {
    coc_log_init(COC_INFO, true, false, NULL);
    if (argc < 2) {
        coc_log(COC_ERROR, "Usage: %s <file name>", argv[0]);
        return 1;
    }
    VM *vm = run_file(argv[1], NULL);
    vm_free(vm);
    coc_log_close();
    return 0;
}