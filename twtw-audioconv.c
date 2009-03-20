/*
 *  twtw-audioconv.c
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

#include "twtw-audioconv.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_preprocess.h>
#include <oggz/oggz.h>

#include "twtw-audio-wavfile.h"
#include "twtw-byteorder.h"
#include "twtw-filesystem.h"
#include "twtw-document.h"  // error codes are here, should fix


#define MAX_FRAME_SIZE 3200
#define MAX_FRAME_BYTES 3200



typedef struct _TwtwPCMInfo {
    int sampleRate;
    int numChannels;
    int dataFormat;
} TwtwPCMInfo;


typedef struct _TwtwSpeexState
{
    gint mode;
    
    FILE *file;
    TwtwPCMInfo pcmInfo;
    int32_t fileDataLeft;
    
    SpeexHeader header;

    void *speexEncState;
    SpeexBits speexBits;
    SpeexPreprocessState *preprocState;
    
    int numFrames;
    int frameSize;
    int lookahead;
    
    void *speexDecState;
    int packetsRead;
    int32_t pcmBytesWritten;

    // when decoding, keep a copy of all decoded speex data so twtwpage can cache it.
    // when encoding, this indicates that all the data is already available as speex encoded
    unsigned char *speexData;
    size_t speexDataSize;
    size_t speexDataWritten;
} TwtwSpeexState;



// this function is borrowed from speexenc.c
static int readPCMSamples(FILE *fin, TwtwPCMInfo *pcmInfo, const int frame_size, short *input, char *buff, int32_t *size)
{   
   unsigned char in[MAX_FRAME_BYTES*2];
   int i;
   short *s;
   int nb_read;
   
   const int lsb = 1;  // data is always LSB right now
   const int channels = pcmInfo->numChannels;
   const int rate = pcmInfo->sampleRate;
   const int bits = pcmInfo->dataFormat;
   g_return_val_if_fail(rate > 0, -1);
   g_return_val_if_fail(bits > 0, -1);
   g_return_val_if_fail(channels > 0, -1);

   if (size && *size<=0) {
      return 0;
   }
   
   /*Read input audio*/
   if (size)
      *size -= bits/8*channels*frame_size;
   if (buff)
   {
      for (i=0;i<12;i++)
         in[i]=buff[i];
      nb_read = fread(in+12,1,bits/8*channels*frame_size-12, fin) + 12;
      if (size)
         *size += 12;
   } else {
      nb_read = fread(in,1,bits/8*channels* frame_size, fin);
   }
   nb_read /= bits/8*channels;

   /*fprintf (stderr, "%d\n", nb_read);*/
   if (nb_read==0)
      return 0;

   s=(short*)in;
   if(bits==8)
   {
      /* Convert 8->16 bits */
      for(i=frame_size*channels-1;i>=0;i--)
      {
         s[i]=(in[i]<<8)^0x8000;
      }
   } else
   {
      /* convert to our endian format */
      for(i=0;i<frame_size*channels;i++)
      {
         if(lsb) 
            s[i]=_le_16(s[i]); 
         else
            s[i]=_be_16(s[i]);
      }
   }

   /* FIXME: This is probably redundent now */
   /* copy to float input buffer */
   for (i=0;i<frame_size*channels;i++)
   {
      input[i]=(short)s[i];
   }

   for (i=nb_read*channels;i<frame_size*channels;i++)
   {
      input[i]=0;
   }


   return nb_read;
}


#define TWTW_SPEEX_NUMFRAMES    10
#define TWTW_SPEEX_MODEID       SPEEX_MODEID_NB
#define TWTW_SPEEX_COMPLEXITY   3
#define TWTW_SPEEX_QUALITY      1
#define TWTW_SPEEX_RATE         8000


