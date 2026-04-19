#include "kernel/types.h"
#include "user/user.h"

void print_help() {
    printf("Upotreba: freeblocks [OPCIJA]\n");
    printf("Prikazuje broj slobodnih blokova na xv6 fajl sistemu.\n");
    printf("\n");
    printf("Opcije:\n");
    printf(" -h, --help   Prikazuju ovaj pomocni meni\n");
    printf("\n");
    printf("Primer: freeblocks\n");
}

int main(int argc, char *argv[]) {

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help();
        exit();
    }

    int slobodni = get_free_blocks();
    int konvert = (slobodni * 512) / 1024;

    printf("Skeniram bitmapu diska...\n");

    printf("---------------------------------\n");
    printf("STATUS DISKA\n");
    printf("---------------------------------\n");
    printf("Slobodnih blokova:    %d\n", slobodni);
    printf("Slobodan prostor:     %d KB\n", konvert);
    printf("---------------------------------\n");

    exit();
}
