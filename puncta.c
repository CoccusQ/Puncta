#include "puncta.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        coc_log(COC_ERROR, "Usage: %s <file name>", argv[0]);
        return 1;
    }
    VM *vm = run_file(argv[1], NULL);
    vm_free(vm);
    return 0;
}