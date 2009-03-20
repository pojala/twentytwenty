/*
 *  twtw-ogg.h
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

#ifndef _TWTW_OGG_H_
#define _TWTW_OGG_H_ 

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
#define WORDS_BIGENDIAN
#endif

#include <ogg/ogg.h>


#define TWTW_MAX_PAGES_PER_FILE 20


/*
The 20:20 file format is Ogg, with a layout roughly like this (N == number of twtw slides):

- ogg skeleton header ("fishead"), 1
- other logical bitstream header packets (must go here to conform with Ogg standard), 1 + N*2
- ogg skeleton bitstream info packets ("fisbone"), 1 + N*2
- twtw document stream "bone" packet, 1
//- twtw image stream headers, N
- twtw speex stream headers, N
- ogg skeleton e.o.s. packet
... image & speex data packets...

The recommended file extension is .oggtw (.ogx == "Ogg for applications" is also appropriate).
*/


typedef struct {
    ogg_uint16_t version_major;             // twtw version number (currently 1)
    ogg_uint16_t version_minor;             // twtw version minor (currently 0)
    ogg_uint32_t num_pages_in_document;     // actual number of slides in this document (up to 20)
    ogg_uint32_t granules_per_page;         // length of a slide in granulepos count (allows for slide packets to be located by granulepos)
} TwtwDocumentHeadPacket;

typedef struct {
    ogg_uint32_t document_flags;
    ogg_uint16_t document_flags_2;  // unused (extension space)
    ogg_uint16_t metadata_field_count;
    
    ogg_uint32_t pic_stream_serials[TWTW_MAX_PAGES_PER_FILE];  // if more document pages are ever allowed, these arrays will be sized by num_pages_in_document
    ogg_uint32_t speex_stream_serials[TWTW_MAX_PAGES_PER_FILE];
    
    char *creator_id[32];       // zero-terminated ASCII identifying the creator application (not meant to be displayed to user)
    char *document_id[16];      // reserved for document identification (perhaps storing a 16-byte UUID, or something like that)

    ogg_uint16_t doc_canvas_x;  // the canvas rectangle in full-pixel units.
    ogg_uint16_t doc_canvas_y;  // this encoded for extension purposes even though the canvas size for 20:20 documents is currently fixed
    ogg_uint16_t doc_canvas_w;  // (canvas width is 640, and implementations with a different display size [e.g. Maemo] do the conversion on file I/O)
    ogg_uint16_t doc_canvas_h;
    
    ogg_uint32_t _reserved_1;
    ogg_uint32_t _reserved_2;
    
    ogg_uint16_t metadata_size_in_bytes;  // custom metadata is stored in key/value UTF-8 string pairs (each string must be zero-terminated);
    char *metadata_fields;                // the number of strings in "metadata_fields" is thus 2*metadata_field_count
} TwtwDocumentBonePacket;


// conversions: twtw internal struct <-> ogg packet.
// naming of these functions follows skeleton.h's example.
int ogg_from_twtwdoc_head(TwtwDocumentHeadPacket *hp, ogg_packet *op);
int ogg_from_twtwdoc_bone(TwtwDocumentBonePacket *bp, ogg_packet *op);
int twtwdoc_head_from_ogg(ogg_packet *op, TwtwDocumentHeadPacket *hp);
int twtwdoc_bone_from_ogg(ogg_packet *op, TwtwDocumentBonePacket *bp);


typedef struct {
    ogg_uint32_t pic_flags;
    ogg_uint32_t sound_duration_in_secs;
    ogg_uint32_t num_curves;
    ogg_uint32_t num_points;
    ogg_uint16_t num_photos;
    ogg_uint16_t _reserved_16;
    ogg_uint32_t _reserved_32;
} TwtwPictureHeadPacket;

int ogg_from_twtwpic_head(TwtwPictureHeadPacket *hp, ogg_packet *op);
int twtwpic_head_from_ogg(ogg_packet *op, TwtwPictureHeadPacket *hp);


#ifdef __cplusplus
}
#endif

#endif  /* _TWTW_OGG_H */
