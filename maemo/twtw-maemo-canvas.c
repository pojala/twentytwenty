/*
 *  twtw-maemo-canvas.c
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
#include "twtw-curves.h"
#include "twtw-editing.h"
#include "twtw-document.h"
#include "twtw-graphicscache.h"
#include "twtw-photo.h"
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdint.h>
#include <math.h>


// implemented in twtw-graphicscache-maemo.c
extern void twtw_set_size_and_parent_for_shared_canvas_cache_surface (gint w, gint h, GdkDrawable *drawable);

// implemented in twtw-maemo-gfx.c
GdkPixbuf *twtw_get_pixbuf_for_ui_element (const char *elementName);

// implemented in twtw-document.c
void twtw_set_default_color_index (gint index);


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static gboolean g_bgCacheIsDirty = TRUE;
static gboolean g_bottomUICacheIsDirty = TRUE;

static TwtwCurveList *g_editedCL = NULL;
static gint g_editingMinY = INT_MIN, g_editingMaxY = INT_MIN;

static gint g_canvasX = 0, g_canvasY = 0, g_canvasW = 640, g_canvasH = 360;

// the "user rect" is the area that displays user content
static GdkRectangle g_canvasUserRect = { 0, 0, 0, 0 };


static void drawCurveList(cairo_t *cr, TwtwCurveList *curve, gint mode);


struct twtwCanvasElementInfo {
    GdkRectangle pencilsRect;
    
    GdkRectangle boomboxRect;
    GdkRectangle recButtonRect;
    GdkRectangle playButtonRect;
    
    GdkRectangle cameraRect;
    
    GdkRectangle leftPaperStackRect;
    GdkRectangle rightPaperStackRect;
    gint paperSheetH;  // height of one paper sheet
    
    GdkRectangle envelopeRect;
    
    GdkRectangle closeRect;
};
static struct twtwCanvasElementInfo g_uiElementInfo;


static inline GdkRectangle makeGdkRect(gint x, gint y, gint w, gint h) {
    GdkRectangle r = { x, y, w, h };
    return r;
}

static inline GdkRectangle outsetGdkRect(GdkRectangle r, gint woff, gint hoff) {
    r.x -= woff/2;
    r.y -= hoff/2;
    r.width += woff;
    r.height += hoff;
    return r;
}

static inline gboolean isMouseInRect(gint x, gint y, GdkRectangle rect) {
    if (rect.width < 1 || rect.height < 1)
        return FALSE;
        
    return (x < rect.x || y < rect.y || x >= rect.x+rect.width || y >= rect.y+rect.height) ? FALSE : TRUE;
}


enum {
    TWTW_CANVASRENDER_FINAL = 0,
    TWTW_CANVASRENDER_PREVIEW = 1
};


// callbacks for the rec/play/etc. buttons
static TwtwCanvasActionCallbacks g_actionCallbacks = { NULL, NULL, NULL, NULL };
static void *g_actionCbData = NULL;

static int g_pendingAction = 0;


void twtw_canvas_set_action_callbacks(GtkWidget *widget, TwtwCanvasActionCallbacks *callbacks, void *cbData)
{
    memcpy(&g_actionCallbacks, callbacks, sizeof(TwtwCanvasActionCallbacks));
    g_actionCbData = cbData;
}

static int g_audioState = 0;
static int g_audioPosition = -1;

void twtw_canvas_set_active_audio_state(GtkWidget *widget, gint state)
{
    if (state != g_audioState) {
        g_audioState = state;
        
        if (state == 0)
            g_audioPosition = -1;
            
        gtk_widget_queue_draw_area (widget, g_canvasX,  g_canvasY + g_canvasH - 64,
                                            g_canvasW,  64);
                                            
        g_bottomUICacheIsDirty = TRUE;
    }
}

void twtw_canvas_set_active_audio_position(gint audioPos)
{
    g_audioPosition = audioPos;
}


void twtw_canvas_set_rect(gint x, gint y, gint w, gint h)
{
    //printf("%s: (%i, %i), (%i, %i)\n", __func__, x, y, w, h);
    g_canvasX = x;
    g_canvasY = y;
    g_canvasW = w;
    g_canvasH = h;
    
    // this is the actual draw area (on Maemo it comes down to 760*406)
    g_canvasUserRect = makeGdkRect(x + 8, y + 8,  w - 8 - 32, h - 8 - 66);
}

void twtw_canvas_did_acquire_drawable(gint w, gint h, GdkDrawable *drawable)
{
    twtw_set_size_and_parent_for_shared_canvas_cache_surface (w, h, drawable);

    g_bgCacheIsDirty = TRUE;
    
    memset(&g_uiElementInfo, 0, sizeof(g_uiElementInfo));    
}


void twtw_canvas_queue_full_redraw(GtkWidget *widget)
{
    //printf("%s: %i *% i\n", __func__, g_canvasW, g_canvasH);

    gtk_widget_queue_draw_area (widget, g_canvasX, g_canvasY, g_canvasW, g_canvasH);
    g_bgCacheIsDirty = TRUE;
    g_bottomUICacheIsDirty = TRUE;
}

void twtw_canvas_queue_audio_status_redraw(GtkWidget *widget)
{
    g_return_if_fail(widget);
    gtk_widget_queue_draw_area (widget, g_canvasX + g_canvasW - 30,  g_canvasY,
                                        30, g_canvasH);
}


static int colorIDFromPointInPencilsBox(int x, int y)
{
    int row = 0;
    int col = 0;
    const int pencilW = 20;
    if (y > 22) {
        row = 1;
        if (x <= 3) col = 0;
        else col = (x-3) / pencilW - 1;
    } else {
        if (x <= 14) col = 0;
        else col = (x-14) / pencilW - 1;
    }
    col = MIN(9, col);
    //printf("col %i, row %i\n", col, row);
    return row*10 + col;
}

static gboolean pendingActionTimerFunc(gpointer data)
{
    GtkWidget *widget = (GtkWidget *)data;
    switch (g_pendingAction) {
        case 1:                        
            g_actionCallbacks.cameraAction(GTK_WIDGET(widget), g_actionCbData);
            break;
    }
    g_pendingAction = 0;
    return FALSE;  // don't allow timer to continue
}


void twtw_canvas_mousedown(GtkWidget *widget, gint x, gint y)
{
    //printf("%s: (%i, %i)\n", __func__, x, y);
    
    gboolean clickCanStartDraw = isMouseInRect(x, y, g_canvasUserRect);
    
    // test whether point is inside one of the active buttons
    {
    struct twtwCanvasElementInfo *elemInfo = &g_uiElementInfo;
    
    if (isMouseInRect(x, y, elemInfo->pencilsRect)) {
        int colorID = colorIDFromPointInPencilsBox(x - elemInfo->pencilsRect.x, y - elemInfo->pencilsRect.y);

        //printf("mouse in pencils box; colorID %i\n", colorID);

        twtw_set_default_color_index (colorID);

        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->cameraRect)) {
        if (g_actionCallbacks.cameraAction) {
            // the camera takes very long to start up (at least on N800), so do it in a timer.
            // this gives us time to display a waiting screen
            
            g_pendingAction = 1;
            g_timeout_add(150, pendingActionTimerFunc, widget);
            twtw_canvas_queue_full_redraw(widget);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->playButtonRect)) {
        if (g_actionCallbacks.playSoundAction) {
            g_actionCallbacks.playSoundAction(widget, g_actionCbData);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->recButtonRect)) {
        if (g_actionCallbacks.recSoundAction) {
            g_actionCallbacks.recSoundAction(widget, g_actionCbData);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->leftPaperStackRect)) {
        int page = twtw_active_document_page_index ();
        if (page > 0) {
            twtw_set_active_document_page_index (page - 1);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->rightPaperStackRect)) {
        int page = twtw_active_document_page_index ();
        if (page < 19) {
            twtw_set_active_document_page_index (page + 1);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, elemInfo->envelopeRect)) {
        if (g_actionCallbacks.sendDocumentAction) {
            g_actionCallbacks.sendDocumentAction(widget, g_actionCbData);
        }
        clickCanStartDraw = FALSE;
    }
    else if (isMouseInRect(x, y, outsetGdkRect(elemInfo->closeRect, 8, 8))) {
        /*if (g_actionCallbacks.closeAppAction) {
            g_actionCallbacks.closeAppAction(widget, g_actionCbData);
        }*/
        gtk_main_quit();
        clickCanStartDraw = FALSE;
    }
    
    
    }
    
    if ( !clickCanStartDraw)
        return;

    TwtwCurveList *newCL = twtw_editing_curvelist_create_with_start_cursor_point (TwtwMakePoint( TWTW_UNITS_FROM_INT(x), TWTW_UNITS_FROM_INT(y) ));
    g_editedCL = newCL;
    
    g_editingMinY = y;
    g_editingMaxY = y;

    //gtk_widget_queue_draw_area (widget, g_canvasX, g_canvasY, g_canvasW, g_canvasH);
}

