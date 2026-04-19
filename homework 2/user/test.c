#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

int main(void) {
    int pos, n_pos;

    pos = get_cursor(); // [cite: 65]
    if (pos < 0) {
        printf("Greska: get_cursor vratio %d\n", pos); //
    } else {
        printf("%d", pos);
    }
    n_pos = 1720;
    set_cursor(n_pos);

    exit();
}
