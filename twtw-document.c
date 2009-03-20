/*
 *  twtw-document.c
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 30.11.2008.
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

#include "twtw-document.h"
#include "twtw-units.h"
#include "twtw-filesystem.h"
#include "twtw-audio.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// for file i/o
#include <oggz/oggz.h>
#include "skeleton.h"

// deflate is used to compress curve and photo data for the on-disk format
#include <zlib.h>

gboolean twtw_deflate(unsigned char *srcBuf, size_t srcLen,
                      unsigned char *dstBuf, size_t dstLen,
                           size_t *outCompressedLen);
                           
gboolean twtw_inflate(unsigned char *srcBuf, size_t srcLen,
                      unsigned char *dstBuf, size_t dstLen,
                           size_t *outDecompressedLen);



// simple pseudorandom algorithm borrowed from oggz_serialno_new()
static gint32 serialno_new()
{
    gint32 serialno = time(NULL);
    gint k;

    do {
        for (k = 0; k < 3 || serialno == 0; k++)
            serialno = 11117 * serialno + 211231;
    } while (serialno == -1);
    return serialno;
}


#define SET_RGB_PTR(p_, r_, g_, b_)   (p_)[0] = r_;  (p_)[1] = g_;  (p_)[2] = b_;


static int g_defaultColor = 15;  // index of black in the default palette

gint8 twtw_default_color_index ()
{
    return g_defaultColor;
}

void twtw_set_default_color_index (gint index)
{
    g_return_if_fail(index >= 0);
    g_return_if_fail(index < 20);
    g_defaultColor = index;
}

unsigned char *twtw_default_color_palette_rgb_array (gint *outNumEntries)
{
    static unsigned char *s_palette = NULL;
    if ( !s_palette) {
        s_palette = g_malloc0(32 * 3);  // extra size just in case something stupid happens
        
        int n = 0;
        SET_RGB_PTR(s_palette+n*3,  102, 204, 0);  n++;
        SET_RGB_PTR(s_palette+n*3,  102, 255, 102); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 255, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 153, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  153, 102, 51); n++;
        SET_RGB_PTR(s_palette+n*3,  51, 51, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 204, 204); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 102, 102); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 0, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  153, 0, 0); n++;
        
        SET_RGB_PTR(s_palette+n*3,  0, 51, 0);  n++;
        SET_RGB_PTR(s_palette+n*3,  0, 102, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  255, 255, 255); n++;
        SET_RGB_PTR(s_palette+n*3,  204, 204, 204); n++;
        SET_RGB_PTR(s_palette+n*3,  102, 102, 102); n++;
        SET_RGB_PTR(s_palette+n*3,  0, 0, 0); n++;
        SET_RGB_PTR(s_palette+n*3,  153, 255, 255); n++;
        SET_RGB_PTR(s_palette+n*3,  0, 153, 255); n++;
        SET_RGB_PTR(s_palette+n*3,  0, 0, 255); n++;
        SET_RGB_PTR(s_palette+n*3,  0, 0, 153);
    }
    if (outNumEntries) *outNumEntries = 20;
    return s_palette;
}

float *twtw_default_color_palette_line_weight_array (gint *outNumEntries)
{
    static float *s_arr = NULL;
    if ( !s_arr) {
        s_arr = g_malloc0(32 * sizeof(float));
        
        int n = 0;
        s_arr[n++] = 1.0;
        s_arr[n++] = 1.2;
        s_arr[n++] = 1.3;
        s_arr[n++] = 1.4;
        s_arr[n++] = 1.2;
        s_arr[n++] = 1.1;
        s_arr[n++] = 1.6;
        s_arr[n++] = 1.4;
        s_arr[n++] = 1.3;
        s_arr[n++] = 1.2;

        s_arr[n++] = 1.0;
        s_arr[n++] = 1.0;
        s_arr[n++] = 2.2;   // white
        s_arr[n++] = 1.0;
        s_arr[n++] = 1.0;
        s_arr[n++] = 1.0;   // black
        s_arr[n++] = 1.6;   // light blue
        s_arr[n++] = 1.3;
        s_arr[n++] = 1.1;
        s_arr[n++] = 1.0;
    }
    if (outNumEntries) *outNumEntries = 20;
    return s_arr;
}


#define TWTW_THUMB_DEFAULT_W  128
#define TWTW_THUMB_DEFAULT_H  40


struct _TwtwPage {
    gint refCount;
    
    gint curveCount;
    TwtwCurveList **curves;
    
    TwtwBook *owner;
    
    size_t soundPCMDataSize;
    gdouble soundDuration;
    char *soundTempPath;
    
    // original speex data if loaded from file
    unsigned char *speexData;
    size_t speexDataSize;
    
    // background photo
    TwtwYUVImage *photoImage;
    
    // thumbnail for preview (paper stack UI)
    TwtwPageThumb thumb;
    gboolean thumbIsDirty;
    
    // state during editing
    gpointer editData;
};


TwtwPage *twtw_page_create_with_owner (TwtwBook *book)
{
    TwtwPage *newpage = g_malloc0(sizeof(TwtwPage));
    
    newpage->refCount = 1;
    newpage->owner = book;
    
    // create thumb
    newpage->thumb.w = TWTW_THUMB_DEFAULT_W;
    newpage->thumb.h = TWTW_THUMB_DEFAULT_H;

#ifdef __APPLE__
    newpage->thumb.rgbHasAlpha = TRUE;
#else
    newpage->thumb.rgbHasAlpha = FALSE;
#endif
    
    newpage->thumb.rgbRowBytes = newpage->thumb.w * ((newpage->thumb.rgbHasAlpha) ? 4 : 3);
    
    newpage->thumb.rgbPixels = g_malloc0(newpage->thumb.rgbRowBytes * newpage->thumb.h);
    newpage->thumb.maskPixels = g_malloc0((newpage->thumb.w / 8) * newpage->thumb.h);
    
    newpage->thumbIsDirty = TRUE;
    
    return newpage;
}

TwtwPage *twtw_page_ref (TwtwPage *page)
{
    if ( !page) return NULL;
    
    page->refCount++;
    return page;
}

void twtw_page_destroy (TwtwPage *page)
{
    if ( !page || page->refCount < 1) return;
    
    page->refCount--;
    
    if (page->refCount == 0) {        
        twtw_page_clear_all_data (page);

        g_free(page->thumb.rgbPixels);
        g_free(page->thumb.maskPixels);
    
        g_free(page);
    }

}

void twtw_page_clear_photo (TwtwPage *page)
{
    g_return_if_fail (page);    

    if (page->photoImage) {
        twtw_yuv_image_destroy(page->photoImage);
        page->photoImage = NULL;
    }
    
    page->thumbIsDirty = TRUE;
}

void twtw_page_clear_audio (TwtwPage *page)
{
    g_return_if_fail (page);    

    page->soundPCMDataSize = 0;
    page->soundDuration = 0.0;

    if (page->speexData) {
        g_free(page->speexData);
        page->speexData = NULL;
    }
}

void twtw_page_clear_curves (TwtwPage *page)
{
    g_return_if_fail (page);    
    int i;
    
    for (i = 0; i < page->curveCount; i++) {
        twtw_curvelist_destroy(page->curves[i]);
        page->curves[i] = NULL;
    }
    g_free(page->curves);
    page->curves = NULL;
    
    page->curveCount = 0;
    
    page->thumbIsDirty = TRUE;
}

void twtw_page_clear_all_data (TwtwPage *page)
{
    twtw_page_clear_curves (page);
    twtw_page_clear_audio (page);
    twtw_page_clear_photo (page);
}


gint twtw_page_get_curves_count (TwtwPage *page)
{
    g_return_val_if_fail (page, 0);
    
    return page->curveCount;
}

TwtwCurveList *twtw_page_get_curve (TwtwPage *page, gint index)
{
    g_return_val_if_fail (page, NULL);
    g_return_val_if_fail (index >= 0 && index < page->curveCount, NULL);
    
    return page->curves[index];
}

void twtw_page_add_curve (TwtwPage *page, TwtwCurveList *curve)
{
    g_return_if_fail (page);
    g_return_if_fail (curve);
    
    if ( !page->curves) {
        page->curveCount = 1;
        page->curves = g_malloc(page->curveCount * sizeof(gpointer));
    } else {
        page->curveCount++;
        page->curves = g_realloc(page->curves, page->curveCount * sizeof(gpointer));
    }
    
    page->curves[page->curveCount - 1] = twtw_curvelist_ref (curve);
    
    page->thumbIsDirty = TRUE;
}

void twtw_page_delete_curve_at_index (TwtwPage *page, gint index)
{
    g_return_if_fail (page);
    g_return_if_fail (index >= 0 && index < page->curveCount);

    twtw_curvelist_destroy (page->curves[index]);
    
    TwtwCurveList **newArr = g_malloc((page->curveCount - 1) * sizeof(gpointer));
    
    if (index > 0)
        memcpy(newArr, page->curves, index*sizeof(gpointer));
    
    if (index < page->curveCount - 1)
        memcpy(newArr + index, page->curves + index + 1, (page->curveCount-1-index)*sizeof(gpointer));
        
    g_free(page->curves);
    page->curves = newArr;
    page->curveCount = page->curveCount - 1;
    
    page->thumbIsDirty = TRUE;
}


gint twtw_page_get_sound_duration_in_seconds (TwtwPage *page)
{
    g_return_val_if_fail (page, 0);
    
    return ceil(page->soundDuration);
}

// sound playback (the buffer is allocated internally by the page object and should not be modified)
gint twtw_page_get_pcm_sound_buffer (TwtwPage *page, short **pcmBuffer, size_t *pcmBufferSize)
{
    g_return_val_if_fail (page, TWTW_PARAMERR);
    g_return_val_if_fail (pcmBuffer && pcmBufferSize, TWTW_PARAMERR);
    
    return 0;
}

void twtw_page_set_associated_pcm_data_size (TwtwPage *page, size_t dataSize)
{
    g_return_if_fail (page);
    
    page->soundPCMDataSize = dataSize;
    
    page->soundDuration = (double)dataSize / (TWTW_PCM_SAMPLERATE * (TWTW_PCM_SAMPLEBITS / 8));
    ///printf("page %p: pcmdata size %i -> set sound duration to: %f\n", page, (int)dataSize, (double)page->soundDuration);
}

void twtw_page_set_cached_speex_data (TwtwPage *page, unsigned char *speexData, size_t speexDataSize)
{
    g_return_if_fail (page);
    
    if (page->speexData)
        g_free(page->speexData);
        
    page->speexData = speexData;
    page->speexDataSize = speexDataSize;
    
    ///printf("page %p: cached speex data size: %i\n", page, (int)page->speexDataSize);
}

unsigned char *twtw_page_get_cached_speex_data (TwtwPage *page, size_t *outSpeexDataSize)
{
    g_return_val_if_fail (page, NULL);
    
    if (outSpeexDataSize) *outSpeexDataSize = page->speexDataSize;
    return page->speexData;
}

// this is called by the UI when the user has finished recording a new clip
void twtw_page_ui_did_record_pcm_with_file_size (TwtwPage *page, gint fileSize)
{
    g_return_if_fail (page);
    
    if (fileSize > 44)
        fileSize -= 44;  // subtract WAV header size (not sure if this is even the correct number, but who cares about a few bytes? :))
        
    twtw_page_set_associated_pcm_data_size(page, fileSize);
    
    // clear out cached speex data
    if (page->speexData) {
        g_free(page->speexData);
        page->speexData = NULL;
    }
    page->speexDataSize = 0;
}


void twtw_page_attach_edit_data (TwtwPage *page, gpointer data)
{
    g_return_if_fail (page);
    
    page->editData = data;
}

gpointer twtw_page_get_edit_data (TwtwPage *page)
{
    g_return_val_if_fail (page, NULL);
    
    return page->editData;
}


// only called by the book object
void twtw_page_set_temp_path_for_pcm_sound (TwtwPage *page, char *path)
{
    g_return_if_fail (page);
    g_return_if_fail (path);
    
    page->soundTempPath = path;
}

const char *twtw_page_get_temp_path_for_pcm_sound_utf8 (TwtwPage *page)
{
    g_return_val_if_fail (page, NULL);
    
    return page->soundTempPath;
}


TwtwYUVImage *twtw_page_get_yuv_photo (TwtwPage *page)
{
    g_return_val_if_fail (page, NULL);
    
    return page->photoImage;
}

void twtw_page_copy_yuv_photo (TwtwPage *page, TwtwYUVImage *photo)
{
    g_return_if_fail (page);
    
    if (page->photoImage) {
        twtw_yuv_image_destroy (page->photoImage);
        page->photoImage = NULL;
    }
        
    if (photo) {
        TwtwYUVImage *newimg = g_malloc0(sizeof(TwtwYUVImage));
        newimg->w = photo->w;
        newimg->h = photo->h;
        newimg->rowBytes = photo->rowBytes;
        newimg->pixelFormat = photo->pixelFormat;
        newimg->buffer = g_malloc(newimg->rowBytes * newimg->h);
        
        if (photo->buffer)
            memcpy(newimg->buffer, photo->buffer, newimg->rowBytes * newimg->h);
            
        page->photoImage = newimg;
    }
    
    page->thumbIsDirty = TRUE;
}

// private method
void twtw_page_render_thumb (TwtwPage *page)
{
    TwtwPageThumb *thumb = &(page->thumb);
    const int dstPixStride = (thumb->rgbHasAlpha) ? 4 : 3;

    if (page->photoImage) {
        // static temp buffer so we don't have to malloc every time -- potentially un-threadsafe
        static unsigned char *s_tempBuffer = NULL;
        static int s_tempBufW = 0;
        static int s_tempBufH = 0;
        
        const int xStride = 2;
        const int yStride = 4;
        const int tempW = page->photoImage->w / xStride;
        const int tempH = page->photoImage->h / yStride;
        const int tempBufRowBytes = tempW * dstPixStride;
        
        if ( !s_tempBuffer || s_tempBufW != tempW || s_tempBufH != tempH) {
            g_free(s_tempBuffer);
            s_tempBuffer = g_malloc(tempBufRowBytes * tempH);
            s_tempBufW = tempW;
            s_tempBufH = tempH;
            ///printf("malloced thumb photo temp buffer for size %i * %i\n", tempW, tempH);
        }
    
        twtw_yuv_image_convert_to_rgb_for_display (page->photoImage,
                                                   s_tempBuffer,
                                                   tempBufRowBytes,
                                                   thumb->rgbHasAlpha,
                                                   xStride,
                                                   yStride);
                                                   
        // scaling, and some color correction to make the thumbnail stack stand out better against the current page's background
        const TwtwFixedNum xInc = FIXD_QDIV(TWTW_UNITS_FROM_INT(tempW), TWTW_UNITS_FROM_INT(thumb->w));
        const TwtwFixedNum yInc = FIXD_QDIV(TWTW_UNITS_FROM_INT(tempH), TWTW_UNITS_FROM_FLOAT(thumb->h * 1.15));
        // there's some non-displayed gunk at the bottom of the image frame, so crop it ^^

        const TwtwFixedNum def_pedestal = TWTW_UNITS_FROM_INT(55);
        const TwtwFixedNum def_gain = TWTW_UNITS_FROM_FLOAT(200.0 / 255.0);
        
        unsigned int x, y;
        for (y = 0; y < thumb->h; y++) {
            unsigned char *src = s_tempBuffer + tempBufRowBytes * TWTW_UNITS_TO_INT( FIXD_MUL(yInc, TWTW_UNITS_FROM_INT(y)) );
            unsigned char *dst = thumb->rgbPixels + thumb->rgbRowBytes * y;

            for (x = 0; x < thumb->w; x++) {
                unsigned char *s = src + dstPixStride * TWTW_UNITS_TO_INT( FIXD_MUL(xInc, TWTW_UNITS_FROM_INT(x)) );
                
                // add a highlight to make the thumbnail stand out better against dark backgrounds
                TwtwFixedNum gain = (y > 80 || x > 10) ? def_gain
                                                      : FIXD_MUL(def_gain, FIXD_ONE + TWTW_UNITS_FROM_FLOAT(powf((float)(10 - x) / 10, 3.0) * 1.5));
                
                TwtwFixedNum pedestal = (y > 15) ? def_pedestal
                                                 : def_pedestal + FIXD_MUL(def_pedestal, TWTW_UNITS_FROM_FLOAT(((float)(15 - y) / 15) * 1.5));
                
                int v0 = TWTW_UNITS_TO_INT( pedestal + FIXD_MUL(TWTW_UNITS_FROM_INT(s[0]), gain) );
                int v1 = TWTW_UNITS_TO_INT( pedestal + FIXD_MUL(TWTW_UNITS_FROM_INT(s[1]), gain) );
                int v2 = TWTW_UNITS_TO_INT( pedestal + FIXD_MUL(TWTW_UNITS_FROM_INT(s[2]), gain) );
                dst[0] = MAX(0, MIN(v0, 255));
                dst[1] = MAX(0, MIN(v1, 255));
                dst[2] = MAX(0, MIN(v2, 255));
                if (dstPixStride == 4)
                    dst[3] = s[3];
                dst += dstPixStride;
            }
        }
        // set mask to full opacity since we have a background photo
        memset(thumb->maskPixels, 0xff, (thumb->w / 8) * thumb->h);
    }
    else {
        memset(thumb->rgbPixels, 0, thumb->rgbRowBytes * thumb->h);
        memset(thumb->maskPixels, 0, (thumb->w / 8) * thumb->h);
    }
    
    int curveCount = twtw_page_get_curves_count (page);
    if (curveCount > 0) {
        unsigned char *rgbPalette = twtw_default_color_palette_rgb_array (NULL);
        g_return_if_fail(rgbPalette);
        
        const TwtwFixedNum canvasScaleMul_x = FIXD_QDIV(TWTW_UNITS_FROM_INT(thumb->w), TWTW_CANONICAL_CANVAS_WIDTH_FIXD);
        const TwtwFixedNum canvasScaleMul_y = FIXD_QDIV(TWTW_UNITS_FROM_INT(thumb->h), TWTW_UNITS_FROM_FLOAT(TWTW_CANONICAL_CANVAS_WIDTH / (16.0 / 9.0)));
        
        const int yDispOffset = 40;
        
        ///printf("%s: canvas scale is %.4f (thumb w %i)\n", __func__, TWTW_UNITS_TO_FLOAT(canvasScaleMul), thumb->w);

        int n;
        for (n = 0; n < curveCount; n++) {
            TwtwCurveList *curve = twtw_page_get_curve(page, n);
            // for each curve, just draw pixels at point vertices

            int segCount = twtw_curvelist_get_segment_count (curve);
            TwtwCurveSegment *segs = twtw_curvelist_get_segment_array (curve);            
            
            const int colorID = twtw_curvelist_get_color_id (curve);
            unsigned char *rgbColor = rgbPalette + ((colorID >= 0) ? (colorID*3) : 0);
            
            int i;
            for (i = 0; i < segCount; i++) {
                TwtwCurveSegment *seg = segs + i;
                TwtwPoint startPoint = seg->startPoint;
                TwtwPoint endPoint = seg->endPoint;
                
                startPoint.y += yDispOffset;
                endPoint.y += yDispOffset;
                
                TwtwPoint midPoint = TwtwMakePoint((startPoint.x + endPoint.x) >> 1, (startPoint.y + endPoint.y) >> 1);
                
                int p0_x = TWTW_UNITS_TO_INT( FIXD_MUL(startPoint.x, canvasScaleMul_x) );
                int p0_y = TWTW_UNITS_TO_INT( FIXD_MUL(startPoint.y, canvasScaleMul_y) );

                int p1_x = TWTW_UNITS_TO_INT( FIXD_MUL(midPoint.x, canvasScaleMul_x) );
                int p1_y = TWTW_UNITS_TO_INT( FIXD_MUL(midPoint.y, canvasScaleMul_y) );

#define WRITEPX(dst_, src_)  \
        (dst_)[0] = src_[0];  (dst_)[1] = src_[1];  (dst_)[2] = (src_)[2];  \
        if (dstPixStride == 4)  (dst_)[3] = 255;

                if (p0_x >= 0 && p0_x < thumb->w && p0_y >= 0 && p0_y < thumb->h) {
                    unsigned char *px = thumb->rgbPixels + thumb->rgbRowBytes*p0_y + dstPixStride*p0_x;
                    WRITEPX(px, rgbColor);
                    
                    if (p0_x < thumb->w-1) {
                        WRITEPX(px+dstPixStride, rgbColor);
                    }
                }
                
                if (p1_x >= 0 && p1_x < thumb->w && p1_y >= 0 && p1_y < thumb->h) {
                    unsigned char *px = thumb->rgbPixels + thumb->rgbRowBytes*p1_y + dstPixStride*p1_x;
                    WRITEPX(px, rgbColor);
                    
                    if (p0_x < thumb->w-1) {
                        WRITEPX(px+dstPixStride, rgbColor);
                    }
                }
#undef WRITEPX
            }
        }
    }
}

TwtwPageThumb *twtw_page_get_thumb (TwtwPage *page)
{
    g_return_val_if_fail (page, NULL);
    
    if (page->thumbIsDirty) {
        twtw_page_render_thumb (page);
        page->thumbIsDirty = FALSE;
    }
    return &(page->thumb);
}

void twtw_page_invalidate_thumb (TwtwPage *page)
{
    g_return_if_fail (page);
    
    page->thumbIsDirty = TRUE;
}



#ifdef __APPLE__
#pragma mark --- book ---
#endif


#define TWTW_DOC_PAGECOUNT 20


struct _TwtwBook {
    gint refCount;
    
    gint32 serialNo;
    
    gint pageCount;
    TwtwPage **pages;
    
    char *tempDirPath;
    size_t tempDirPathLen;
    
    gint32 flags;
    char *authorStr;
    char *titleStr;
};



TwtwBook *twtw_book_create ()
{
    TwtwBook *newbook = g_malloc0(sizeof(TwtwBook));
    
    newbook->refCount = 1;
    
    newbook->pageCount = TWTW_DOC_PAGECOUNT;
    newbook->pages = g_malloc(newbook->pageCount * sizeof(gpointer));

    gint i;
    for (i = 0; i < newbook->pageCount; i++) {
        newbook->pages[i] = twtw_page_create_with_owner (newbook);
    }

    twtw_book_regen_serialno (newbook);

    return newbook;
}

TwtwBook *twtw_book_ref (TwtwBook *book)
{
    if ( !book) return NULL;
    
    book->refCount++;
    return book;
}

void twtw_book_clean_temp_files (TwtwBook *book)
{
    g_return_if_fail (book);
    
    twtw_filesys_clean_temp_files_at_path (book->tempDirPath, book->tempDirPathLen);
    g_free(book->tempDirPath);
    book->tempDirPath = NULL;
    book->tempDirPathLen = 0;    
}

const char *twtw_book_get_temp_path (TwtwBook *book)
{
    g_return_val_if_fail (book, NULL);
    
    return book->tempDirPath;
}

void twtw_book_destroy (TwtwBook *book)
{
    if ( !book || book->refCount < 1) return;
    
    book->refCount--;
    
    if (book->refCount == 0) {
        int i;
        for (i = 0; i < book->pageCount; i++) {
            twtw_page_destroy (book->pages[i]);
            book->pages[i] = NULL;
        }
        g_free(book->pages);
        book->pages = NULL;
        book->pageCount = 0;
   
        if (book->tempDirPath) {
            twtw_book_clean_temp_files (book);
        }
    
        g_free(book->authorStr);
        g_free(book->titleStr);
    
        g_free(book);
    }
}

gint32 twtw_book_get_serialno (TwtwBook *book)
{
    g_return_val_if_fail (book, 0);
    return book->serialNo;
}

gint32 twtw_book_regen_serialno (TwtwBook *book)
{
    g_return_val_if_fail (book, 0);
    
    if (book->tempDirPath) {
        twtw_filesys_clean_temp_files_at_path (book->tempDirPath, book->tempDirPathLen);
        g_free(book->tempDirPath);
    }
    
    book->serialNo = serialno_new();
    
    twtw_filesys_generate_temp_dir_path_for_book (book->serialNo, &(book->tempDirPath), &(book->tempDirPathLen));
    
    int i;
    for (i = 0; i < book->pageCount; i++) {
        char str[64];
        sprintf(str, "page%02d.wav", i);
        char *path = twtw_filesys_append_path_component (book->tempDirPath, str);
    
        TwtwPage *page = book->pages[i];
        twtw_page_set_temp_path_for_pcm_sound (page, path);
    }
    
    return book->serialNo;
}

// page access
gint twtw_book_get_page_count (TwtwBook *book)
{
    g_return_val_if_fail (book, 0);
    
    return book->pageCount;
}

TwtwPage *twtw_book_get_page (TwtwBook *book, gint index)
{
    g_return_val_if_fail (book, NULL);
    g_return_val_if_fail (index >= 0 && index < book->pageCount, NULL);
    
    return book->pages[index];
}


const char *twtw_book_get_author (TwtwBook *book)
{
    g_return_val_if_fail (book, NULL);
    return book->authorStr;
}

void twtw_book_set_author (TwtwBook *book, const char *str)
{
    g_return_if_fail (book);
    
    g_free(book->authorStr);
    book->authorStr = (str) ? g_strdup(str) : NULL;
}

const char *twtw_book_get_title (TwtwBook *book)
{
    g_return_val_if_fail (book, NULL);
    return book->titleStr;
}

void twtw_book_set_title (TwtwBook *book, const char *str)
{
    g_return_if_fail (book);
    
    g_free(book->titleStr);
    book->titleStr = (str) ? g_strdup(str) : NULL;
}

gint32 twtw_book_get_flags (TwtwBook *book)
{
    g_return_val_if_fail (book, 0);
    return book->flags;
}

void twtw_book_set_flags (TwtwBook *book, gint32 flags)
{
    g_return_if_fail (book);
    book->flags = flags;
}




#ifdef __APPLE__
#pragma mark ---- active document (global state) ----
#endif

static TwtwBook *g_doc = NULL;
static gint g_docPageIndex = 0;


typedef struct {
    TwtwDocumentNotificationCallback cbFunc;
    void *cbData;
} TwtwNotifCallback;

// this array can contain null cbFuncs, so user must check
static TwtwNotifCallback *g_notifCallbacks = NULL;
static gint g_notifCbCount = 0;
static gint g_notifCbCapacity = 0;
static void *g_withinNotifCb = NULL;  // for recursion prevention

void twtw_notify_about_doc_change (const gint notifID)
{
    if ( !g_notifCallbacks) return;
    
    gint i;
    for (i = 0; i < g_notifCbCount; i++) {
        TwtwNotifCallback *cb = g_notifCallbacks + i;
        
        if (cb->cbFunc && cb != g_withinNotifCb) {
            g_withinNotifCb = cb;
            cb->cbFunc(notifID, cb->cbData);
            g_withinNotifCb = NULL;
        }
    }
}


TwtwBook *twtw_active_document ()
{
    if ( !g_doc) {
        g_doc = twtw_book_create ();
        
        twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_REPLACED);
    }
    return g_doc;
}

void twtw_set_active_document (TwtwBook *book)
{
    if (book != g_doc) {
        // destroy will also call temp cleanup, but just in case there's a reference lingering around,
        // do explicit cleanup now
        twtw_book_clean_temp_files (g_doc);
    
        twtw_book_destroy (g_doc);
        g_doc = twtw_book_ref (book);
        
        twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_REPLACED);
    }
}

TwtwPage *twtw_active_document_page ()
{
    TwtwBook *doc = twtw_active_document();
    return twtw_book_get_page (doc, g_docPageIndex);
}

gint twtw_active_document_page_index ()
{
    return g_docPageIndex;
}

void twtw_set_active_document_page_index (gint index)
{
    if (index != g_docPageIndex) {
        g_docPageIndex = index;
        
        twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_PAGE_INDEX_CHANGED);
    }
}


void twtw_add_active_document_notif_callback (TwtwDocumentNotificationCallback func, void *data)
{
    if ( !func) return;
    
    if ( !g_notifCallbacks) {
        g_notifCbCapacity = 16;
        g_notifCallbacks = g_malloc0 (g_notifCbCapacity * sizeof(TwtwNotifCallback));
    }
    else if (g_notifCbCapacity < g_notifCbCount+1) {
        g_notifCbCapacity += 16;
        g_notifCallbacks = g_realloc (g_notifCallbacks, g_notifCbCapacity * sizeof(TwtwNotifCallback));
    }

    g_notifCallbacks[g_notifCbCount].cbFunc = func;
    g_notifCallbacks[g_notifCbCount].cbData = data;
    g_notifCbCount++;
}

void twtw_remove_active_document_notif_callback (TwtwDocumentNotificationCallback func)
{
    if ( !func) return;
    
    if ( !g_notifCallbacks || g_notifCbCount < 1) return;
    
    int i;
    for (i = 0; i < g_notifCbCount; i++) {
        if (g_notifCallbacks[i].cbFunc == func) {
            memset(g_notifCallbacks+i, 0, sizeof(TwtwNotifCallback));
            break;
        }
    }
}



#ifdef __APPLE__
#pragma mark --- file I/O ---
#endif

#include "twtw-ogg.h"
#include "twtw-byteorder.h"
#include "twtw-audioconv.h"


// a stream of type TWTW_STREAM_PICTURE can contain both curve data and photo data.
// they are identified by a fourCC and a private header; these are the header sizes
// ("twCu" == curve data, "twPh" == photo data)
#define TWTW_HEADERSIZE_twCu   16
#define TWTW_HEADERSIZE_twPh   38


// ------ reading ------

enum {
    TWTW_STREAM_SKELETON = 1,
    TWTW_STREAM_DOCUMENT,
    TWTW_STREAM_PICTURE,
    TWTW_STREAM_SPEEX
};

typedef struct {
    long serialno;
    long type;

    TwtwPictureHeadPacket picHead;
    
    SpeexHeader *speexHead;
    TwtwSpeexStatePtr speexState;
} TwtwOggStreamInfo;

typedef struct {
    gint streamCount;
    TwtwOggStreamInfo *streamInfos;
    
    long skeletonStreamSerial;
    long documentStreamSerial;

    TwtwDocumentHeadPacket docHead;
    TwtwDocumentBonePacket docBone;
    gboolean docIsValid;
    
    TwtwBook *newBook;
} TwtwOggFileInfo;


static int oggzCbReadPacket_countStreamBOS(OGGZ *oggz, ogg_packet *op, long serialno, TwtwOggFileInfo *fileInfo) //void *userData)
{
    //printf("%s: %p, %p, %i, %p\n", __func__, oggz, op, (int)serialno, userData);
    //TwtwOggFileInfo *fileInfo = (TwtwOggFileInfo *)userData;

    //printf("stream %i, bos %i, packet bytes %i\n", serialno, op->b_o_s, op->bytes);
    //printf("fileinfo: streamcount %i, streaminfos %p\n", fileInfo->streamCount, fileInfo->streamInfos);

    if (op->b_o_s == 0)  // break when a non-BOS packet is found
        return OGGZ_STOP_OK;

    gint i;
    for (i = 0; i < fileInfo->streamCount; i++) {
        if (fileInfo->streamInfos[i].serialno == serialno)
            return 0;  // somehow this serial was already listed (shouldn't happen)
    }
    
    //printf("1\n");
    
    fileInfo->streamCount++;    
    fileInfo->streamInfos = ( !fileInfo->streamInfos) ? g_malloc0(fileInfo->streamCount * sizeof(TwtwOggStreamInfo))
                                                      : g_realloc(fileInfo->streamInfos, fileInfo->streamCount * sizeof(TwtwOggStreamInfo));

    //printf("2\n");
    

    TwtwOggStreamInfo *info = fileInfo->streamInfos + (fileInfo->streamCount - 1);
    memset(info, 0, sizeof(TwtwOggStreamInfo));
    info->serialno = serialno;
    info->type = 0;

    //printf("3\n");
    if (op->bytes >= 8) {
        char buf[8];
        memset(buf, 0, 8);
        memcpy(buf, op->packet, 7);
        //printf(".... stream %i: identifier is '%s'\n", fileInfo->streamCount-1, buf);
    }
        
    if (op->bytes >= 8) {
        // check if this is an Ogg skeleton stream
        if (info->type == 0) {
            fishead_packet *fp = g_malloc0(sizeof(fishead_packet));
            int result = fishead_from_ogg(op, fp);
            if (result == 0 && fp->version_major >= 3) {  // supported skeleton version
                info->type = TWTW_STREAM_SKELETON;
                //printf(".. found skeleton stream: serial %i\n", (int)serialno);
                fileInfo->skeletonStreamSerial = serialno;
            }
            g_free(fp);
        }
        if (info->type == 0) {
            //printf("  4b\n");        
            int result = twtwdoc_head_from_ogg(op, &(fileInfo->docHead));
            if (result == 0) {
                info->type = TWTW_STREAM_DOCUMENT;
                //printf(".. found document stream: serial %i\n", (int)serialno);
                fileInfo->documentStreamSerial = serialno;
            }
        }
        if (info->type == 0) {
            //printf("  4c\n");        
            int result = twtwpic_head_from_ogg(op, &(info->picHead));
            if (result == 0) {
                info->type = TWTW_STREAM_PICTURE;
                //printf(".. found picture stream: serial %i, curvecount %i, sound %i\n", (int)serialno, info->picHead.num_curves, info->picHead.sound_duration_in_secs);
            }
        }
        if (info->type == 0) {
            SpeexHeader *speexHead = g_malloc0(sizeof(SpeexHeader));
            //printf("  4d\n");       
            int result = twtw_identify_speex_header(op, speexHead);
            if (result == 0) {
                info->type = TWTW_STREAM_SPEEX;
                printf(".. found speex stream: serial %i, rate %i, bitstream version %i, framesize %i, frames per packet %i,  mode %i\n",
                                (int)serialno, (int)speexHead->rate, (int)speexHead->mode_bitstream_version, (int)speexHead->frame_size, (int)speexHead->frames_per_packet, (int)speexHead->mode);
                
                if (speexHead->rate == 8000) {
                    info->speexHead = g_malloc0(sizeof(SpeexHeader));
                    memcpy(info->speexHead, speexHead, sizeof(SpeexHeader));
                    //printf("  ... created speexhead %p\n", info->speexHead);
                }
            }
            g_free(speexHead);
        }
    }
    return 0;
}

static int oggzCbReadPacket_findDocumentBone(OGGZ *oggz, ogg_packet *op, long serialno, TwtwOggFileInfo *fileInfo)
{
    //TwtwOggFileInfo *fileInfo = (TwtwOggFileInfo *)userData;
    g_assert(op);
    //printf("%s: %p, %p, %i, %p\n", __func__, oggz, op, (int)serialno, fileInfo);


    //printf("%s: serial %i, packet %i, bytes %i, eos %i\n", __func__, (int)serialno, (int)op->packetno, (int)op->bytes, (int)op->e_o_s);

    if (serialno == fileInfo->documentStreamSerial && op->bytes > 8) {
        int result = twtwdoc_bone_from_ogg(op, &(fileInfo->docBone));
        if (result == 0) {
            fileInfo->docIsValid = TRUE;
            return OGGZ_STOP_OK;
        } else {
            printf("** failed to read document bone packet (packet is %i bytes)\n", (int)op->bytes);
            fileInfo->docIsValid = FALSE;
            return OGGZ_STOP_ERR;
        }
    }
    return 0;
}

static int oggzCbReadPacket_findEOS(OGGZ *oggz, ogg_packet *op, long serialno, void *userData)
{
    g_assert(op);
    //TwtwOggFileInfo *fileInfo = (TwtwOggFileInfo *)userData;
    //printf("%s: %i (%i), packet %i, bytes %i, eos %i\n", __func__, (int)serialno, (int)fileInfo->documentStreamSerial, (int)op->packetno, (int)op->bytes, (int)op->e_o_s);
    
    if (op->e_o_s)
        return OGGZ_STOP_OK;
    else
        return 0;
}

static int readPictureFromOggPacketIntoBook(TwtwBook *book, int pageIndex, ogg_packet *op, TwtwPictureHeadPacket *picHead)
{
    g_assert(book);
    g_assert(op);
    TwtwPage *page = twtw_book_get_page (book, pageIndex);
    g_return_val_if_fail (page, OGGZ_STOP_ERR);
    
    //printf("%s: index %i, packet bytes %i, curvecount %i\n", __func__, pageIndex, op->bytes, picHead->num_curves);
    
    unsigned char *data = (unsigned char *)(op->packet);
    gint i;
    
    if (picHead->num_photos > 0) {
        int photoCount = picHead->num_photos;
        // we'll only use the first photo and skip the rest
        for (i = 0; i < photoCount; i++) {
            if (memcmp(data, "twPh", 4)) {  // check for header
                printf("** stream for page index %i: photo %i / %i: failed header check (data position is %i / %i)\n",
                                        pageIndex, i, photoCount, (int)(data - (unsigned char *)op->packet), op->bytes);
                return OGGZ_STOP_ERR;
            }
            unsigned int photoSerializedSize = _le_32 (*((uint32_t *)(data+4)));
            unsigned int photoDataOriginalSize = _le_32 (*((uint32_t *)(data+8)));
            int photoWidth =    _le_16 (*((uint16_t *)(data+12)));
            int photoHeight =   _le_16 (*((uint16_t *)(data+14)));
            int photoRowBytes = _le_16 (*((uint16_t *)(data+16)));
            uint32_t photoPixelFormat = *((uint32_t *)(data+18));  // pixel format fourCCs are little-endian by definition
            uint32_t compressedPixelFormat = *((uint32_t *)(data+22));

            // image dst rectangle; currently unused by the editor, but it's stored in the file format
            // in case the need eventually arises to have multiple photos within a page.
            // width/height of -1 is encoded to mean "use canvas size".            
            int16_t dstRect[4];
            dstRect[0] = _le_16_s (*((uint16_t *)(data+26)));
            dstRect[1] = _le_16_s (*((uint16_t *)(data+28)));
            dstRect[2] = _le_16_s (*((uint16_t *)(data+30)));
            dstRect[3] = _le_16_s (*((uint16_t *)(data+32)));
            
            // metadata size; currently unused.
            uint32_t metadataSizeInBytes = _le_32 (*((uint32_t *)(data+34)));
            
            data += TWTW_HEADERSIZE_twPh + metadataSizeInBytes;
            
            g_return_val_if_fail(photoSerializedSize < op->bytes, OGGZ_STOP_ERR);  // sanity check
            
            TwtwYUVImage *yuvImage = twtw_yuv_image_create_from_serialized (data, photoSerializedSize, photoWidth, photoHeight, compressedPixelFormat, photoDataOriginalSize);
            
            if (yuvImage) {
                twtw_page_copy_yuv_photo (page, yuvImage);
            }
            twtw_yuv_image_destroy (yuvImage);
            
            data += photoSerializedSize;
            /*
            // inflate
            size_t infDataAvailSize = MAX(4096, photoDataInflatedSize);
            unsigned char *infData = g_malloc(infDataAvailSize);        
            size_t inflatedSize = 0;
            twtw_inflate(data, photoDataDeflatedSize,  infData, infDataAvailSize,  &inflatedSize);
            
            printf("did inflate photo: %i -> %i, size %i * %i px, rowbytes %i\n", photoDataDeflatedSize, inflatedSize, photoWidth, photoHeight, photoRowBytes);

            if (photoRowBytes < photoWidth*2)
                photoRowBytes = photoWidth*2;  // sanity check
            
            TwtwYUVImage yuvImage;
            memset(&yuvImage, 0, sizeof(yuvImage));
            yuvImage.w = photoWidth;
            yuvImage.h = photoHeight;
            yuvImage.rowBytes = photoRowBytes;
            yuvImage.pixelFormat = photoPixelFormat;
            yuvImage.buffer = infData;
            
            twtw_page_copy_yuv_photo (page, &yuvImage);
            
            data += photoDataDeflatedSize;
            
            g_free(infData);            
            */            
        }
    }
        
    gint curveCount = picHead->num_curves;
    for (i = 0; i < curveCount; i++) {
        if (memcmp(data, "twCu", 4)) {  // check for header before each curve
            printf("** stream for page index %i: curve %i / %i: failed header check (data position is %i / %i)\n",
                                        pageIndex, i, curveCount, (int)(data - (unsigned char *)op->packet), op->bytes);
            return OGGZ_STOP_ERR;
        }
        uint32_t curveDataDeflatedSize = _le_32 (*((uint32_t *)(data+4)));
        uint32_t curveDataInflatedSize = _le_32 (*((uint32_t *)(data+8)));
        
        // metadata size; currently unused.
        uint32_t metadataSizeInBytes = _le_32 (*((uint32_t *)(data+12)));
        
        data += TWTW_HEADERSIZE_twCu + metadataSizeInBytes;
        
        g_return_val_if_fail(curveDataDeflatedSize < op->bytes, OGGZ_STOP_ERR);  // sanity check
        
        // inflate
        size_t infDataAvailSize = MAX(4096, curveDataInflatedSize);
        unsigned char *infData = g_malloc(infDataAvailSize);
        
        size_t inflatedSize = 0;
        twtw_inflate(data, curveDataDeflatedSize,  infData, infDataAvailSize,  &inflatedSize);
    
        // make curve list
        TwtwCurveList *curvelist = twtw_curvelist_create_from_serialized (infData, inflatedSize);
        
        data += curveDataDeflatedSize;
    
        g_free(infData);
        
        //printf("  .. created curvelist: seg count %i, datasize %i\n", twtw_curvelist_get_segment_count(curvelist), curveDataSize);
        
        if (curvelist)
            twtw_page_add_curve (page, curvelist);
    }
    
    return 0;
}


