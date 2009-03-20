/*
 *  twtw-byteorder.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 4.12.2008.
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


// basic byte order flippers à la Ogg nomenclature.
// these are static functions -> no multiple inclusion of this header

#ifndef _TWTW_BYTEORDER_H_
#define _TWTW_BYTEORDER_H_

#include <stdint.h>


#ifndef FUNCATTR_PURE
 // these are GCC-specific
 #define FUNCATTR_PURE           __attribute__ ((pure))
 #define FUNCATTR_ALWAYS_INLINE  __attribute__ ((always_inline))
#endif


#if defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
 #define WORDS_BIGENDIAN
#endif


static inline uint16_t _le_16 (uint16_t s)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline uint16_t _le_16 (uint16_t s)
{
  uint16_t ret=s;
#ifdef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

static inline int16_t _le_16_s (int16_t s)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline int16_t _le_16_s (int16_t s)
{
  int16_t ret=s;
#ifdef WORDS_BIGENDIAN
  uint16_t u = _le_16( *((uint16_t *)&s) );
  ret = *((int16_t *)&u);
#endif
  return ret;
}

static inline uint16_t _be_16 (uint16_t s)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline uint16_t _be_16 (uint16_t s)
{
  uint16_t ret=s;
#ifndef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

#define _from_le_16  _le_16
#define _from_be_16  _be_16


static inline uint32_t _le_32 (uint32_t i)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline uint32_t _le_32 (uint32_t i)
{
   uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static inline int32_t _le_32_s (int32_t s)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline int32_t _le_32_s (int32_t s)
{
  int32_t ret=s;
#ifdef WORDS_BIGENDIAN
  uint32_t u = _le_32( *((uint32_t *)&s) );
  ret = *((int32_t *)&u);
#endif
  return ret;
}

static inline int64_t _le_64 (int64_t l)  FUNCATTR_PURE  FUNCATTR_ALWAYS_INLINE;
static inline int64_t _le_64 (int64_t l)
{
  int64_t ret=l;
  unsigned char *ucptr = (unsigned char *)&ret;
#ifdef WORDS_BIGENDIAN
  unsigned char temp;

  temp = ucptr [0] ;
  ucptr [0] = ucptr [7] ;
  ucptr [7] = temp ;

  temp = ucptr [1] ;
  ucptr [1] = ucptr [6] ;
  ucptr [6] = temp ;

  temp = ucptr [2] ;
  ucptr [2] = ucptr [5] ;
  ucptr [5] = temp ;

  temp = ucptr [3] ;
  ucptr [3] = ucptr [4] ;
  ucptr [4] = temp ;

#endif
  return (*(int64_t *)ucptr);
}

#define _from_le_32  _le_32
#define _from_le_64  _le_64


#endif

