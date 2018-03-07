/* interface.c - common parts of the simple LADSPA hosts
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

#include "interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PROGRAM_NAME must be defined by the interface */
#define PACKAGE_NAME "ng-jackspa"
#define PACKAGE_SUMMARY \
  PACKAGE_NAME " is a set of simple interfaces that host a LADSPA plugin, " \
  "providing JACK ports for its audio inputs and outputs, and dynamic setting " \
  "of its control inputs."
#define PACKAGE_DESCRIPTION  "ng-jackspa is accompanied by a manual page."


static gint verbose = 1;
static gboolean verbose_more(const gchar *opt, const gchar *arg,
                             gpointer data, GError **error)
{ verbose++; return TRUE; }
static gboolean verbose_less(const gchar *opt, const gchar *arg,
                             gpointer data, GError **error)
{ verbose--; return TRUE; }

static GOptionEntry interface_entries[] =
  {
    /* long, short, flags, arg_type, arg_data, description, arg_description */
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  (gpointer)verbose_more, "Be more verbose", NULL },
    { "quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  (gpointer)verbose_less, "Be quieter", NULL },
    { 0 }
  };

/* Return a member of a NULL-terminated string array */
gchar *glib_strv_index(unsigned long i, gchar **str_array)
{
	for (; str_array && *str_array && i; str_array++, i--);
	  /* it seems *str_array should be set to NULL to mark the end */
	if (str_array)
		return *str_array;
	return NULL;
}


GOptionContext * interface_context()
{
	/* Context & Main entries */
	GOptionContext *context;
	context = g_option_context_new("PLUGIN_LIBRARY UID_OR_LABEL");
	g_option_context_set_help_enabled(context, TRUE);
	g_option_context_set_ignore_unknown_options(context, TRUE);
	g_option_context_set_summary
	  (context, PROGRAM_NAME " is part of ng-jackspa.\n\n" PACKAGE_SUMMARY);
	g_option_context_set_description(context, PACKAGE_DESCRIPTION);
	g_option_context_add_main_entries(context, interface_entries, NULL);

	/* Group of control options */
	GOptionGroup *control_group;
	control_group = g_option_group_new
	  ( "control", "Control interface",
	    "Show options of the control interface", NULL, NULL );
	g_option_group_add_entries(control_group, control_entries);
	g_option_context_add_group(context, control_group);

	/* Group of jackspa options */
	GOptionGroup *jackspa_group;
	jackspa_group = g_option_group_new
	  ( "jackspa", "JACK and LADSPA",
	    "Show options of the jackspa module", NULL, NULL );
	g_option_group_add_entries(jackspa_group, jackspa_entries);
	g_option_context_add_group(context, jackspa_group);

	return context;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
