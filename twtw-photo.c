/*
 *  twtw-photo.c
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

#include "twtw-photo.h"
#include "twtw-curves.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#if defined(__STDC_VERSION__) && (__STDC_VERSION__ == 199901L)
 #define RESTRICT restrict
#elif defined(__GNUC__)
 #define RESTRICT __restrict
#else
 #define RESTRICT
#endif


extern gboolean twtw_deflate(unsigned char *srcBuf, size_t srcLen,
                      unsigned char *dstBuf, size_t dstLen,
                           size_t *outCompressedLen);
                           
extern gboolean twtw_inflate(unsigned char *srcBuf, size_t srcLen,
                      unsigned char *dstBuf, size_t dstLen,
                           size_t *outDecompressedLen);



void twtw_yuv_image_destroy (TwtwYUVImage *image)
{
    if ( !image) return;
    
    if (image->buffer) g_free(image->buffer);
        
    memset(image, 0, sizeof(TwtwYUVImage));
    g_free(image);
}

TwtwYUVImage *twtw_yuv_image_copy (TwtwYUVImage *photo)
{
    if ( !photo) return NULL;

    TwtwYUVImage *newimg = g_malloc0(sizeof(TwtwYUVImage));
    newimg->w = photo->w;
    newimg->h = photo->h;
    newimg->rowBytes = photo->rowBytes;
    newimg->pixelFormat = photo->pixelFormat;
    newimg->buffer = g_malloc(newimg->rowBytes * newimg->h);
    
    if (photo->buffer)
        memcpy(newimg->buffer, photo->buffer, newimg->rowBytes * newimg->h);
            
    return newimg;
}


#ifndef FIXD_255
 #define FIXD_255  FIXD_FROM_INT(255)
#endif

#ifndef FIXD_CLAMP_255
 #define FIXD_CLAMP_255(a_)   ( (a_ > FIXD_255) ? FIXD_255 : ((a_ < 0) ? 0 : a_) )
#endif

#define LUTSIZE 256

static unsigned char *getGammaLUTForBGPhoto()
{
    static unsigned char *s_bgPhotoGammaLUT = NULL;  
    if ( !s_bgPhotoGammaLUT) {
        // color correct with an S-shaped catmull-rom curve.
        // (this is not the proper way to compute the curve at constant X intervals, but it makes no practical difference)
        TwtwCurveSegment seg;
        memset(&seg, 0, sizeof(seg));
        seg.segmentType = TWTW_SEG_CATMULLROM;
        seg.startPoint = TwtwMakeFloatPoint(0.0, 0.0);
        seg.endPoint = TwtwMakeFloatPoint(255.0, 255.0);
        seg.controlPoint1 = TwtwMakeFloatPoint(-0.9 * 255.0,  -0.2 * 255.0);
        seg.controlPoint2 = TwtwMakeFloatPoint(2.9 * 255.0,   0.8 * 255.0);
        
        TwtwPoint *lutCurvePoints = g_malloc(LUTSIZE*2 * sizeof(TwtwPoint));
        twtw_calc_catmullrom_curve (&seg, LUTSIZE*2, lutCurvePoints);
        
        //printf("... %i: %.4f; %i: %.4f; %i: %.4f\n",  3, TWTW_UNITS_TO_FLOAT(lutCurvePoints[3].y),  10, TWTW_UNITS_TO_FLOAT(lutCurvePoints[10].y), 
        //                                                    50, TWTW_UNITS_TO_FLOAT(lutCurvePoints[50].y) );
    
        s_bgPhotoGammaLUT = g_malloc0(LUTSIZE + 16); // extra size for overflow paranoia
        int n;
        for (n = 0; n < LUTSIZE; n++) {
            //float f = (float)n / 255.0f;
            //float ccf = powf(f, 0.7f);
            //ccf = MIN(1.0f, MAX(0.0f, ccf)); 
            //ccf *= 255.0f;
            TwtwUnit wantedX = TWTW_UNITS_FROM_INT(n);
            
            int ix = 0;
            for (ix = 0; ix < LUTSIZE*2-1; ix++) {
                if (lutCurvePoints[ix].x >= wantedX) break;
            }
                
            TwtwUnit y = lutCurvePoints[ix].y;
            
            //y = FIXD_QMUL(y, FIXD_255);
            s_bgPhotoGammaLUT[n] = FIXD_TO_INT( FIXD_CLAMP_255(y) );  //(unsigned char)ccf;
        }
        
        g_free(lutCurvePoints);
    }
    return s_bgPhotoGammaLUT;
}


static inline float yFromRGB_rec601_f(const float r, const float g, const float b) {
    return  16.0f + ( 65.481f * r + 128.553f * g +  24.966f * b);  }

static inline float cbFromRGB_rec601_f(const float r, const float g, const float b) {
    return  128.0f + (-37.797f * r + -74.203f * g +   112.0f * b);  }

static inline float crFromRGB_rec601_f(const float r, const float g, const float b) {
    return  128.0f + (  112.0f * r + -93.786f * g + -18.214f * b);  }

    
TwtwYUVImage *twtw_yuv_image_create_from_rgb_with_default_size (unsigned char *srcBuf, const size_t srcRowBytes, const gboolean srcHasAlpha)
{
    g_return_val_if_fail(srcBuf, NULL);
    
    const int w = TWTW_CAM_IMAGEWIDTH;
    const int h = TWTW_CAM_IMAGEHEIGHT;
    
    g_return_val_if_fail(srcRowBytes >= w * 3, NULL);
    
    TwtwYUVImage *img = g_malloc0(sizeof(TwtwYUVImage));
    img->w = w;
    img->h = h;
    img->rowBytes = w * 2;
    img->pixelFormat = TWTW_CAM_FOURCC;
    img->buffer = g_malloc(img->rowBytes * img->h);
    
    // this is called rarely, so I think we can afford to do this slow float-based conversion here
    
    int srcStride = (srcHasAlpha) ? 4 : 3;
    
    int i, j;
    const float toFloatMult = (1.0f / 255.0f);
    
    for (i = 0; i < h; i++) {
        unsigned char *src = (unsigned char *)(srcBuf + srcRowBytes * i);
        unsigned int *dst = (unsigned int *)(img->buffer + img->rowBytes * i);
        for (j = w/2; j; j--) {
            register unsigned int iy1, iy2, iCb, iCr;
            register float y1, y2, Cb, Cr;
            register float r1, r2, g1, g2, b1, b2;
            
            r1 = src[0] * toFloatMult;
            g1 = src[1] * toFloatMult;
            b1 = src[2] * toFloatMult;
            src += srcStride;

            y1 = yFromRGB_rec601_f(r1, g1, b1);
            Cb = cbFromRGB_rec601_f(r1, g1, b1);
            Cr = crFromRGB_rec601_f(r1, g1, b1);

            r2 = src[0] * toFloatMult;
            g2 = src[1] * toFloatMult;
            b2 = src[2] * toFloatMult;
            src += srcStride;
                        
            y2 = yFromRGB_rec601_f(r2, g2, b2);
            
            iy1 = ((unsigned int)y1) & 0xff;
            iy2 = ((unsigned int)y2) & 0xff;
            iCb = ((unsigned int)Cb) & 0xff;
            iCr = ((unsigned int)Cr) & 0xff;
            
#if !defined(WORDS_BIGENDIAN)
            dst[0] = (iCb << 0) | (iy1 << 8) | (iCr << 16) | (iy2 << 24);
#else
            dst[0] = (iCb << 24) | (iy1 << 16) | (iCr << 8) | (iy2 << 0);
#endif            
            dst++;
        }
    }
    return img;
}


void twtw_yuv_image_convert_to_rgb_for_display (TwtwYUVImage *yuvImage, unsigned char *dstBuf, const size_t dstRowBytes,
                                                const gboolean includeAlpha,
                                                const gint srcXStride,
                                                const gint srcYStride)
{
    g_return_if_fail(yuvImage);
    g_return_if_fail(yuvImage->buffer);
    g_return_if_fail(dstBuf);

    const unsigned int w = yuvImage->w;
    const unsigned int h = yuvImage->h;
    const size_t srcRowBytes = yuvImage->rowBytes;
    unsigned char *srcBuf = yuvImage->buffer;

    const unsigned int dstStride = (includeAlpha) ? 4 : 3;

    //printf("%s: dstrb %i, srcrb %i, stride %i\n", __func__, dstRowBytes, srcRowBytes, stride);
    
    // tweak down the color saturation.
    // this is applied to make the heavily chroma-compressed images
    // look better as the background for the vector graphics.
    const TwtwFixedNum fx_photoChromaMul = FIXD_FROM_FLOAT(0.6);

    // an s-shaped tone curve is applied to make N800-taken images look a bit better
    const unsigned char *outLUT = getGammaLUTForBGPhoto();
    g_return_if_fail(outLUT);
            
    // constants for YUV conversion
    const TwtwFixedNum fx_lumaMul = FIXD_FROM_FLOAT(1.164);
    const TwtwFixedNum fx_crMul_r = FIXD_FROM_FLOAT(1.596);
    const TwtwFixedNum fx_crMul_g = FIXD_FROM_FLOAT(0.813);
    const TwtwFixedNum fx_cbMul_g = FIXD_FROM_FLOAT(0.391);
    const TwtwFixedNum fx_cbMul_b = FIXD_FROM_FLOAT(2.018);
    
    const unsigned int xStride = (srcXStride > 0) ? srcXStride : 1;
    const unsigned int yStride = (srcYStride > 0) ? srcYStride : 1;

    const unsigned int srcNumPixels =  (xStride == 1) ? (w / 2) : w;  // if stride==1 (i.e. no pixels are skipped), we process two source pixels at a time
    const unsigned int srcRealStride = (xStride == 1) ? 1 : (xStride / 2);
    
    unsigned int y;
    for (y = 0; y < h; y += yStride) {
        unsigned int * RESTRICT src = (unsigned int *)(srcBuf + srcRowBytes*y);
        unsigned char * RESTRICT dst = (unsigned char *)(dstBuf + dstRowBytes*(y / yStride));
        unsigned int n;
        for (n = 0; n < srcNumPixels; n += srcRealStride) {
            int r0, g0, b0, r1, g1, b1;
            unsigned int v = src[n];
            // pixel format used by twtw is UYVY (i.e. byte order is { Cb, Y0, Cr, Y1 })
#if !defined(WORDS_BIGENDIAN)
            int cb = (v) & 0xff;
            int y0 = (v >> 8) & 0xff;
            int cr = (v >> 16) & 0xff;
            int y1 = (v >> 24) & 0xff;
#else
            int y1 = (v) & 0xff;
            int cr = (v >> 8) & 0xff;
            int y0 = (v >> 16) & 0xff;
            int cb = (v >> 24) & 0xff;
#endif
            /*
            R = 1.164(Y-16) + 1.596(Cr-128)
            G = 1.164(Y-16) - 0.813(Cr-128) - 0.391(Cb-128)
            B = 1.164(Y-16) + 2.018(Cb-128)
            */
            cr = FIXD_FROM_INT(cr - 128);
            cb = FIXD_FROM_INT(cb - 128);
            y0 = FIXD_FROM_INT(y0 - 16);
            y1 = FIXD_FROM_INT(y1 - 16);
            
            cr = FIXD_MUL(cr, fx_photoChromaMul);
            cb = FIXD_MUL(cb, fx_photoChromaMul);
   
            int scaled_cr_r = FIXD_MUL(fx_crMul_r, cr);
            int scaled_cr_g = FIXD_MUL(fx_crMul_g, cr);
            int scaled_cb_g = FIXD_MUL(fx_cbMul_g, cb);
            int scaled_cb_b = FIXD_MUL(fx_cbMul_b, cb);
            int scaled_y0 = FIXD_MUL(fx_lumaMul, y0);
            
            r0 = scaled_y0 + scaled_cr_r;
            g0 = scaled_y0 - scaled_cr_g - scaled_cb_g;
            b0 = scaled_y0 + scaled_cb_b;
            dst[0] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(r0) ) ];
            dst[1] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(g0) ) ];
            dst[2] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(b0) ) ];
            
            if (includeAlpha) {
                dst[3] = 255;
            }
            dst += dstStride;

            if (xStride == 1) {
                int scaled_y1 = FIXD_MUL(fx_lumaMul, y1);
            
                r1 = scaled_y1 + scaled_cr_r;
                g1 = scaled_y1 - scaled_cr_g - scaled_cb_g;
                b1 = scaled_y1 + scaled_cb_b;
                dst[0] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(r1) ) ];
                dst[1] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(g1) ) ];
                dst[2] = outLUT[ FIXD_TO_INT( FIXD_CLAMP_255(b1) ) ];

                if (includeAlpha) {
                    dst[3] = 255;
                }
                dst += dstStride;
            }
        }
    }
}