static int readSpeexFromOggPacketIntoBook(TwtwBook *book, int pageIndex, ogg_packet *op, SpeexHeader *speexHead, TwtwSpeexStatePtr *pSpeexState)
{
    g_return_val_if_fail (pSpeexState, OGGZ_STOP_ERR);

    TwtwPage *page = twtw_book_get_page (book, pageIndex);
    g_return_val_if_fail (page, OGGZ_STOP_ERR);
    
    if (*pSpeexState == NULL) {
        // the file hasn't been opened yet
        const char *path = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
        g_return_val_if_fail (path, OGGZ_STOP_ERR);
        
        //printf("%s: page %i: initing decode to path '%s'\n", __func__, pageIndex, path);

        twtw_speex_init_decoding_to_pcm_path_utf8 (path, strlen(path), pSpeexState);
        if (*pSpeexState == NULL) {
            printf("** failed to init speex decoding (path %s)\n", path);
            return OGGZ_STOP_ERR;
        }
        twtw_speex_read_apply_header (*pSpeexState, speexHead);
    }
    
    if (0 == twtw_speex_read_data_from_ogg_packet (*pSpeexState, op)) {
        return 0;
    } else {
        printf("*** failed to read ogg speex packet\n");
        return OGGZ_STOP_ERR;
    }
}