int twtw_speex_init_encoding_from_pcm_path_utf8 (const char *srcPath, size_t srcPathLen,
                                               TwtwSpeexStatePtr *outState)
{
    g_return_val_if_fail(srcPath, TWTW_PARAMERR);
    g_return_val_if_fail(outState, TWTW_PARAMERR);

    spx_int32_t vbr_enabled=0;
    //spx_int32_t vbr_max=0;
    //int abr_enabled=0;
    //spx_int32_t vad_enabled=0;
    //spx_int32_t dtx_enabled=0;
    
    // frames per packet
    int nframes = TWTW_SPEEX_NUMFRAMES;
    
    SpeexPreprocessState *preprocess = NULL;
    int denoise_enabled = TRUE;
    int agc_enabled = TRUE;  // automatic gain control
    
    
    const char *speex_version = NULL;
    speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);

    printf("going to encode using speex, version is: '%s'\n", speex_version);

    FILE *file = twtw_open_readb_utf8(srcPath, srcPathLen);
    if ( !file)
        return TWTW_FILEERR;

    // check for WAV header
    int wavSampleRate = 0;
    int wavNumChannels = 0;
    int wavFormat = 0;
    int32_t wavDataSize = 0;
    {
        char first_bytes[12];
        fread(first_bytes, 1, 12, file);
        if (strncmp(first_bytes, "RIFF", 4) == 0) {
            if (twtw_read_wav_header(file, &wavSampleRate, &wavNumChannels, &wavFormat, &wavDataSize) == -1) {
                printf("*** error opening wav file (unknown header), path: %s\n", srcPath);
                fclose(file);
                return TWTW_FILEERR;
            } else {
                printf("WAV header read ok: rate %i, numch %i, format %i; data left %i\n", wavSampleRate, wavNumChannels, wavFormat, wavDataSize);
            }
        } else {
            // assume file is RAW PCM -- TODO
            printf("*** raw format unsupported (path: %s)\n", srcPath);
            return TWTW_FILEERR;
        }
    }

            
    SpeexHeader header;
    memset(&header, 0, sizeof(header));

    // speex defaults: narrowband mode, 8 kHz
    int modeID = TWTW_SPEEX_MODEID;
    int32_t complexity = TWTW_SPEEX_COMPLEXITY;
    int32_t quality = TWTW_SPEEX_QUALITY;
    int32_t rate = TWTW_SPEEX_RATE;
    
    const SpeexMode *mode = speex_lib_get_mode (modeID);
    
    speex_init_header(&header, rate, 1, mode);
    
    header.frames_per_packet = nframes;
    header.vbr = vbr_enabled;
    header.nb_channels = 1;


    void *encState = speex_encoder_init(mode);
    
    speex_encoder_ctl(encState, SPEEX_SET_COMPLEXITY, &complexity);
    speex_encoder_ctl(encState, SPEEX_SET_QUALITY, &quality);
    speex_encoder_ctl(encState, SPEEX_SET_SAMPLING_RATE, &rate);


    int32_t frame_size = 0;
    int32_t lookahead = 0;
    speex_encoder_ctl(encState, SPEEX_GET_FRAME_SIZE, &frame_size);
    speex_encoder_ctl(encState, SPEEX_GET_LOOKAHEAD, &lookahead);

    /*
    if (quality >= 0)
    {
        if (vbr_enabled)
        {
            if (vbr_max>0)
                speex_encoder_ctl(encState, SPEEX_SET_VBR_MAX_BITRATE, &vbr_max);
            speex_encoder_ctl(encState, SPEEX_SET_VBR_QUALITY, &vbr_quality);
        }
        else
            speex_encoder_ctl(encState, SPEEX_SET_QUALITY, &quality);
    }
    if (bitrate)
    {
        speex_encoder_ctl(encState, SPEEX_SET_BITRATE, &bitrate);
    }
    if (vbr_enabled)
    {
        tmp=1;
        speex_encoder_ctl(encState, SPEEX_SET_VBR, &tmp);
    }
    else if (vad_enabled)
    {
        tmp=1;
        speex_encoder_ctl(encState, SPEEX_SET_VAD, &tmp);
    }
    if (dtx_enabled)
        speex_encoder_ctl(encState, SPEEX_SET_DTX, &tmp);
    
    if (abr_enabled)
        speex_encoder_ctl(encState, SPEEX_SET_ABR, &abr_enabled);
    */

   if (denoise_enabled || agc_enabled) {
      preprocess = speex_preprocess_state_init(frame_size, rate);
      speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise_enabled);
      speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC, &agc_enabled);
      lookahead += frame_size;
      ///printf("inited preproc (%p), lookahead now %i\n", preprocess, lookahead);
   }

    SpeexBits bits;
    speex_bits_init(&bits);


    TwtwSpeexState *state = g_malloc0(sizeof(TwtwSpeexState));
    state->file = file;
    state->pcmInfo.sampleRate = wavSampleRate;
    state->pcmInfo.numChannels = wavNumChannels;
    state->pcmInfo.dataFormat = wavFormat;
    state->fileDataLeft = wavDataSize;
    
    state->speexEncState = encState;
    state->header = header;
    state->speexBits = bits;
    state->numFrames = nframes;
    state->frameSize = frame_size;
    state->lookahead = lookahead;
    state->preprocState = preprocess;

    *outState = state;
    return 0;
}