void twtw_canvas_mousedragged(GtkWidget *widget, gint x, gint y)
{
    if ( !g_editedCL) return;

    //printf("%s: (%i, %i)\n", __func__, x, y);

    twtw_editing_curvelist_add_cursor_point (g_editedCL, TwtwMakePoint( TWTW_UNITS_FROM_INT(x), TWTW_UNITS_FROM_INT(y) ));

    if (y < g_editingMinY)
        g_editingMinY = y;
    if (y > g_editingMaxY)
        g_editingMaxY = y;

    // add some extra pixels at top+bottom of redraw area to account for stroke rounding
    int redrawY = MAX(0, g_editingMinY - 2);
    int redrawH = MIN(g_canvasH-redrawY,  g_editingMaxY - g_editingMinY + 4);

    gtk_widget_queue_draw_area (widget, g_canvasX, redrawY, g_canvasW - 30, redrawH);
}


static void drawCurveIntoCacheSurface(TwtwCurveList *curve)
{
    TwtwCacheSurface *surf = twtw_shared_canvas_cache_surface();    
    {
    cairo_t *cacheCtx = (cairo_t *) twtw_cache_surface_begin_drawing (surf);

        drawCurveList(cacheCtx, curve, TWTW_CANVASRENDER_FINAL);

    twtw_cache_surface_end_drawing (surf);
    }
}