static int oggzCbReadPacket_readIntoBook(OGGZ *oggz, ogg_packet *op, long serialno, TwtwOggFileInfo *fileInfo)
{
    //TwtwOggFileInfo *fileInfo = (TwtwOggFileInfo *)userData;
    //printf("%s: %p, %p, %i, %p\n", __func__, oggz, op, (int)serialno, fileInfo);
    g_assert(fileInfo);
    g_return_val_if_fail(fileInfo->newBook, OGGZ_STOP_ERR);
    
    if (op->e_o_s) return 0;

    //printf("%s: %i, packet %i, bytes %i, eos %i\n", __func__, (int)serialno, (int)op->packetno, (int)op->bytes, (int)op->e_o_s);
        
    long i, j;
    for (i = 0; i < fileInfo->docHead.num_pages_in_document; i++) {
        if (serialno == fileInfo->docBone.pic_stream_serials[i]) {
            // this is a picture stream; find the pertinent picture header
            TwtwPictureHeadPacket *picHead = NULL;
            for (j = 0; j < fileInfo->streamCount; j++) {
                if (fileInfo->streamInfos[j].serialno == serialno)
                    picHead = &(fileInfo->streamInfos[j].picHead);
            }
            if ( !picHead) {
                printf("** %s: couldn't find picHead for this stream (%i, index in doc %i)\n", __func__, (int)serialno, i);
            } else
                return readPictureFromOggPacketIntoBook(fileInfo->newBook, i, op, picHead);
        }
        else if (serialno == fileInfo->docBone.speex_stream_serials[i]) {
            // this is a speex stream; find the pertinent picture header
            SpeexHeader *speexHead = NULL;
            TwtwSpeexStatePtr *pSpeexState = NULL;
            for (j = 0; j < fileInfo->streamCount; j++) {
                if (fileInfo->streamInfos[j].serialno == serialno) {
                    speexHead = fileInfo->streamInfos[j].speexHead;
                    pSpeexState = &(fileInfo->streamInfos[j].speexState);
                }
            }
            if ( !speexHead) {
                printf("** %s: couldn't find speexHead for this stream (%i, index in doc %i)\n", __func__, (int)serialno, i);
            } else {
                return readSpeexFromOggPacketIntoBook(fileInfo->newBook, i, op, speexHead, pSpeexState);
            }
        }
    }
    
    return 0;
}

