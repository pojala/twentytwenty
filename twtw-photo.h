/*
 *  twtw-photo.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 14.3.2009.
 *  Copyright 2009 Pauli Olavi Ojala. All rights reserved.
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
    the twtw photo compression format is very simple.
    
    the in-memory format is YCbCr 4:2:2 ('UYVY' layout) because it's the native format
    that can be directly acquired from most cameras (e.g. N800/N810).
    
    the on-disk format is a zip-compressed 4:2:0 format where additionally bits are truncated
    to achieve higher compression. this format pretty much sucks compared to anything more
    modern (like JPEG... :)), but it serves the purpose of compressing images down to the
    10-30 kB range, and it has two advantages:
    
      - no dependencies on external libraries
      
      - native YUV with lossless roundtripping to 4:2:2
        -> no generational loss when twtw documents are opened and resaved
*/

#ifndef _TWTW_PHOTO_H_
#define _TWTW_PHOTO_H_

#include <glib.h>
#include <stdint.h>
#include "twtw-units.h"


#if defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
#define WORDS_BIGENDIAN
#endif


#if !defined(WORDS_BIGENDIAN)
 #define MAKE_FOURCC_LE(a,b,c,d)        (uint32_t)((a)|(b)<<8|(c)<<16|(d)<<24)
 #define STR_FOURCC_LE(f)               (uint32_t)(((f)[0])|((f)[1]<<8)|((f)[2]<<16)|((f)[3]<<24))
#else
 #define MAKE_FOURCC_LE(a,b,c,d)        (uint32_t)((d)|(c)<<8|(b)<<16|(a)<<24)
 #define STR_FOURCC_LE(f)               (uint32_t)(((f)[3])|((f)[2]<<8)|((f)[1]<<16)|((f)[0]<<24))
#endif


// photo data
typedef struct _TwtwYUVImage {
    int w;
    int h;
    int rowBytes;
    uint32_t pixelFormat;
    unsigned char *buffer;
} TwtwYUVImage;

// images are cropped to this aspect ratio when shot is taken
#define TWTW_CAM_IMAGEWIDTH  320
#define TWTW_CAM_IMAGEHEIGHT 200

// UYVY is the most common YUV format on desktop platforms,
// and it's the default pixel format on Maemo (at least for the N800 camera),
// so it's a sensible choice for 20:20's in-memory format
#define TWTW_CAM_FOURCC_STR     "UYVY"
#define TWTW_CAM_FOURCC         MAKE_FOURCC_LE('U', 'Y', 'V', 'Y')

// identifier for the private compressed format used for serialized representation
#define TWTW_CAM_COMPRESSED_FOURCC_STR      "twYZ"
#define TWTW_CAM_COMPRESSED_FOURCC          MAKE_FOURCC_LE('t', 'w', 'Y', 'Z')



#ifdef __cplusplus
extern "C" {
#endif

void twtw_yuv_image_destroy (TwtwYUVImage *image);

// when dstHasAlpha is specified, writes 255 as last element in 32-bit pixel; otherwise writes 24-bit pixels.
// xstride/ystride arguments can be used to create a downscaled thumbnail
void twtw_yuv_image_convert_to_rgb_for_display (TwtwYUVImage *yuvImage, unsigned char *dstBuf, const size_t dstRowBytes,
                                                const gboolean dstHasAlpha,
                                                const gint srcXStride,
                                                const gint srcYStride);

// for converting RGB / RGBA data (this is not used by the Maemo version, which acquires YUV images directly from the camera)
TwtwYUVImage *twtw_yuv_image_create_from_rgb_with_default_size (unsigned char *srcBuf, const size_t srcRowBytes, const gboolean srcHasAlpha);

// on-disk format for photos
void twtw_yuv_image_serialize (TwtwYUVImage *image, unsigned char **outData, size_t *outDataSize, uint32_t *outDataFourCC);

TwtwYUVImage *twtw_yuv_image_create_from_serialized (unsigned char *data, size_t dataSize, int w, int h, uint32_t dataFourCC, size_t origDataSize);

#ifdef __cplusplus
}
#endif


#endif  // _TWTW_PHOTO_H_
