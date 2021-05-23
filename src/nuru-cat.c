#define NURU_IMPLEMENTATION
#define NURU_SCOPE static

#include <stdio.h>      // fprintf(), stdout, setlinebuf()
#include <stdlib.h>     // EXIT_SUCCESS, EXIT_FAILURE, rand()
#include <stdint.h>     // uint8_t, uint16_t, ...
#include <inttypes.h>   // PRIu8, PRIu16, ...
#include <unistd.h>     // getopt(), STDOUT_FILENO
#include <math.h>       // ceil()
#include <time.h>       // time(), nanosleep(), struct timespec
#include <signal.h>     // sigaction(), struct sigaction
#include <termios.h>    // struct winsize, struct termios, tcgetattr(), ...
#include <sys/ioctl.h>  // ioctl(), TIOCGWINSZ
#include <locale.h>     // setlocale(), LC_CTYPE
#include <wchar.h>      // fwide(), wchar_t
#include "nuru.h"       // nuru minimal reference implementation

// program information

#define PROGRAM_NAME "nuru-cat"
#define PROGRAM_URL  "https://github.com/domsson/nuru-cat"

#define PROGRAM_VER_MAJOR 0
#define PROGRAM_VER_MINOR 0
#define PROGRAM_VER_PATCH 1

// ANSI escape codes
// https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit

#define ANSI_FONT_RESET   L"\x1b[0m"
#define ANSI_FONT_BOLD    L"\x1b[1m"
#define ANSI_FONT_NORMAL  L"\x1b[22m"
#define ANSI_FONT_FAINT   L"\x1b[2m"

#define ANSI_HIDE_CURSOR  L"\e[?25l"
#define ANSI_SHOW_CURSOR  L"\e[?25h"

#define ANSI_CLEAR_SCREEN L"\x1b[2J"
#define ANSI_CURSOR_RESET L"\x1b[H"

// these are flags used for signal handling

typedef struct options
{
	char *nui_file;        // nuru image file to load
	char *nug_file;        // nuru glyph palette file to load
	char *nuc_file;        // nuru color palette file to load
	uint8_t info;          // print image info and exit
	uint8_t fg;            // custom foreground color
	uint8_t bg;            // custom background color
	uint8_t help : 1;      // show help and exit
	uint8_t version : 1;   // show version and exit
}
options_s;

/*
 * Parse command line args into the provided options_s struct.
 */
static void
parse_args(int argc, char **argv, options_s *opts)
{
	opterr = 0;
	int o;
	while ((o = getopt(argc, argv, "b:c:f:g:ihV")) != -1)
	{
		switch (o)
		{
			case 'b':
				opts->bg = 1;
				break;
			case 'c':
				opts->nuc_file = optarg;
				break;
			case 'f':
				opts->fg = 1;
				break;
			case 'g':
				opts->nug_file = optarg;
				break;
			case 'h':
				opts->help = 1;
				break;
			case 'i':
				opts->info = 1;
				break;
			case 'V':
				opts->version = 1;
				break;
		}
	}
	if (optind < argc)
	{
		opts->nui_file = argv[optind];
	}
}

/*
 * Print usage information.
 */
static void
help(const char *invocation, FILE *where)
{
	fprintf(where, "USAGE\n");
	fprintf(where, "\t%s [OPTIONS...] image_file\n\n", invocation);
	fprintf(where, "OPTIONS\n");
	fprintf(where, "\t-h\tprint this help text and exit\n");
	fprintf(where, "\t-p FILE\t palette file to use\n");
	fprintf(where, "\t-V\tprint version information and exit\n");
}

/*
 * Print version information.
 */
static void
version(FILE *where)
{
	fprintf(where, "%s %d.%d.%d\n%s\n", PROGRAM_NAME,
			PROGRAM_VER_MAJOR, PROGRAM_VER_MINOR, PROGRAM_VER_PATCH,
			PROGRAM_URL);
}

/*
 * Try to figure out the terminal size, in character cells, and return that 
 * info in the given winsize structure. Returns 0 on succes, -1 on error.
 * However, you might still want to check if the ws_col and ws_row fields 
 * actually contain values other than 0. They should. But who knows.
 */
static int
term_wsize(struct winsize *ws)
{
#ifndef TIOCGWINSZ
	return -1;
#endif
	return ioctl(STDOUT_FILENO, TIOCGWINSZ, ws);
}

/*
 * Turn echoing of keyboard input on/off.
 */
static int
term_echo(int on)
{
	struct termios ta;
	if (tcgetattr(STDIN_FILENO, &ta) != 0)
	{
		return -1;
	}
	ta.c_lflag = on ? ta.c_lflag | ECHO : ta.c_lflag & ~ECHO;
	return tcsetattr(STDIN_FILENO, TCSAFLUSH, &ta);
}

/*
 * Prepare the terminal for the next paint iteration.
 */
static void
term_clear()
{
	fputws(ANSI_CLEAR_SCREEN, stdout);
	fputws(ANSI_CURSOR_RESET, stdout);
}

