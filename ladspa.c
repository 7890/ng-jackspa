/* ladspa.c - helper functions related to LADSPA
 *   based on files from the LADSPA SDK
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

/* dlopen() wrapper.  When the filename is not an absolute path, search the
 * LADSPA_PATH.
 */
static void *ladspa_dlopen(const char *file, int flag)
{
	char *strbuf;
	const char *LADSPA_PATH, *start, *end;
	size_t filenlength;
	void *handle; /* returned handle */

	filenlength = strlen(file);
	handle = NULL;

	if (file[0] == '/') {
		/* The filename is absolute */
		handle = dlopen(file, flag);
		if (handle != NULL)
			return handle;
	}
	else if ((LADSPA_PATH = getenv("LADSPA_PATH")) ) {
		/* Check along the LADSPA_PATH path to see if we can find the file
		 * there. We do NOT call dlopen() directly as this would find plugins
		 * on the LD_LIBRARY_PATH */
		for (start = LADSPA_PATH; *start; start = *end == ':' ? ++end : end) {
			for (end = start; *end != ':' && *end; end++);

			if (!(strbuf = malloc(filenlength + 2 + (end - start)))) {
				fputs("memory allocation error\n", stderr);
				continue;
			}
			strncpy(strbuf, start, end - start);
			if (end > start && *(end - 1) != '/') {
				strbuf[end - start] = '/';
				strcpy(strbuf + 1 + (end - start), file);
			}
			else
				strcpy(strbuf, file);

			handle = dlopen(strbuf, flag);

			free(strbuf);
			if (handle != NULL)
				return handle;
		}
	}

	/* As a last effort, try to add the suffix ".so" and recurse */
	if (filenlength <= 3 || strcmp(file + filenlength - 3, ".so")) {
		if (!(strbuf = malloc(filenlength + 4)))
			fputs("memory allocation error\n", stderr);
		else {
			strcpy(strbuf, file);
			strcat(strbuf, ".so");

			handle = ladspa_dlopen(strbuf, flag);

			free(strbuf);
			if (handle != NULL)
				return handle;
		}
	}

	/* If nothing has worked, then at least we can make sure we set the correct
	 * error message - and this should correspond to a call to dlopen() with
	 * the actual filename requested.
	 * The dlopen() manual page does not specify whether the first or last
	 * error message will be kept when multiple calls are made to dlopen().
	 * We've covered the former case - now we can handle the latter by calling
	 * dlopen() again here.
	 */
	return dlopen(file, flag);
}