// --- disk format ---

void twtw_yuv_image_serialize (TwtwYUVImage *image, unsigned char **outData, size_t *outDataSize, uint32_t *outDataFourCC)
{
    g_return_if_fail(image);
    g_return_if_fail(outData);
    g_return_if_fail(outDataSize);
    
    const int yw = image->w;
    const int yh = image->h;
    
    const int cw = yw / 2;
    const int ch = yh / 2;

    // chroma data will be truncated
    const int chromaBits = 5;
    const int cTruncRowBytes = (chromaBits * cw) / 8;
    const int cTruncSize = cTruncRowBytes * ch;

    // contains the final plane data
    size_t planarBufDataSize = (yw * yh) + cTruncSize + cTruncSize;
    unsigned char *planarBuf = g_malloc(planarBufDataSize);

    // separate image into planes
    unsigned char *yPlane = planarBuf;
    unsigned char *cbPlane = g_malloc(cw * ch);
    unsigned char *crPlane = g_malloc(cw * ch);
    
    // luma plane
    unsigned int x, y;
    for (y = 0; y < image->h; y++) {
        unsigned char * RESTRICT src = image->buffer + image->rowBytes*y;
        unsigned char * RESTRICT dst = yPlane + yw*y;
        for (x = 0; x < image->w; x++) {
            // truncate 2 bits from luma for substantially better compression when data is deflated.
            // this seems heavy-handed, but most mobile device LCDs don't display more than 18-bit (6-bpc) color,
            // and in fact images taken on an N800 or mobile phone seem to be so gritty in general that the lowest bits
            // are practically just noise.
            // some kind of smoothing algorithm would be nice when displaying these images on a desktop computer, however...
            unsigned char luma = (src[1] & 0xfc);
            *dst++ = luma;
            src += 2;
        }
    }
    // chroma planes
    for (y = 0; y < ch; y++) {
        unsigned char * RESTRICT src1 = image->buffer + image->rowBytes*(y*2);
        unsigned char * RESTRICT src2 = image->buffer + image->rowBytes*(y*2 + 1);
        unsigned char * RESTRICT dst_cb = cbPlane + cw*y;
        unsigned char * RESTRICT dst_cr = crPlane + cw*y;
        for (x = 0; x < cw; x++) {
            unsigned int cb1 = src1[0];
            unsigned int cr1 = src1[2];
            src1 += 4;
            unsigned int cb2 = src2[0];
            unsigned int cr2 = src2[2];
            src2 += 4;
            
            // blend chroma samples from two rows
            unsigned int cb = (cb1 + cb2) >> 1;
            unsigned int cr = (cr1 + cr2) >> 1;
            
            *dst_cb++ = (cb & 0xff);
            *dst_cr++ = (cr & 0xff);
        }
    }

    // truncate chroma data
    {
    unsigned char * RESTRICT cbTruncBuf = planarBuf + (yw * yh);
    unsigned char * RESTRICT crTruncBuf = cbTruncBuf + cTruncSize;
    
    unsigned int ns = 0, nd = 0;
    while (nd < cTruncSize) {
        // this code did 4-bit truncation (2 values -> 1 byte)
        /*
        const unsigned int cOffset = 7;
        unsigned int cb1 = cOffset + cbPlane[ns];   // add an offset to prevent e.g. a value of 127 rounding wrong way
        unsigned int cb2 = cOffset + cbPlane[ns+1];        
        unsigned int cr1 = cOffset + crPlane[ns];
        unsigned int cr2 = cOffset + crPlane[ns+1];
        ns += 2;
        
        unsigned int cbPack = (cb1 & 0xf0) | (cb2 >> 4);
        unsigned int crPack = (cr1 & 0xf0) | (cr2 >> 4);
        
        cbTruncBuf[nd] = (cbPack & 0xff);
        crTruncBuf[nd] = (crPack & 0xff);
        nd++;
        
        //if (nd < cTruncRowBytes)  printf("%i: cb %x (%u, %u) / cb %x (%u, %u)\n", nd, cbTrunc, cb1, cb2, crTrunc, cr1, cr2);
        */
        // this code does 5-bit truncation (8 values -> 5 bytes)
        const unsigned int cOffset = 4;
        #define TRUNC(c_)  (((c_) >> 3) & 0x1f)
        
        unsigned int cb1 = cOffset + cbPlane[ns];
        unsigned int cb2 = cOffset + cbPlane[ns+1];
        unsigned int cb3 = cOffset + cbPlane[ns+2];
        unsigned int cb4 = cOffset + cbPlane[ns+3];
        unsigned int cb5 = cOffset + cbPlane[ns+4];
        unsigned int cb6 = cOffset + cbPlane[ns+5];
        unsigned int cb7 = cOffset + cbPlane[ns+6];
        unsigned int cb8 = cOffset + cbPlane[ns+7];
        
        unsigned int cr1 = cOffset + crPlane[ns];
        unsigned int cr2 = cOffset + crPlane[ns+1];
        unsigned int cr3 = cOffset + crPlane[ns+2];
        unsigned int cr4 = cOffset + crPlane[ns+3];
        unsigned int cr5 = cOffset + crPlane[ns+4];
        unsigned int cr6 = cOffset + crPlane[ns+5];
        unsigned int cr7 = cOffset + crPlane[ns+6];
        unsigned int cr8 = cOffset + crPlane[ns+7];
        ns += 8;
        
        cbTruncBuf[nd+0] = (TRUNC(cb1) << 3) | (TRUNC(cb2) >> 2);
        cbTruncBuf[nd+1] = (TRUNC(cb2) << 6) | (TRUNC(cb3) << 1) | (TRUNC(cb4) >> 4);
        cbTruncBuf[nd+2] = (TRUNC(cb4) << 4) | (TRUNC(cb5) >> 1);
        cbTruncBuf[nd+3] = (TRUNC(cb5) << 7) | (TRUNC(cb6) << 2) | (TRUNC(cb7) >> 3);
        cbTruncBuf[nd+4] = (TRUNC(cb7) << 5) | (TRUNC(cb8));

        crTruncBuf[nd+0] = (TRUNC(cr1) << 3) | (TRUNC(cr2) >> 2);
        crTruncBuf[nd+1] = (TRUNC(cr2) << 6) | (TRUNC(cr3) << 1) | (TRUNC(cr4) >> 4);
        crTruncBuf[nd+2] = (TRUNC(cr4) << 4) | (TRUNC(cr5) >> 1);
        crTruncBuf[nd+3] = (TRUNC(cr5) << 7) | (TRUNC(cr6) << 2) | (TRUNC(cr7) >> 3);
        crTruncBuf[nd+4] = (TRUNC(cr7) << 5) | (TRUNC(cr8));
        nd += 5;
        #undef TRUNC
    }
    }
    
    size_t defDataAvailSize = planarBufDataSize + 512;
    unsigned char *defData = g_malloc(defDataAvailSize);
    size_t deflatedSize = 0;
    twtw_deflate(planarBuf, planarBufDataSize,  defData, defDataAvailSize,  &deflatedSize);

    printf("deflated planar image: orig data size %i --> compressed %i (%.3f)\n", planarBufDataSize, deflatedSize, (double)deflatedSize / planarBufDataSize);


    *outData = defData;
    *outDataSize = deflatedSize;

    if (outDataFourCC)
        *outDataFourCC = TWTW_CAM_COMPRESSED_FOURCC;

    // clean up temp buffers
    g_free(cbPlane);
    g_free(crPlane);
    g_free(planarBuf);
}