gint twtw_book_create_from_path_utf8 (const char *path, size_t pathLen, TwtwBook **outBook)
{
    g_return_val_if_fail (path && pathLen > 0, TWTW_PARAMERR);
    g_return_val_if_fail (outBook, TWTW_PARAMERR);
    
    OGGZ *oggz;
    ogg_packet op;
    memset(&op, 0, sizeof(op));
    
    //printf("%s: going to open\n", __func__);
    
    oggz = oggz_open(path, OGGZ_READ);    
    if ( !oggz)
        return TWTW_FILEERR;

    //printf("... file open ok\n");
    
    gint retval = 0;
    TwtwOggFileInfo *fileInfo = g_malloc0(sizeof(TwtwOggFileInfo));
    
    oggz_set_read_callback(oggz, -1, (OggzReadPacket)oggzCbReadPacket_countStreamBOS, fileInfo);
    
    gint n;
    do {
        n = oggz_read(oggz, 64);
    } while (n > 0);
    
    ///printf("stream count: %i -- document page count: %i\n", fileInfo->streamCount, fileInfo->docHead.num_pages_in_document);
    
    if ( !fileInfo->documentStreamSerial || fileInfo->documentStreamSerial == -1
                                         || fileInfo->docHead.num_pages_in_document <= 0) {
        printf("*** no document stream found in Ogg file (%i streams found; %i; %i pages)\n", fileInfo->streamCount, fileInfo->documentStreamSerial,
                                                                                              fileInfo->docHead.num_pages_in_document);
        retval = TWTW_INVALIDFORMATERR;
        goto bail;
    }
    
    char dummy1[16];
    memset(dummy1, 0, 16);

    char dummy2[16];
    memset(dummy2, 0, 16);
    
    char dummy3[16];
    memset(dummy3, 0, 16);
    
    // read until the document body is found
    oggz_set_read_callback(oggz, -1, (OggzReadPacket)oggzCbReadPacket_findDocumentBone, fileInfo);
    do {
        n = oggz_read(oggz, 64);
        //printf("oggzread: %i\n", n);
    } while (n > 0);
    
    if ( !fileInfo->docIsValid) {
        printf("** no valid document body found (wanted serial is %i)\n", fileInfo->documentStreamSerial);
        retval = TWTW_INVALIDFORMATERR;
        goto bail;
    }

    /*for (i = 0; i < fileInfo->docHead.num_pages_in_document; i++) {
        printf("picture stream serial - %i: %i\n", i, fileInfo->docBone.pic_stream_serials[i]);
    }*/
    
    // the speex headers are located here
    
    // read until document's EOS
    oggz_set_read_callback(oggz, fileInfo->documentStreamSerial, oggzCbReadPacket_findEOS, fileInfo);
    do {
        n = oggz_read(oggz, 64);
    } while (n > 0);

    //printf("ok! creating book\n");
    
    // we can start filling the book
    fileInfo->newBook = twtw_book_create();
    
    oggz_set_read_callback(oggz, -1, (OggzReadPacket)oggzCbReadPacket_readIntoBook, fileInfo);
    do {
        n = oggz_read(oggz, 64);
    } while (n > 0);
    
    
bail:    
    oggz_close(oggz);

    *outBook = fileInfo->newBook;
 
    
    // clean up stream info data
    int j;
    for (j = 0; j < fileInfo->streamCount; j++) {
        TwtwOggStreamInfo *info = &(fileInfo->streamInfos[j]);
    
        if (info->speexHead) {
            g_free(info->speexHead);
        
            // if we decoded something, let the book know about it
            int pcmBytesDecoded = 0;
            unsigned char *speexData = NULL;
            size_t speexDataSize = 0;
            if (info->speexState) {
                twtw_speex_read_finish (info->speexState, &pcmBytesDecoded, &speexData, &speexDataSize);
            }
            
            // find page associated with this stream
            int i;
            for (i = 0; i < fileInfo->docHead.num_pages_in_document; i++) {
                if (info->serialno == fileInfo->docBone.speex_stream_serials[i]) {
                    TwtwPage *page = twtw_book_get_page (fileInfo->newBook, i);
                    
                    twtw_page_set_associated_pcm_data_size (page, pcmBytesDecoded);
                    
                    twtw_page_set_cached_speex_data (page, speexData, speexDataSize);
                }
            }
        }        
        memset(info, 0, sizeof(*info));
    }

    if (fileInfo->newBook && fileInfo->docIsValid) {
        fileInfo->newBook->flags = fileInfo->docBone.document_flags;
     
        // apply metadata fields
        if (fileInfo->docBone.metadata_fields) {
            int numFields = fileInfo->docBone.metadata_field_count;
            char *md = fileInfo->docBone.metadata_fields;
            size_t mdBytesTotal = fileInfo->docBone.metadata_size_in_bytes;
            size_t mdBytesDone = 0;
            int i;
            for (i = 0; i < numFields; i++) {
                char *mdKeyStr = md;
                while (mdBytesDone < mdBytesTotal && *md != 0) {
                    md++;
                    mdBytesDone++;
                }
                if (mdBytesDone >= mdBytesTotal)
                    break;
                md++;
                mdBytesDone++;
                
                char *mdValueStr = md;
                while (mdBytesDone < mdBytesTotal && *md != 0) {
                    md++;
                    mdBytesDone++;
                }
                if (mdBytesDone >= mdBytesTotal)
                    break;
                md++;
                mdBytesDone++;
                    
                if (0 == strcmp(mdKeyStr, "author")) {
                    twtw_book_set_author(fileInfo->newBook, mdValueStr);
                }
                else if (0 == strcmp(mdKeyStr, "title")) {
                    twtw_book_set_title(fileInfo->newBook, mdValueStr);
                }                
            }
        }
    }

    _ogg_free(fileInfo->docBone.metadata_fields);
    g_free(fileInfo->streamInfos);
        
    // done with fileInfo
    g_free(fileInfo);
    
    // at some point there was a stack overwrite bug when opening broken files, this was used as a debugging aid on Maemo
    for (j = 0; j < 16; j++) {
        if (dummy1[j] != 0) printf("** stack junk in dummy1: index %i -- %i\n", j, (int)dummy1[j]);
        if (dummy2[j] != 0) printf("** stack junk in dummy2: index %i -- %i\n", j, (int)dummy2[j]);
        if (dummy3[j] != 0) printf("** stack junk in dummy3: index %i -- %i\n", j, (int)dummy3[j]);
    }
    
    return retval;
}


