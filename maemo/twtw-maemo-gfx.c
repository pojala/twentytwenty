/*
 *  twtw-maemo-gfx.c
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

#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

// these files were created using the gdk-pixbuf-csource utility, e.g.:
//   "gdk-pixbuf-csource --struct --name=boombox  MANKKA3.png > inlinegfx-boombox.h"
#include "gfx/maemo-800px/inlinegfx-boombox.h"
#include "gfx/maemo-800px/inlinegfx-pencils.h"
#include "gfx/maemo-800px/inlinegfx-camera.h"
#include "gfx/maemo-800px/inlinegfx-envelope.h"
#include "gfx/maemo-800px/inlinegfx-paper.h"
#include "gfx/maemo-800px/inlinegfx-playbutton.h"
#include "gfx/maemo-800px/inlinegfx-pausebutton.h"
#include "gfx/maemo-800px/inlinegfx-recbutton.h"
#include "gfx/maemo-800px/inlinegfx-stopbutton.h"



GdkPixbuf *twtw_get_pixbuf_for_ui_element (const char *elementName)
{
    static GdkPixbuf *s_pixbuf_boombox = NULL;
    static GdkPixbuf *s_pixbuf_pencils = NULL;
    static GdkPixbuf *s_pixbuf_camera = NULL;
    static GdkPixbuf *s_pixbuf_envelope = NULL;
    static GdkPixbuf *s_pixbuf_paper = NULL;
    static GdkPixbuf *s_pixbuf_playbutton = NULL;
    static GdkPixbuf *s_pixbuf_pausebutton = NULL;
    static GdkPixbuf *s_pixbuf_recbutton = NULL;
    static GdkPixbuf *s_pixbuf_stopbutton = NULL;


    if (0 == strcmp(elementName, "boombox")) {
        if ( !s_pixbuf_boombox) {
            s_pixbuf_boombox = gdk_pixbuf_from_pixdata(&inline_boombox, TRUE, NULL);
        }
        return s_pixbuf_boombox;
    }

    if (0 == strcmp(elementName, "pencils")) {
        if ( !s_pixbuf_pencils) {
            s_pixbuf_pencils = gdk_pixbuf_from_pixdata(&inline_pencils, TRUE, NULL);
        }
        return s_pixbuf_pencils;
    }

    if (0 == strcmp(elementName, "camera")) {
        if ( !s_pixbuf_camera) {
            s_pixbuf_camera = gdk_pixbuf_from_pixdata(&inline_camera, TRUE, NULL);
        }
        return s_pixbuf_camera;
    }

    if (0 == strcmp(elementName, "envelope")) {
        if ( !s_pixbuf_envelope) {
            s_pixbuf_envelope = gdk_pixbuf_from_pixdata(&inline_envelope, TRUE, NULL);
        }
        return s_pixbuf_envelope;
    }

    if (0 == strcmp(elementName, "paper")) {
        if ( !s_pixbuf_paper) {
            s_pixbuf_paper = gdk_pixbuf_from_pixdata(&inline_paper, TRUE, NULL);
        }
        return s_pixbuf_paper;
    }

    if (0 == strcmp(elementName, "playbutton")) {
        if ( !s_pixbuf_playbutton) {
            s_pixbuf_playbutton = gdk_pixbuf_from_pixdata(&inline_playbutton, TRUE, NULL);
        }
        return s_pixbuf_playbutton;
    }

    if (0 == strcmp(elementName, "recbutton")) {
        if ( !s_pixbuf_recbutton) {
            s_pixbuf_recbutton = gdk_pixbuf_from_pixdata(&inline_recbutton, TRUE, NULL);
        }
        return s_pixbuf_recbutton;
    }

    if (0 == strcmp(elementName, "stopbutton")) {
        if ( !s_pixbuf_stopbutton) {
            s_pixbuf_stopbutton = gdk_pixbuf_from_pixdata(&inline_stopbutton, TRUE, NULL);
        }
        return s_pixbuf_stopbutton;
    }

    if (0 == strcmp(elementName, "pausebutton")) {
        if ( !s_pixbuf_pausebutton) {
            s_pixbuf_pausebutton = gdk_pixbuf_from_pixdata(&inline_pausebutton, TRUE, NULL);
        }
        return s_pixbuf_pausebutton;
    }

    printf("** unable to find ui element '%s'\n", elementName);
    return NULL;
}