void twtw_canvas_mouseup(GtkWidget *widget, gint x, gint y)
{
    if ( !g_editedCL) return;  // nothing to do if a curve was not started

    //printf("%s: (%i, %i)\n", __func__, x, y);

    twtw_editing_curvelist_finish_at_cursor_point (g_editedCL, TwtwMakePoint( TWTW_UNITS_FROM_INT(x), TWTW_UNITS_FROM_INT(y) ));
    
    // add created curve to active document
    TwtwPage *page = twtw_active_document_page ();
    twtw_page_add_curve (page, g_editedCL);
    
    drawCurveIntoCacheSurface(g_editedCL);

    twtw_curvelist_destroy (g_editedCL);
    g_editedCL = NULL;
    
    g_editingMinY = g_editingMaxY = INT_MIN;

    gtk_widget_queue_draw_area (widget, g_canvasX, g_canvasY, g_canvasW, g_canvasH);
}


#define DEFAULT_PREVIEW_LINE_WIDTH 1.2f

static void drawCurveList(cairo_t *cr, TwtwCurveList *curve, gint mode)
{
    g_return_if_fail(cr);
    g_return_if_fail(curve);
  
    int segCount = twtw_curvelist_get_segment_count (curve);
    TwtwCurveSegment *segs = twtw_curvelist_get_segment_array (curve);
    
    const gboolean isPreview = (mode == TWTW_CANVASRENDER_PREVIEW);

    unsigned char *rgbPalette = twtw_default_color_palette_rgb_array (NULL);
    float *paletteLineWeights = twtw_default_color_palette_line_weight_array (NULL);
    g_return_if_fail(rgbPalette);
    g_return_if_fail(paletteLineWeights);
    
    const int colorID = twtw_curvelist_get_color_id (curve);
    const float lineWMul = (paletteLineWeights[colorID] > 0.0f) ? paletteLineWeights[colorID] : 1.0;
    
    const double fColorMul = 1.0 / 255.0;
    cairo_set_source_rgba(cr,  rgbPalette[colorID*3] * fColorMul, rgbPalette[colorID*3+1] * fColorMul, rgbPalette[colorID*3+2] * fColorMul,  1.0);

    cairo_new_path (cr);

    if (lineWMul > 1.1) {
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    }
    
    int i;
    for (i = 0; i < segCount; i++) {
        TwtwCurveSegment *seg = segs + i;
    
        const double startX = TWTW_UNITS_TO_FLOAT(seg->startPoint.x);
        const double startY = TWTW_UNITS_TO_FLOAT(seg->startPoint.y);
        
        const double endX = TWTW_UNITS_TO_FLOAT(seg->endPoint.x);
        const double endY = TWTW_UNITS_TO_FLOAT(seg->endPoint.y);
    
        float startW = TWTW_UNITS_TO_FLOAT(seg->startWeight);
        float endW = TWTW_UNITS_TO_FLOAT(seg->endWeight);
        if (startW < 0.0001f) startW = 0.7f;
        if (endW < 0.0001f) endW = 0.7f;

        float currentLineW = lineWMul * ((isPreview) ? DEFAULT_PREVIEW_LINE_WIDTH : startW);
        cairo_set_line_width(cr, currentLineW);

        if (isPreview) {
            if (i == 0)
                cairo_move_to(cr, startX, startY);
        } else {
            cairo_move_to(cr, startX, startY);
        }
            
        if (seg->segmentType == TWTW_SEG_CATMULLROM &&
                !twtw_is_invalid_point(seg->controlPoint1) &&
                !twtw_is_invalid_point(seg->controlPoint2)) {
                
            TwtwUnit segLen = twtw_point_distance (seg->startPoint, seg->endPoint);
            
                if (segLen > TWTW_UNITS_FROM_INT(1)) {
                    gint steps;
                    if (isPreview)
                        steps = MIN(2, TWTW_UNITS_TO_INT(segLen) * 2);
                    else
                        steps = MIN(16, TWTW_UNITS_TO_INT(segLen) * 2);

                    TwtwPoint twarr[steps];
                    
                    twtw_calc_catmullrom_curve (seg, steps, twarr);

                    int j;
                    for (j = 0; j < steps; j++) {
                        double x = TWTW_UNITS_TO_FLOAT(twarr[j].x);
                        double y = TWTW_UNITS_TO_FLOAT(twarr[j].y);

                        if (isPreview) {
                            cairo_line_to(cr, x, y);
                        } else {
                            cairo_line_to(cr, x, y);
                            
                            float u = (float)(j+1) / (float)steps;
                            float newLineW = lineWMul * (startW + u*(endW-startW));
                            
                            // only do stroke + reset line width if the difference is large enough
                            if (newLineW != currentLineW && ((newLineW < 1.7 && fabsf(newLineW - currentLineW) > 0.07) ||
                                                             (newLineW < 0.4 && fabsf(newLineW - currentLineW) > 0.02) ||
                                                             (fabsf(newLineW - currentLineW) > 0.5))
                                ) {
                                cairo_stroke(cr);
                                cairo_move_to(cr, x, y);

                                cairo_set_line_width(cr, newLineW);
                                currentLineW = newLineW;
                            }
                        }
                    }
                }
        }
        
        cairo_line_to(cr, endX, endY);
    }
    
    cairo_stroke(cr);
}


