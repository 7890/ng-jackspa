/* control.c - interface to the controls of a jackspa plugin instance
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
#include <math.h>
#include <string.h>
#include <regex.h>
#include "control.h"
#include "interface.h"

LADSPA_Data control_rounding(const control_t *control, LADSPA_Data val)
{
	if (control->type == JACKSPA_INT || control->type == JACKSPA_TOGGLE)
		return nearbyintf(val);
	return val;
}

void control_exchange(control_t *control)
{
	LADSPA_Data buf;
	buf = *control->val;
	*control->val = control->sel;
	control->sel = buf;
}

/* Initial config (command line switches) */
gchar **control_bounds = NULL;
gchar **control_inits = NULL;
gchar **control_defaults = NULL;
gboolean parse_bounds(const gchar *opt, const gchar *arg,
                      gpointer data, GError **error)
{
	g_strfreev(control_bounds); /* discard a possible previous option */
	control_bounds = g_strsplit(arg, ":", -1);
	return TRUE;
}
gboolean parse_inits(const gchar *opt, const gchar *arg,
                     gpointer data, GError **error)
{
	g_strfreev(control_inits); /* discard a possible previous option */
	control_inits = g_strsplit(arg, ":", -1);
	return TRUE;
}
gboolean parse_defaults(const gchar *opt, const gchar *arg,
                        gpointer data, GError **error)
{
	g_strfreev(control_defaults); /* discard a possible previous option */
	control_defaults = g_strsplit(arg, ":", -1);
	return TRUE;
}

const GOptionEntry control_entries[] =
  {
    /* long, short, flags, arg_type, arg_data, description, arg_description */
    { "bounds", 'b', 0, G_OPTION_ARG_CALLBACK, parse_bounds,
	  "Colon separated list of multiplexed min/max values", "bound_values" },
    { "inits", 'i', 0, G_OPTION_ARG_CALLBACK, parse_inits,
	  "Colon separated list of initial values", "init_values" },
    { "defaults", 'd', 0, G_OPTION_ARG_CALLBACK, parse_defaults,
	  "Colon separated list of default values", "default_values" },
    { 0 }
  };


typedef struct {
	char with_ini;
	LADSPA_Data ini;
	char with_def;
	LADSPA_Data def;
	char with_min;
	LADSPA_Data min;
	char with_max;
	LADSPA_Data max;
} control_init_t;

/* see the interface .h file */
int control_set_value(LADSPA_Data *val, char *cmd, control_t *control)
{
	LADSPA_Data buf;
	regex_t reg;
	regmatch_t matches[1];
	const char *percent_r="^%[0-9][0-9]$";
	char *e;
	int rc;
	char err[30];

	if (cmd == NULL || !strcmp(cmd, "")) /* skip */
		return 1;
	else if (!strcmp(cmd, "<")) /* min */
		*val = control->min;
	else if (!strcmp(cmd, ">")) /* max */
		*val = control->max;
	else if (cmd[0] == '%') { /* percentage */
		if ((rc = regcomp(&reg, percent_r, 0)) ) {
			regerror(rc, &reg, err, sizeof(err));
			return (fprintf(stderr, "regex compilation failed: %s\n", err), -1);
		}
		if (regexec(&reg, cmd, 1, matches, 0))
			return (regfree(&reg), -1);
		*val = (LADSPA_Data)strtof(&cmd[1], NULL) / 100.0;
		*val = control_rounding
		  (control, (1.0-*val) * control->min + *val * control->max);
		regfree(&reg);
	}
	else if (!strcmp(cmd, "d")) { /* default */
		if (control->def)
			*val = *control->def;
		else
			return -1;
	}
	else if (!strcmp(cmd, "a")) /* active */
		*val = *control->val;
	else if (!strcmp(cmd, "s")) /* selected */
		*val = control->sel;
	else { /* float */
		buf = (LADSPA_Data)strtof(cmd, &e);
		if (e == cmd || *e != '\0')
			return -1;
		*val = buf;
	}

	return 0;
}

/* Find the initial config of the control at index ctrl */
int control_set_init(control_init_t *init,
                     unsigned long ctrl, control_t *control)
{
	int ret = 0, rc;

	rc = control_set_value(&init->min, glib_strv_index(2*ctrl, control_bounds), control);
	init->with_min = rc ? 0 : 1;
	if (rc < 0) ret = rc;
	rc = control_set_value(&init->max, glib_strv_index(2*ctrl+1, control_bounds), control);
	init->with_max = rc ? 0 : 1;
	if (rc < 0) ret = rc;

	rc = control_set_value(&init->ini, glib_strv_index(ctrl, control_inits), control);
	init->with_ini = rc ? 0 : 1;
	if (rc < 0) ret = rc;

	rc = control_set_value(&init->def, glib_strv_index(ctrl, control_defaults), control);
	init->with_def = rc ? 0 : 1;
	if (rc < 0) ret = rc;

	return ret;
}