int twtw_speex_init_with_speex_buffer (unsigned char *speexBuf, size_t speexBufSize, TwtwSpeexStatePtr *outState)
{
    g_return_val_if_fail(speexBuf, TWTW_PARAMERR);
    g_return_val_if_fail(outState, TWTW_PARAMERR);

    TwtwSpeexState *state = g_malloc0(sizeof(TwtwSpeexState));
    state->speexData = speexBuf;
    state->speexDataSize = speexBufSize;
    state->speexDataWritten = 0;
    
    // we need to create a speex header for ogg writing
    
    // speex defaults: narrowband mode, 8 kHz
    const SpeexMode *mode = speex_lib_get_mode (TWTW_SPEEX_MODEID);
    
    speex_init_header(&(state->header), TWTW_SPEEX_RATE, 1, mode);
    
    state->header.frames_per_packet = TWTW_SPEEX_NUMFRAMES;
    state->header.vbr = 0;
    state->header.nb_channels = 1;

    state->numFrames = TWTW_SPEEX_NUMFRAMES;
    state->frameSize = 160;  // AFAICT this shouldn't change
    state->lookahead = 40;   // don't know if this is fixed - ugh

    *outState = state;    
    return 0;
}

int twtw_speex_write_header_to_oggz(TwtwSpeexStatePtr state, OGGZ *oggz, long serialno)
{
    g_return_val_if_fail (state, -1);
    g_return_val_if_fail (oggz, -1);
    g_return_val_if_fail (serialno != -1, -1);

    int packet_size;
    ogg_packet op;
    memset(&op, 0, sizeof(op));
    
    op.packet = (unsigned char *) speex_header_to_packet(&(state->header), &packet_size);
    op.bytes = packet_size;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;

    oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
    while ((oggz_write (oggz, 32)) > 0);
    
    free(op.packet);  // speex uses malloc internally
    return 0;
}


static void writeExistingSpeexDataToOggz(TwtwSpeexStatePtr state, OGGZ *oggz, long serialno)
{
    int nframes = state->numFrames;
    int frameSize = state->frameSize;
    if (nframes == 0) nframes = TWTW_SPEEX_NUMFRAMES;
    if (frameSize == 0) frameSize = 160;
    
    unsigned char *data = state->speexData;
    int dataSize = state->speexDataSize;
    int dataWritten = 0;
    
    int packetSize = 99;  // FIXME - hardcoded for nframes == 10
    int frameN = 0;
    
    while (dataWritten < dataSize) {
        ogg_packet op;
        memset(&op, 0, sizeof(op));

        int dataLeft = dataSize - dataWritten;
        int isEOS = (dataLeft <= packetSize);
        int framesInPacket = (isEOS) ? ceil((double)dataLeft / frameSize) : nframes;
        
        frameN += framesInPacket;

        op.packet = data + dataWritten;
        op.bytes = (isEOS) ? (dataSize - dataWritten) : packetSize;        
        op.e_o_s = (isEOS) ? 1 : 0;
        op.granulepos = (frameN+1)*frameSize;
        op.packetno = -1;
        
        //printf ("  writing packet from existing data, granulepos: %d, id %d (%d), packetbytes %i\n", (int)op.granulepos, frameN, nframes, op.bytes);
        
        oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
        while ((oggz_write (oggz, 400)) > 0);
        
        dataWritten += op.bytes;            
    }
    printf("wrote %i bytes of existing speex data\n", dataWritten);
}


