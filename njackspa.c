/* njackspa.c - simple ncurses LADSPA host for the Jack Audio Connection Kit
 *   based on njconnect.c by Xj <xj@wp.pl>
 * Copyright © 2012,2013 Xj <xj@wp.pl>
 * Copyright © 2013 Géraud Meyer <graud@gmx.com>
 *
 * This file is part if ng-jackspa.
 *
 * ng-jackspa is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with ng-jackspa.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ncurses.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "jackspa.h"
#include "control.h"

#define PROGRAM_NAME "njackspa"
#include "interface.c"
#include "curses.c"

#define ERR_OUT(format, arg...) ( endwin(), fprintf(stderr, format "\n", ## arg), refresh() )

#define CON_NAME "Controls"
#define HELP "'q'uit, '?', 'TAB', U/D-selection, L/R-change, 'd/D'efault, '>/<', '<n>'0%, 'u/U'pdate, e'x/X'change, 'i'nput, 'N'arrow"
#define ERR_NODEFAULT "No default value"
#define ERR_INVALIDVALUE "Invalid value entered"

#define NUM_WINDOWS 1

#define WCON_X 0
#define WCON_Y 0
#define WCON_W cols
#define WCON_H rows - 1

#define WHLP_X 0
#define WHLP_Y rows - 1
#define WHLP_W cols
#define WHLP_H 0

struct window {
	WINDOW *window_ptr;
	state_t *state;
	controls_t controls;
	unsigned long count; /* number of controls */
	unsigned long index;
	bool selected;
	int height;
	int width;
	const char *name;
	enum { Oneline, Twolines } layout; /* state variable */
	enum { Active, Alternative } sel; /* state variable */
};

void window_item_next(struct window* w) { if (w->index < w->count - 1) w->index++; }
void window_item_previous(struct window* w) { if (w->index > 0) w->index--; }
void suppress_jack_log(const char *msg) { ; /* Just suppress Jack SPAM here ;-) */ }


int init_window(struct window *window, state_t *state)
{
	int rc = 0;

	rc = control_buildall(&window->count, &window->controls, state);
	window->state = state;
	window->index = 0;

	return rc;
}

void draw_border(struct window * window_ptr)
{
	int col = ( window_ptr->width - strlen(window_ptr->name) -
	            strlen(window_ptr->state->descriptor->Label) - 5 ) / 2;
	if (col < 0) col = 0;

	/* 0, 0 gives default characters for the vertical and horizontal lines */
	box(window_ptr->window_ptr, 0, 0);

	if (window_ptr->selected) {
		wattron(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(4));
		mvwprintw( window_ptr->window_ptr, 0, col, "=[%s:%s]=",
		           window_ptr->state->descriptor->Label, window_ptr->name );
		wattroff(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(4));
	}
	else
		mvwprintw( window_ptr->window_ptr, 0, col, " [%s:%s] ",
		           window_ptr->state->descriptor->Label, window_ptr->name );
}

