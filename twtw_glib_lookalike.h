/*
 *  twtw_glib_lookalike.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 12.3.2009.
 *  Copyright 2009 Pauli Olavi Ojala. All rights reserved.
 *
 */
/*
    This file is part of TwentyTwenty.

    TwentyTwenty is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TwentyTwenty is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TwentyTwenty.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TWTW_GLIB_H_
#define _TWTW_GLIB_H_

/*
TwentyTwenty only uses a handful of the most basic glib functions.
This file provides minimal wrappers for them, and the basic types if necessary.
*/


#ifndef __G_TYPES_H__
typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;
typedef void * gpointer;

typedef unsigned long gsize;

#define G_GNUC_MALLOC    __attribute__((__malloc__))

#endif

gpointer g_malloc (gsize n_bytes) G_GNUC_MALLOC;
gpointer g_malloc0 (gsize n_bytes) G_GNUC_MALLOC;
gpointer g_realloc (gpointer mem, gsize n_bytes);
void	 g_free	(gpointer mem);

gchar *g_strdup	(const gchar *str) G_GNUC_MALLOC;

void g_return_if_fail_warning (const char *log_domain,
			       const char *pretty_function,
			       const char *expression);

void    g_assertion_message_expr        (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr);

#endif