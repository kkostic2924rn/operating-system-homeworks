// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "traps.h"
#include "spinlock.h"
#include "file.h"
#include "memlayout.h"
#include "proc.h"
#include "x86.h"
#include "printf.h"

#include <stdarg.h>

static void consputc(int);

static int panicked = 0;

//flag za aktivnost search mode-a
static int act_search_flag = 0;
//red u kojem se nalazi search bar
static char buffer_search[81];
//duzina trazene reci
static int length_search = 0;
//cuvanje poslednjeg reda, kad se upali search
static short screen_bu[80];
//redni broj trenutne pronadjene reci
static int trenutni = 0;
//brojac pronadjenih reci
static int found_cnt = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static int to_lower(int c) {
	if(c >= 'A' && c <= 'Z')
		return c + 'a' - 'A';
	return c;
}

static void refresh_view(void) {
	int i, j;
	ushort *crt = (ushort*) P2V(0xb8000);

	found_cnt = 0;

	for(i = 0; i < 24*80; i++) {
		if(act_search_flag) {
			crt[i] = (crt[i] & 0xff) | 0xF000;
		}
		else {
			crt[i] = (crt[i] & 0xff) | 0x0700;
		}
	}

	if(length_search == 0 || !act_search_flag) return;

	for(i = 0; i < 24*80 - length_search; i++) {
		int found = 1;

		for(j = 0; j < length_search; j++) {
			if((i + j >= 24*80 || to_lower(crt[i+j] & 0xff) != to_lower(buffer_search[j]))) {
				found = 0;
				break;
			}
		}
		if(found) found_cnt++;
	}

	if (found_cnt == 0) {
		trenutni = 0;
		return;
	}

	// 3. Osiguraj da je 'trenutni' uvek u opsegu (Circular logic)
	// Ovo rešava problem sa pomeranjem unazad (negativni brojevi) i unapred
	trenutni = (trenutni % found_cnt + found_cnt) % found_cnt;

	// 4. Drugi prolaz: Bojenje reči
	int temp_idx = 0;
	for(i = 0; i <= 24*80 - length_search; i++) {
		int found = 1;
		for(j = 0; j < length_search; j++) {
			if(to_lower(crt[i+j] & 0xff) != to_lower(buffer_search[j])) {
				found = 0;
				break;
			}
		}

		if(found) {
			// Ako je trenutni indeks onaj koji tražimo, boji u zeleno (0x2F), inače ljubičasto (0x5F)
			unsigned short clr = (temp_idx == trenutni) ? 0x2F00 : 0x5F00;
			for (j = 0; j < length_search; j++) {
				crt[i+j] = (crt[i+j] & 0xff) | clr;
			}
			temp_idx++;
		}
	}
}

static void toggle_search(void) {
	int i;
	ushort *crt = (ushort*)P2V(0xb8000);
	trenutni = 0;

	if(!act_search_flag) {
		act_search_flag = 1;
		for (i = 0; i < 80; i++) {
			screen_bu[i] = crt[24*80 + i];
		}

		char *prompt = "Search: ";
		for(i = 0; i < strlen(prompt); i++) {
			crt[24*80 + i] = (prompt[i] & 0xff) | 0x0200;
		}

		for(i = 0; i < length_search; i++) {
			crt[24*80 + strlen(prompt) + i] = (buffer_search[i] & 0xff) | 0x0800;
		}

		refresh_view();
	} else {
		act_search_flag = 0;
		for(i = 0; i < 80; i++)
			crt[24*80 + i] = screen_bu[i];
		refresh_view();
	}
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int locking;
	va_list args;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	va_start(args, fmt);

	fnvprintf(consputc, fmt, args);

	va_end(args);

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		if(c == (0x1000 | 'f')) {
			toggle_search();
			continue;
		}
		if(act_search_flag) {
			if(c == (0x1000 | 'n')) {
				trenutni++;
				refresh_view();
			} else if(c == (0x1000 | 'p')) {
				trenutni--;
				refresh_view();
			} else if(c == BACKSPACE || c == '\x7f' || c == C('H')) {
				if(length_search > 0) {
					length_search--;
					trenutni = 0;
					buffer_search[length_search] = '\0';
					crt[24*80 + 8 + length_search] = ' ' | 0x0800;
					refresh_view();
				}
			} else if (c >= 32 && c <= 126 && length_search < 72){
				buffer_search[length_search++] = (char)c;

				crt[24*80 + 8 + (length_search - 1)] = (c & 0xff) | 0x0800;
				trenutni = 0;
				refresh_view();

			}
			continue;
		}
		switch(c){
		case C('P'):  // Process listing.
			// procdump() lofound_cnt = 0cks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				consputc(c);
				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