// ------ writing ------

static void createFisboneForTwtwDocument(ogg_packet *op, long documentSerialno)
{
    fisbone_packet fp;
    memset(&fp, 0, sizeof(fp));    
    fp.serial_no = documentSerialno;
    fp.nr_header_packet = 2;
    fp.granule_rate_n = 100;
    fp.granule_rate_d = 1;
    fp.start_granule = 100;
    fp.preroll = 0;
    fp.granule_shift = 0;
    
    fisbone_add_message_header_field(&fp, "Content-Type", "application/x-twtw.document");
    ogg_from_fisbone(&fp, op);
    op->packetno = -1;  // means oggz will fill
}

static void createFisboneForTwtwPicture(ogg_packet *op, long serialno)
{
    fisbone_packet fp;
    memset(&fp, 0, sizeof(fp));    
    fp.serial_no = serialno;
    fp.nr_header_packet = 1;
    fp.granule_rate_n = 100;
    fp.granule_rate_d = 1;
    fp.start_granule = 100;
    fp.preroll = 0;
    fp.granule_shift = 0;

    fisbone_add_message_header_field(&fp, "Content-Type", "application/x.twtw-picture");
    ogg_from_fisbone(&fp, op);
    op->packetno = -1;  // means oggz will fill
}


static void writePacketNowAndCleanupPacketBuffer(OGGZ *oggz, ogg_packet *op, long serialno)
{
/*    do {
        oggz_write_feed (oggz, op, serialno, OGGZ_FLUSH_AFTER, NULL);
    } while (oggz_write (oggz, 32) > 0);
  */
    int writeRet = oggz_write_feed(oggz, op, serialno, OGGZ_FLUSH_AFTER, NULL);
    while ((oggz_write (oggz, 32)) > 0);  
    
    if (op->packet)
        _ogg_free(op->packet);
    memset(op, 0, sizeof(ogg_packet));
}