static GdkPixbuf *createGdkPixbufFromTwtwYUVImage(TwtwYUVImage *yuvImage)
{
    g_return_val_if_fail(yuvImage, NULL);
    g_return_val_if_fail(yuvImage->buffer, NULL);

    const int w = yuvImage->w;
    const int h = yuvImage->h;
    
    GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,  FALSE /*has alpha*/,  8 /*bits per sample*/,
                                        w, h);
    if ( !pixbuf) return NULL;
    
    const size_t dstRowBytes = gdk_pixbuf_get_rowstride (pixbuf);
    unsigned char *dstBuf = gdk_pixbuf_get_pixels (pixbuf);

    twtw_yuv_image_convert_to_rgb_for_display (yuvImage, dstBuf, dstRowBytes, FALSE, 1, 1);
    
    return pixbuf;
}


static void drawBackgroundPhotoFromPage(cairo_t *cr, TwtwPage *page, int w, int h)
{
    TwtwYUVImage *image = twtw_page_get_yuv_photo (page);

    //printf("%s: should draw YUV photo: %p (page %p)\n", __func__, image, page);
    
    if ( !image) return;
    
    GdkPixbuf *pixbuf = createGdkPixbufFromTwtwYUVImage(image);
    GdkPixbuf *upscaledPixbuf = gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_BILINEAR);

    cairo_save(cr);
    gdk_cairo_set_source_pixbuf (cr, upscaledPixbuf, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    
    gdk_pixbuf_unref(upscaledPixbuf);
    gdk_pixbuf_unref(pixbuf);
}