// previous (spring 2009) format with only 4 bits for each chroma channel.
// replaced with 'twYb' that has 5-bit chroma.
#define TWTW_CAM_COMPRESSED_4BITCHROMA_FOURCC          MAKE_FOURCC_LE('t', 'w', 'Y', 'Z')

// full 8-bit chroma is also supported, although the client doesn't write it currently.
#define TWTW_CAM_COMPRESSED_8BITCHROMA_FOURCC          MAKE_FOURCC_LE('t', 'w', 'Y', 'c')



TwtwYUVImage *twtw_yuv_image_create_from_serialized (unsigned char *deflatedData, size_t deflatedDataSize, int w, int h, uint32_t dataFourCC, size_t origDataSize)
{
    if ( !deflatedData || deflatedDataSize < 1) return NULL;
    if (w < 1 || h < 1) return NULL;
    
    if (dataFourCC != TWTW_CAM_COMPRESSED_FOURCC && dataFourCC != TWTW_CAM_COMPRESSED_4BITCHROMA_FOURCC && dataFourCC != TWTW_CAM_COMPRESSED_8BITCHROMA_FOURCC) {
        char s[5] = "____";
        memcpy(s, (char *)(&dataFourCC), 4);
        printf("*** %s: unsupported fourCC: '%s'\n", __func__, s);
        return NULL;
    }
    
    // inflate
    size_t infDataAvailSize = (origDataSize > 0) ? MAX(4096, origDataSize) : (256*1024);
    unsigned char *infData = g_malloc(infDataAvailSize);        
    size_t inflatedSize = 0;
    twtw_inflate(deflatedData, deflatedDataSize,  infData, infDataAvailSize,  &inflatedSize);
    
    printf("did inflate photo: %i -> %i, size %i * %i px, fourCC is 0x%x\n", deflatedDataSize, inflatedSize, w, h, dataFourCC);

    const unsigned int yw = w;
    const unsigned int yh = h;
    const unsigned int cw = yw / 2;
    const unsigned int ch = yh / 2;

    // chroma data is truncated
    const unsigned int chromaBits = (dataFourCC == TWTW_CAM_COMPRESSED_FOURCC) ? 5 
                                               : ((dataFourCC == TWTW_CAM_COMPRESSED_4BITCHROMA_FOURCC) ? 4 : 8);
    const unsigned int cTruncRowBytes = (chromaBits * cw) / 8;
    const unsigned int cTruncSize = cTruncRowBytes * ch;
    
    // check that data is large enough
    g_return_val_if_fail (inflatedSize >= (yw * yh) + cTruncSize + cTruncSize, NULL);
    
    unsigned char *yPlane = infData;
    unsigned char *cbTruncBuf = yPlane + (yw * yh);
    unsigned char *crTruncBuf = cbTruncBuf + cTruncSize;

    // expand truncated chroma back to 8-bit
    unsigned char *cbPlane = g_malloc(cw * ch);
    unsigned char *crPlane = g_malloc(cw * ch);
    {
    unsigned int ns = 0, nd = 0;
        
    if (dataFourCC == TWTW_CAM_COMPRESSED_8BITCHROMA_FOURCC) {
        memcpy(cbPlane, cbTruncBuf, cTruncSize);
        memcpy(crPlane, crTruncBuf, cTruncSize);
    } else if (dataFourCC == TWTW_CAM_COMPRESSED_4BITCHROMA_FOURCC) {
        while (ns < cTruncSize) {
            unsigned int cbPacked = cbTruncBuf[ns];
            unsigned int crPacked = crTruncBuf[ns];
            ns++;
            
            cbPlane[nd+0] = (cbPacked & 0xf0);
            cbPlane[nd+1] = (cbPacked & 0x0f) << 4;
            crPlane[nd+0] = (crPacked & 0xf0);
            crPlane[nd+1] = (crPacked & 0x0f) << 4;
            nd += 2;
        }
    } else {
        while (ns < cTruncSize) {
            unsigned int cbPacked1 = cbTruncBuf[ns];
            unsigned int cbPacked2 = cbTruncBuf[ns+1];
            unsigned int cbPacked3 = cbTruncBuf[ns+2];
            unsigned int cbPacked4 = cbTruncBuf[ns+3];
            unsigned int cbPacked5 = cbTruncBuf[ns+4];

            unsigned int crPacked1 = crTruncBuf[ns];
            unsigned int crPacked2 = crTruncBuf[ns+1];
            unsigned int crPacked3 = crTruncBuf[ns+2];
            unsigned int crPacked4 = crTruncBuf[ns+3];
            unsigned int crPacked5 = crTruncBuf[ns+4];
            ns += 5;

            unsigned int cb1 = (cbPacked1 >> 3)  & 0x1f;
            unsigned int cb2 = ((cbPacked1 << 2) | ((cbPacked2 >> 6) & 3))  & 0x1f;
            unsigned int cb3 = (cbPacked2 >> 1)  & 0x1f;
            unsigned int cb4 = ((cbPacked2 << 4) | ((cbPacked3 >> 4) & 15))  & 0x1f;
            unsigned int cb5 = ((cbPacked3 << 1) | ((cbPacked4 >> 7) & 1))   & 0x1f;
            unsigned int cb6 = (cbPacked4 >> 2)  & 0x1f;
            unsigned int cb7 = ((cbPacked4 << 3) | ((cbPacked5 >> 5) & 7))   & 0x1f;
            unsigned int cb8 = (cbPacked5)  & 0x1f;
            cbPlane[nd+0] = cb1 << 3;
            cbPlane[nd+1] = cb2 << 3;
            cbPlane[nd+2] = cb3 << 3;
            cbPlane[nd+3] = cb4 << 3;
            cbPlane[nd+4] = cb5 << 3;
            cbPlane[nd+5] = cb6 << 3;
            cbPlane[nd+6] = cb7 << 3;
            cbPlane[nd+7] = cb8 << 3;

            unsigned int cr1 = (crPacked1 >> 3)  & 0x1f;
            unsigned int cr2 = ((crPacked1 << 2) | ((crPacked2 >> 6) & 3))  & 0x1f;
            unsigned int cr3 = (crPacked2 >> 1)  & 0x1f;
            unsigned int cr4 = ((crPacked2 << 4) | ((crPacked3 >> 4) & 15))  & 0x1f;
            unsigned int cr5 = ((crPacked3 << 1) | ((crPacked4 >> 7) & 1))   & 0x1f;
            unsigned int cr6 = (crPacked4 >> 2)  & 0x1f;
            unsigned int cr7 = ((crPacked4 << 3) | ((crPacked5 >> 5) & 7))   & 0x1f;
            unsigned int cr8 = (crPacked5)  & 0x1f;
            crPlane[nd+0] = cr1 << 3;
            crPlane[nd+1] = cr2 << 3;
            crPlane[nd+2] = cr3 << 3;
            crPlane[nd+3] = cr4 << 3;
            crPlane[nd+4] = cr5 << 3;
            crPlane[nd+5] = cr6 << 3;
            crPlane[nd+6] = cr7 << 3;
            crPlane[nd+7] = cr8 << 3;
            nd += 8;
        }
    }
    }
    

    size_t uyvyRowBytes = w * 2;
    unsigned char *uyvyBuf = g_malloc(uyvyRowBytes * h);
    
    unsigned int x, y;
    
    for (y = 0; y < h; y++) {
        unsigned char * RESTRICT dst = uyvyBuf + uyvyRowBytes*y;
        unsigned char * RESTRICT src_y = yPlane + yw*y;
        ///unsigned char * RESTRICT src_cb = cbTruncBuf + cTruncRowBytes*(y >> 1);  <<-- algorithm for 4-bit trunc (didn't look good)
        ///unsigned char * RESTRICT src_cr = crTruncBuf + cTruncRowBytes*(y >> 1);
        unsigned char * RESTRICT src_cb = cbPlane + cw*(y >> 1);
        unsigned char * RESTRICT src_cr = crPlane + cw*(y >> 1);
        
        if ((y & 1) == 1 && y < (h-1)) {
            // interpolate chroma samples for even rows
            unsigned char * RESTRICT src_cb_next = src_cb + cw; //cTruncRowBytes;
            unsigned char * RESTRICT src_cr_next = src_cr + cw; //cTruncRowBytes;

            for (x = 0; x < cw; x++) {
                unsigned int cb = *src_cb;
                unsigned int cr = *src_cr;
                unsigned int cb_next = *src_cb_next;
                unsigned int cr_next = *src_cr_next;
                src_cb++;
                src_cr++;
                src_cb_next++;
                src_cr_next++;
                
                cb = (cb + cb_next) >> 1;
                cr = (cr + cr_next) >> 1;
                
                dst[0] = cb & 0xff;
                dst[1] = src_y[0];
                dst[2] = cr & 0xff;
                dst[3] = src_y[1];
                dst += 4;
                src_y += 2;
            }

            /*
            for (x = 0; x < yw/4; x++) {
                unsigned int cb1 = (*src_cb & 0xf0);
                unsigned int cb2 = (*src_cb & 0x0f) << 4;
                unsigned int cr1 = (*src_cr & 0xf0);
                unsigned int cr2 = (*src_cr & 0x0f) << 4;
                unsigned int cb1_next = (*src_cb_next & 0xf0);
                unsigned int cb2_next = (*src_cb_next & 0x0f) << 4;
                unsigned int cr1_next = (*src_cr_next & 0xf0);
                unsigned int cr2_next = (*src_cr_next & 0x0f) << 4;
                src_cb += 1;
                src_cr += 1;
                src_cb_next += 1;
                src_cr_next += 1;
                
                cb1 = (cb1 + cb1_next) >> 1;
                cb2 = (cb2 + cb2_next) >> 1;
                cr1 = (cr1 + cr1_next) >> 1;
                cr2 = (cr2 + cr2_next) >> 1;
                
                dst[0] = (cb1 & 0xff);
                dst[2] = (cr1 & 0xff);
                dst[4] = (cb2 & 0xff);
                dst[6] = (cr2 & 0xff);
                
                dst[1] = src_y[0];
                dst[3] = src_y[1];
                dst[5] = src_y[2];
                dst[7] = src_y[3];
            
                dst += 8;
                src_y += 4;
            }
            */
        }
        else {  // no interpolation needed
            for (x = 0; x < cw; x++) {
                unsigned int cb = *src_cb;
                unsigned int cr = *src_cr;
                src_cb++;
                src_cr++;
                
                dst[0] = cb;
                dst[1] = src_y[0];
                dst[2] = cr;
                dst[3] = src_y[1];
                dst += 4;
                src_y += 2;
            }
            /*
            for (x = 0; x < yw/4; x++) {
                unsigned int cb1 = (*src_cb & 0xf0);
                unsigned int cb2 = (*src_cb & 0x0f) << 4;
                unsigned int cr1 = (*src_cr & 0xf0);
                unsigned int cr2 = (*src_cr & 0x0f) << 4;
                src_cb += 1;
                src_cr += 1;
            
                dst[0] = (cb1 & 0xff);
                dst[2] = (cr1 & 0xff);
                dst[4] = (cb2 & 0xff);
                dst[6] = (cr2 & 0xff);
                
                dst[1] = src_y[0];
                dst[3] = src_y[1];
                dst[5] = src_y[2];
                dst[7] = src_y[3];
            
                dst += 8;
                src_y += 4;
            }*/
        }
    }
    
    TwtwYUVImage *image = g_malloc0(sizeof(TwtwYUVImage));
    image->w = w;
    image->h = h;
    image->pixelFormat = TWTW_CAM_FOURCC;
    image->rowBytes = uyvyRowBytes;
    image->buffer = uyvyBuf;
    
    g_free(infData);
    g_free(cbPlane);
    g_free(crPlane);
    return image;
}


