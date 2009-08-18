/*
 *  twtw-fixedpoint.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 5.12.2008.
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
/*
    Based on work by Tomas Frydrych: http://tthef.net/blog/?p=139
    -- look-up table approach for fixed-point sqrt comes from Allegro library, which is "giftware" licensed.
*/

#ifndef _TWTW_FIXEDPOINT_H_
#define _TWTW_FIXEDPOINT_H_

#include <stdint.h>


// 16.16 fixed point number
typedef int32_t TwtwFixedNum;

// decimal part size in bits
#define FIXD_Q   16
#define FIXD_1   65536

#define FIXD_ONE            ((int32_t) 65536)
#define FIXD_ONE_FLOAT      ((float)   65536.0f)
#define FIXD_HALF           ((int32_t) 32768)
#define FIXD_TENTH          ((int32_t) 6554)
#define FIXD_MAX            ((int32_t) 0x7fffffff)
#define FIXD_MIN            ((int32_t) 0x80000000)

#define FIXD_TO_FLOAT(x)    ((float) ((long)(x) / FIXD_ONE_FLOAT))
#define FIXD_TO_DOUBLE(x)   ((double) ((long)(x) / 65536.0))

#define FIXD_FROM_FLOAT(x)  ((TwtwFixedNum) (x * FIXD_ONE_FLOAT))
#define FIXD_FROM_DOUBLE(x) ((TwtwFixedNum) (x * 65536.0))

#define FIXD_TO_INT(x)      ((x) >> FIXD_Q)

#define FIXD_FROM_INT(x)    ((x) << FIXD_Q)



#define FIXD_MUL(x,y)       ((x) >> 8) * ((y) >> 8)

#define FIXD_DIV(x,y)       ((((x) << 8) / (y)) << 8)

#define FIXD_QMUL(x,y)      (twtw_fixed_qmul (x,y))
#define FIXD_QDIV(x,y)      (twtw_fixed_qdiv (x,y))



// --- inline functions ---

#if defined(MAEMO)
 #ifndef __armv5te__
  #define __armv5te__
 #endif
 #ifndef __arm__
  #define __arm__
 #endif
#endif

#ifndef FUNCATTR_PURE
 // these are GCC-specific
  #define FUNCATTR_PURE           __attribute__ ((pure))
  #define FUNCATTR_ALWAYS_INLINE  __attribute__ ((always_inline))
#endif

#ifndef TWTWINLINE
 // again a GCC-specific declaration.
 // Apple's gcc respects the always_inline attribute, but Maemo 4.1 SDK's gcc apparently doesn't
 // so "extern inline" is required
 #ifdef __APPLE__
  #define TWTWINLINE inline
 #else
  #define TWTWINLINE extern inline
 #endif
#endif


TWTWINLINE TwtwFixedNum twtw_fixed_qmul (TwtwFixedNum a, TwtwFixedNum b)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwFixedNum twtw_fixed_qmul (TwtwFixedNum a, TwtwFixedNum b)
{
// 2009.03.20 - single-inst qmul doesn't work as expected on N800,
// so I'm taking it out for now
#if 0 && defined(__armv5te__)
    int res;
    __asm__("smulbb  %0,%1,%2;\n"
              : "=&r" (res) \
              : "%r"(a), "r"(b));
    return (TwtwFixedNum)res;
    
#elif defined(__arm__) && !defined(__APPLE__)
    // 2009.07.20 - this inline asm doesn't compile on iPhone SDK
    int res, temp1;
    __asm__("smull %0, %1, %2, %3     \n"
            "mov   %0, %0,     lsr %4 \n"
            "add   %0, %0, %1, lsl %5 \n"
                    : "=r" (res), "=r" (temp1) \
                    : "r"(a), "r"(b), "i"(FIXD_Q), "i"(32 - FIXD_Q)
            );
    return (TwtwFixedNum)res;
    
#else
    long long r = (long long) a * (long long) b;
    
    return (TwtwFixedNum)(r >> FIXD_Q);
#endif
}

TWTWINLINE TwtwFixedNum twtw_fixed_qdiv (TwtwFixedNum a, TwtwFixedNum b)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
TWTWINLINE TwtwFixedNum twtw_fixed_qdiv (TwtwFixedNum a, TwtwFixedNum b)
{
  return (TwtwFixedNum)( (((int64_t)a) << FIXD_Q) / b );
}



// --- math functions ---

#ifdef __cplusplus
extern "C" {
#endif

// fixed-point square root
TwtwFixedNum twtw_fixed_sqrt (TwtwFixedNum x);

#ifdef __cplusplus
}
#endif

#endif  //_TWTW_FIXEDPOINT_H_

