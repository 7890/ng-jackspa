/* jackspa.c - LADSPA plugin instance with JACK audio ports
 * Copyright © 2007 Nick Thomas
 * Copyright © 2013 Géraud Meyer <graud@gmx.com>
 *
 * This file is part of ng-jackspa.
 *
 * ng-jackpsa is free software; you can redistribute it and/or modify it under
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

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jackspa.h"
#include "interface.h"

#include "ladspa.c"


/* Initial config (command line) */
static gboolean jackexactname = FALSE;
static gboolean jacknostartserver = FALSE;
static gchar *jackclientname = NULL;
static gchar **portnames = NULL;
gboolean parse_names(const gchar *opt, const gchar *arg,
                     gpointer data, GError **error)
{
	g_strfreev(portnames); /* discard a possible previous option */
	portnames = g_strsplit(arg, ":", -1);
	return TRUE;
}
static gboolean controlvout = FALSE;
static gint controlvin = 0;
gboolean parse_cvio(const gchar *opt, const gchar *arg,
                   gpointer data, GError **error)
{
	controlvin = 2;
	return TRUE;
}
const GOptionEntry jackspa_entries[] =
  {
    /* long, short, flags, arg_type, arg_data, description, arg_description */
    { "jack-exactname", 'N', 0, G_OPTION_ARG_NONE, &jackexactname,
      "Do not allow JACK to generate an unused client name", NULL },
    { "jack-nostartserver", 'S', 0, G_OPTION_ARG_NONE, &jacknostartserver,
      "Do not start a JACK server if none is running", NULL },
    { "jack-client", 'j', 0, G_OPTION_ARG_STRING, &jackclientname,
      "Use the given name instead of the concatenation of a fixed prefix and the plugin label", "client_name" },
    { "names", 'n', 0, G_OPTION_ARG_CALLBACK, parse_names,
      "Colon separated list of overriding names for the LADPSA audio and control ports", "port_names" },
    { "controlv-out", 'O', 0, G_OPTION_ARG_NONE, &controlvout,
      "Export the control outputs as control voltages on JACK audio ports", NULL },
    { "controlv-in", 'I', 0, G_OPTION_ARG_NONE, &controlvin,
      "Export the control inputs as control voltages on JACK input audio ports", NULL },
    { "controlv-inasout", 'T', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, parse_cvio,
      "Export the control inputs as control voltages on JACK output audio ports not on input ones", NULL },
    { 0 }
  };
static char *ladspa_library = NULL;
typedef struct {
	char is_label;
	union { unsigned long uid; char *label; } id;
} plugin_id_t;
static plugin_id_t ladspa_id = { 0 };


/* Finds the plugin descriptor with the given ID in the given object file,
 * storing it in state->descriptor. Returns 1 on success. On failure, returns 0
 * and sets *error to an appropriate error message.
 */
int find_plugin(state_t *state, const char *file, const plugin_id_t plugin_id,
                const char **error)
{
	int err = 0;
	unsigned long i; /* port index */
	LADSPA_Descriptor *(*descriptor_fun)(unsigned long index);
	LADSPA_Descriptor *descriptor;

	/* Open the library. */
	state->library = ladspa_dlopen(file, RTLD_LAZY);
	if (!state->library)
		err = 1, *error = dlerror();

	/* Find the ladspa_descriptor() function. */
	if (!err) {
		descriptor_fun = (LADSPA_Descriptor *(*)(unsigned long))
		  dlsym(state->library, "ladspa_descriptor");
		if (!descriptor_fun)
			err = 1, *error = dlerror();
	}

	/* Find the appropriate descriptor. */
	for (i = 0; !err; i++) {
		descriptor = descriptor_fun(i);

		if (!descriptor)
			err = 1, *error = plugin_id.is_label ?
			  "no such plugin label in the given library" :
			  "no such plugin UID in the given library";
		else if ( (!plugin_id.is_label && descriptor->UniqueID == plugin_id.id.uid) ||
		          (plugin_id.is_label && !strcmp(descriptor->Label, plugin_id.id.label)) )
		{
			state->descriptor = descriptor;
			break;
		}
	}

	return !err;
}

int close_plugin(state_t *state, const char **error)
{
	state->descriptor = NULL;

	if (dlclose(state->library))
		return (*error = dlerror(), 0);
	state->library = NULL;

	return 1;
}

