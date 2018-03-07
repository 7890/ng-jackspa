/* jackspa.h - API for LADSPA plugin instances with JACK audio ports
 * Copyright © 2007 Nick Thomas
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

#ifndef JACKSPA_H
#define JACKSPA_H
#ifdef __cplusplus
extern "C" {
#endif

#include <jack/jack.h>
#include <ladspa.h>
#include <glib.h>

typedef struct {
	char *client_name;
	const char **port_names; /* indexed by the LADSPA port index */
	jack_client_t *jack_client;
	jack_port_t **jack_ports; /* indexed by the LADSPA port index */
	unsigned long num_control_ports; /* input control ports */
	unsigned long num_meter_ports; /* output control ports */
	float *control_port_values; /* indexed by the LADSPA port index */
	LADSPA_Handle handle;
	LADSPA_Descriptor *descriptor;
	void *library;
} state_t;

/* Initial config (command line switches) */
extern const GOptionEntry jackspa_entries[];

int jackspa_init(state_t *state, int argc, char **argv);

int jackspa_fini(state_t *state);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