static void drawEntireActivePageInCache()
{
    TwtwPage *page = twtw_active_document_page ();
    TwtwCacheSurface *surf = twtw_shared_canvas_cache_surface();
    
    int w = twtw_cache_surface_get_width(surf);
    int h = twtw_cache_surface_get_height(surf);
    
    twtw_cache_surface_clear_rect (surf, 0, 0, w, h);
    
    {
    cairo_t *cacheCtx = (cairo_t *) twtw_cache_surface_begin_drawing (surf);

        drawBackgroundPhotoFromPage(cacheCtx, page, w, h);

        int curveCount = twtw_page_get_curves_count (page);
        int i;
        for (i = 0; i < curveCount; i++) {
            drawCurveList(cacheCtx, twtw_page_get_curve(page, i), TWTW_CANVASRENDER_FINAL);
        }

    twtw_cache_surface_end_drawing (surf);
    }
}


static void drawUIElement(cairo_t *cr, double canvasW, double canvasH, const char *elementName,
                                       double x, double y, gboolean flipY, double *outW, double *outH)
{
    GdkPixbuf *pixbuf = twtw_get_pixbuf_for_ui_element(elementName);
    if ( !pixbuf) return;
    int w = gdk_pixbuf_get_width(pixbuf);
    int h = gdk_pixbuf_get_height(pixbuf);
    
    if (flipY) {
        y = canvasH - y - h;
    }
    cairo_save(cr);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);
    cairo_restore(cr);
    
    if (outW) *outW = w;
    if (outH) *outH = h;
}

static void drawCloseButton(cairo_t *cr, GdkRectangle rect)
{
    cairo_save(cr);
    cairo_set_source_rgba(cr,  0, 0, 0, 1);
    cairo_set_line_width(cr,  4.0);
    
    //printf("%s: %i, %i, %i, %i\n", __func__, rect.x, rect.y, rect.width, rect.height);
    
    cairo_move_to(cr, rect.x, rect.y);
    cairo_line_to(cr, rect.x+rect.width,  rect.y+rect.height);
    cairo_stroke(cr);
    
    cairo_move_to(cr, rect.x+rect.width, rect.y);
    cairo_line_to(cr, rect.x,  rect.y+rect.height);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}


static TwtwCacheSurface *g_bottomUICacheSurf = NULL;

#define BOTTOM_UI_H  64

static TwtwCacheSurface *g_picOverlayCacheSurf = NULL;

#define PIC_OVERLAY_H  64

#define BOTTOM_UI_ACTIVE_AREA_H  116


static void drawCanvasOverlapPortionInCache()
{
    // TODO
}