/* The JACK processing callback. */
int process(jack_nframes_t nframes, void *arg)
{
	state_t *state = (state_t *)arg;
	unsigned long p; /* loop variable for ports */
	const LADSPA_PortDescriptor *port; /* loop variable for port descriptors */
	void *buffer; /* JACK buffer */
	LADSPA_Data buf; /* buffer for the control outputs */

	/* Connect audio ports and unused control ports */
	for ( p = 0, port = state->descriptor->PortDescriptors;
	      p < state->descriptor->PortCount; p++, port++ )
	{
		if (LADSPA_IS_PORT_CONTROL(*port)) {
			if (LADSPA_IS_PORT_INPUT(*port) && !controlvin)
				continue;
			if (LADSPA_IS_PORT_OUTPUT(*port) && !controlvout) {
				state->descriptor->connect_port(state->handle, p, &buf);
				  /* forget the control output ports values */
				continue;
			}
		}

		buffer = jack_port_get_buffer(state->jack_ports[p], nframes);
		if (LADSPA_IS_PORT_CONTROL(*port)) {
			if (LADSPA_IS_PORT_OUTPUT(*port))
				memset(buffer, 0, (size_t)nframes); /* might be needless */
				  /* only the first sample will be set */
			else if (controlvin == 1 && !jack_port_connected(state->jack_ports[p]))
				*(LADSPA_Data *)buffer = state->control_port_values[p];
				  /* use the previous value (or the value set in the UI) */
			else if (controlvin == 1)
				state->control_port_values[p] = *(LADSPA_Data *)buffer;
				  /* record the new control value */
			else {
				memset(buffer, 0, (size_t)nframes); /* might be needless */
				*(LADSPA_Data *)buffer = state->control_port_values[p];
			}
		}
		state->descriptor->connect_port
		  (state->handle, p, (LADSPA_Data *)buffer);
	}

	/* Run the plugin. */
	state->descriptor->run(state->handle, nframes);

	return 0;
}

/* Initializes JACK, opening the appropriate ports, instantiating the LADSPA
 * Plugin, and starting the synthesis thread running.. Returns 1 on success. On
 * failure, returns 0 and sets *error to an appropriate error message.
 */
int init_jack(state_t *state, const char **error)
{
	static char client_name_prefix[] = "jackspa_";
	int err = 0;
	jack_options_t oflags;
	jack_status_t jack_status;
	unsigned long p; /* loop variable for ports */
	const LADSPA_PortDescriptor *port; /* loop variable for port descriptors */
	enum JackPortFlags pflags;

	/* Allocate memory for the client name. */
	if (!jackclientname) {
		state->client_name = (char *)calloc
		  ( sizeof(client_name_prefix) + strlen(state->descriptor->Label),
		    sizeof(char) );
		if (!state->client_name)
			err = 1, *error = strerror(errno);
	}

	/* Set the client name. */
	if (!err) {
		if (jackclientname)
			state->client_name = jackclientname;
		else {
			/* state->client_name[0] = '\0'; */ /* done by calloc */
			strcat(state->client_name, client_name_prefix);
			strcat(state->client_name, state->descriptor->Label);
		}
	}

	/* Open JACK. */
	if (!err) {
		oflags = JackNullOption;
		if (jackexactname) oflags |= JackUseExactName;
		if (jacknostartserver) oflags |= JackNoStartServer;
		state->jack_client =
		  jack_client_open(state->client_name, oflags, &jack_status);
		if (!state->jack_client)
			err = 1, *error = "could not connect to JACK";
	}

	/* Free memory for the client name */
	if (!jackclientname) free(state->client_name);
	if (!err) state->client_name = jack_get_client_name(state->jack_client);

	/* Allocate memory for the list of ports. */
	if (!err) {
		state->jack_ports = (jack_port_t **)calloc
		  (state->descriptor->PortCount, sizeof(jack_port_t *));
		if (!state->jack_ports)
			err = 1, *error = strerror(errno);
	}

	/* Allocate memory for the list of control port values. */
	if (!err) {
		state->control_port_values = (LADSPA_Data *)calloc
		  ((size_t)state->descriptor->PortCount, sizeof(LADSPA_Data));
		if (!state->control_port_values)
			err = 1, *error = strerror(errno);
	}

	/* Register ports. */
	state->num_control_ports = 0; state->num_meter_ports = 0;
	for ( p = 0, port = state->descriptor->PortDescriptors;
	      !err && p < state->descriptor->PortCount; p++, port++ )
	{
		/* Get the actual port name */
		state->port_names[p] = glib_strv_index(p, portnames);
		if (!state->port_names[p] || *state->port_names[p] == '\0')
			state->port_names[p] = state->descriptor->PortNames[p];

		/* Count control ports and skip non exported ports */
		if (LADSPA_IS_PORT_CONTROL(*port)) {
			if (LADSPA_IS_PORT_INPUT(*port)) {
				state->num_control_ports++;
				if (!controlvin) continue;
			}
			else {
				state->num_meter_ports++;
				if (!controlvout) continue;
			}
		}

		if (LADSPA_IS_PORT_INPUT(*port)) {
			if (controlvin != 1 && LADSPA_IS_PORT_CONTROL(*port))
				pflags = JackPortIsOutput;
			else
				pflags = JackPortIsInput;
		}
		else
			pflags = JackPortIsOutput;

		state->jack_ports[p] = jack_port_register
		  ( state->jack_client, state->port_names[p],
		    JACK_DEFAULT_AUDIO_TYPE, pflags, 0 );

		if (!state->jack_ports[p])
			err = 1, *error = "could not register JACK ports";
	}

	/* Register our processing callback. */
	if (!err)
		if (jack_set_process_callback(state->jack_client, &process, state))
			err = 1, *error = "could not register the JACK processing callback";

	/* Instantiate the LADSPA plugin. */
	if (!err) {
		state->handle = state->descriptor->instantiate
		  (state->descriptor, jack_get_sample_rate(state->jack_client));
		if (!state->handle)
			err = 1, *error = "could not instantiate the plugin.";
	}

	/* Connect input control ports. */
	if (!err)
		for (p = 0; p < state->descriptor->PortCount; p++)
			if ( LADSPA_IS_PORT_CONTROL(state->descriptor->PortDescriptors[p]) &&
			     LADSPA_IS_PORT_INPUT(state->descriptor->PortDescriptors[p]))
			{
				state->descriptor->connect_port
				  (state->handle, p, &state->control_port_values[p]);
				/* state->control_port_values[p] = 0.0; */ /* done by calloc() */
			}

	/* Activate the LADSPA plugin. */
	if (!err)
		if (state->descriptor->activate)
			state->descriptor->activate(state->handle);

	/* Get the bits flowing. */
	if (!err)
		if (jack_activate(state->jack_client))
			err = 1, *error = "could not activate audio processing";

	return !err;
}

