/*
 *  twtw-maemo.h
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

#ifndef _TWTW_MAEMO_H_
#define _TWTW_MAEMO_H_

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libosso.h>
#include <osso-log.h>
#include <hildon/hildon-program.h>
#include <hildon/hildon-window.h>
#include <gconf/gconf-client.h>

#include <gtk/gtkmain.h>
#include <cairo/cairo.h>
#include "twtw-units.h"


#define SERVICE_NAME "twentytwenty"
#define SERVICE_NAME_FULL "com.anioni.twentytwenty"

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "0.2.0"
#endif


typedef struct
{
    // maemo app state
    HildonProgram *program;
    HildonWindow *window;
  
    osso_context_t *osso;
    GConfClient *gconf_client;
  
    // main screen widgets
    GtkWidget *drawingArea;
    GtkWidget *infoLabel;

    // callback ids
    guint drawingAreaButtonPressCallbackID;
    guint drawingAreaButtonReleaseCallbackID;
} TwtwMaemoAppData;


typedef struct
{
    void (*recSoundAction) (void *, void *);
    void (*playSoundAction) (void *, void *);
    void (*cameraAction) (void *, void *);
    void (*sendDocumentAction) (void *, void *);
    void (*closeAppAction) (void *, void *);
} TwtwCanvasActionCallbacks;


void twtw_canvas_mousedown(GtkWidget *widget, gint x, gint y);
void twtw_canvas_mousedragged(GtkWidget *widget, gint x, gint y);
void twtw_canvas_mouseup(GtkWidget *widget, gint x, gint y);

void twtw_canvas_draw(cairo_t *cr, gint x, gint y, gint w, gint h);

void twtw_canvas_set_rect(gint x, gint y, gint w, gint h);
void twtw_canvas_did_acquire_drawable(gint w, gint h, GdkDrawable *drawable);

void twtw_canvas_queue_full_redraw(GtkWidget *widget);
void twtw_canvas_queue_audio_status_redraw(GtkWidget *widget);

void twtw_canvas_set_action_callbacks(GtkWidget *widget, TwtwCanvasActionCallbacks *callbacks, void *cbData);

void twtw_canvas_set_active_audio_state(GtkWidget *widget, gint state);
void twtw_canvas_set_active_audio_position(gint audioPos);

// OS X style utility, returns time in seconds
double twtw_absolute_time_get_current();

#endif

