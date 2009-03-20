/*
 *  twtw-ogg.c
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 28.11.2008.
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

#include "twtw-ogg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static inline unsigned short
_le_16 (unsigned short s)
{
  unsigned short ret=s;
#ifdef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

static inline ogg_uint32_t
_le_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static inline ogg_int64_t
_le_64 (ogg_int64_t l)
{
  ogg_int64_t ret=l;
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
  return (*(ogg_int64_t *)ucptr);
}


// --- document stream headers ---

#define TWTWDOCHEAD_SIZE        (8 + 4*2 + 4*4)

#define TWTWDOCBONE_BASE_SIZE   (8 + 8 + (2*4)*(TWTW_MAX_PAGES_PER_FILE) + 32 + 16 + 8 + 8 + 2)

static const char TWTWDOCHEAD_IDENTIFIER[9] = "twdoc--\0\0";
static const char TWTWDOCBONE_IDENTIFIER[9] = "twdocBo\0\0";



int ogg_from_twtwdoc_head(TwtwDocumentHeadPacket *fp, ogg_packet *op)
{
    if (!fp || !op) return -1;

    memset(op, 0, sizeof(*op));
    op->packet = _ogg_calloc(TWTWDOCHEAD_SIZE, 1);
    if (!op->packet) return -1;

    memcpy(op->packet, TWTWDOCHEAD_IDENTIFIER, 8);
    *((ogg_uint16_t*)(op->packet+8)) = _le_16 (fp->version_major);
    *((ogg_uint16_t*)(op->packet+10)) = _le_16 (fp->version_minor);
    *((ogg_uint32_t*)(op->packet+12)) = _le_32 (fp->num_pages_in_document); 
    *((ogg_uint32_t*)(op->packet+16)) = _le_32 (fp->granules_per_page);

    op->b_o_s = 1;
    op->e_o_s = 0;
    op->bytes = TWTWDOCHEAD_SIZE;  // length of the packet in bytes
    return 0;
}

int ogg_from_twtwdoc_bone(TwtwDocumentBonePacket *fp, ogg_packet *op)
{
    if (!fp || !op) return -1;

    size_t packetSize = TWTWDOCBONE_BASE_SIZE + fp->metadata_size_in_bytes;

    memset(op, 0, sizeof(*op));
    op->packet = _ogg_calloc(packetSize, 1);
    if (!op->packet) return -1;

    memcpy(op->packet, TWTWDOCBONE_IDENTIFIER, 8);
    *((ogg_uint32_t*)(op->packet+8)) = _le_32 (fp->document_flags);
    *((ogg_uint16_t*)(op->packet+12)) = _le_16 (fp->document_flags_2);
    *((ogg_uint16_t*)(op->packet+14)) = _le_16 (fp->metadata_field_count);

    int n = 16;
    int i;
    for (i = 0; i < TWTW_MAX_PAGES_PER_FILE; i++) {
        *((ogg_uint32_t*)(op->packet+n)) =   _le_32 (fp->pic_stream_serials[i]);
        *((ogg_uint32_t*)(op->packet+n+4)) = _le_32 (fp->speex_stream_serials[i]);
        n += 8;
    }

    memcpy(op->packet+n, fp->creator_id, 32);
    n += 32;
    memcpy(op->packet+n, fp->document_id, 16);
    n += 16;
    
    *((ogg_uint16_t*)(op->packet+n+0)) = _le_16 (fp->doc_canvas_x);
    *((ogg_uint16_t*)(op->packet+n+2)) = _le_16 (fp->doc_canvas_y);
    *((ogg_uint16_t*)(op->packet+n+4)) = _le_16 (fp->doc_canvas_w);
    *((ogg_uint16_t*)(op->packet+n+6)) = _le_16 (fp->doc_canvas_h);
    n += 8;
    
    *((ogg_uint16_t*)(op->packet+n+0)) = _le_32 (fp->_reserved_1);
    *((ogg_uint16_t*)(op->packet+n+4)) = _le_32 (fp->_reserved_2);
    n += 8;

    *((ogg_uint16_t*)(op->packet+n)) = _le_16 (fp->metadata_size_in_bytes);
    n += 2;
    if (fp->metadata_size_in_bytes > 0) {
        memcpy(op->packet+n, fp->metadata_fields, fp->metadata_size_in_bytes);
    }

    op->b_o_s = 0;
    op->e_o_s = 0;
    op->bytes = packetSize;
    return 0;

}

int twtwdoc_head_from_ogg(ogg_packet *op, TwtwDocumentHeadPacket *fp)
{
	///printf("%s: op %p, fp %p, ident %s\n", __func__, op, fp, TWTWDOCHEAD_IDENTIFIER);

    if (!fp) return -1;

    if (memcmp(op->packet, TWTWDOCHEAD_IDENTIFIER, 8))
	  return -1;

    ///printf("... 1\n");
    ///printf("    : %i\n", (int)*((ogg_uint16_t*)(op->packet+8)));

    unsigned short maj = *((ogg_uint16_t*)(op->packet+8));
    unsigned short min = *((ogg_uint16_t*)(op->packet+10));
    
    fp->version_major = _le_16(maj);
    fp->version_minor = _le_16(min);
    //fp->version_major = _le_16 (*((ogg_uint16_t*)(op->packet+8)));
    //fp->version_minor = _le_16 (*((ogg_uint16_t*)(op->packet+10)));
    
    ///printf("...2 (%i, %i) \n", (int)maj, (int)min);
    
    fp->num_pages_in_document = _le_32 (*((ogg_uint32_t*)(op->packet+12)));
    fp->granules_per_page = _le_32 (*((ogg_uint32_t*)(op->packet+16)));
    
    ///printf("...3 \n");
    
    return 0;
}

int twtwdoc_bone_from_ogg(ogg_packet *op, TwtwDocumentBonePacket *fp)
{
    if (!fp) return -1;

    memset(fp, 0, sizeof(*fp));

    if (op->bytes < TWTWDOCBONE_BASE_SIZE-2 || memcmp(op->packet, TWTWDOCBONE_IDENTIFIER, 8))
	  return -1;

    fp->document_flags   =      _le_32 (*((ogg_uint32_t*)(op->packet+8)));
    fp->document_flags_2 =      _le_16 (*((ogg_uint16_t*)(op->packet+12)));
    fp->metadata_field_count =  _le_16 (*((ogg_uint16_t*)(op->packet+14)));

    int n = 16;
    int i;
    for (i = 0; i < TWTW_MAX_PAGES_PER_FILE; i++) {
        fp->pic_stream_serials[i]   = _le_32 (*((ogg_uint32_t*)(op->packet+n)));
        fp->speex_stream_serials[i] =  _le_32 (*((ogg_uint32_t*)(op->packet+n+4)));
        n += 8;
    }

    memcpy(fp->creator_id, op->packet+n, 32);
    n += 32;
    memcpy(fp->document_id, op->packet+n, 16);
    n += 16;
    
    fp->doc_canvas_x = _le_16 (*((ogg_uint16_t*)(op->packet+n+0)));
    fp->doc_canvas_y = _le_16 (*((ogg_uint16_t*)(op->packet+n+2)));
    fp->doc_canvas_w = _le_16 (*((ogg_uint16_t*)(op->packet+n+4)));
    fp->doc_canvas_h = _le_16 (*((ogg_uint16_t*)(op->packet+n+6)));
    n += 8;
    
    fp->_reserved_1 = _le_32 (*((ogg_uint32_t*)(op->packet+n+0)));
    fp->_reserved_2 = _le_32 (*((ogg_uint32_t*)(op->packet+n+4)));
    n += 8;

    fp->metadata_size_in_bytes = (fp->metadata_field_count > 0) ? _le_16 (*((ogg_uint16_t*)(op->packet+n))) : 0;
    n += 2;
    if (fp->metadata_size_in_bytes > 0) {
        fp->metadata_fields = _ogg_malloc(fp->metadata_size_in_bytes);
        memcpy(fp->metadata_fields, op->packet+n, fp->metadata_size_in_bytes);
    }
    
    ///printf("metadata size: %i; field count %i\n", fp->metadata_size_in_bytes, fp->metadata_field_count);

    return 0;
}


// --- picture stream header ---

#define TWTWPICHEAD_SIZE  (8 + 4*4 + 2*2 + 4)  // magic ID + 4 uint32s + 2 uint16s + 1 uint32

#define TWTWPICHEAD_IDENTIFIER "twtwpic\0"


int ogg_from_twtwpic_head(TwtwPictureHeadPacket *hp, ogg_packet *op)
{
    if (!hp || !op) return -1;

    memset(op, 0, sizeof(*op));
    op->packet = _ogg_calloc(TWTWPICHEAD_SIZE, 1);
    if (!op->packet) return -1;

    memcpy(op->packet, TWTWPICHEAD_IDENTIFIER, 8);
    *((ogg_uint32_t*)(op->packet+8)) = _le_32 (hp->pic_flags);
    *((ogg_uint32_t*)(op->packet+12)) = _le_32 (hp->sound_duration_in_secs);
    *((ogg_uint32_t*)(op->packet+16)) = _le_32 (hp->num_curves);
    *((ogg_uint32_t*)(op->packet+20)) = _le_32 (hp->num_points);
    *((ogg_uint16_t*)(op->packet+24)) = _le_16 (hp->num_photos);
    *((ogg_uint16_t*)(op->packet+26)) = _le_16 (hp->_reserved_16);
    *((ogg_uint32_t*)(op->packet+28)) = _le_32 (hp->_reserved_32);

    op->b_o_s = 1;
    op->e_o_s = 0;
    op->bytes = TWTWPICHEAD_SIZE;  /* length of the packet in bytes */
    return 0;
}

int twtwpic_head_from_ogg(ogg_packet *op, TwtwPictureHeadPacket *hp)
{
    if (memcmp(op->packet, TWTWPICHEAD_IDENTIFIER, 8))
        return -1;
      
    hp->pic_flags = _le_32 (*((ogg_uint32_t*)(op->packet+8)));
    hp->sound_duration_in_secs = _le_32 (*((ogg_uint32_t*)(op->packet+12)));
    hp->num_curves = _le_32 (*((ogg_uint32_t*)(op->packet+16)));
    hp->num_points = _le_32 (*((ogg_uint32_t*)(op->packet+20)));
    hp->num_photos = _le_16 (*((ogg_uint16_t*)(op->packet+24)));
    hp->_reserved_16 = _le_16 (*((ogg_uint16_t*)(op->packet+26)));
    hp->_reserved_32 = _le_32 (*((ogg_uint32_t*)(op->packet+28)));
    
    return 0;
}

