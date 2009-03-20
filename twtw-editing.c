/*
 *  twtw-editing.c
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 29.11.2008.
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

#include "twtw-editing.h"
#include "twtw-units.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>


// state during editing
typedef struct _TwtwCurveEditData {
    gint smoothedCount;
} TwtwCurveEditData;



TwtwCurveList *twtw_editing_curvelist_create_with_start_cursor_point (TwtwPoint point)
{
    TwtwCurveList *curvelist = twtw_curvelist_create ();
    
    twtw_curvelist_set_implicit_start_point (curvelist, point);
    
    TwtwCurveEditData *editData = g_malloc0(sizeof(TwtwCurveEditData));
    twtw_curvelist_attach_edit_data (curvelist, editData);
    
    return curvelist;
}


#if defined(MAEMO)
 // these look nice for Maemo's hires screen and pen action
 #define MAX_SMOOTHCOUNT 32
 #define MAX_SMOOTHPOINTDIST TWTW_UNITS_FROM_INT(8)
#else
 // these values seem to work better for mouse interaction
 #define MAX_SMOOTHCOUNT 20
 #define MAX_SMOOTHPOINTDIST TWTW_UNITS_FROM_INT(3)
#endif



#define START_STROKE_WEIGHT (4 * FIXD_TENTH)  //TWTW_UNITS_FROM_FLOAT(0.4)


void twtw_editing_curvelist_add_cursor_point (TwtwCurveList *curvelist, TwtwPoint p)
{
    g_return_if_fail (curvelist);
    
    TwtwCurveEditData *editData = twtw_curvelist_get_edit_data (curvelist);
    g_return_if_fail (editData);
    
    gint segCount = twtw_curvelist_get_segment_count (curvelist);
    
    // previous point
    TwtwCurveSegment *prevSeg = (segCount > 0) ? twtw_curvelist_get_last_segment_ptr(curvelist) : NULL;
    TwtwPoint pp = (prevSeg) ? prevSeg->endPoint
                             : twtw_curvelist_get_implicit_start_point(curvelist);
                                  
    TwtwUnit dist = twtw_point_distance (pp, p);
    
    TwtwUnit startWeight = (prevSeg) ? prevSeg->endWeight : START_STROKE_WEIGHT;
    TwtwUnit endWeight;
    {
    float d = TWTW_UNITS_TO_FLOAT(dist);
    float w;
    /*
    if (d > 2.0) {
        w = 1.0 + log10f((d-2.0)) * 0.5;
    } else {
        w = 0.5 + 0.5 * (d / 2.0);
    }*/
    const float lim = 5.0; // was 2
    if (d > lim) {
        w = 1.0 + log2f(d-lim) * 0.3;
    } else {
        w = 0.5 + 0.5 * (d / lim);
    }
    endWeight = TWTW_UNITS_FROM_FLOAT(w);
    //printf("..weight is %.4f (%.4f)\n", w, d);
    }
    
    gboolean addSeg = TRUE;
        
    if (editData->smoothedCount < MAX_SMOOTHCOUNT && dist < MAX_SMOOTHPOINTDIST && segCount > 0) {
            // for short segments, modify the previous segment for smoothing
            //TwtwCurveSegment prevSeg = twtw_curvelist_get_last_segment (curvelist);
            
            TwtwPoint v1 = TwtwMakePoint(prevSeg->endPoint.x - prevSeg->startPoint.x, prevSeg->endPoint.y - prevSeg->startPoint.y);
            TwtwPoint v2 = TwtwMakePoint(p.x - pp.x, p.y - pp.y);
            
            v1 = twtw_point_vec_normalize (v1);
            v2 = twtw_point_vec_normalize (v2);
            //TwtwUnit len1 = twtw_point_distance(TwtwMakePoint(0, 0), v1);
            //TwtwUnit len2 = twtw_point_distance(TwtwMakePoint(0, 0), v2);
            
            TwtwUnit dotprod = FIXD_QMUL(v1.x, v2.x) + FIXD_QMUL(v1.y, v2.y);
            
            // no fixed-point acos implementation handy
            float dotprod_f = fabsf(TWTW_UNITS_TO_FLOAT(dotprod));
            float angle = acosf(MIN(1.0, MAX(0.0, dotprod_f)));  // clamp to account for fixed-point inaccuracy

            if (angle < 1.4f) {
                TwtwCurveSegment modSeg = *prevSeg;
                TwtwUnit prevLen = twtw_point_distance(modSeg.endPoint, modSeg.startPoint);
                TwtwUnit thisLen = FIXD_MUL(dist, TWTW_UNITS_FROM_INT(2));
                TwtwUnit totalLen = prevLen + thisLen;
                
                TwtwUnit weight1 = FIXD_DIV(prevLen, totalLen);
                TwtwUnit weight2 = FIXD_DIV(thisLen, totalLen);
                
                TwtwPoint newVec = TwtwMakePoint( FIXD_QMUL(weight1, v1.x) + FIXD_QMUL(weight2, v2.x),
                                                  FIXD_QMUL(weight1, v1.y) + FIXD_QMUL(weight2, v2.y) );
                
                TwtwPoint newEndPoint = TwtwMakePoint( pp.x + newVec.x, pp.y + newVec.y );
                modSeg.endPoint = newEndPoint;
                
                TwtwUnit newLen = twtw_point_distance(modSeg.endPoint, modSeg.startPoint);
                
                twtw_curvelist_replace_segment (curvelist, segCount-1, &modSeg);
                
                addSeg = FALSE;
                editData->smoothedCount++;
                
                if (segCount > 2) {
                    modSeg = twtw_curvelist_get_segment (curvelist, segCount-2);
                    prevLen = twtw_point_distance(modSeg.endPoint, modSeg.startPoint);
                    
                    if (prevLen < FIXD_MUL(newLen, FIXD_HALF) && prevLen < TWTW_UNITS_FROM_FLOAT(5.8)) {
                        twtw_curvelist_delete_segment (curvelist, segCount-2);
                        
                        segCount = twtw_curvelist_get_segment_count (curvelist);
                        if (segCount > 2) {
                            modSeg = twtw_curvelist_get_segment (curvelist, segCount-2);
                            prevLen = twtw_point_distance(modSeg.endPoint, modSeg.startPoint);
                            
                            if (prevLen < FIXD_MUL(newLen, 4*FIXD_TENTH)) {
                                twtw_curvelist_delete_segment (curvelist, segCount-2);
                                
                                segCount = twtw_curvelist_get_segment_count (curvelist);
                                if (segCount > 2) {
                                    modSeg = twtw_curvelist_get_segment (curvelist, segCount-2);
                                    prevLen = twtw_point_distance(modSeg.endPoint, modSeg.startPoint);
                            
                                    if (prevLen < FIXD_MUL(newLen, 3*FIXD_TENTH)) {
                                        twtw_curvelist_delete_segment (curvelist, segCount-2);
                                        
                                        segCount = twtw_curvelist_get_segment_count (curvelist);
                                    }
                                }
                            }
                        }
                        twtw_curvelist_ensure_continuous_in_range (curvelist, 0, segCount);
                    }
                }            
            }
    }
        
    if (addSeg) {
            TwtwCurveSegment twseg;
            twseg.segmentType = TWTW_SEG_CATMULLROM;
            twseg.startPoint = pp;
            twseg.endPoint = p;
            twseg.controlPoint1 = TwtwMakePoint(TWTW_UNIT_NAN, TWTW_UNIT_NAN);
            twseg.controlPoint2 = TwtwMakePoint(TWTW_UNIT_NAN, TWTW_UNIT_NAN);
            twseg.startWeight = startWeight;
            twseg.endWeight = endWeight;
        
            twtw_curvelist_append_segment (curvelist, &twseg);
            
            editData->smoothedCount = 0;
    }
}


void twtw_editing_curvelist_finish_at_cursor_point (TwtwCurveList *curvelist, TwtwPoint point)
{
    g_return_if_fail (curvelist);

    twtw_curvelist_set_implicit_end_point (curvelist, point);

    TwtwCurveEditData *editData = twtw_curvelist_get_edit_data (curvelist);
    g_free(editData);
    twtw_curvelist_attach_edit_data (curvelist, NULL);
}

