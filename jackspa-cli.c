/* jackspa-cli.c - simple CLI LADSPA host for the Jack Audio Connection Kit
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include "jackspa.h"
#include "control.h"

#define PROGRAM_NAME "jackspa-cli"
#include "interface.c"

#define NOTIF_BACKGROUND "continuing to run in the background"

volatile sig_atomic_t stop_signaled = 0;

static void stop_handler(int s)
{
	stop_signaled = 1;
}

static int tty = -1; /* controlling terminal or -1 */

static void background_handler(int s)
{
	int i;

	/* setup a child that will stop us then wake us up */
	i = fork();
	if (i < 0) {
		/* error, use the default handler */
		signal(SIGTSTP, SIG_DFL);
		kill(0, SIGTSTP);
	}
	else if (i == 0) {
		if (tty >= 0 && verbose)
			write( STDOUT_FILENO, PROGRAM_NAME ": " NOTIF_BACKGROUND "\n",
			  sizeof(PROGRAM_NAME)-1 + 2 + sizeof(NOTIF_BACKGROUND)-1 + 1 );
		/* make sure the parent is not in the foreground (or has no ctty) */
		for (i = 1; tcgetpgrp(tty) == getpgid(getppid()); i <<= 1) {
			kill(getppid(), SIGSTOP); /* let the shell regain control of the tty */
			usleep(i), kill(getppid(), SIGCONT);
		}
		_exit(0);
    }
}

volatile sig_atomic_t print_signaled = 0;

static void printcontrols_handler(int s)
{
	print_signaled = 1;
}

void printcontrols_async(unsigned long c, controls_t controls)
{
	if (!print_signaled) return;

	for (; c; c--, controls++)
		printf("%e\t# %s\n", *(*controls)->val, (*controls)->name);
	print_signaled = 0;
}

int main(int argc, char *argv[])
{
	int rc = 0;

	tty = open("/dev/tty", O_NOCTTY);

	/* Signals */
	struct sigaction act;
	act.sa_flags = 0;
	rc = !sigemptyset(&act.sa_mask);
	if (rc) {
		/* SIGINT handler to exit gracefully */
		act.sa_handler = stop_handler;
		rc = !sigaction(SIGINT, &act, NULL) && rc;
		/* SIGTTIN handler to avoid being stopped */
		act.sa_handler = SIG_IGN;
		rc = !sigaction(SIGTTIN, &act, NULL) && rc;
		/* SIGTSTP handler to go into the background */
		act.sa_handler = background_handler;
		rc = !sigaction(SIGTSTP, &act, NULL) && rc;
		/* SIGCHLD handler to avoid a child to become defunct */
		act.sa_handler = SIG_IGN;
		rc = !sigaction(SIGCHLD, &act, NULL) && rc;
		/* SIGUSR1 to print the active values */
		act.sa_handler = printcontrols_handler;
		rc = !sigaction(SIGUSR1, &act, NULL) && rc;
	}
	if (!rc)
		fputs(PROGRAM_NAME ": error while setting up the signal handlers\n", stderr);

	/* Command line options */
	GOptionContext *context = interface_context();
	GError *error = NULL;
	if (!g_option_context_parse(context, &argc, &argv, &error))
		return (fprintf(stderr, "option parsing failed: %s\n", error->message), -1);

	/* Initialize jackspa */
	state_t state;
	if (!jackspa_init(&state, argc, argv))
		return 1;

	/* Initialize the controls with the requested values */
	unsigned long c;
	controls_t controls;
	if (control_buildall(&c, &controls, &state))
		return 2;

	/* Wait */
	if (!isatty(fileno(stdin)))
		while (!stop_signaled) {
			printcontrols_async(c, controls);
			sleep(-1);
		}
	else
		while (!stop_signaled && !feof(stdin)) {
			printcontrols_async(c, controls);
			if (getpgrp() == tcgetpgrp(STDIN_FILENO))
				/* we are in the foreground process group */
				getchar();
			else usleep(100000);
		}

	control_cleanupall(c, &controls);
	jackspa_fini(&state);

	return 0;
}
