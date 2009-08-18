/*
 *  twtw-graphicscache.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 2.12.2008.
 *  Copyright 2008 Pauli Olavi Ojala. All rights reserved.
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

#ifndef _TWTW_GRAPHICSCACHE_H_
#define _TWTW_GRAPHICSCACHE_H_

#include "twtw-glib.h"
#include "twtw-curves.h"


// contains a platform-native graphics surface that can be drawn into
// (on Mac the underlying implementation is a CGContext, on Maemo it's a GdkPixmap)
typedef struct _TwtwCacheSurface TwtwCacheSurface;


#ifdef __cplusplus
extern "C" {
#endif

// surface used by the main canvas
TwtwCacheSurface *twtw_shared_canvas_cache_surface ();

// w/h can be -1 to create a surface with the same size as "surf"
TwtwCacheSurface *twtw_cache_surface_create_similar (TwtwCacheSurface *surf, gint w, gint h);

void twtw_cache_surface_destroy(TwtwCacheSurface *surf);


gint twtw_cache_surface_get_width (TwtwCacheSurface *surf);
gint twtw_cache_surface_get_height (TwtwCacheSurface *surf);

void twtw_cache_surface_clear_rect (TwtwCacheSurface *surf, gint x, gint y, gint w, gint h);

// returns a platform-specific context (e.g. a cairo_t * or CGContextRef)
void *twtw_cache_surface_begin_drawing (TwtwCacheSurface *surf);
void twtw_cache_surface_end_drawing (TwtwCacheSurface *surf);

// returns a platform-specific pattern or image (e.g. a cairo_pattern_t *)
void *twtw_cache_surface_get_sourceable (TwtwCacheSurface *surf);

#ifdef __cplusplus
}
#endif

#endif  // _TWTW_GRAPHICSCACHE_H_