void draw_controls(struct window* window_ptr)
{
	int row, col, color, high, rows, cols;
	  /* start position of a control, colors, screen size */
	unsigned long c; /* loop variable for controls */
	long offset; /* first displayed control index */
	control_t *control;
	char fmt[40]; /* buffer for printf format strings */
	int nw, vw, bw; /* computed field widths */
	int height; /* lines for one control */

	getmaxyx(window_ptr->window_ptr, rows, cols);
	if (window_ptr->layout == Oneline) {
		nw = cols-2 - (cols/6-2)*4 - 9;
		vw = cols/6-2;
		bw = vw;
		height = 1;
	}
	else {
		bw = cols/3-2;
		nw = 2*bw + 4;
		vw = cols-2 - 2*(cols/3-2) - 6;
		height = 2;
	}
	offset = (long)window_ptr->index+1 + (2-rows)/height;
	if ((rows-2)/height < 1) offset--;
	if (offset < 0) offset = 0;

	col = 1;
	for (c = offset, row = 1; c < window_ptr->count && row < rows-1; c++, row++) {
		control = (window_ptr->controls)[c];
		high = (row-1 == (window_ptr->index - offset) * height) ?
		  (window_ptr->selected) ? 3 : 2 : 1;
		  /* currently selected widget (higlighted or normal) */
		color = (row-1 == (window_ptr->index - offset) * height) ?
		  (window_ptr->selected) ? 4 : 2 : 1;
		  /* emphasized widget (white or normal) */

		/* Name [nw] */
		wattron(window_ptr->window_ptr, COLOR_PAIR(color));
		snprintf(fmt, sizeof(fmt), "%%-%d.%ds", nw, nw);
		mvwprintw(window_ptr->window_ptr, row, col, fmt, control->name);
		/* Separator ':' [2] */
		wattron(window_ptr->window_ptr, COLOR_PAIR(1));
		wprintw(window_ptr->window_ptr, ": ");
		/* Active value (bold) [vw] */
		snprintf(fmt, sizeof(fmt), "%%%dF", vw);
		if (window_ptr->sel == Active) {
			wattron(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(high));
			wprintw(window_ptr->window_ptr, fmt, *control->val);
			wattroff(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(high));
		}
		else {
			wattron(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(color));
			wprintw(window_ptr->window_ptr, fmt, *control->val);
			wattroff(window_ptr->window_ptr, WA_BOLD|COLOR_PAIR(color));
		}
		if (window_ptr->layout == Oneline) {
			/* Type [3] */
			wattron(window_ptr->window_ptr, COLOR_PAIR(1));
			wprintw( window_ptr->window_ptr, " %s ",
			         (control->type == JACKSPA_TOGGLE) ?
		           "?" : (control->type == JACKSPA_INT) ? "i" : "f" );
			/* Selection (emphasized) [vw] */
			snprintf(fmt, sizeof(fmt), "%%-%dF", vw);
			if (window_ptr->sel == Active)
				wattron(window_ptr->window_ptr, COLOR_PAIR(color));
			else
				wattron(window_ptr->window_ptr, COLOR_PAIR(high));
			wprintw(window_ptr->window_ptr, fmt, control->sel);
			/* Range [2*bw + 4] */
			wattron(window_ptr->window_ptr, COLOR_PAIR(1));
			snprintf(fmt, sizeof(fmt), " [%%-%dF~%%%dF]", bw, bw);
			wprintw(window_ptr->window_ptr, fmt, control->min, control->max);
			wattroff(window_ptr->window_ptr, COLOR_PAIR(1));
		}
		else {
			wclrtoeol(window_ptr->window_ptr);

			row++;
			/* Range & Type [2*bw + 6] */
			wattron(window_ptr->window_ptr, COLOR_PAIR(1));
			snprintf( fmt, sizeof(fmt), "  %%-%dF~%s~%%%dF ", bw,
			  (control->type == JACKSPA_TOGGLE) ?
		           "?" : (control->type == JACKSPA_INT) ? "i" : "f",
			  bw );
			mvwprintw(window_ptr->window_ptr, row, col, fmt, control->min, control->max);
			wattroff(window_ptr->window_ptr, COLOR_PAIR(1));
			/* Selection (emphasized) [vw] */
			snprintf(fmt, sizeof(fmt), "%%%dF", vw);
			if (window_ptr->sel == Active)
				wattron(window_ptr->window_ptr, COLOR_PAIR(color));
			else
				wattron(window_ptr->window_ptr, COLOR_PAIR(high));
			wprintw(window_ptr->window_ptr, fmt, control->sel);
			wattroff(window_ptr->window_ptr, COLOR_PAIR(1));
		}
		wclrtoeol(window_ptr->window_ptr);
	}
	if (row < rows-1 && window_ptr->layout != Oneline) {
		/* clear the unused last line */
		wmove(window_ptr->window_ptr, row, col);
		wclrtoeol(window_ptr->window_ptr);
	}

	draw_border(window_ptr);
	wrefresh(window_ptr->window_ptr);
}

void
create_window(struct window * window_ptr, int height, int width,
              int starty, int startx, const char * name)
{
	window_ptr->window_ptr = newwin(height, width, starty, startx);
	window_ptr->selected = FALSE;
	window_ptr->width = width;
	window_ptr->height = height;
	window_ptr->name = name;
	window_ptr->index = 0;
	window_ptr->controls = NULL;
	window_ptr->count = 0;
	window_ptr->layout = Oneline;
	window_ptr->sel = Alternative;
	scrollok(window_ptr->window_ptr, FALSE);
}

void
resize_window(struct window * window_ptr, int height, int width, int starty, int startx)
{
	/* delwin(window_ptr->window_ptr); */
	/* window_ptr->window_ptr = newwin(height, width, starty, startx); */
	wresize(window_ptr->window_ptr, height, width);
	mvwin(window_ptr->window_ptr, starty, startx);
	window_ptr->width = width;
	window_ptr->height = height;
}