static void drawNonOverlappingUIElementsInCache()
{
    int activePageIndex = twtw_active_document_page_index ();
    int canvasH = g_canvasH;
    int w = g_canvasW;
    int h = BOTTOM_UI_H;
    
    if ( !g_bottomUICacheSurf) {
        g_bottomUICacheSurf = twtw_cache_surface_create_similar (twtw_shared_canvas_cache_surface(), w, h);
    }
    TwtwCacheSurface *surf = g_bottomUICacheSurf;
    g_return_if_fail(surf);
    
    struct twtwCanvasElementInfo *elementInfo = &g_uiElementInfo;
    
    twtw_cache_surface_clear_rect (surf, 0, 0, w, h);
    
    {
    cairo_t *cr = (cairo_t *) twtw_cache_surface_begin_drawing (surf);

    double ew = 0, eh = 0;
    double accx = -12;
    drawUIElement(cr, w, h, "pencils", accx, 0, TRUE, &ew, &eh);
    elementInfo->pencilsRect = makeGdkRect(accx, canvasH-eh, ew, eh);
    accx += ew;
    accx -= 16;
    
    drawUIElement(cr, w, h, "boombox", accx, 0, TRUE, &ew, &eh);
    elementInfo->boomboxRect = makeGdkRect(accx, canvasH-eh, ew, eh);
    accx += ew;
    accx -= 12;
    
    double recX = ceil(accx - ew*0.67);
    drawUIElement(cr, w, h, (g_audioState == 1) ? "stopbutton" : "recbutton",
                  recX, 5, TRUE, &ew, &eh);
    elementInfo->recButtonRect = makeGdkRect(recX, canvasH-eh-5,  ew, eh);
    
    double playX = recX + ew + 8;
    drawUIElement(cr, w, h, (g_audioState == 2) ? "pausebutton" : "playbutton",
                  playX, 5, TRUE, &ew, &eh);
    elementInfo->playButtonRect = makeGdkRect(playX, canvasH-eh-5,  ew, eh);
    
    
    drawUIElement(cr, w, h, "camera", accx, 0, TRUE, &ew, &eh);
    elementInfo->cameraRect = makeGdkRect(accx, canvasH-eh, ew, eh);
    accx += ew;
    accx += 2;
    
    int paperStackH;
    int n;
    for (n = 0; n <= activePageIndex; n++) {
        drawUIElement(cr, w, h, "paper", accx, 4.0*n, TRUE, &ew, &eh);
    }
    paperStackH = eh + 4*(n-1);
    elementInfo->leftPaperStackRect = makeGdkRect(accx, canvasH-paperStackH, ew, paperStackH);
    accx += ew;
    accx -= 24;
    
    for (; n < 20; n++) {
        drawUIElement(cr, w, h, "paper", accx, 4.0*(n-activePageIndex-1), TRUE, &ew, &eh);
    }
    paperStackH = eh + 4*(20-activePageIndex-1);
    elementInfo->rightPaperStackRect = makeGdkRect(accx, canvasH-paperStackH, ew, paperStackH);
    accx += ew;
    accx -= 12;
    
    int envelopeY = 2;
    drawUIElement(cr, w, h, "envelope", accx, envelopeY, TRUE, &ew, &eh);
    elementInfo->envelopeRect = makeGdkRect(accx, canvasH-eh-envelopeY, ew, eh);
    accx += ew;
        
    //cairo_set_source_rgba(cr, 1, 0.5, 0, 1);
    //cairo_paint(cr);
    
    twtw_cache_surface_end_drawing (surf);
    }
}

