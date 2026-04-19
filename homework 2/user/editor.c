#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

#define WORKING_BUF  1760   // 22 reda * 80 kolona
#define STAT_ROW     23
#define COMMAND_ROW  24

// Bafer za radnu povrsinu (redovi 1-22)
static char work_buf[WORKING_BUF];
// Broj karaktera u baferu
static int buf_len = 0;
// Putanja do fajla
static char *filename;
// Prati nesacuvane izmene
static int dirty = 0;
//screen backup
static short sc_backup[2000];
// Poslednja pozicija kursora u radnoj povrsini
int last_work_pos = 80;

//extern static ushort *crt = (ushort*)P2V(0xb8000);

void toggle_ui(void) {
    set_cursor(0);
    printf("---------- EDITOR: e-----------------------------", filename);

    set_cursor(80);
    for (int i = 0; i < buf_len; i++) {
        printf("%c", work_buf[i]);
    }
    set_cursor(80 + buf_len);
    for(int i = 80 + buf_len; i < 22 * 80; i++) printf(" ");

    set_cursor(23 * 80);
    int slobodni = get_free_blocks();
    printf("Slobodno: %d blk | [$] Komande | [Enter] Novi red", slobodni);
    set_cursor(24*80);
    printf("Komanda: ");
    set_cursor(80 + buf_len);
}

void refresh() {
    set_cursor(0);
    printf("---------- EDITOR: %s-----------------------------", filename);

    set_cursor(23 * 80);
    int slobodni = get_free_blocks();
    printf("Slobodno: %d blk | [$] Komande | [Enter] Novi red", slobodni);
    set_cursor(24*80);
    printf("Komanda: ");
    set_cursor(80 + buf_len);
}

void print_help(void) {
    printf("--- XV6 EDITOR HELP ---\n");
    printf("Upotreba: editor <filename> [OPCIJA]\n");
    printf("Interaktivni tekstualni editor za xv6.\n");
    printf("\n");
    printf("Opcije:\n");
    printf("  -h, --help    Prikazuje ovaj help meni\n");
    printf("\n");
    printf("KOMANDE (aktiviraju se tasterom $):\n");
    printf("  save  -> Trajno cuva sadrzaj na disk\n");
    printf("  stats -> Prikazuje fizicke adrese prvih 5 blokova datoteke\n");
    printf("  quit  -> Izlaz iz editora (upozorava ako nije sacuvano)\n");
    printf("\n");
    printf("DODATNO:\n");
    printf("  [Backspace] -> Brise karakter i pomera kursor unazad\n");
    printf("  [Enter]     -> Novi red (automatsko pozicioniranje)\n");
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        print_help();
        exit();
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        exit();
    }

    filename = argv[1];
    memset(work_buf, 0, WORKING_BUF);

    int n = read_path(filename, work_buf);
    if (n > 0) {
        buf_len = n;
    } else if (n == -2) {
        printf("Greska: '%s' je uredjaj (T_DEV), ne moze se editovati.\n", filename);
        exit();
    }

    toggle_ui();

    char c;
    while (read(0, &c, 1) > 0) {
        // int cur = get_cursor();
        // int in_cmd = (cur >= COMMAND_ROW * 80 && cur < (COMMAND_ROW + 1) * 80);
        //
        // if(in_cmd) {
        //     //unutar komandne linije
        //     toggle_ui();
        //     set_cursor(last_work_pos);
        // }

        if (c == '$') {
            char cmd[10];
            toggle_ui();
            set_cursor(COMMAND_ROW * 80 + 10);
            int cmd_len = 0;

        } else if (c == '\b' || c == 127) { // Backspace
            if (buf_len > 0) {
                buf_len--;
                work_buf[buf_len] = '\0';
                dirty = 1;
                toggle_ui();
            }
        } else if (c == '\n' || c == '\r') { // Enter
            if (buf_len < WORKING_BUF) {
                work_buf[buf_len++] = '\n';
                dirty = 1;
                toggle_ui();
            }
        } else { // Obicno kucanje
            if (buf_len < WORKING_BUF) {
                work_buf[buf_len++] = c;
                dirty = 1;
                toggle_ui();
            }
        }
    }

    exit();
}