int twtw_speex_write_all_data_to_oggz_and_finish(TwtwSpeexStatePtr state, OGGZ *oggz, long serialno)
{
    g_return_val_if_fail(state, TWTW_PARAMERR);
    g_return_val_if_fail(oggz, TWTW_PARAMERR);    
    g_return_val_if_fail(serialno != -1, TWTW_PARAMERR);
    
    // if there's existing data, we can just write it out wholesale
    if (state->speexData) {
        writeExistingSpeexDataToOggz(state, oggz, serialno);
        memset(state, 0, sizeof(*state));
        g_free(state);
        return 0;
    }

    short inputBuf[MAX_FRAME_SIZE];
    char bitsBuf[MAX_FRAME_BYTES];
    int nbBytes;
    int nb_samples, nb_encoded;
    int total_samples = 0;
    int total_written = 0;
    int eos = 0;
    int frameN = -1;
    int nframes = state->numFrames;
    int frameSize = state->frameSize;
    int lookahead = state->lookahead;
    
    ogg_packet op;
    memset(&op, 0, sizeof(op));
    
    void *encState = state->speexEncState;
    SpeexBits *bits = &(state->speexBits);
    SpeexPreprocessState *preprocState = state->preprocState;
    
    g_return_val_if_fail(encState, -1);
    g_return_val_if_fail(bits, -1);
    
    int32_t readSize = state->fileDataLeft;
    ///printf("%s: starting to read, data left %i, framesize %i, lookahead %i\n", __func__, readSize, frameSize, lookahead);
    
    nb_samples = readPCMSamples(state->file, &(state->pcmInfo), frameSize, inputBuf, NULL, &readSize);
    
    if (nb_samples == 0)
        eos = 1;
    
    total_samples += nb_samples;
    nb_encoded = -lookahead;
    
    //printf("  ... data left: %i\n", readSize);
    
    // main encoding loop (one frame per iteration)
    while ( !eos || total_samples > nb_encoded)
    {
        frameN++;
        
        if (preprocState)
            speex_preprocess(preprocState, inputBuf, NULL);
        
        speex_encode_int(encState, inputBuf, bits);
        nb_encoded += frameSize;
        
        nb_samples = readPCMSamples(state->file, &(state->pcmInfo), frameSize, inputBuf, NULL, &readSize);
        total_samples += nb_samples;

        eos = (nb_samples == 0) ? 1 : 0;

        op.e_o_s = (eos && total_samples <= nb_encoded) ? 1 : 0;
        
        ///printf("  frame %i: data left: %i  eos: %i (%i)\n", frameN, readSize, op.e_o_s, eos);
        
        if ((frameN+1) % nframes == 0) {
            speex_bits_insert_terminator(bits);
            nbBytes = speex_bits_write(bits, bitsBuf, MAX_FRAME_BYTES);
            speex_bits_reset(bits);
            
            op.packet = (unsigned char *)bitsBuf;
            op.bytes = nbBytes;
            op.b_o_s = 0;
                            
            op.granulepos = (frameN+1)*frameSize - lookahead;
            
            if (op.granulepos > total_samples)
                op.granulepos = total_samples;
                
            //printf ("  writing packet, speex enc granulepos: %d, id %d (%d), packetbytes %i\n", (int)op.granulepos, frameN, nframes, op.bytes);
            
            op.packetno = -1;  //allow oggz to fill (was: 2 + frameN/nframes)
            
            oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
            total_written += nbBytes;
            
            while ((oggz_write (oggz, 400)) > 0);            
        }
    }
    
    
    if ((frameN+1) % nframes != 0) {
        while ((frameN+1) % nframes != 0)
        {
            frameN++;
            speex_bits_pack(bits, 15, 5);
        }
        nbBytes = speex_bits_write(bits, bitsBuf, MAX_FRAME_BYTES);
        op.packet = (unsigned char *)bitsBuf;
        op.bytes = nbBytes;
        op.b_o_s = 0;
        op.e_o_s = 1;
        op.granulepos = (frameN+1)*frameSize - lookahead;
        
        if (op.granulepos > total_samples)
            op.granulepos = total_samples;
        
        op.packetno = -1;  //2+id/nframes;

        oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
        total_written += nbBytes;
        ///printf("wrote last uneven frame with %i bytes\n", nbBytes);
        
        while ((oggz_write (oggz, 400)) > 0);
    }
    
    printf("done with speex enc; total written: %i bytes\n", total_written);
    
    speex_encoder_destroy(state->speexEncState);
    speex_bits_destroy( &(state->speexBits) );
    
    if (state->preprocState)
        speex_preprocess_state_destroy(state->preprocState);

    fclose(state->file);
    
    memset(state, 0, sizeof(*state));
    g_free(state);
    return 0;
}