static void drawCanvasOverlappingUI(cairo_t *cr, gboolean drawImages)
{
    int canvasW = g_canvasW;
    int canvasH = g_canvasH;
    GdkPixbuf *pixbuf;

    GdkRectangle *userRect = &g_canvasUserRect;
    
    //printf("userrect: (%i, %i)  (%i, %i)\n", userRect->x, userRect->y, userRect->width, userRect->height);
    
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    
    // vertical lines
    cairo_rectangle(cr, 0, 0,  userRect->x, canvasH);
    cairo_fill(cr);
    
    cairo_rectangle(cr, userRect->x+userRect->width, 0,  canvasW-(userRect->x+userRect->width), canvasH);
    cairo_fill(cr);

    // horizontal lines
    cairo_rectangle(cr, 0, 0, canvasW, userRect->y);
    cairo_fill(cr);
    
    cairo_rectangle(cr, 0, userRect->y+userRect->height,  canvasW, canvasH-(userRect->y+userRect->height));
    cairo_fill(cr);
    
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 1);
    cairo_rectangle(cr, userRect->x-0.5, userRect->y-0.5, userRect->width+1.0, userRect->height+1.0);
    cairo_set_line_width(cr, 0.8);
    cairo_stroke(cr);
    
    // X button in top-right corner
    g_uiElementInfo.closeRect = makeGdkRect(g_canvasW - 22, 12, 12, 12);
    drawCloseButton(cr, g_uiElementInfo.closeRect);
    

    
    if ( !drawImages) {
        return;  // early exit
    }

    cairo_save(cr);
        
    pixbuf = twtw_get_pixbuf_for_ui_element("pencils");
    GdkRectangle *pencilsRect = &(g_uiElementInfo.pencilsRect);
    
    gdk_cairo_set_source_pixbuf(cr, pixbuf, pencilsRect->x, pencilsRect->y);
    cairo_rectangle(cr, pencilsRect->x, pencilsRect->y, pencilsRect->width,  canvasH - pencilsRect->y - BOTTOM_UI_H);
    cairo_fill(cr);

    pixbuf = twtw_get_pixbuf_for_ui_element("boombox");
    GdkRectangle *boomboxRect = &(g_uiElementInfo.boomboxRect);

    gdk_cairo_set_source_pixbuf(cr, pixbuf, boomboxRect->x, boomboxRect->y);
    cairo_rectangle(cr, boomboxRect->x, boomboxRect->y, boomboxRect->width,  canvasH - boomboxRect->y - BOTTOM_UI_H);
    cairo_fill(cr);
    
    pixbuf = twtw_get_pixbuf_for_ui_element("paper");
    const int paperH = gdk_pixbuf_get_height(pixbuf);
    const int activePageIndex = twtw_active_document_page_index ();
    int n;
    GdkRectangle *leftStackRect = (&g_uiElementInfo.leftPaperStackRect);
    if (leftStackRect->height > BOTTOM_UI_H) {
        for (n = 0; n <= activePageIndex; n++) {
            int x = leftStackRect->x;
            int w = leftStackRect->width;
            int y0 = 4*n;
            int y1 = y0 + paperH;
            if (y1 > BOTTOM_UI_H) {
                //printf("should draw L paper %i\n", n);
                int cy1 = g_canvasH - y1;
                gdk_cairo_set_source_pixbuf(cr, pixbuf, x, cy1);
                cairo_rectangle(cr, x, cy1, w, paperH);
                cairo_fill(cr);
            }
        }
    }
    
    GdkRectangle *rightStackRect = (&g_uiElementInfo.rightPaperStackRect);
    if (rightStackRect->height > BOTTOM_UI_H) {
        for (n = activePageIndex; n < 20; n++) {
            int x = rightStackRect->x;
            int w = rightStackRect->width;
            int y0 = 4*(n-activePageIndex);
            int y1 = y0 + paperH;
            if (y1 > BOTTOM_UI_H) {
                //printf("should draw R paper %i\n", n);
                int cy1 = g_canvasH - y1;
                gdk_cairo_set_source_pixbuf(cr, pixbuf, x, cy1);
                cairo_rectangle(cr, x, cy1, w, paperH);
                cairo_fill(cr);
            }
        }
    }
    
    cairo_restore(cr);
}


void twtw_canvas_draw(cairo_t *cr, gint x, gint y, gint w, gint h)
{
    if (g_pendingAction != 0) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 1, 0.9, 0, 1);
        cairo_paint(cr);
        cairo_restore(cr);
        
        if (g_pendingAction == 1) {
            drawUIElement(cr, g_canvasW, g_canvasH, "camera", g_canvasW/2 - 20, g_canvasH/2 - 10, FALSE, NULL, NULL);
        }
        
        return;
    }

    TwtwPage *page = twtw_active_document_page ();
    //int activePageIndex = twtw_active_document_page_index ();

    g_return_if_fail(page);


    //printf("%s: %i, %i, %i, %i -- curvecount %i\n", __func__, x, y, w, h, twtw_page_get_curves_count(page));

    gboolean doClear = TRUE;
    gboolean doDrawUI = TRUE;
    if (g_editedCL) {
        //doClear = FALSE;
        
        // only draw the UI components if the redraw area extends low enough
        doDrawUI = ((y + h) > (g_canvasH - BOTTOM_UI_ACTIVE_AREA_H)) ? TRUE : FALSE;
    }

    if (doClear) {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 1, 1, 1, 1);
        cairo_paint(cr);
        cairo_restore(cr);
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 1);


    if (g_bgCacheIsDirty) {
        drawEntireActivePageInCache();
        g_bgCacheIsDirty = FALSE;
        //printf("did render bg cache for canvas\n");
    }
    
    // this call will refresh the element positions
    if (g_bottomUICacheIsDirty || !g_bottomUICacheSurf) {
        drawNonOverlappingUIElementsInCache();
        g_bottomUICacheIsDirty = FALSE;
    }
    
    
    // copy cached strokes into the view
    {
    TwtwCacheSurface *cacheSurf = twtw_shared_canvas_cache_surface();
    GdkPixmap *cachePixmap = GDK_PIXMAP( twtw_cache_surface_get_sourceable (cacheSurf) );
    
    cairo_save(cr);
    gdk_cairo_set_source_pixmap(cr, cachePixmap, 0.0, 0.0);
    if (0 && doClear) {
        cairo_paint(cr);
    } else {
        cairo_rectangle(cr, g_canvasUserRect.x, g_canvasUserRect.y,
                            g_canvasUserRect.width, g_canvasUserRect.height);
        cairo_fill(cr);
    }
    cairo_restore(cr);
    }

