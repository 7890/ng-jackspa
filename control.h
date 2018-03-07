/* control.h - API for interfaces to the controls of a jackspa plugin instance
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

#ifndef CONTROL_H
#define CONTROL_H
#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <ladspa.h>
#include "jackspa.h"

enum ctype {
	JACKSPA_FLOAT,
	JACKSPA_INT,
	JACKSPA_TOGGLE
};

typedef struct {
	unsigned long port;
	unsigned long ctrl;
	const char *name;
	const LADSPA_PortDescriptor *desc;
	const LADSPA_PortRangeHint *hint;
	/* value in the plugin */
	LADSPA_Data *val;
	/* values selected in the interface */
	LADSPA_Data sel;
	/* value range */
	LADSPA_Data min;
	LADSPA_Data max;
	LADSPA_Data *def;
	enum ctype type;
	struct { LADSPA_Data fine; LADSPA_Data coarse; } inc;
} control_t;


/* Return a value close to val that is valid for the given control */
/* val is supposed to be inside the min/max of control */
LADSPA_Data control_rounding(const control_t *control, LADSPA_Data val);

/* Exchange the selected and the active values */
void control_exchange(control_t *control);

/* Interpret the given command to obtain a value depending of the control
 * parameters.
 * Return 0 if the value was set, 1 if the command was to skip, -1 otherwise
 * (invalid command or error) */
int control_set_value(LADSPA_Data *dest, char *cmd, control_t *control);


/* Initial config (command line switches) */
extern const GOptionEntry control_entries[];

/* Compute the type, the bounds, the default value and the increments */
int control_init(control_t *control, state_t *state, unsigned long port,
                 unsigned long ctrl);

void control_fini(control_t *control);


/* Array of pointers */
/* Using an array type would be invalid C++ */
typedef control_t **controls_t;

/* Allocate an array *controls and initialise it with allocated controls (and
 * set *count to the number of controls) */
int control_buildall(unsigned long *count, controls_t *controls, state_t *state);

/* Deallocate the array *controls */
void control_cleanupall(unsigned long count, controls_t *controls);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
