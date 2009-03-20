/*
 *  twtw-audio-apple.h
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

#ifndef _TWTW_AUDIO_H_
#define _TWTW_AUDIO_H_


#include <stdlib.h>
#include <string.h>


/* The PCM format used in 20:20 is fixed as the following:
    - 8 kHz
    - 16 bits/sample
    - little-endian.
    
   Conversion to/from Speex is done only when exporting or importing documents.
*/

#define TWTW_PCM_SAMPLERATE 8000
#define TWTW_PCM_SAMPLEBITS 16
#define TWTW_PCM_LITTLE_ENDIAN 1


enum {
    TWTW_AUDIOSTATUS_IDLE = 0,
    TWTW_AUDIOSTATUS_REC = 1,
    TWTW_AUDIOSTATUS_PLAY
}; 


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TwtwAudioCallbacks {
    void (*audioCompletedFunc) (int, void *);
    void (*audioInProgressFunc) (int, double, void *);
} TwtwAudioCallbacks;


void twtw_audio_init ();

void twtw_audio_deinit ();


int twtw_audio_pcm_record_to_path_utf8 (const char *path, size_t pathLen, int seconds, TwtwAudioCallbacks callbacks, void *cbData);

int twtw_audio_pcm_play_from_path_utf8 (const char *path, size_t pathLen, TwtwAudioCallbacks callbacks, void *cbData);

void twtw_audio_pcm_stop ();


#ifdef __cplusplus
}
#endif

#endif