static void appendMetadataKeyValuePairToBuffer(char **mdStr, size_t *mdSize, const char *key, const char *value)
{
    size_t keySize = strlen(key);
    size_t valueSize = strlen(value);
    size_t newMdSize = *mdSize + keySize + 1 + valueSize + 1;
    *mdStr = (*mdStr) ? g_realloc(*mdStr, newMdSize) : g_malloc(newMdSize);
    
    memcpy(*mdStr+*mdSize, key, keySize);
    (*mdStr)[*mdSize+keySize] = 0;
    
    memcpy(*mdStr+*mdSize+keySize+1, value, valueSize);
    (*mdStr)[*mdSize+keySize+1+valueSize] = 0;
    
    *mdSize = newMdSize;    
}


gint twtw_book_write_to_path_utf8 (TwtwBook *book, const char *path, size_t pathLen)
{
    g_return_val_if_fail (book, TWTW_PARAMERR);
    g_return_val_if_fail (path && pathLen > 0, TWTW_PARAMERR);

    long i;
    OGGZ *oggz;
    ogg_packet op;
    memset(&op, 0, sizeof(op));
        
    oggz = oggz_open(path, OGGZ_WRITE);
    
    if ( !oggz)
        return TWTW_FILEERR;
        
    // --- 1. headers ---
    // order: skeleton head ("fishead"), document head, picture heads, speex heads
    
    long documentSerialno = twtw_book_get_serialno(book);
    long skeletonSerialno = -1;
    while (skeletonSerialno == -1 || skeletonSerialno == documentSerialno)
        skeletonSerialno = oggz_serialno_new(oggz);
    
    long pictureSerials[20];
    long speexSerials[20];
    TwtwSpeexStatePtr speexStates[20];
    for (i = 0; i < 20; i++) {
        pictureSerials[i] = oggz_serialno_new(oggz);
        speexSerials[i] = oggz_serialno_new(oggz);
        speexStates[i] = NULL;
    }
    
    ///printf("oggz %p:\n ---- 1----- \n    primary stream serialno %i; skeleton serial %i\n", oggz, documentSerialno, skeletonSerialno);

    {
        fishead_packet fp;
        memset(&fp, 0, sizeof(fp));
        fp.ptime_n = 419 * 1000;
        fp.ptime_d = 1000;
        fp.btime_n = 0;
        fp.btime_d = 1000;
        
        ogg_from_fishead(&fp, &op);
    }
    writePacketNowAndCleanupPacketBuffer(oggz, &op, skeletonSerialno);
    
    {
        TwtwDocumentHeadPacket docHead;
        memset(&docHead, 0, sizeof(docHead));
        docHead.version_major = 1;
        docHead.version_minor = 0;
        docHead.num_pages_in_document = 20;
        docHead.granules_per_page = 1000;
    
        ogg_from_twtwdoc_head(&docHead, &op);
    }
    writePacketNowAndCleanupPacketBuffer(oggz, &op, documentSerialno);


    for (i = 0; i < 20; i++) {
        // write picture headers
        TwtwPage *page = twtw_book_get_page (book, i);
        TwtwPictureHeadPacket picHead;
        memset(&picHead, 0, sizeof(picHead));
        picHead.pic_flags = 0;
        picHead.sound_duration_in_secs = twtw_page_get_sound_duration_in_seconds (page);
        picHead.num_curves = twtw_page_get_curves_count(page);
        picHead.num_points = -1;  // twtw_page_get_total_point_count(page);
        picHead.num_photos = (twtw_page_get_yuv_photo(page)) ? 1 : 0;
        //picHead.fg_color_rgba_be = 0;
        //picHead.bg_color_rgba_be = 0xffffffff;

        ogg_from_twtwpic_head(&picHead, &op);
        writePacketNowAndCleanupPacketBuffer(oggz, &op, pictureSerials[i]);
    }
    
    for (i = 0; i<  20; i++) {
        // write speex headers
        TwtwPage *page = twtw_book_get_page (book, i);
        int duration = twtw_page_get_sound_duration_in_seconds (page);
        gboolean useStream = FALSE;
        
        if (duration > 0) {
            TwtwSpeexStatePtr twtwSpeexState = NULL;
            size_t existingBufSize = 0;
            unsigned char *existingBuf = twtw_page_get_cached_speex_data (page, &existingBufSize);
            
            if (existingBuf) {
                // there's an existing Speex buffer available
                if (0 == twtw_speex_init_with_speex_buffer (existingBuf, existingBufSize, &twtwSpeexState)) {
                    useStream = TRUE;
                }
            } else {
                // do PCM->Speex encoding
                const char *audioPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
                if (0 == twtw_speex_init_encoding_from_pcm_path_utf8 (audioPath, strlen(audioPath), &twtwSpeexState)) {
                    useStream = TRUE;
                }
            }
            if (useStream) {
                int result = twtw_speex_write_header_to_oggz (twtwSpeexState, oggz, speexSerials[i]);
                speexStates[i] = twtwSpeexState;
                printf("  page %i: writing speex header to serial %i (existing buffer: %p)\n", i, speexSerials[i], existingBuf);
            }

        }
        if ( !useStream)
            speexSerials[i] = -1;
    }
    
    /*
    const char *audioPath = "/testrec-8k.wav";
    TwtwSpeexStatePtr twtwSpeexState = NULL;
    if (0 == twtw_speex_init_encoding_from_pcm_path_utf8 (audioPath, strlen(audioPath), &twtwSpeexState)) {
        int result = twtw_speex_write_header_to_oggz (twtwSpeexState, oggz, speexSerials[0]);
        printf("   writing speex header to serial %i: result %i\n", speexSerials[0], result);
    }
    */

    ///printf("oggz %p:\n ---- 2----- \n    primary stream serialno %i; skeleton serial %i\n", oggz, documentSerialno, skeletonSerialno);

    // --- 2. skeleton bone packets ---
    // a "fisbone" packet to describe each stream
    createFisboneForTwtwDocument(&op, documentSerialno);
    writePacketNowAndCleanupPacketBuffer(oggz, &op, skeletonSerialno);

    for (i = 0; i < 20; i++) {
        long serial = pictureSerials[i];
        createFisboneForTwtwPicture(&op, serial);
        op.packetno = -1;
        op.e_o_s = 0;
        writePacketNowAndCleanupPacketBuffer(oggz, &op, skeletonSerialno);
    }

    for (i = 0; i < 20; i++) {
        long serial = speexSerials[i];
        
        // TODO: should write speex skeleton fisbones
        
        //.... createFisboneForSpeex(&op, serial);
        //writePacketNowAndCleanupPacketBuffer(oggz, &op, serial);
    }
    

    ///printf("oggz %p:\n ---- 3----- \n    primary stream serialno %i; skeleton serial %i\n", oggz, documentSerialno, skeletonSerialno);
    
    // --- 3. data stream header packets ---
    // document "bone" packet
    {
        //createBonePacketForTwtwDocument(&op, documentSerialno, book);
        TwtwDocumentBonePacket bp;
        memset(&bp, 0, sizeof(bp));

        bp.document_flags = book->flags;
        
        for (i = 0; i < 20; i++) {
            bp.pic_stream_serials[i] = pictureSerials[i];
            bp.speex_stream_serials[i] = speexSerials[i];
        }

        char *creatorID =
#if defined(__APPLE__)
                "20:20_MacOSX";
#elif defined(MAEMO)
                "20:20_Maemo";
#elif defined(__WIN32__) || defined(WIN32)
                "20:20_Windows";
#else
                "20:20";
#endif
        memcpy(bp.creator_id, creatorID, MIN(32, strlen(creatorID)));
        
        // document ID could be used to store a 16-byte UUID eventually
        memset(bp.document_id, 0, 16);
        
        // document canvas rect. this is written out just in case,
        // even though it's a fixed value in the current version of 20:20
        bp.doc_canvas_x = 0;
        bp.doc_canvas_y = 0;
        bp.doc_canvas_w = TWTW_CANONICAL_CANVAS_WIDTH;
        bp.doc_canvas_h = round((double)TWTW_CANONICAL_CANVAS_WIDTH / (16.0 / 9.0));

        // arbitrary metadata
        bp.metadata_field_count = 0;
        char *mdStr = NULL;
        size_t mdSize = 0;

        const char *authorStr = twtw_book_get_author (book);
        if (authorStr) {
            bp.metadata_field_count++;
            appendMetadataKeyValuePairToBuffer(&mdStr, &mdSize, "author", authorStr);
        }
        const char *titleStr = twtw_book_get_title (book);
        if (titleStr) {
            bp.metadata_field_count++;
            appendMetadataKeyValuePairToBuffer(&mdStr, &mdSize, "title", titleStr);
        }
        if (mdSize > 0 && mdStr) {
            bp.metadata_size_in_bytes = mdSize;
            bp.metadata_fields = mdStr;
        }
        ///printf("mdsize %i (author '%s', title '%s')\n", mdSize, authorStr, titleStr);

        ogg_from_twtwdoc_bone(&bp, &op);
        
        g_free(mdStr);
    }
    op.packetno = -1;
    op.e_o_s = 0;
    ///printf("  writing document bone: stream %i, packet size %i\n", documentSerialno, op.bytes);
    
    writePacketNowAndCleanupPacketBuffer(oggz, &op, documentSerialno);
    
    // (the picture streams don't have special headers)
    
    
    ///printf("oggz %p:\n ---- 4 ----- \n    primary stream serialno %i; skeleton serial %i\n", oggz, documentSerialno, skeletonSerialno);
    
    // --- 4. EOS for skeleton and document ---
    memset (&op, 0, sizeof(op));
    op.packetno = -1;  // oggz will set correct packetno
    op.e_o_s = 1;
    writePacketNowAndCleanupPacketBuffer(oggz, &op, skeletonSerialno);

    memset(&op, 0, sizeof(op));
    op.packetno = -1;
    op.e_o_s = 1;
    writePacketNowAndCleanupPacketBuffer(oggz, &op, documentSerialno);


    // --- 5. data streams for pictures ---
    for (i = 0; i < 20; i++) {
        long serialno = pictureSerials[i];
        TwtwPage *page = twtw_book_get_page (book, i);
        gint j;

        unsigned char *pagePictureData = NULL;
        size_t pagePictureDataSize = 0;
        
        // - write background photo -
        TwtwYUVImage *photo = twtw_page_get_yuv_photo (page);
        if (photo && photo->buffer) {
            const size_t photoDataSize = photo->rowBytes * photo->h;
            
            /*
            // deflate photo image data
            size_t defDataAvailSize = photoDataSize + 512;
            unsigned char *defData = g_malloc(defDataAvailSize);
            size_t deflatedSize = 0;
            twtw_deflate(photo->buffer, photoDataSize,  defData, defDataAvailSize,  &deflatedSize);
            
            printf("deflated YUV photo: orig data size %i -> %i  (%i * %i px)\n", photoDataSize, deflatedSize, photo->w, photo->h);
            */
            
            size_t serializedPhotoSize = 0;
            unsigned char *serializedPhotoData = NULL;
            uint32_t serPhotoFourCC = 0;
            twtw_yuv_image_serialize (photo, &serializedPhotoData, &serializedPhotoSize, &serPhotoFourCC);
            
                                    
            const int photoHeaderSize = TWTW_HEADERSIZE_twPh;
            pagePictureDataSize += serializedPhotoSize + photoHeaderSize;  
        
            pagePictureData = ( !pagePictureData) ? g_malloc(pagePictureDataSize)
                                                  : g_realloc(pagePictureData, pagePictureDataSize);
            
            unsigned char *thisData = pagePictureData + pagePictureDataSize - serializedPhotoSize - photoHeaderSize;
            memcpy(thisData, "twPh", 4);
            *((uint32_t *)(thisData+4)) = _le_32 ((uint32_t)serializedPhotoSize);
            *((uint32_t *)(thisData+8)) = _le_32 ((uint32_t)photoDataSize);

            *((uint16_t *)(thisData+12)) = _le_16 ((uint16_t)photo->w);
            *((uint16_t *)(thisData+14)) = _le_16 ((uint16_t)photo->h);
            *((uint16_t *)(thisData+16)) = _le_16 ((uint16_t)photo->rowBytes);
            *((uint32_t *)(thisData+18)) = photo->pixelFormat;  // original image fourCC (already little-endian by definition)
            *((uint32_t *)(thisData+22)) = serPhotoFourCC;      // compressed image fourCC
            
            // image dst rectangle; currently unused by the editor, but it's stored in the file format
            // in case the need eventually arises to have multiple photos within a page.
            // width/height of -1 is encoded to mean "use canvas size".            
            int16_t dstRect[4] = { 0, 0, -1, -1 };
            *((int16_t *)(thisData+26)) = _le_16_s (dstRect[0]);
            *((int16_t *)(thisData+28)) = _le_16_s (dstRect[1]);
            *((int16_t *)(thisData+30)) = _le_16_s (dstRect[2]);
            *((int16_t *)(thisData+32)) = _le_16_s (dstRect[3]);
            
            // metadata size in bytes (for expansion; currently unused)
            *((uint32_t *)(thisData+34)) = _le_32 (0);
            
            memcpy(thisData+photoHeaderSize, serializedPhotoData, serializedPhotoSize);
            
            g_free(serializedPhotoData);
        }
        
        // - write curves -
        const gint curveCount = twtw_page_get_curves_count(page);
        
        for (j = 0; j < curveCount; j++) {
            TwtwCurveList *curve = twtw_page_get_curve (page, j);

            size_t serDataSize = 0;
            unsigned char *serData = NULL;
            twtw_curvelist_serialize (curve, &serData, &serDataSize);
            
            // deflate curve data
            size_t defDataAvailSize = serDataSize + 512;
            unsigned char *defData = g_malloc(defDataAvailSize);
            
            size_t deflatedSize = 0;
            twtw_deflate(serData, serDataSize,  defData, defDataAvailSize,  &deflatedSize);
            
            //if (serDataSize > 0) {
                const int curveHeaderSize = TWTW_HEADERSIZE_twCu;
                pagePictureDataSize += deflatedSize + curveHeaderSize;  
            
                pagePictureData = ( !pagePictureData) ? g_malloc(pagePictureDataSize)
                                                      : g_realloc(pagePictureData, pagePictureDataSize);
                
                // add 4-byte ID + deflated data size + original data size before serialized curve data block
                unsigned char *thisData = pagePictureData + pagePictureDataSize - deflatedSize - curveHeaderSize;
                memcpy(thisData, "twCu", 4);
                *((uint32_t *)(thisData+4)) = _le_32 ((uint32_t)deflatedSize);
                *((uint32_t *)(thisData+8)) = _le_32 ((uint32_t)serDataSize);
                
                // metadata size; currently unused.
                uint32_t metadataSizeInBytes = 0;
                *((uint32_t *)(thisData+12)) = _le_32 (metadataSizeInBytes);
                
                memcpy(thisData+curveHeaderSize, defData, deflatedSize);

                ///printf("writing curve %i: datasize %i (deflated from %i)\n", j, (int)deflatedSize, (int)serDataSize);

                // test to verify that deserialization works
                /*
                if (twtw_curvelist_get_segment_count(curve) > 4) {
                    TwtwCurveList *testDecCurve = twtw_curvelist_create_from_serialized (pageCurveData + pageCurveDataSize - serDataSize, serDataSize);
                    
                    TwtwCurveSegment *origSeg = twtw_curvelist_get_segment_array(curve) + 3;
                    TwtwCurveSegment *decodedSeg = twtw_curvelist_get_segment_array(testDecCurve) + 3;
                    
                    printf("...decoding test:\n  type %i -- %i\n  startx %.3f -- %.3f\n  endx %.3f -- %.3f\n",
                                origSeg->segmentType, decodedSeg->segmentType,
                                TWTW_UNITS_TO_FLOAT(origSeg->startPoint.x), TWTW_UNITS_TO_FLOAT(decodedSeg->startPoint.x),
                                TWTW_UNITS_TO_FLOAT(origSeg->endPoint.x), TWTW_UNITS_TO_FLOAT(decodedSeg->endPoint.x)
                                );
                    
                    twtw_curvelist_destroy(testDecCurve);
                }
                */
            //}
            g_free(defData);
            g_free(serData);
        }
        
        // write data packet for picture stream
        if (pagePictureDataSize > 0 && pagePictureData) {
            memset(&op, 0, sizeof(op));
            op.packet = pagePictureData;
            op.packetno = -1;
            op.b_o_s = 0;
            op.e_o_s = 0;
            op.bytes = pagePictureDataSize;
            
            oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
            while ((oggz_write (oggz, 32)) > 0);
            
            ///printf("picture stream %i (serial %i): wrote ogg packet of %i bytes\n", i, serialno, op.bytes);
            
            g_free(pagePictureData);
            pagePictureData = NULL;
        }
        
        // write EOS packet for picture stream
        memset (&op, 0, sizeof(op));
        op.packetno = -1;
        op.e_o_s = 1;
        writePacketNowAndCleanupPacketBuffer(oggz, &op, serialno);
    }

    
    // --- 6. data streams for speex ---
    for (i = 0; i <  20; i++) {
        if (speexStates[i]) {
            int result = twtw_speex_write_all_data_to_oggz_and_finish (speexStates[i], oggz, speexSerials[i]);
            
            if (result != 0)
                printf("**** speex write failed (page %i; err %i)\n", i, result);
        }
    }

    oggz_close(oggz);    
    return 0;
}


