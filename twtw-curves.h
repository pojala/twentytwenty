/*
 *  twtw-curves.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 25.11.2008.
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

#ifndef _TWTW_CURVES_H_
#define _TWTW_CURVES_H_

#include "twtw-glib.h"
#include "twtw-units.h"


// twtw documents currenly use a fixed canvas width of 640 units when encoded;
// curve serialization implementation scales the curve data into this width when writing files, and the other way around.
#define TWTW_CANONICAL_CANVAS_WIDTH       640
#define TWTW_CANONICAL_CANVAS_WIDTH_FIXD  TWTW_UNITS_FROM_INT(640)


// following is a big old hack to allow device clients to store document curves
// at native resolution while working on a document, then convert to canonical on read/write.

#if defined(MAEMO)
    // Maemo screen width is 800px => 125% of canonical
  #define TWTW_CURVESER_SCALE_IN    TWTW_UNITS_FROM_FLOAT(1.25)
  #define TWTW_CURVESER_SCALE_OUT   TWTW_UNITS_FROM_FLOAT(0.8)
#elif defined(__APPLE__) && !defined(__MACOSX__)
    // iPhone screen width is 480px => 75% of canonical
  #define TWTW_CURVESER_SCALE_IN         TWTW_UNITS_FROM_FLOAT(0.75)
  #define TWTW_CURVESER_SCALE_OUT        TWTW_UNITS_FROM_FLOAT(4.0/3.0)
#else
  #define TWTW_CURVESER_SCALE_IN    FIXD_ONE
  #define TWTW_CURVESER_SCALE_OUT   FIXD_ONE
  #define TWTW_CURVESER_IS_IDENTITY 1
#endif



typedef enum {
    TWTW_SEG_LINEAR = 0,
    TWTW_SEG_BEZIER,
    TWTW_SEG_CATMULLROM
} TwtwSegmentType;


typedef struct _TwtwCurveSegment {
    gint segmentType;
    TwtwPoint startPoint;
    TwtwPoint endPoint;
    TwtwPoint controlPoint1;
    TwtwPoint controlPoint2;
    TwtwUnit startWeight;
    TwtwUnit endWeight;
} TwtwCurveSegment;


typedef struct _TwtwCurveList TwtwCurveList;


#ifdef __cplusplus
extern "C" {
#endif

// --- curvelist object ---

// naming of these create/retain/release functions follows Cairo's model
TwtwCurveList *twtw_curvelist_create ();
TwtwCurveList *twtw_curvelist_ref (TwtwCurveList *curvelist);
TwtwCurveList *twtw_curvelist_copy (TwtwCurveList *curvelist);
void twtw_curvelist_destroy (TwtwCurveList *curvelist);

gint twtw_curvelist_get_segment_count (TwtwCurveList *curvelist);
TwtwCurveSegment twtw_curvelist_get_segment (TwtwCurveList *curvelist, gint index);
//TwtwCurveSegment twtw_curvelist_get_last_segment (TwtwCurveList *curvelist);
TwtwCurveSegment *twtw_curvelist_get_last_segment_ptr (TwtwCurveList *curvelist);
TwtwCurveSegment *twtw_curvelist_get_segment_array (TwtwCurveList *curvelist);

void twtw_curvelist_append_segment (TwtwCurveList *curvelist, TwtwCurveSegment *seg);
void twtw_curvelist_append_segment_continuous (TwtwCurveList *curvelist, TwtwCurveSegment *seg);

void twtw_curvelist_replace_segment (TwtwCurveList *curvelist, gint index, const TwtwCurveSegment *seg);

void twtw_curvelist_delete_segment (TwtwCurveList *curvelist, gint index);

void twtw_curvelist_ensure_continuous_in_range (TwtwCurveList *curvelist, gint index, gint rangeLen);


void twtw_curvelist_set_closed (TwtwCurveList *curvelist, gboolean isClosed);
gboolean twtw_curvelist_get_closed (TwtwCurveList *curvelist);

void twtw_curvelist_set_color_id (TwtwCurveList *curvelist, gint8 colorID);
gint8 twtw_curvelist_get_color_id (TwtwCurveList *curvelist);

// implicit start/end points can be used for catmull-rom spline computation
void twtw_curvelist_set_implicit_start_point (TwtwCurveList *curvelist, TwtwPoint p);
void twtw_curvelist_set_implicit_end_point (TwtwCurveList *curvelist, TwtwPoint p);
TwtwPoint twtw_curvelist_get_implicit_start_point (TwtwCurveList *curvelist);
TwtwPoint twtw_curvelist_get_implicit_end_point (TwtwCurveList *curvelist);

void twtw_curvelist_attach_edit_data (TwtwCurveList *curvelist, gpointer data);
gpointer twtw_curvelist_get_edit_data (TwtwCurveList *curvelist);

// serialization (this will be written to a "twtw picture" ogg stream)
void twtw_curvelist_serialize (TwtwCurveList *curvelist, unsigned char **outData, size_t *outDataSize);
TwtwCurveList *twtw_curvelist_create_from_serialized (unsigned char *data, size_t dataSize);

// --- curve segment utils ---

void twtw_calc_bezier_curve (const TwtwCurveSegment *seg, const gint steps, TwtwPoint *outArray);  // outArray must be of size >= steps

void twtw_calc_catmullrom_curve (const TwtwCurveSegment *seg, const gint steps, TwtwPoint *outArray);


#ifdef __cplusplus
}
#endif

#endif  // _TWTW_CURVES_H_