typedef enum {
	st_want_plugin_file,
	st_want_plugin_id,
	st_done
} arg_parser_state;

/* Parses command-line arguments. Returns 1 on success. On failure, returns 0
 * and sets *error to an appropriate error message.
 */
int parse_args(state_t *state, int argc, char **argv, char **plugin_file,
               plugin_id_t *plugin_id, const char **error)
{
	int err = 0;
	int i; /* arg index */
	arg_parser_state st = st_want_plugin_file;
	char *e;

	argc--, argv++; /* skip the program name */

	for (i = 0; i < argc; i++)
		switch (st) {
		case st_want_plugin_file:
			*plugin_file = argv[i];
			st = st_want_plugin_id;
			break;

		case st_want_plugin_id:
			plugin_id->is_label = 0, plugin_id->id.uid = strtol(argv[i], &e, 0);
			if (e == argv[i] || *e != '\0')
				plugin_id->is_label = 1, plugin_id->id.label = argv[i];
			st = st_done;
			break;

		case st_done:
		default:
			err = 1, *error = "extra arguments given";
			break;
		}

	if (st != st_done)
		err = 1, *error = "must supply a plugin file and a plugin ID";

	return !err;
}

int jackspa_init(state_t *state, int argc, char **argv)
{
	const char *error;
	int err = 0;

	state->jack_client = 0;

	err = !parse_args(state, argc, argv, &ladspa_library, &ladspa_id, &error);

	if (!err)
		err = !find_plugin(state, ladspa_library, ladspa_id, &error);

	if (!err) {
		state->port_names =
		  (const char **)calloc(state->descriptor->PortCount, sizeof(char *));
		if (!state->port_names)
			err = 1, error = "memory allocation error";
	}

	if (!err)
		err = !init_jack(state, &error);

	if (err)
		fprintf(stderr, "jackspa error: %s\n", error);

	return !err;
}

int jackspa_fini(state_t *state)
{
	int err = 0;
	const char *error;

	if ((err = jack_client_close(state->jack_client)))
		return !err;
	state->jack_client = 0;

	if (state->descriptor->deactivate)
		state->descriptor->deactivate(state->handle);
	state->descriptor->cleanup(state->handle);
	state->handle = 0;

	free(state->control_port_values), state->control_port_values = NULL;
	free(state->jack_ports), state->jack_ports = NULL;
	state->num_control_ports = 0; state->num_meter_ports = 0;

	free(state->port_names);

	err = !close_plugin(state, &error);

	return !err;
}