#ifdef __APPLE__
#pragma mark --- decoding ---
#endif


int twtw_speex_init_decoding_to_pcm_path_utf8 (const char *path, size_t pathLen, TwtwSpeexStatePtr *outState)
{
    g_return_val_if_fail(path, TWTW_PARAMERR);
    g_return_val_if_fail(outState, TWTW_PARAMERR);

    const char *speex_version = NULL;
    speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
    printf("going to decode speex, library version is: '%s'\n", speex_version);

    FILE *file = twtw_open_writeb_utf8(path, pathLen);
    if ( !file) {
        printf("** unable to write to path: %s\n", path);
        return TWTW_FILEERR;
    }

    int rate = 8000;
    int numChannels = 1;

    twtw_write_wav_header(file, rate, numChannels, 16, 0);
    
    SpeexBits bits;
    speex_bits_init(&bits);
    
    
    TwtwSpeexState *state = g_malloc0(sizeof(TwtwSpeexState));
    *outState = state;
    state->file = file;
    state->pcmInfo.sampleRate = rate;
    state->pcmInfo.numChannels = numChannels;
    state->pcmInfo.dataFormat = 16;
    state->fileDataLeft = -1;
    
    state->speexEncState = NULL;
    state->speexBits = bits;
    
    return 0;
}

int twtw_identify_speex_header (ogg_packet *op, SpeexHeader *outSpeexHeader)
{
    g_return_val_if_fail(op, -1);
    
    if (op->bytes < 5 || memcmp(op->packet, "Speex", 5))
        return -1;
        
    if (outSpeexHeader) {
        SpeexHeader *spxhead = speex_packet_to_header((char *)op->packet, op->bytes);
        
        memcpy(outSpeexHeader, spxhead, sizeof(SpeexHeader));
        free(spxhead);  // speex uses regular malloc for its returned objects
    }
    return 0;
}

int twtw_speex_read_apply_header (TwtwSpeexStatePtr state, SpeexHeader *header)
{
    g_return_val_if_fail (state, -1);
    g_return_val_if_fail (header, -1);
    
    ///printf("%s (headerptr %p): mode in speex header is %i, rate %i\n", __func__, header, (int)header->mode, (int)header->rate);
    
    const SpeexMode *mode = speex_lib_get_mode(header->mode);
    
    void *decState = speex_decoder_init(mode);
    g_return_val_if_fail (decState, -1);

    int32_t frameSize = 0;
    int32_t lookahead = 0;
    speex_decoder_ctl(decState, SPEEX_GET_FRAME_SIZE, &frameSize);
    speex_decoder_ctl(decState, SPEEX_GET_LOOKAHEAD, &lookahead);

    int32_t rate = header->rate;
    speex_decoder_ctl(decState, SPEEX_SET_SAMPLING_RATE, &rate);

    int32_t enhancerEnabled = 1;
    speex_decoder_ctl(decState, SPEEX_SET_ENH, &enhancerEnabled);
    
    
    state->speexDecState = decState;
    state->header = *header;
    state->numFrames = header->frames_per_packet;
    state->frameSize = frameSize;
    state->lookahead = lookahead;
    state->packetsRead = 0;
    state->pcmBytesWritten = 0;
    
    state->speexDataSize = 0;
    
    return 0;
}