int control_init(control_t *control, state_t *state, unsigned long port,
                 unsigned long ctrl)
{
	control->port = port;
	control->ctrl = ctrl;
	control->desc = &state->descriptor->PortDescriptors[port];
	control->hint = &state->descriptor->PortRangeHints[port];
	LADSPA_PortRangeHintDescriptor descriptor = control->hint->HintDescriptor;
	LADSPA_Data lower_bound = control->hint->LowerBound;
	LADSPA_Data upper_bound = control->hint->UpperBound;
	control_init_t init;

	control->name = state->port_names[port];
	control->val = &state->control_port_values[port];

	/* control->min, control->max */
	if (LADSPA_IS_HINT_SAMPLE_RATE(descriptor)) {
		int sample_rate = jack_get_sample_rate(state->jack_client);
		lower_bound *= sample_rate;
		upper_bound *= sample_rate;
	}
	if ( LADSPA_IS_HINT_BOUNDED_BELOW(descriptor) &&
	     LADSPA_IS_HINT_BOUNDED_ABOVE(descriptor) )
	{
		control->min = lower_bound;
		control->max = upper_bound;
	}
	else if (LADSPA_IS_HINT_BOUNDED_BELOW(descriptor)) {
		control->min = lower_bound;
		control->max = 1.0;
	}
	else if (LADSPA_IS_HINT_BOUNDED_ABOVE(descriptor)) {
		control->min = 0.0;
		control->max = upper_bound;
	}
	else {
		control->min = -1.0;
		control->max = 1.0;
	}

	/* control->def */
	if (LADSPA_IS_HINT_HAS_DEFAULT(descriptor)) {
		control->def = (LADSPA_Data *)malloc(sizeof(LADSPA_Data));
		if (!control->def)
			return (fprintf(stderr, "memory allocation error\n"), 1);
		switch (descriptor & LADSPA_HINT_DEFAULT_MASK) {
		case LADSPA_HINT_DEFAULT_MINIMUM:
			*control->def = lower_bound;
			break;
		case LADSPA_HINT_DEFAULT_LOW:
			*control->def = lower_bound * 0.75 + upper_bound * 0.25;
			break;
		case LADSPA_HINT_DEFAULT_MIDDLE:
			*control->def = lower_bound * 0.5 + upper_bound * 0.5;
			break;
		case LADSPA_HINT_DEFAULT_HIGH:
			*control->def = lower_bound * 0.25 + upper_bound * 0.75;
			break;
		case LADSPA_HINT_DEFAULT_MAXIMUM:
			*control->def = upper_bound;
			break;
		case LADSPA_HINT_DEFAULT_0:
			*control->def = 0.0;
			break;
		case LADSPA_HINT_DEFAULT_1:
			*control->def = 1.0;
			break;
		case LADSPA_HINT_DEFAULT_100:
			*control->def = 100.0;
			break;
		case LADSPA_HINT_DEFAULT_440:
			*control->def = 440.0;
			break;
		default:
			fprintf(stderr, "default not found\n");
			free(control->def), control->def = NULL;
		}
	}
	else
		control->def = NULL;

	/* Check the default */
	if (control->def) {
		if (*control->def < control->min) {
			fprintf(stderr, "default smaller than the minimum\n");
			*control->def = control->min;
		}
		if (*control->def > control->max) {
			fprintf(stderr, "default greater than the maximum\n");
			*control->def = control->max;
		}
	}

	/* control->inc & Overrides */
	if (LADSPA_IS_HINT_TOGGLED(descriptor)) {
		control->min = 0.0;
		control->max = 1.0;
		control->inc.fine = 1.0;
		control->inc.coarse = 1.0;
		control->type = JACKSPA_TOGGLE;
		if (control->def) *control->def = nearbyintf(*control->def);
	}
	else if (LADSPA_IS_HINT_INTEGER(descriptor)) {
		control->min = nearbyintf(control->min);
		control->max = nearbyintf(control->max);
		control->inc.fine = 1.0;
		control->inc.coarse = 1.0;
		control->type = JACKSPA_INT;
		if (control->def) *control->def = nearbyintf(*control->def);
	}
	else {
		control->inc.fine = (control->max - control->min) / 500;
		control->inc.coarse = (control->max - control->min) / 50;
		control->type = JACKSPA_FLOAT;
	}

	/* Initial config */
	if (control_set_init(&init, ctrl, control) < 0)
		return (fprintf(stderr, "invalid initial value given\n"), 1);
	if (init.with_min)
		control->min = init.min;
	if (init.with_max)
		control->max = init.max;
	if (init.with_def) {
		if (!control->def) {
			control->def = (LADSPA_Data *)malloc(sizeof(LADSPA_Data));
			if (!control->def)
				return (fprintf(stderr, "memory allocation error\n"), 1);
		}
		*control->def = init.def;
	}

	/* control->sel, control->val */
	if (control->def)
		control->sel = *control->def;
	else
		control->sel = control->min;
	if (init.with_ini)
		*control->val = init.ini;
	else
		*control->val = control->sel;

	return 0;
}

void control_fini(control_t *control)
{
	free(control->def), control->def = NULL;
}

int control_buildall(unsigned long *count, controls_t *controls, state_t *state)
{
	int rc = 0;
	unsigned long p, c; /* loop variables for ports */

	*count = state->num_control_ports;
	*controls = (controls_t)calloc(sizeof(control_t *), (size_t)*count);
	if (!*controls)
		return (fprintf(stderr, "memory allocation error\n"), 1);

	for (p = 0, c = 0; p < state->descriptor->PortCount; p++)
		if ( LADSPA_IS_PORT_INPUT(state->descriptor->PortDescriptors[p]) &&
		     LADSPA_IS_PORT_CONTROL(state->descriptor->PortDescriptors[p]) )
		{
			(*controls)[c] = (control_t *)malloc(sizeof(control_t));
			if (!(*controls)[c])
				return (fprintf(stderr, "memory allocation error\n"), 1);
			if (control_init((*controls)[c], state, p, c))
				rc = 1;
			c++;
		}

	return rc;
}

void control_cleanupall(unsigned long count, controls_t *controls)
{
	while (count) {
		control_fini((*controls)[--count]);
		free((*controls)[count]);
	}
	free(*controls);
}