/*
    TwtwCurveList *cl = twtw_curvelist_create ();
    TwtwCurveSegment seg;
    memset(&seg, 0, sizeof(seg));
    seg.segmentType = TWTW_SEG_CATMULLROM;

    seg.startPoint = TwtwMakeFloatPoint(100, 100);
    seg.endPoint = TwtwMakeFloatPoint(400, 180);
    twtw_curvelist_append_segment (cl, &seg);

    seg.startPoint = seg.endPoint;
    seg.endPoint = TwtwMakeFloatPoint(600, 400);
    twtw_curvelist_append_segment (cl, &seg);

    twtw_curvelist_set_implicit_start_point (cl, TwtwMakeFloatPoint(50, 200));
    twtw_curvelist_set_implicit_end_point (cl, TwtwMakeFloatPoint(600, 400));

    drawCurveList(cr, cl);
*/

/*
    int curveCount = twtw_page_get_curves_count (page);
    int i;
    for (i = 0; i < curveCount; i++) {
        drawCurveList(cr, twtw_page_get_curve (page, i));
    }
*/
    if (g_editedCL) {
        //cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
    
        drawCurveList(cr, g_editedCL, TWTW_CANVASRENDER_PREVIEW);
        
        //cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
    }


    drawCanvasOverlappingUI(cr, doDrawUI);

    // draw bottom UI
    if (y+h > g_canvasH - BOTTOM_UI_H) {
        double bottomUI_y = g_canvasH - BOTTOM_UI_H;
    
        // copy the bottom UI elements into view
        cairo_save(cr);
        gdk_cairo_set_source_pixmap(cr, GDK_PIXMAP( twtw_cache_surface_get_sourceable (g_bottomUICacheSurf) ),
                                0.0, bottomUI_y);
        cairo_rectangle(cr, 0, bottomUI_y, g_canvasW, BOTTOM_UI_H);
        cairo_paint(cr);
        cairo_restore(cr);
    }
    
    cairo_new_path (cr);
    
    //printf("%s: x %i, w %i\n", __func__, x, w);
    
    // draw audio UI on righthand side
    if (x+w > g_canvasW - 26) {
        double audioUI_x = g_canvasW - 22;
        double audioUI_y = floor(g_canvasH - (g_canvasH * 0.2));
        
        const double audioMarkerSize = 11;
        const double yMargin = 6;
        
        int activePos = g_audioPosition;
        int soundDuration = twtw_page_get_sound_duration_in_seconds (twtw_active_document_page());
        
        soundDuration = MAX(activePos, soundDuration);

        cairo_set_source_rgba(cr, 0, 0, 0, 1);

        double y = audioUI_y;
        int i;
        for (i = 0; i < 20; i++) {
            y -= audioMarkerSize;
            
            cairo_arc(cr, audioUI_x+audioMarkerSize*0.5, y+audioMarkerSize*0.5,  audioMarkerSize*0.5,  0, 2*M_PI);
            
            if (soundDuration >= (i+1)) {
                if (activePos >= (i+1)) {
                    cairo_set_source_rgba(cr, 0, 1, 0.15, 1);
                } else {
                    cairo_set_source_rgba(cr, 0, 0.2, 1, 1);
                }
                cairo_fill(cr);
                
                cairo_set_source_rgba(cr, 0, 0, 0, 1);
            }
            
            cairo_stroke(cr);
            
            y -= yMargin;
        }
    }
}


