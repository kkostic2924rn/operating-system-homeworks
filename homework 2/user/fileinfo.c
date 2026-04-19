#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

void print_help()
{
	printf("Upotreba: fileinfo <putanja_do_fajla> [OPCIJA]\n");
	printf("Prikazuje detaljnu mapu fizickih blokova dadoteke na disku\n");
	printf("\n");
	printf("Opcije:\n");
	printf(" -h, --help    Prikazuje ovaj help meni\n");
}

int
main(int argc, char *argv[])
{
	if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		print_help();
		exit();
	}

	int fd = open(argv[1], 0);
	if (fd < 0) {
		printf("fileinfo: Datoteka '%s' ne postoji.\n", argv[1]);
		exit();
	}

	struct fileblks fb;
	if (get_file_blocks(fd, &fb) == -1) {
		printf("fileinfo: Sistemski poziv nije uspeo.\n");
		close(fd);
		exit();
	}

	struct stat st;
	fstat(fd, &st);

	printf("\n--- INSPEKCIJA DATOTEKE: %s ---\n", argv[1]);
	printf("Ukupna velicina: %d bajtova\n", st.size);
	printf("Zauzetih blokova: %d\n", fb.num_blocks);
	printf("Slobodno u zadnjem bloku: %d bajtova\n", fb.last_block_free);
	printf("--------------------------------------\n");


	printf("DIREKTNI BLOKOVI (0-11):\n");
	if (fb.num_blocks == 0) {
		printf("Fajl nema dodeljenih blokova.\n");
	} else {
		int limit = (fb.num_blocks < 12) ? fb.num_blocks : 12;
		for (int i = 0; i < limit; i++) {
			printf("[%d] ", fb.blocks[i]);
		}
		printf("\n");
	}

	if (fb.num_blocks > 12) {
		printf("INDIREKTNI BLOKOVI (12-139):\n");
		for (int i = 12; i < fb.num_blocks; i++) {
			printf("[%d] ", fb.blocks[i]);
			// Novi red nakon svakih 10 blokova radi preglednosti
			if ((i - 11) % 10 == 0) printf("\n");
		}
		printf("\n");
	}

	printf("--------------------------------------\n");

	close(fd);
	exit();
}