void cleanup_windows(struct window* windows)
{
	short i;
	struct window* w = windows;

	for (i = 0; i < NUM_WINDOWS; i++, w++)
		control_cleanupall(w->count, &w->controls);
}

unsigned short
select_window(struct window* windows, int cur, int nex)
{
	if (nex == cur)
		return cur;
	else if (nex >= NUM_WINDOWS)
		nex = 0;
	else if (nex < 0)
		nex = NUM_WINDOWS - 1;

	windows[cur].selected = FALSE;
	windows[nex].selected = TRUE;
	return nex;
}

void draw_help(WINDOW* w, int c, const char* msg, float dsp_load, bool rt)
{
	int cols;
	cols = getmaxx(w);

	wmove(w, 0, 0);
	wclrtoeol(w);

	wattron(w, COLOR_PAIR(c));
	mvwprintw(w, 0, 1, msg);
	wattroff(w, COLOR_PAIR(c));

	wattron(w, COLOR_PAIR(7));
	mvwprintw(w, 0, cols-12, "DSP:%4.2f%s", dsp_load, rt ? "@RT" : "!RT" );
	wattroff(w, COLOR_PAIR(7));

	wrefresh(w);
}

#define cl ((*C)[W->index])
  /* used in main() */

int main(int argc, char *argv[])
{
	unsigned short rc = 0; /* return code */
	int rows, cols; /* screen size */
	struct window windows[NUM_WINDOWS];
	unsigned short window_selection = 0; /* state variable */
	struct window *W; /* selected window */
	controls_t *C; /* controls of the selected window */
	LADSPA_Data *mod; /* value of the selected window to be acted upon */
	unsigned long c; /* loop variable for controls */
	WINDOW *help_window;
	const char *err_message = NULL; /* state variable */
	bool want_refresh = FALSE, help = TRUE; /* state variables */
	bool rt; /* jack RT capability */
	int k, i; /* current curses character, loop variable */
	LADSPA_Data val; /* buffering variable */
	char input[40] = { 0 }; /* text input */

	/* Command line options */
	GOptionContext *context = interface_context();
	GError *error = NULL;
	if (!g_option_context_parse(context, &argc, &argv, &error))
		return (ERR_OUT("option parsing failed: %s\n", error->message), -1);

	/* Initialize ncurses */
	initscr();
	curs_set(0); /* set cursor invisible */
	cbreak();
	noecho();
	getmaxyx(stdscr, rows, cols);

	if (has_colors() == FALSE) {
		rc = -1, ERR_OUT("Your terminal does not support color");
		goto qxit;
	}
	start_color();
	init_pair(1, COLOR_CYAN, COLOR_BLACK);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);
	init_pair(3, COLOR_BLACK, COLOR_GREEN);
	init_pair(4, COLOR_WHITE, COLOR_BLACK);
	init_pair(5, COLOR_BLACK, COLOR_RED);
	init_pair(6, COLOR_YELLOW, COLOR_BLACK);
	init_pair(7, COLOR_BLUE, COLOR_BLACK);

	/* Create Help Window */
	help_window = newwin(WHLP_H, WHLP_W, WHLP_Y, WHLP_X);
	keypad(help_window, TRUE);
	wtimeout(help_window, 3000);

	/* Some Jack versions are very aggressive in breaking view */
	jack_set_info_function(suppress_jack_log);
	jack_set_error_function(suppress_jack_log);

	/* Initialize jack */
	state_t state;
	endwin();
	if (!jackspa_init(&state, argc, argv)) {
		rc = -1;
		goto qxit;
	}
	refresh();
	rt = (bool)jack_is_realtime(state.jack_client);

	/* Create windows */
	create_window(windows+0, WCON_H, WCON_W, WCON_Y, WCON_X, CON_NAME);
	windows[window_selection].selected = TRUE;

	/* Build controls */
	endwin();
	if (init_window(windows+0, &state)) {
		rc = -1;
		goto quit_no_clean;
	}
	refresh();

