/*
 *  twtw-curves.c
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

#include "twtw-curves.h"
#include "twtw-units.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>


// default color for newly created curves
extern gint8 twtw_default_color_index ();


struct _TwtwCurveList {
    gint refCount;
    
    gint segCount;
    TwtwCurveSegment *segs;
    
    // used to provide "invisible" start/end points for Catmull-Rom curves
    TwtwPoint implicitStartPoint;
    TwtwPoint implicitEndPoint;
    
    // state during editing
    gpointer editData;
    
    // display properties
    gint8 colorID;
    gboolean isClosed;
};


TwtwCurveList *twtw_curvelist_create ()
{
    TwtwCurveList *newlist = g_malloc0(sizeof(TwtwCurveList));
    
    newlist->refCount = 1;
    
    newlist->implicitStartPoint.x = TWTW_UNIT_NAN;
    newlist->implicitStartPoint.y = TWTW_UNIT_NAN;

    newlist->implicitEndPoint.x = TWTW_UNIT_NAN;
    newlist->implicitEndPoint.y = TWTW_UNIT_NAN;
    
    newlist->colorID = twtw_default_color_index ();
    
    return newlist;
}

TwtwCurveList *twtw_curvelist_ref (TwtwCurveList *curvelist)
{
    if ( !curvelist) return NULL;
    
    curvelist->refCount++;
    return curvelist;
}

void twtw_curvelist_destroy (TwtwCurveList *curvelist)
{
    if ( !curvelist || curvelist->refCount < 1) return;
    
    curvelist->refCount--;
    
    if (curvelist->refCount == 0) {        
        g_free(curvelist->segs);    
        curvelist->segs = NULL;
        
        curvelist->segCount = 0;
        
        g_free(curvelist);
    }
}


gint twtw_curvelist_get_segment_count (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, 0);
    
    return curvelist->segCount;
}

TwtwCurveSegment twtw_curvelist_get_segment (TwtwCurveList *curvelist, gint index)
{
    if ( !curvelist) {
        g_return_if_fail_warning (G_LOG_DOMAIN, __PRETTY_FUNCTION__, "no curvelist");
        TwtwCurveSegment zeroSeg = { 0, {0,0}, {0,0}, {0,0}, {0,0} };
        return zeroSeg;
    }
    if (index < 0 || index >= curvelist->segCount) {
        g_return_if_fail_warning (G_LOG_DOMAIN, __PRETTY_FUNCTION__, "index out of bounds");
        TwtwCurveSegment zeroSeg = { 0, {0,0}, {0,0}, {0,0}, {0,0} };
        return zeroSeg;
    }
    
    return curvelist->segs[index];
}

TwtwCurveSegment twtw_curvelist_get_last_segment (TwtwCurveList *curvelist)
{
    return twtw_curvelist_get_segment (curvelist, twtw_curvelist_get_segment_count(curvelist) - 1);
}

TwtwCurveSegment *twtw_curvelist_get_last_segment_ptr (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, NULL);
    g_return_val_if_fail (curvelist->segCount > 0, NULL);
    
    return &(curvelist->segs[curvelist->segCount - 1]);
}


TwtwCurveSegment *twtw_curvelist_get_segment_array (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, NULL);
    
    return curvelist->segs;
}


static void setCatmullRomControlPointsInCurve (TwtwCurveList *curvelist, gint index)
{
    TwtwCurveSegment *newSeg = curvelist->segs + index;
    
    if (newSeg->segmentType != TWTW_SEG_CATMULLROM)
        return;

    if (index > 0 && curvelist->segCount > 1) {
        TwtwCurveSegment *prevSeg = curvelist->segs + (index - 1);
        
        newSeg->controlPoint1 = prevSeg->startPoint;
        
        if (prevSeg->segmentType == TWTW_SEG_CATMULLROM)
            prevSeg->controlPoint2 = newSeg->endPoint;
    }
    else {
        if ( !twtw_is_invalid_point(curvelist->implicitStartPoint)) {
            newSeg->controlPoint1 = curvelist->implicitStartPoint;
        }
    }
    
    if (index == curvelist->segCount - 1) {
        if ( !twtw_is_invalid_point(curvelist->implicitEndPoint)) {
            newSeg->controlPoint2 = curvelist->implicitEndPoint;
        }
    } else {
        TwtwCurveSegment *nextSeg = curvelist->segs + (index + 1);
        
        newSeg->controlPoint2 = nextSeg->endPoint;
        
        if (nextSeg->segmentType == TWTW_SEG_CATMULLROM)
            nextSeg->controlPoint1 = newSeg->startPoint;
    }
}

void twtw_curvelist_ensure_continuous_in_range (TwtwCurveList *curvelist, gint index, gint rangeLen)
{
    g_return_if_fail (curvelist);
    g_return_if_fail (index >= 0 && index < curvelist->segCount);
    if (rangeLen < 1) return;
    
    gint endIndex = index + rangeLen;
    g_return_if_fail (endIndex <= curvelist->segCount);
    
    gint i;
    for (i = index; i < endIndex; i++) {
        TwtwCurveSegment *seg = curvelist->segs + i;
        TwtwCurveSegment *prevSeg = (i-1 >= 0) ? (curvelist->segs + (i-1)) : NULL;
        
        if (prevSeg)
            prevSeg->endPoint = seg->startPoint;
    }
}

void twtw_curvelist_replace_segment (TwtwCurveList *curvelist, gint index, const TwtwCurveSegment *aSeg)
{
    g_return_if_fail (curvelist);
    g_return_if_fail (aSeg);
    g_return_if_fail (index >= 0 && index < curvelist->segCount);
    
    memcpy(curvelist->segs+index, aSeg, sizeof(TwtwCurveSegment));
    
    if (aSeg->segmentType == TWTW_SEG_CATMULLROM) {
        setCatmullRomControlPointsInCurve(curvelist, index);
    }
}

void twtw_curvelist_delete_segment (TwtwCurveList *curvelist, gint index)
{
    g_return_if_fail (curvelist);
    g_return_if_fail (index >= 0 && index < curvelist->segCount);
    
    gint newSegCount = curvelist->segCount - 1;
    
    TwtwCurveSegment *newArr = g_malloc(newSegCount * sizeof(TwtwCurveSegment));
    
    if (index > 0)
        memcpy(newArr, curvelist->segs, index * sizeof(TwtwCurveSegment));
        
    if (index < curvelist->segCount - 1)
        memcpy(newArr + index,  curvelist->segs + index + 1,  (curvelist->segCount-1-index) * sizeof(TwtwCurveSegment));
        
    g_free(curvelist->segs);
    curvelist->segs = newArr;
    curvelist->segCount = newSegCount;
    
    if (index > 0 && curvelist->segCount > 0)
        setCatmullRomControlPointsInCurve(curvelist, index-1);
    if (index < curvelist->segCount)
        setCatmullRomControlPointsInCurve(curvelist, index);
}


void twtw_curvelist_append_segment (TwtwCurveList *curvelist, TwtwCurveSegment *aSeg)
{
    g_return_if_fail (curvelist);
    g_return_if_fail (aSeg);
    
    if ( !curvelist->segs) {
        curvelist->segCount = 1;
        curvelist->segs = g_malloc(curvelist->segCount * sizeof(TwtwCurveSegment));
    } else {
        curvelist->segCount++;
        curvelist->segs = g_realloc(curvelist->segs, curvelist->segCount * sizeof(TwtwCurveSegment));
    }
    
    TwtwCurveSegment *newSeg = curvelist->segs + (curvelist->segCount - 1);
    
    memcpy(newSeg, aSeg, sizeof(TwtwCurveSegment));
    
    // if this is a cardinal (catmull-rom) segment, set the prev/next points best we can
    if (newSeg->segmentType == TWTW_SEG_CATMULLROM) {
        setCatmullRomControlPointsInCurve(curvelist, curvelist->segCount - 1);
    }
}

void twtw_curvelist_append_segment_continuous (TwtwCurveList *curvelist, TwtwCurveSegment *aSeg)
{
    g_return_if_fail (curvelist);
    g_return_if_fail (aSeg);
    g_return_if_fail (curvelist->segCount < 1);  // continuous only makes sense if there's a segment to continue from
    
    TwtwCurveSegment cseg;
    memcpy(&cseg, aSeg, sizeof(TwtwCurveSegment));
    
    cseg.startPoint = curvelist->segs[curvelist->segCount-1].endPoint;
    
    twtw_curvelist_append_segment(curvelist, &cseg);
}

void twtw_curvelist_set_closed (TwtwCurveList *curvelist, gboolean isClosed)
{
    g_return_if_fail (curvelist);
    
    curvelist->isClosed = isClosed;
}

gboolean twtw_curvelist_get_closed (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, FALSE);
    
    return curvelist->isClosed;
}

void twtw_curvelist_set_color_id (TwtwCurveList *curvelist, gint8 colorID)
{
    g_return_if_fail (curvelist);
    
    curvelist->colorID = colorID;
}

gint8 twtw_curvelist_get_color_id (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, 0);
    
    return curvelist->colorID;
}


void twtw_curvelist_set_implicit_start_point (TwtwCurveList *curvelist, TwtwPoint p)
{
    g_return_if_fail (curvelist);

    curvelist->implicitStartPoint = p;
    
    if (curvelist->segCount > 0) {
        TwtwCurveSegment *firstSeg = curvelist->segs;
        if (firstSeg->segmentType == TWTW_SEG_CATMULLROM) {
            firstSeg->controlPoint1 = curvelist->implicitStartPoint;
        }
    }
}

void twtw_curvelist_set_implicit_end_point (TwtwCurveList *curvelist, TwtwPoint p)
{
    g_return_if_fail (curvelist);

    curvelist->implicitEndPoint = p;
    
    if (curvelist->segCount > 0) {
        TwtwCurveSegment *lastSeg = curvelist->segs + (curvelist->segCount - 1);
        if (lastSeg->segmentType == TWTW_SEG_CATMULLROM) {
            lastSeg->controlPoint2 = curvelist->implicitEndPoint;
        }
    }
}

TwtwPoint twtw_curvelist_get_implicit_start_point (TwtwCurveList *curvelist)
{
    return curvelist->implicitStartPoint;
}

TwtwPoint twtw_curvelist_get_implicit_end_point (TwtwCurveList *curvelist)
{
    return curvelist->implicitEndPoint;
}


void twtw_curvelist_attach_edit_data (TwtwCurveList *curvelist, gpointer data)
{
    g_return_if_fail (curvelist);
    
    curvelist->editData = data;
}

gpointer twtw_curvelist_get_edit_data (TwtwCurveList *curvelist)
{
    g_return_val_if_fail (curvelist, NULL);
    
    return curvelist->editData;
}


// ---- serialization ----

#include "twtw-byteorder.h"


// twtw documents use a canonical canvas width of 640 pixels.
// Maemo screen is 800px, so must scale up 25% when reading documents.

// TODO: move canvas in/out scaling somewhere more suitable

#if defined(MAEMO)
 #define CURVESER_SCALE_OUT   TWTW_UNITS_FROM_FLOAT(0.8)
 #define CURVESER_SCALE_IN    TWTW_UNITS_FROM_FLOAT(1.25)
 
 #define SER_OUT_PT(p_)       { (p_)->x = FIXD_QMUL((p_)->x, CURVESER_SCALE_OUT);   \
                                (p_)->y = FIXD_QMUL((p_)->y, CURVESER_SCALE_OUT); }
                              
 #define SER_IN_PT(p_)        { (p_)->x = FIXD_QMUL((p_)->x, CURVESER_SCALE_IN);   \
                                (p_)->y = FIXD_QMUL((p_)->y, CURVESER_SCALE_IN); }
                                
 #define SER_OUT_UNIT(v_)     FIXD_QMUL(v_, CURVESER_SCALE_OUT)
 #define SER_IN_UNIT(v_)      FIXD_QMUL(v_, CURVESER_SCALE_IN)

#else
 #define CURVESER_SCALE_OUT   FIXD_ONE
 #define CURVESER_SCALE_IN    FIXD_ONE
 
 #define SER_OUT_PT(p_)      { }
 #define SER_IN_PT(p_)       { }

 #define SER_OUT_UNIT(v_)    ( v_ )
 #define SER_IN_UNIT(v_)     ( v_ )

#endif



void twtw_curvelist_serialize (TwtwCurveList *curvelist, unsigned char **outData, size_t *outDataSize)
{
    g_return_if_fail(curvelist);
    g_return_if_fail(outData && outDataSize);
    
    const gint segCount = curvelist->segCount;
    g_return_if_fail(segCount < 32700 && segCount >= 0);  // sanity check

    const gint headerSize = 2 + 8 + 8 + 1 + 1 + 8;  // 8 bytes for expansion
    unsigned char header[headerSize];
    memset(header, 0, headerSize);
    
    TwtwPoint startP = curvelist->implicitStartPoint;
    TwtwPoint endP = curvelist->implicitEndPoint;
    SER_OUT_PT(&startP);
    SER_OUT_PT(&endP);
    
    *((uint16_t *)(header+0)) = _le_16(segCount);
    *((uint32_t *)(header+2)) = _le_32(startP.x);
    *((uint32_t *)(header+6)) = _le_32(startP.y);
    *((uint32_t *)(header+10)) = _le_32(endP.x);
    *((uint32_t *)(header+14)) = _le_32(endP.y);
    *((uint8_t *)(header+18)) = curvelist->colorID;
    *((uint8_t *)(header+19)) = (curvelist->isClosed) ? 1 : 0;  // this byte could be used for other flags as well
    
    if (segCount == 0) {
        *outDataSize = headerSize;
        *outData = g_malloc(*outDataSize);
        memcpy(*outData, header, headerSize);
        return;
    }
        
    // data size: startpoint + startweight + for each segment: (segtype + endpoint + endweight + [maybe control points])
    gint pointDataSize = 8 + 4;
    gint i;
    for (i = 0; i < segCount; i++) {
        TwtwCurveSegment *seg = curvelist->segs + i;
        TwtwPoint segEndPoint = seg->endPoint;
        SER_OUT_PT(&segEndPoint);

        gint endPointSize = 8;                
        if (twtw_point_is_nearly_integral(segEndPoint, 3)) {
            endPointSize = 4;
        }
        
        gint segExtraDataSize;
        switch (seg->segmentType) {
            case TWTW_SEG_LINEAR:
            case TWTW_SEG_CATMULLROM:
                segExtraDataSize = 0;  break;
            case TWTW_SEG_BEZIER:
                segExtraDataSize = 2 * 8;  break;
        }
        
        pointDataSize += 1 + endPointSize + 4 + segExtraDataSize;
    }
    
    ///printf("serializing curvelist %p:  segcount %i --> datasize %i\n", curvelist, segCount, pointDataSize);
    
    *outDataSize = headerSize + pointDataSize;
    *outData = g_malloc(*outDataSize);
    memcpy(*outData, header, headerSize);
    
    unsigned char *pdata = *outData + headerSize;
    
    TwtwPoint seg0StartPoint = curvelist->segs[0].startPoint;
    SER_OUT_PT(&seg0StartPoint);
    
    *((uint32_t *)(pdata)) =   _le_32(seg0StartPoint.x);
    *((uint32_t *)(pdata+4)) = _le_32(seg0StartPoint.y);
    *((uint32_t *)(pdata+8)) = _le_32(curvelist->segs[0].startWeight);
    pdata += 12;
    
    int integralPointsWritten = 0;
    
    for (i = 0; i < segCount; i++) {
        TwtwCurveSegment *seg = curvelist->segs + i;
        TwtwPoint segEndPoint = seg->endPoint;
        SER_OUT_PT(&segEndPoint);
        
        uint8_t segType = seg->segmentType;
        gboolean writeAsIntegral = FALSE;
        if (twtw_point_is_nearly_integral(segEndPoint, 3)) {  // ignore noise within lowest 3 bits of point decimal part
            writeAsIntegral = TRUE;
            segType |= (1 << 7); 
            integralPointsWritten++;
        }

        *((uint8_t *)(pdata)) = segType;
        pdata += 1;
        
        if ( !writeAsIntegral) {
            *((uint32_t *)(pdata+0)) = _le_32(segEndPoint.x);
            *((uint32_t *)(pdata+4)) = _le_32(segEndPoint.y);
            //printf("  %i: non-integral %x, %x  (%x, %x)\n", i, (int)TWTW_UNITS_TO_INT(seg->endPoint.x), (int)TWTW_UNITS_TO_INT(seg->endPoint.y), seg->endPoint.x, seg->endPoint.y);
            pdata += 8;
        } else {
            uint16_t integralX = TWTW_UNITS_TO_INT(segEndPoint.x);
            uint16_t integralY = TWTW_UNITS_TO_INT(segEndPoint.y);
            //printf("  %i: integral %x, %x  (%x, %x)\n", i, (int)integralX, (int)integralY, seg->endPoint.x, seg->endPoint.y);
            *((uint16_t *)(pdata+0)) = _le_16(integralX);
            *((uint16_t *)(pdata+2)) = _le_16(integralY);
            pdata += 4;
        }
        
        *((uint32_t *)(pdata)) = _le_32(seg->endWeight);
        pdata += 4;
        
        if (seg->segmentType == TWTW_SEG_BEZIER) {
            TwtwPoint cp1 = seg->controlPoint1;
            TwtwPoint cp2 = seg->controlPoint2;
            SER_OUT_PT(&cp1);
            SER_OUT_PT(&cp2);
            *((uint32_t *)(pdata)) =   _le_32(cp1.x);
            *((uint32_t *)(pdata+4)) = _le_32(cp1.y);
            *((uint32_t *)(pdata+8)) = _le_32(cp2.x);
            *((uint32_t *)(pdata+12)) = _le_32(cp2.y);
            pdata += 16;
        }
    }
    
    g_return_if_fail((int)(pdata - *outData) == headerSize+pointDataSize);
    ///printf("   .. written %i bytes; %i points were integral (out of %i)\n", (int)(pdata - *outData), integralPointsWritten, segCount);
}

TwtwCurveList *twtw_curvelist_create_from_serialized (unsigned char *data, size_t dataSize)
{
    g_return_val_if_fail(data, NULL);

    const gint headerSize = 2 + 8 + 8 + 1 + 1 + 8;  // 8 bytes for expansion
    g_return_val_if_fail(dataSize >= headerSize, NULL);
    
    unsigned char *header = data;
    gint segCount = _from_le_16( *((uint16_t *)(header+0)) );
    g_return_val_if_fail(segCount >= 0, NULL);
    g_return_val_if_fail(segCount < 32700, NULL);


    TwtwCurveList *newlist = g_malloc0(sizeof(TwtwCurveList));
    newlist->refCount = 1;
    newlist->segCount = segCount;

    newlist->implicitStartPoint.x = _from_le_32( *((uint32_t *)(header+2)) );
    newlist->implicitStartPoint.y = _from_le_32( *((uint32_t *)(header+6)) );
    
    newlist->implicitEndPoint.x = _from_le_32( *((uint32_t *)(header+10)) );
    newlist->implicitEndPoint.y = _from_le_32( *((uint32_t *)(header+14)) );
    
    SER_IN_PT(&(newlist->implicitStartPoint));
    SER_IN_PT(&(newlist->implicitEndPoint));
    
        //newlist->implicitStartPoint.x = FIXD_QMUL(newlist->implicitStartPoint.x, CURVESER_SCALE_IN);
        //newlist->implicitStartPoint.y = FIXD_QMUL(newlist->implicitStartPoint.y, CURVESER_SCALE_IN);
        //newlist->implicitEndPoint.x = FIXD_QMUL(newlist->implicitEndPoint.x, CURVESER_SCALE_IN);
        //newlist->implicitEndPoint.y = FIXD_QMUL(newlist->implicitEndPoint.y, CURVESER_SCALE_IN);
    
    newlist->colorID = *(header+18);
    newlist->isClosed = (*(header+19) & 0x01) ? TRUE : FALSE;
    
    
    TwtwCurveSegment *segArray = NULL;
    if (segCount > 0) {
        unsigned char *pdata = data + headerSize;
        segArray = g_malloc0(segCount * sizeof(TwtwCurveSegment));
        
        newlist->segs = segArray;
        
        TwtwUnit x = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+0)) ) );
        TwtwUnit y = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+4)) ) );
        segArray[0].startPoint.x = x;
        segArray[0].startPoint.y = y;
        segArray[0].startWeight =  _from_le_32( *((uint32_t *)(pdata+8)) );
        pdata += 12;
        
        TwtwCurveSegment *prevSeg = NULL;
        gint i;
        for (i = 0; i < segCount; i++) {
            TwtwCurveSegment *dseg = segArray + i;
            
            uint8_t decSegType = *pdata;
            pdata += 1;
            
            dseg->segmentType = decSegType & (0xf);
            
            if (decSegType & (1 << 7)) {  // point was encoded as integral
                uint16_t integralX = _from_le_16( *((uint16_t *)(pdata+0)) );
                uint16_t integralY = _from_le_16( *((uint16_t *)(pdata+2)) );
                dseg->endPoint.x = TWTW_UNITS_FROM_INT(integralX);
                dseg->endPoint.y = TWTW_UNITS_FROM_INT(integralY);
                pdata += 4;
            } else {
                dseg->endPoint.x = _from_le_32( *((uint32_t *)(pdata+0)) );
                dseg->endPoint.y = _from_le_32( *((uint32_t *)(pdata+4)) );
                //printf("  %i: decoding non-integral point (%x, %x)\n", i, dseg->endPoint.x, dseg->endPoint.y);
                pdata += 8;
            }
            if (CURVESER_SCALE_IN != FIXD_ONE) {
                SER_IN_PT(&(dseg->endPoint));
            }
            
            dseg->endWeight  = _from_le_32( *((uint32_t *)(pdata)) );
            pdata += 4;
            
            if (dseg->segmentType == TWTW_SEG_BEZIER) {
                TwtwUnit cp1_x = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+0)) ) );
                TwtwUnit cp1_y = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+4)) ) );
                TwtwUnit cp2_x = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+8)) ) );
                TwtwUnit cp2_y = SER_IN_UNIT( _from_le_32( *((uint32_t *)(pdata+12)) ) );
                dseg->controlPoint1.x = cp1_x;
                dseg->controlPoint1.y = cp1_y;
                dseg->controlPoint2.x = cp2_x;
                dseg->controlPoint2.y = cp2_y;
                pdata += 16;
            }
            
            if (prevSeg) {
                dseg->startPoint = prevSeg->endPoint;
                dseg->startWeight = prevSeg->endWeight;
            }
            prevSeg = dseg;
            
            setCatmullRomControlPointsInCurve (newlist, i);
        }
        
        ///printf("   .. decoded %i bytes (header %i bytes)\n", (int)(pdata - data), (int)headerSize);
    }
    
    return newlist;
}



// ---- curve utils ----

void twtw_calc_bezier_curve (const TwtwCurveSegment *pseg, const gint steps, TwtwPoint *outArray)
{
    g_return_if_fail (pseg);
    g_return_if_fail (outArray);
    g_return_if_fail (steps > 0);

    TwtwPoint A, B, C, D;
    const TwtwCurveSegment seg = *pseg;
    const TwtwUnit f_2 = FIXD_FROM_INT(2);
    const TwtwUnit f_3 = FIXD_FROM_INT(3);

    TwtwPoint tangent2;
    tangent2.x = FIXD_MUL(f_3,  (seg.endPoint.x - seg.controlPoint2.x));
    tangent2.y = FIXD_MUL(f_3,  (seg.endPoint.y - seg.controlPoint2.y));
    
    D = seg.startPoint;

    C.x = FIXD_MUL(f_3,  (seg.controlPoint1.x - seg.startPoint.x));
    C.y = FIXD_MUL(f_3,  (seg.controlPoint1.y - seg.startPoint.y));
    
    B.x = FIXD_MUL(f_3, (seg.endPoint.x - seg.startPoint.x)) - FIXD_MUL(f_2, C.x) - tangent2.x;
    B.y = FIXD_MUL(f_3, (seg.endPoint.y - seg.startPoint.y)) - FIXD_MUL(f_2, C.y) - tangent2.y;
    
    A.x = FIXD_MUL(f_2, (seg.startPoint.x - seg.endPoint.x)) + C.x + tangent2.x;
    A.y = FIXD_MUL(f_2, (seg.startPoint.y - seg.endPoint.y)) + C.y + tangent2.y;

    int inc;
    int incct;
    switch (steps) {
        default:  inc = 0; break;
        case 2:   inc = FIXD_HALF;  break;
        case 4:   inc = FIXD_ONE >> 2;  break;
        case 8:   inc = FIXD_ONE >> 3;  break;
        case 16:  inc = FIXD_ONE >> 4;  break;
        case 32:  inc = FIXD_ONE >> 5;  break;
        case 64:  inc = FIXD_ONE >> 6;  break;
    }
    
    int i;
    for (i = 0; i < steps; i++) {
        TwtwUnit u;
        if (inc) {
            u = incct;
            incct += inc;
        } else {
            u = (FIXD_FROM_INT(i) / steps);
        }
        
        TwtwUnit ax = FIXD_MUL(u, A.x);
        TwtwUnit ay = FIXD_MUL(u, A.y);

        TwtwUnit bx = FIXD_MUL(u, B.x + ax);
        TwtwUnit by = FIXD_MUL(u, B.y + ay);
        
        TwtwUnit cx = FIXD_MUL(u, C.x + bx);
        TwtwUnit cy = FIXD_MUL(u, C.y + by);
        
        outArray[i].x = D.x + cx;
        outArray[i].y = D.y + cy;
    }
}


static void computeTangentPointsForCardinalSeg(TwtwCurveSegment *seg)
{
    // cardinal spline tangent points are computed from the previous and next points.
    // these need to be passed as cp1/cp2.
    // formula for the tangents is: T[i] = a * ( P[i+1] - P[i-1] )
    TwtwPoint p0 = seg->controlPoint1;
    TwtwPoint p1 = seg->startPoint;
    TwtwPoint p2 = seg->endPoint;
    TwtwPoint p3 = seg->controlPoint2;
    
    // "tightness constant" is 0.5 for catmull-rom splines
    const TwtwUnit tightness_a1 = FIXD_HALF;
    const TwtwUnit tightness_a2 = FIXD_HALF;
    
    const TwtwUnit t1_x = FIXD_QMUL(tightness_a1, (p2.x - p0.x));
    const TwtwUnit t1_y = FIXD_QMUL(tightness_a1, (p2.y - p0.y));
    
    const TwtwUnit t2_x = FIXD_QMUL(tightness_a2, (p3.x - p1.x));
    const TwtwUnit t2_y = FIXD_QMUL(tightness_a2, (p3.y - p1.y));
    
    seg->controlPoint1.x = t1_x;
    seg->controlPoint1.y = t1_y;
    
    seg->controlPoint2.x = t2_x;
    seg->controlPoint2.y = t2_y;
}

void twtw_calc_catmullrom_curve (const TwtwCurveSegment *pseg, const gint steps, TwtwPoint *outArray)
{
    g_return_if_fail (pseg);
    g_return_if_fail (outArray);
    g_return_if_fail (steps > 0);

    TwtwCurveSegment seg = *pseg;
    const TwtwUnit f_1 = FIXD_FROM_INT(1);
    const TwtwUnit f_2 = FIXD_FROM_INT(2);
    const TwtwUnit f_3 = FIXD_FROM_INT(3);

    computeTangentPointsForCardinalSeg(&seg);

    // hermite curve parameter matrix
    const TwtwUnit m11 =  f_2,  m12 = -f_3,  m14 =  f_1;
    const TwtwUnit m21 = -f_2,  m22 =  f_3;
    const TwtwUnit m31 =  f_1,  m32 = -f_2,  m33 =  f_1;
    const TwtwUnit m41 =  f_1,  m42 = -f_1;
    
    int inc;
    int incct = 0;
    switch (steps) {
        default:  inc = 0; break;
        case 2:   inc = FIXD_HALF;  break;
        case 4:   inc = FIXD_ONE >> 2;  break;
        case 8:   inc = FIXD_ONE >> 3;  break;
        case 16:  inc = FIXD_ONE >> 4;  break;
        case 32:  inc = FIXD_ONE >> 5;  break;
        case 64:  inc = FIXD_ONE >> 6;  break;
    }
    
    int i;
    for (i = 0; i < steps; i++) {
        TwtwUnit u;
        if (inc) {
            u = incct;
            incct += inc;
        } else {
            u = (FIXD_FROM_INT(i) / steps);
        }
        
        const TwtwUnit u2 = FIXD_QMUL(u, u);
        const TwtwUnit u3 = FIXD_QMUL(u, u2);
        
        const TwtwUnit h1 = FIXD_MUL(m11, u3) + FIXD_MUL(m12, u2) + m14;
        const TwtwUnit h2 = FIXD_MUL(m21, u3) + FIXD_MUL(m22, u2);
        const TwtwUnit h3 = FIXD_MUL(m31, u3) + FIXD_MUL(m32, u2) + FIXD_MUL(m33, u);
        const TwtwUnit h4 = FIXD_MUL(m41, u3) + FIXD_MUL(m42, u2);
        
        outArray[i].x = FIXD_MUL(h1, seg.startPoint.x) + FIXD_MUL(h2, seg.endPoint.x) + FIXD_MUL(h3, seg.controlPoint1.x) + FIXD_MUL(h4, seg.controlPoint2.x);
        outArray[i].y = FIXD_MUL(h1, seg.startPoint.y) + FIXD_MUL(h2, seg.endPoint.y) + FIXD_MUL(h3, seg.controlPoint1.y) + FIXD_MUL(h4, seg.controlPoint2.y);
    }
}