// deflate (zip compression algorithm) is used to compress curve and photo data

gboolean twtw_deflate(unsigned char *srcBuf, size_t srcLen,
                           unsigned char *dstBuf, size_t dstLen,
                           size_t *outCompressedLen)
{
    const int deflateQuality = 7;
    int zret;
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));

    if (Z_OK != deflateInit(&stream, deflateQuality))
        return FALSE;
        
    stream.avail_in = srcLen;
    stream.avail_out = dstLen;
    stream.next_in = (unsigned char *)srcBuf;
    stream.next_out =  (unsigned char *)dstBuf;
    
    zret = deflate(&stream, Z_FINISH);
    
    ///printf("did deflate for twtwdoc, inbytes %i --> outbytes %i\n", (int)srcLen, (int)stream.total_out);
        
    deflateEnd(&stream);
    
    *outCompressedLen = stream.total_out;    
    return (zret == Z_STREAM_END) ? TRUE : FALSE;
}

gboolean twtw_inflate(unsigned char *srcBuf, size_t srcLen,
                           unsigned char *dstBuf, size_t dstLen,
                           size_t *outDecompressedLen)
{
    int zret;
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));

    if (Z_OK != inflateInit(&stream))
        return FALSE;
        
    stream.avail_in = srcLen;    
    stream.avail_out = dstLen;
    stream.next_in = (unsigned char *)srcBuf;
    stream.next_out =  (unsigned char *)dstBuf;
    
    zret = inflate(&stream, Z_FINISH);
    
    ///printf("did inflate for twtwdoc, inbytes %i --> outbytes %i\n", (int)srcLen, (int)stream.total_out);
        
    inflateEnd(&stream);
    
    *outDecompressedLen = stream.total_out;    
    return (zret == Z_STREAM_END) ? TRUE : FALSE;
}



