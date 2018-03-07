/* interface.h - entities common to the modules, possibly defined by the interface
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

#ifndef INTERFACE_H
#define INTERFACE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

/* Return a member of a NULL-terminated array */
extern gchar *glib_strv_index(unsigned long i, gchar **str_array);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
