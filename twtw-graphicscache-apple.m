/*
 *  twtw-graphicscache-apple.m
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

#include "twtw-graphicscache.h"
#include "twtw-graphicscache-priv.h"
#include <Foundation/Foundation.h>

#if !defined(__MACOSX__)
#include <CoreGraphics/CoreGraphics.h>
#endif


struct _TwtwCacheSurface {
    CGContextRef cgContext;

    gint w;
    gint h;    
    size_t rowBytes;
    unsigned char *frameBuf;
};


static TwtwCacheSurface g_mainCanvas = { NULL, 0, 0, 0, NULL };


static void recreateCGBitmapContext(TwtwCacheSurface *surf, int w, int h)
{
    if (surf->cgContext) {
        CGContextRelease(surf->cgContext);
        surf->cgContext = NULL;
    }
        
    if (surf->frameBuf)
        g_free(surf->frameBuf);

    surf->w = w;
    surf->h = h;
    surf->rowBytes = w * 4;
    surf->frameBuf = (w > 0 && h > 0) ? g_malloc(surf->rowBytes * h) : NULL;

    if (w < 1 || h < 1) {
        printf("*** %s: unable to create surface with zero size", __func__);
        return;
    }

    CGColorSpaceRef cspace = CGColorSpaceCreateDeviceRGB();
    
    surf->cgContext = CGBitmapContextCreate(surf->frameBuf,
                                                   w, h, 8,
                                                   surf->rowBytes,
                                                   cspace,
                                                   kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cspace);
    
    if ( !surf->cgContext) {
        NSLog(@"*** %s: unable to create cgsurface (%i * %i)", __func__, w, h);
    } else {
        CGContextClearRect(surf->cgContext, CGRectMake(0, 0, w, h));
    }
}


void twtw_set_size_for_shared_canvas_cache_surface (gint w, gint h)
{
    if (w != g_mainCanvas.w || h != g_mainCanvas.h) {
        NSLog(@"recreating shared canvas surface with size %i * %i", w, h);
        recreateCGBitmapContext(&g_mainCanvas, w, h);
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
    //g_return_val_if_fail(surf, NULL);
    //g_return_val_if_fail(surf->cgContext, NULL);    
    
    if (w < 0 && surf)  w = surf->w;
    if (h < 0 && surf)  h = surf->h;
    
    TwtwCacheSurface *newsurf = g_new0(TwtwCacheSurface, 1);
    
    recreateCGBitmapContext(newsurf, w, h);
    return newsurf;
}

void twtw_cache_surface_destroy(TwtwCacheSurface *surf)
{
    if ( !surf) return;

    recreateCGBitmapContext(surf, 0, 0);
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
    g_return_if_fail(surf->cgContext);
    
    ///NSLog(@"%s: %i, %i, %i, %i", __func__, x, y, w, h);
    
    if (w == 0 || h == 0)
        return;
    
    CGContextClearRect(surf->cgContext, CGRectMake(x, y, w, h));
}


// returns a platform-specific context (e.g. a cairo_t *)
void *twtw_cache_surface_begin_drawing (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, NULL);
    g_return_val_if_fail(surf->cgContext, NULL);    
    
    return surf->cgContext;
}

void twtw_cache_surface_end_drawing (TwtwCacheSurface *surf)
{
    g_return_if_fail(surf);
}

void *twtw_cache_surface_get_sourceable (TwtwCacheSurface *surf)
{
    g_return_val_if_fail(surf, NULL);
    g_return_val_if_fail(surf->cgContext, NULL);    
    
    return surf->cgContext;    
}