/*
 * Prepare the terminal for our matrix shenanigans.
 */
static void
term_setup(options_s *opts)
{
	fputws(ANSI_HIDE_CURSOR, stdout);

	// TODO image's default BG color
	//fputs(DEFAULT_BG, stdout);
	// TODO image's default FG color
	//fputws(DEFAULT_FG, stdout);
	
	term_echo(0);                      // don't show keyboard input
	
	// set the buffering to fully buffered, we're adult and flush ourselves
	//setvbuf(stdout, NULL, _IOFBF, 0);
}

/*
 * Make sure the terminal goes back to its normal state.
 */
static void
term_reset()
{
	fputws(ANSI_FONT_RESET, stdout);   // resets font colors and effects
	fputws(ANSI_SHOW_CURSOR, stdout);  // show the cursor again
	term_echo(1);                      // show keyboard input

	//setvbuf(stdout, NULL, _IOLBF, 0);
}

static void
color_fg(uint8_t color)
{
	wprintf(L"\x1b[38;5;%hhum", color);
}

static void
color_bg(uint8_t color)
{
	wprintf(L"\x1b[48;5;%hhum", color);
}

static int
print_nui(nuru_img_s *img, nuru_pal_s *pal, uint16_t cols, uint16_t rows)
{
	nuru_cell_s *cell = NULL;

	for (uint16_t r = 0; r < img->rows; ++r)
	{
		for (uint16_t c = 0; c < img->cols; ++c)
		{
			cell = nuru_get_cell(img, c, r);
			if (cell->fg != img->fg_key)
			{
				color_fg(cell->fg);
			}
			if (cell->bg != img->bg_key)
			{
				color_bg(cell->bg);
			}
			wchar_t ch = pal ? pal->codepoints[cell->ch] : cell->ch;
			fputwc(ch, stdout);
			fputws(ANSI_FONT_RESET, stdout);
		}	
		fputwc('\n', stdout);
	}
	
	return -1;
}

void
info(nuru_img_s *img)
{
	fprintf(stdout, "signature:  %s\n", img->signature);
	fprintf(stdout, "version:    %d\n", img->version);
	fprintf(stdout, "color_mode: %d\n", img->color_mode);
	fprintf(stdout, "glpyh_mode: %d\n", img->glyph_mode);
	fprintf(stdout, "mdata_mode: %d\n", img->mdata_mode);
	fprintf(stdout, "cols:       %d\n", img->cols);
	fprintf(stdout, "rows:       %d\n", img->rows);
	fprintf(stdout, "ch_key:     %d\n", img->ch_key);
	fprintf(stdout, "fg_key:     %d\n", img->fg_key);
	fprintf(stdout, "bg_key:     %d\n", img->bg_key);
	fprintf(stdout, "glyph_pal:  %s\n", img->glyph_pal);
	fprintf(stdout, "color_pal:  %s\n", img->color_pal);
}

int
main(int argc, char **argv)
{
	// parse command line options
	options_s opts = { 0 };
	parse_args(argc, argv, &opts);

	if (opts.help)
	{
		help(argv[0], stdout);
		return EXIT_SUCCESS;
	}

	if (opts.version)
	{
		version(stdout);
		return EXIT_SUCCESS;
	}

	if (opts.nui_file == NULL)
	{
		fprintf(stderr, "No image file given\n");
		return EXIT_FAILURE;
	}

	// load nuru image file
	nuru_img_s nui = { 0 };
	if (nuru_img_load(&nui, opts.nui_file) == -1)
	{
		fprintf(stderr, "Error loading image file: %s\n", opts.nui_file);
		return EXIT_FAILURE;
	}

	if (opts.info)
	{
		info(&nui);
		return EXIT_SUCCESS;
	}

	// load nuru glyph palette file
	nuru_pal_s nup = { 0 };
	if (opts.nug_file)
	{
		if (nuru_pal_load(&nup, opts.nug_file) == -1)
		{
			fprintf(stderr, "Error loading palette file: %s\n", opts.nug_file);
			return EXIT_FAILURE;
		}
	}

	// get the terminal dimensions
	struct winsize ws = { 0 };
	if (term_wsize(&ws) == -1)
	{
		fprintf(stderr, "Failed to determine terminal size\n");
		return EXIT_FAILURE;
	}

	if (ws.ws_col == 0 || ws.ws_row == 0)
	{
		fprintf(stderr, "Terminal size not appropriate\n");
		return EXIT_FAILURE;
	}

	// ensure unicode / wide-character support
	setlocale(LC_CTYPE, "");
	if (fwide(stdout, 1) != 1)
	{
		fprintf(stderr, "Couldn't put terminal in wide-character mode\n");
		return EXIT_FAILURE;
	}

	// display nuru image
	term_setup(&opts);
	term_clear();
	print_nui(&nui, opts.nug_file ? &nup : NULL, ws.ws_col, ws.ws_row);

	// clean up and cya 
	nuru_img_free(&nui);
	term_reset();
	return EXIT_SUCCESS;
}