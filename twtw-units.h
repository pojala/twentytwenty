/*
 *  twtw-units.h
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

#ifndef _TWTW_UNITS_H_
#define _TWTW_UNITS_H_

#include "twtw-fixedpoint.h"
#include <stdlib.h>


#include "twtw-glib.h"


typedef int32_t TwtwUnit;    // TwtwUnit maps directly to TwtwFixedNum


#define TWTW_UNITS_FROM_INT(x)        FIXD_FROM_INT ((x))
#define TWTW_UNITS_TO_INT(x)          FIXD_TO_INT ((x))

#define TWTW_UNITS_FROM_FLOAT(x)      FIXD_FROM_FLOAT ((x))
#define TWTW_UNITS_TO_FLOAT(x)        FIXD_TO_FLOAT ((x))

#define TWTW_UNITS_FROM_FIXED(x)      (x)
#define TWTW_UNITS_TO_FIXED(x)        (x)

#ifdef INT32_MIN
 #define TWTW_UNIT_NAN  INT32_MIN
#else
 #define TWTW_UNIT_NAN  INT_MIN
#endif

#define twtw_is_invalid_coord(v_)  (v_ == TWTW_UNIT_NAN)


typedef struct _TwtwPoint {
    TwtwUnit x;
    TwtwUnit y;
} TwtwPoint;

#define twtw_is_invalid_point(p_)  ((p_).x == TWTW_UNIT_NAN || (p_).y == TWTW_UNIT_NAN)

typedef struct _TwtwSize {
    TwtwUnit w;
    TwtwUnit h;
} TwtwSize;

typedef struct _TwtwRect {
    TwtwUnit x;
    TwtwUnit y;
    TwtwUnit w;
    TwtwUnit h;
} TwtwRect;


TWTWINLINE TwtwPoint TwtwMakePoint(TwtwUnit x, TwtwUnit y)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwPoint TwtwMakePoint(TwtwUnit x, TwtwUnit y)
{
    TwtwPoint p = { x, y };
    return p;
}

TWTWINLINE TwtwPoint TwtwMakeFloatPoint(float x, float y)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwPoint TwtwMakeFloatPoint(float x, float y)
{
    TwtwPoint p;
    p.x = TWTW_UNITS_FROM_FLOAT(x);
    p.y = TWTW_UNITS_FROM_FLOAT(y);
    return p;
}



// --- inline math utils ---

TWTWINLINE gboolean twtw_point_is_integral (TwtwPoint p)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE gboolean twtw_point_is_integral (TwtwPoint p)
{
    const int32_t mask = FIXD_ONE - 1;
    int32_t xdec = p.x & mask;
    int32_t ydec = p.y & mask;
    return (xdec == 0 && ydec == 0) ? TRUE : FALSE;
}

TWTWINLINE gboolean twtw_point_is_nearly_integral (TwtwPoint p, int32_t allowedError)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE gboolean twtw_point_is_nearly_integral (TwtwPoint p, int32_t allowedError)
{
    const int32_t mask = (FIXD_ONE - 1) & (~((( (int32_t)1 << allowedError) - 1)) );
    int32_t xdec = p.x & mask;
    int32_t ydec = p.y & mask;
    return (xdec == 0 && ydec == 0) ? TRUE : FALSE;
}

TWTWINLINE TwtwUnit twtw_point_distance (TwtwPoint p1, TwtwPoint p2)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwUnit twtw_point_distance (TwtwPoint p1, TwtwPoint p2)
{
    if (p1.x == p2.x && p1.y == p2.y) return 0;
    
    TwtwUnit w = abs(p2.x - p1.x);
    TwtwUnit h = abs(p2.y - p1.y);
    return twtw_fixed_sqrt( (FIXD_MUL(w, w) + FIXD_MUL(h, h)) );
}

TWTWINLINE TwtwPoint twtw_point_vec_normalize (TwtwPoint p)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwPoint twtw_point_vec_normalize (TwtwPoint p)
{
    if (p.x == 0 && p.y == 0) return p;
    
    TwtwUnit vecLen = twtw_point_distance(TwtwMakePoint(0, 0), p);
    
    if (vecLen == 0) return TwtwMakePoint(0, 0);
    
    p.x = FIXD_QDIV(p.x, vecLen);
    p.y = FIXD_QDIV(p.y, vecLen);
    return p;
}

#endif // _TWTW_UNITS_H_

