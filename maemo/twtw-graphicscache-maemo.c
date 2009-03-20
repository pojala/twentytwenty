/*
 *  twtw-graphicscache-maemo.c
 *  TwentyTwenty
 *
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

#include "twtw-maemo.h"
#include "twtw-graphicscache.h"



struct _TwtwCacheSurface {
    GdkPixmap *pixmap;

    gint w;
    gint h;

    cairo_t *activeCairoCtx;
};


static TwtwCacheSurface g_mainCanvas = { NULL, 0, 0, NULL };


static void recreatePixmap(TwtwCacheSurface *surf, int w, int h, GdkDrawable *parentDrawable)
{
    if (surf->pixmap) {
        g_object_unref(surf->pixmap);
        surf->pixmap = NULL;
    }

    surf->w = w;
    surf->h = h;

    if (w < 1 || h < 1)
        return;

    surf->pixmap = gdk_pixmap_new(parentDrawable, w, h, -1);

    cairo_t *cr = gdk_cairo_create( GDK_DRAWABLE(surf->pixmap) );
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1, 1, 0.7, 1);
    cairo_paint(cr);    
    cairo_destroy(cr);
    cr = NULL;

    ///printf("%s: created pixmap (%i, %i, parent %p)\n", __func__, w, h, parentDrawable);
}


void twtw_set_size_and_parent_for_shared_canvas_cache_surface (gint w, gint h, GdkDrawable *drawable)
{
    if (w != g_mainCanvas.w || h != g_mainCanvas.h) {
        recreatePixmap(&g_mainCanvas, w, h, drawable);
    }
}

// surface used by the main canvas
TwtwCacheSurface *twtw_shared_canvas_cache_surface ()
{
    return &g_mainCanvas;
}

// w/h can be -1 to create a surface with the same size as "surf"
TwtwCacheSurface *twtw_cache_surface_create_similar (TwtwCacheSurface *surf, gint w, gint h)
{
    g_return_val_if_fail(surf, NULL);
    g_return_val_if_fail(surf->pixmap, NULL);    
    
    if (w < 0 && surf)  w = surf->w;
    if (h < 0 && surf)  h = surf->h;
    
    TwtwCacheSurface *newsurf = g_new0(TwtwCacheSurface, 1);
    
    recreatePixmap(newsurf, w, h, surf->pixmap);
    return newsurf;
}

void twtw_cache_surface_destroy(TwtwCacheSurface *surf)
{
    if ( !surf) return;

    recreatePixmap(surf, 0, 0, NULL);
    g_free(surf);
}


gint twtw_cache_surface_get_width (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, 0);
    
    return surf->w;
}

gint twtw_cache_surface_get_height (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, 0);
    
    return surf->h;
}

void twtw_cache_surface_clear_rect (TwtwCacheSurface *surf, gint x, gint y, gint w, gint h)
{
    g_return_if_fail(surf);
    g_return_if_fail(surf->pixmap);
    
    //printf("%s: %i, %i, %i, %i", __func__, x, y, w, h);
    
    if (w == 0 || h == 0)
        return;
    
    cairo_t *cr = gdk_cairo_create( GDK_DRAWABLE(surf->pixmap) );
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    //cairo_set_source_rgba(cr, 1, 1, 0.7, 1);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_paint(cr);    
    cairo_destroy(cr);
    cr = NULL;
}


// returns a platform-specific context (e.g. a cairo_t *)
void *twtw_cache_surface_begin_drawing (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, NULL);
    g_return_val_if_fail(surf->pixmap, NULL);    
    g_return_val_if_fail(surf->activeCairoCtx == NULL, NULL);    

    surf->activeCairoCtx = gdk_cairo_create( GDK_DRAWABLE(surf->pixmap) );

    g_return_val_if_fail(surf->activeCairoCtx, NULL);

    return surf->activeCairoCtx;
}

void twtw_cache_surface_end_drawing (TwtwCacheSurface *surf)
{
    g_return_if_fail(surf);
    g_return_if_fail(surf->activeCairoCtx);

    cairo_destroy(surf->activeCairoCtx);
    surf->activeCairoCtx = NULL;
}

void *twtw_cache_surface_get_sourceable (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, NULL);
    g_return_val_if_fail(surf->pixmap, NULL);    
    
    return surf->pixmap;
}

