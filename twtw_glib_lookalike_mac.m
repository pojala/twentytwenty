/*
 *  twtw_glib_lookalike.c
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

#include "twtw_glib_lookalike.h"
#include <stdlib.h>
#include <Foundation/Foundation.h>


gpointer g_malloc (gsize n_bytes) 
{
    return malloc(n_bytes);
}

gpointer g_malloc0 (gsize n_bytes)
{
    return calloc(n_bytes, 1);
}

gpointer g_realloc (gpointer mem, gsize n_bytes)
{
    return realloc(mem, n_bytes);
}

void g_free	(gpointer mem)
{
    return free(mem);
}

gchar *g_strdup	(const gchar *str)
{
    return strdup(str);
}

void g_return_if_fail_warning (const char *log_domain,
			       const char *pretty_function,
			       const char *expression)
{
    NSLog(@"*** TwentyTwenty function assertion error: %s (%s): %s", log_domain, pretty_function, expression);
}

void    g_assertion_message_expr        (const char     *domain,
                                         const char     *file,
                                         int             line,
                                         const char     *func,
                                         const char     *expr)
{
    NSLog(@"*** TwentyTwenty assertion error: %s (%s): %s (line %i)", domain, func, expr, line);
}