loop:
	for (i = 0; i < NUM_WINDOWS; i++) draw_controls(windows+i);
	W = &windows[window_selection];
	C = &W->controls;
	mod = W->sel == Active ? cl->val : &cl->sel;

	if (err_message) {
		draw_help(help_window, 5, err_message, jack_cpu_load(state.jack_client), rt);
		err_message = NULL;
	}
	else
		draw_help(help_window, 6, help ? HELP : W->state->descriptor->Name,
		          jack_cpu_load(state.jack_client), rt);

	switch (k = wgetch(help_window)) {
	case KEY_EXIT:
	case 'q':
		rc = 0; goto quit;
	case 'r':
	case KEY_RESIZE:
		getmaxyx(stdscr, rows, cols);
		wresize(help_window, WHLP_H, WHLP_W);
		mvwin(help_window, WHLP_Y, WHLP_X);
		resize_window(windows+0, WCON_H, WCON_W, WCON_Y, WCON_X);
		goto refresh;
	case 'u':
		if (W->sel == Active)
			cl->sel = *cl->val;
		else
			*cl->val = cl->sel;
		goto loop;
	case 'U':
		if (W->sel == Active)
			for (c = 0; c < W->count; c++)
				(*C)[c]->sel = *(*C)[c]->val;
		else
			for (c = 0; c < W->count; c++)
				*(*C)[c]->val = (*C)[c]->sel;
		goto loop;
	case 'x':
		control_exchange(cl);
		goto loop;
	case 'X':
		for (c = 0; c < W->count; c++)
			control_exchange((W->controls)[c]);
		goto loop;
	case KEY_BACKSPACE:
	case 'd':
		if (cl->def) *mod = *cl->def;
		else err_message = ERR_NODEFAULT;
		if (k == KEY_BACKSPACE) window_item_next(W);
		goto loop;
	case 'D':
		if (W->sel == Active) {
			for (c = 0; c < W->count; c++)
				if ((*C)[c]->def) *(*C)[c]->val = *(*C)[c]->def;
				else err_message = ERR_NODEFAULT;
		}
		else {
			for (c = 0; c < W->count; c++)
				if ((*C)[c]->def) (*C)[c]->sel = *(*C)[c]->def;
				else err_message = ERR_NODEFAULT;
		}
		goto loop;
	case 'h':
	case KEY_LEFT:
		*mod -= cl->inc.coarse;
		if (*mod < cl->min) *mod = cl->min;
		goto loop;
	case 'l':
	case KEY_RIGHT:
		*mod += cl->inc.coarse;
		if (*mod > cl->max) *mod = cl->max;
		goto loop;
	case 'H':
		*mod -= cl->inc.fine;
		if (*mod < cl->min) *mod = cl->min;
		goto loop;
	case 'L':
		*mod += cl->inc.fine;
		if (*mod > cl->max) *mod = cl->max;
		goto loop;
	case '<':
		*mod = cl->min;
		goto loop;
	case '>':
		*mod = cl->max;
		goto loop;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		val = ((LADSPA_Data)(k - 48)) / 10.0;
		*mod = control_rounding(cl, (1.0 - val) * cl->min + val * cl->max);
		goto loop;
	case 'i':
	case CTRL('O'):
		curs_set(1);
		wtextentry(help_window, input, sizeof(input));
		if (control_set_value(mod, input, cl) < 0)
			err_message = ERR_INVALIDVALUE;
		curs_set(0);
		goto loop;
	case KEY_DOWN:
	case 'j':
		window_item_next(W);
		goto loop;
	case KEY_UP:
	case 'k':
		window_item_previous(W);
		goto loop;
	case KEY_HOME:
	case 'g':
		(windows+window_selection)->index = 0;
		goto loop;
	case KEY_END:
	case 'G':
		(windows+window_selection)->index = (windows+window_selection)->count - 1;
		goto loop;
	case '\t':
		W->sel = W->sel == Active ? Alternative : Active;
		goto loop;
	case 'J':
		W->sel = Alternative;
		goto loop;
	case 'K':
		W->sel = Active;
		goto loop;
	case KEY_BTAB:
	case 'N':
	case CTRL('L'):
		W->layout = W->layout == Oneline ? Twolines : Oneline;
		goto refresh;
	case '?':
		help = !help;
	}
	if (!want_refresh) goto loop;
refresh:
	want_refresh = FALSE;
	for (i = 0; i < NUM_WINDOWS; i++) {
		if (windows[i].index > windows[i].count - 1) windows[i].index = 0;
		wclear(windows[i].window_ptr);
	}
	goto loop;

quit:
	cleanup_windows(windows);
quit_no_clean:
	jackspa_fini(&state);
qxit:
	endwin();

	return rc;
}
