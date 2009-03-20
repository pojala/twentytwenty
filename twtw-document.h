/*
 *  twtw-document.h
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

#ifndef _TWTW_DOCUMENT_H_
#define _TWTW_DOCUMENT_H_

#include <glib.h>
#include "twtw-curves.h"
#include "twtw-photo.h"


typedef struct _TwtwPage TwtwPage;
typedef struct _TwtwBook TwtwBook;


// error values returned by those page/book sound and file util methods that return a gint error number
enum {
    TWTW_UNKNOWNERR = -10,
    TWTW_FILEERR = -43,
    TWTW_PARAMERR = -50,
    TWTW_INVALIDFORMATERR = -100
};

// document notification IDs
enum {
    TWTW_NOTIF_DOCUMENT_REPLACED = 1,
    TWTW_NOTIF_DOCUMENT_PAGE_INDEX_CHANGED,
    TWTW_NOTIF_DOCUMENT_PAGE_MODIFIED,
    TWTW_NOTIF_DOCUMENT_MODIFIED
};

typedef void (*TwtwDocumentNotificationCallback) (gint notifID, void *userData);


// page thumbnail
typedef struct _TwtwPageThumb {
    gint w;
    gint h;
    gint rgbRowBytes;
    gboolean rgbHasAlpha;       // on some platforms like OS X (Quartz), 24-bit pixels are not directly supported, so it makes sense to include alpha
    unsigned char *rgbPixels;   // 8-bit RGB or RGBA pixel data
    unsigned char *maskPixels;  // 1-bit mask pixel data
} TwtwPageThumb;



#ifdef __cplusplus
extern "C" {
#endif

// ---- application defaults ----
unsigned char *twtw_default_color_palette_rgb_array (gint *outNumEntries);
float *twtw_default_color_palette_line_weight_array (gint *outNumEntries);
gint8 twtw_default_color_index ();

// ---- application global document state ----
TwtwBook *twtw_active_document ();
void twtw_set_active_document (TwtwBook *book);

TwtwPage *twtw_active_document_page ();
gint twtw_active_document_page_index ();
void twtw_set_active_document_page_index (gint index);

void twtw_add_active_document_notif_callback (TwtwDocumentNotificationCallback callback, void *data);
void twtw_remove_active_document_notif_callback (TwtwDocumentNotificationCallback callback);


// ---- book object ----

// naming of these create/retain/release functions follows Cairo's model
TwtwBook *twtw_book_create ();
TwtwBook *twtw_book_ref (TwtwBook *book);
void twtw_book_destroy (TwtwBook *book);

gint32 twtw_book_get_serialno (TwtwBook *book);
gint32 twtw_book_regen_serialno (TwtwBook *book);

// file i/o
gint twtw_book_create_from_path_utf8 (const char *path, size_t pathLen, TwtwBook **outBook);
gint twtw_book_write_to_path_utf8 (TwtwBook *book, const char *path, size_t pathLen);

void twtw_book_clean_temp_files (TwtwBook *book);

// page access
gint twtw_book_get_page_count (TwtwBook *book);
TwtwPage *twtw_book_get_page (TwtwBook *book, gint index);

// metadata
const char *twtw_book_get_author (TwtwBook *book);
void twtw_book_set_author (TwtwBook *book, const char *str);

const char *twtw_book_get_title (TwtwBook *book);
void twtw_book_set_title (TwtwBook *book, const char *str);

gint32 twtw_book_get_flags (TwtwBook *book);
void twtw_book_set_flags (TwtwBook *book, gint32 flags);


// ---- page object ----

void twtw_page_clear_all_data (TwtwPage *page);
void twtw_page_clear_curves (TwtwPage *page);
void twtw_page_clear_photo (TwtwPage *page);
void twtw_page_clear_audio (TwtwPage *page);

// curves
gint twtw_page_get_curves_count (TwtwPage *page);
TwtwCurveList *twtw_page_get_curve (TwtwPage *page, gint index);

void twtw_page_add_curve (TwtwPage *page, TwtwCurveList *curve);
void twtw_page_delete_curve_at_index (TwtwPage *page, gint index);

// audio
gint twtw_page_get_sound_duration_in_seconds (TwtwPage *page);

// sound playback. the buffer is allocated internally by the page object and should not be modified.
// pcm sound data's sample rate and other properties are fixed (defined in twtw-audio.h)
gint twtw_page_get_pcm_sound_buffer (TwtwPage *page, short **pcmBuffer, size_t *pcmBufferSize);

// to associate a recorded sound with this page
const char *twtw_page_get_temp_path_for_pcm_sound_utf8 (TwtwPage *page);

// photo
TwtwYUVImage *twtw_page_get_yuv_photo (TwtwPage *page);
void twtw_page_copy_yuv_photo (TwtwPage *page, TwtwYUVImage *photo);

// thumbnail
TwtwPageThumb *twtw_page_get_thumb (TwtwPage *page);
void twtw_page_invalidate_thumb (TwtwPage *page);

// state during editing
void twtw_page_attach_edit_data (TwtwPage *page, gpointer data);
gpointer twtw_page_get_edit_data (TwtwPage *page);


#ifdef __cplusplus
}
#endif

#endif  // _TWTW_DOCUMENT_H_