// write the length of the audio data into the file header
static void closeWAVFile(FILE *fout, int32_t audio_size)
{
    if (fseek(fout,4,SEEK_SET)==0)
    {
         int tmp;
         tmp = _le_32(audio_size+36);
         fwrite(&tmp,4,1,fout);
         if (fseek(fout,32,SEEK_CUR)==0)
         {
            tmp = _le_32(audio_size);
            fwrite(&tmp,4,1,fout);
         } else
         {
            fprintf (stderr, "First seek worked, second didn't\n");
         }
    } else {
        int ferr = ferror(fout);
        fprintf (stderr, "Cannot seek on wave file, size will be incorrect (ferror: %i)\n", ferr);
    }

    fclose(fout);
}

int twtw_speex_read_data_from_ogg_packet (TwtwSpeexStatePtr state, ogg_packet *op)
{
    g_return_val_if_fail (state, -1);
    g_return_val_if_fail (op, -1);
    g_return_val_if_fail (state->file, -1);

    SpeexBits *bits = &(state->speexBits);
    void *decState = state->speexDecState;
    const int nframes = state->numFrames;
    
    short outputBuf[MAX_FRAME_SIZE];
    int eos = (op->e_o_s) ? 1 : 0;
    int doAbort = FALSE;

    speex_bits_read_from(bits, (char *)op->packet, op->bytes);
    
    ///printf("  speex packet %i: read %i bytes\n", state->packetsRead, op->bytes);
    
    // append the new data to the cache buffer
    if (op->bytes > 0) {
        size_t newTotalDataSize = state->speexDataSize + op->bytes;
        state->speexData = ( !state->speexData) ? g_malloc(newTotalDataSize)
                                                : g_realloc(state->speexData, newTotalDataSize);
        
        memcpy(state->speexData + state->speexDataSize, op->packet, op->bytes);
        state->speexDataSize = newTotalDataSize;
    }

    state->packetsRead++;
        
    long j;
    for (j = 0; j < nframes; j++) {
        int result = speex_decode_int(decState, bits, outputBuf);
        
        if (result != 0) {
            if (result == -2) {
                printf("** Speex decoding error: corrupted stream?\n");
            }
            doAbort = TRUE;
        }
        if (speex_bits_remaining(bits) < 0) {
            printf("** Speex decoding overflow\n");
            doAbort = TRUE;
        }
        
#if !defined(__LITTLE_ENDIAN__) && ( defined(WORDS_BIGENDIAN) || defined(__BIG_ENDIAN__) )
        // source data is LSB, so flip
        long i;
        long bufSize = state->frameSize * 1;  // single channel
        for (i = 0; i < bufSize; i++) {
            short v = outputBuf[i];
            outputBuf[i] = _le_16(v);
        }
#endif
        
        fwrite(outputBuf, sizeof(short), state->frameSize, state->file);
        state->pcmBytesWritten += state->frameSize * sizeof(short);
    }
    
    if (doAbort || eos) {
        closeWAVFile(state->file, state->pcmBytesWritten);
        state->file = NULL;
        
        speex_decoder_destroy(decState);
        memset(state, 0, sizeof(*state));
    }
    
    return (doAbort) ? -1 : 0;
}


int twtw_speex_read_finish (TwtwSpeexStatePtr state, int *outPCMBytes, unsigned char **outSpeexData, size_t *outSpeexDataSize)
{
    if (state->file)
        closeWAVFile(state->file, state->pcmBytesWritten);

    if (state->speexDecState)
        speex_decoder_destroy(state->speexDecState);

    if (outSpeexData && outSpeexDataSize) {
        *outSpeexData = state->speexData;
        *outSpeexDataSize = state->speexDataSize;
    } else {
        g_free(state->speexData);
    }

    speex_bits_destroy( &(state->speexBits) );
    
    if (outPCMBytes)
        *outPCMBytes = state->pcmBytesWritten;
    
    memset(state, 0, sizeof(*state));
    
    g_free(state);
    return 0;
}
