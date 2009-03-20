/*
 *  twtw-audioconv.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 30.11.2008.
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

#ifndef _TWTW_AUDIOCONV_H_
#define _TWTW_AUDIOCONV_H_

#include <oggz/oggz.h>
#include <speex/speex_header.h>


// currently these functions read and write only .WAV files


typedef struct _TwtwSpeexState *TwtwSpeexStatePtr;

#ifdef __cplusplus
extern "C" {
#endif

// writing speex data to ogg
int twtw_speex_init_encoding_from_pcm_path_utf8 (const char *srcPath, size_t srcPathLen, TwtwSpeexStatePtr *outState);
int twtw_speex_init_with_speex_buffer (unsigned char *speexBuf, size_t speexBufSize, TwtwSpeexStatePtr *outState);
int twtw_speex_write_header_to_oggz (TwtwSpeexStatePtr state, OGGZ *oggz, long serialno);
int twtw_speex_write_all_data_to_oggz_and_finish (TwtwSpeexStatePtr state, OGGZ *oggz, long serialno);  // destroys the state object

// ogg packet util
int twtw_identify_speex_header (ogg_packet *op, SpeexHeader *outSpeexHeader);  // returns 0 if packet is a speex header, and copies to outSpeexHeader

// reading speex data from ogg
int twtw_speex_init_decoding_to_pcm_path_utf8 (const char *path, size_t pathLen, TwtwSpeexStatePtr *outState);
int twtw_speex_read_apply_header (TwtwSpeexStatePtr state, SpeexHeader *speexHeader);
int twtw_speex_read_data_from_ogg_packet (TwtwSpeexStatePtr state, ogg_packet *op);  // closes the file if packet is EOS
int twtw_speex_read_finish (TwtwSpeexStatePtr state, int *outNumWrittenPCMBytes, unsigned char **outSpeexData, size_t *outSpeexDataSize);

#ifdef __cplusplus
}
#endif

#endif