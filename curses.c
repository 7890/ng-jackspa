/* curses.c - helper functions related to curses
 * Copyright © 2013 Géraud Meyer <graud@gmx.com>
 *
 * This file is part of ng-jackspa.
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

#define CTRL(key) ((key)-64)

/* Read user text input from a curses window with line editing keys.
 * The result is obtained from the window itself, not from the keyboard; a
 * space marks the end of the input; if the text contains a ^O character, the
 * input is cancelled (as soon as the ^O is processed).
 * The terminal must not echo, and it should be in cbreak mode (rather than in
 * line editing mode, aka cooked mode); a pseudo line editing terminal mode is
 * emulated, with additional emacs-like edit/move keys.
 */
int wtextentry(WINDOW *win, char *result, size_t size)
{
	short s; /* state varibale */
	int k; /* keyboard input */
	char c; /* input char */
	int row, col;

	wclear(win);

	for (s = 1; s > 0; )
		switch(k = wgetch(win)) {
		case KEY_CANCEL:
		case CTRL('O'): /* cancel */
		case '\e':
			s = 2;
			ungetch('\n');
			break;
		case KEY_ENTER:
		case '\n': /* eof */
			if (s != 1) s = -1;
			else s = 0;
			break;
		case KEY_BACKSPACE: /* erase */
		case '\b':
			getyx(win, row, col), wmove(win, row, col-1), wdelch(win);
			break;
		case KEY_DC: /* delete */
		case CTRL('D'):
			wdelch(win);
			break;
		case CTRL('U'): /* kill */
			wmove(win, 0, 0), wclrtoeol(win);
			break;
		case CTRL('K'):
			wclrtoeol(win);
			break;
		case KEY_UP: /* default/previous input field */
		case CTRL('P'):
			wmove(win, 0, 0), wclrtoeol(win);
			wmove(win, 0, 0), waddnstr(win, result, size);
			break;
		case CTRL('B'):
		case KEY_LEFT:
			getyx(win, row, col), wmove(win, row, col-1);
			break;
		case CTRL('F'):
		case KEY_RIGHT:
			getyx(win, row, col);
			if (col+1 < size) wmove(win, row, col+1);
			break;
		case KEY_HOME:
		case CTRL('A'):
			wmove(win, 0, 0);
			break;
		case KEY_END:
		case CTRL('E'):
			getyx(win, row, col);
			for (; col < size-1 && (c = mvwinch(win, row, col)&A_CHARTEXT) != ' '; col++);
			break;
		case ERR:
			break;
		default:
			getyx(win, row, col);
			waddch(win, k);
			break;
		}

	if (s < 0)
		return (result[0] = '\0', 1);
	else {
		for (col = 0; col < size-1 && (c = mvwinch(win, 0, col)&A_CHARTEXT) != ' '; col++)
			result[col] = c;
		result[col] = '\0';
	}

	return 0;
}
