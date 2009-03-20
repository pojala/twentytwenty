/*
 *  twtw-audio-maemo.c
 *  TwentyTwenty
 *
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

#include <stdio.h>
#include <stdint.h>
#include <glib.h>
#include "twtw-audio.h"
#include "twtw-audio-wavfile.h"
#include <gst/gst.h>


static GstElement *g_pipeline = NULL;
static int g_pipelineMode = 0;
static TwtwAudioCallbacks g_audioCallbacks = { NULL, NULL };
static gpointer g_audioCbData = NULL;
static gboolean g_recIsWAVFormat = FALSE;
static char *g_recPath = NULL;


#define DEFAULT_REC_BLOCKSIZE "160"

#define STOP_DELAY 1000
#define REC_UPDATE_INTERVAL 750
#define PLAY_UPDATE_INTERVAL 200


extern double twtw_absolute_time_get_current();


static void stopPipeline()
{
    if (g_pipeline) {
        gst_element_set_state (g_pipeline, GST_STATE_NULL);
        gst_object_unref (GST_OBJECT(g_pipeline));
        g_pipeline = NULL;
    }
  
    if (g_pipelineMode == TWTW_AUDIOSTATUS_REC && g_recPath) {
        printf("stopping recording; pcm path is %s\n", g_recPath);
        
        int len = strlen(g_recPath);
        if (len > 3 && 0 != memcmp(g_recPath+len-3, "wav", 3)) {
            // is not a wav file, so add the wav header
            char *wavPath = g_malloc0(len + 1);
            memcpy(wavPath, g_recPath, len-4);
            memcpy(wavPath+len-4, ".wav", 4);
            
            FILE *pcmFile = fopen(g_recPath, "rb");
            FILE *wavFile = fopen(wavPath, "wb");
            if ( !pcmFile || !wavFile) {
                printf("** can't open file for WAV wrapping (src %p, dst %p)\n", pcmFile, wavFile);
            } else {
                fseek(pcmFile, 0, SEEK_END);
                size_t pcmSize = ftell(pcmFile);

                if (pcmSize < 1) {
                    printf("** pcm file size is zero\n");
                } else {
                    if (fseek(pcmFile, 0, SEEK_SET)) {
                        printf("** unable to seek on pcm file\n");
                    } else {
                        char *pcmData = g_malloc(pcmSize);
                        
                        size_t bytesRead = fread(pcmData, 1, pcmSize, pcmFile);
                        if (bytesRead != pcmSize) {
                            printf("** unable to read all pcm data (%i, expected %i)\n", (int)bytesRead, (int)pcmSize);
                        } else {
                            twtw_write_wav_header(wavFile, TWTW_PCM_SAMPLERATE, 1, TWTW_PCM_SAMPLEBITS, pcmSize);
                            
                            size_t bytesWritten = fwrite(pcmData, 1, pcmSize, wavFile);
                            printf("wrote %i bytes of PCM data to path: %s\n", (int)bytesWritten, wavPath);
                        }
                        
                        g_free(pcmData);
                    }
                }
            }
            if (pcmFile) fclose(pcmFile);
            if (wavFile) fclose(wavFile);
            if (wavPath) g_free(wavPath);
        }        
    }
    g_free(g_recPath);
    g_recPath = NULL;
    

    if (g_pipelineMode != 0 && g_audioCallbacks.audioCompletedFunc) {
        g_audioCallbacks.audioCompletedFunc(g_pipelineMode, g_audioCbData);
    }
    
    g_pipelineMode = 0;
    g_audioCbData = NULL;
}

static void busEOSMessageReceived (GstBus * bus, GstMessage * message, gpointer data)
{
    printf("%s: mode %i; cbdata is %p\n", __func__, g_pipelineMode, g_audioCbData);

    stopPipeline();
}



static gboolean timerFunc(void *data)
{
    double *timerData = (double *)data;
    g_return_val_if_fail(data, FALSE);

    double startingTime = *timerData;
    double currentTime = twtw_absolute_time_get_current();
    double timeDone = currentTime - startingTime;
    if (timeDone < 0.0)
        timeDone = 0.0;
    if (timeDone > 100.0)
        timeDone = 100.0;

    //printf("%s: mode %i, starttime %.3f -- currenttime %.3f\n", __func__, g_pipelineMode, startingTime, currentTime);

    if (g_pipelineMode == 0) {
        g_free(timerData);
        return FALSE;  // cancels the timer
    }
    
    // adjust times a bit (matches the non-play/rec UI values more closely)
    if (g_pipelineMode == TWTW_AUDIOSTATUS_PLAY)
        timeDone -= 0.15;
    else
        timeDone += 0.1;
    
    if (g_audioCallbacks.audioInProgressFunc) {
        g_audioCallbacks.audioInProgressFunc(g_pipelineMode, timeDone, g_audioCbData);
    }
    return TRUE;
}



static GstCaps *createAudioCapsFilter()
{
    return gst_caps_new_simple(
                "audio/x-raw-int",
                "rate", G_TYPE_INT, TWTW_PCM_SAMPLERATE,
                "signed", G_TYPE_BOOLEAN, TRUE,
                "channels", G_TYPE_INT, 1,
                "endianness", G_TYPE_INT, 1234,  // little-endian
                "width", G_TYPE_INT, TWTW_PCM_SAMPLEBITS,
                "depth", G_TYPE_INT, TWTW_PCM_SAMPLEBITS,
                NULL);
}

static int wavRecord(const char *soundPath)
{
    g_return_val_if_fail (NULL == g_pipeline, -1);
    g_return_val_if_fail (soundPath, -1);

    int retval = 0;
    GstElement *src = NULL;
    GstElement *sink = NULL;
    GstElement *filter = NULL;
    GstElement *queue = NULL;
    GstElement *parse = NULL;
    GstCaps *caps = NULL;

    char *newPath = NULL;

    g_pipeline = gst_pipeline_new ("pipeline");

            src = gst_element_factory_make("dsppcmsrc", "source");
            g_object_set(G_OBJECT (src),
                            "blocksize", DEFAULT_REC_BLOCKSIZE,
                            //"dtx", 0,
                            NULL);
                    
            filter = gst_element_factory_make("capsfilter", "filter");
            if ( !filter)
                printf("** failed to create capsfilter\n");
            
            parse = gst_element_factory_make("wavenc", "enc");
            if ( !parse) {
                printf("** failed to create wavenc\n");
                
                int len = strlen(soundPath);
                if (len > 3) {
                    newPath = g_malloc0(len+1);
                    memcpy(newPath, soundPath, len-3);
                    memcpy(newPath+len-3, "pcm", 3);
                }
            }
                  
            printf("rec dst location is: %s\n", (newPath) ? newPath : soundPath);
                        
            caps = createAudioCapsFilter();
            
            g_object_set(G_OBJECT(filter), 
                            "caps", caps,
                            NULL);

            sink = gst_element_factory_make("filesink", "sink");
            g_object_set(G_OBJECT(sink), 
                            "location", (newPath) ? newPath : soundPath,
                            NULL);

    g_recIsWAVFormat = (parse) ? TRUE : FALSE;

    if (parse) {
        gst_bin_add_many(GST_BIN(g_pipeline), src, parse, sink, NULL);

        if ( !gst_element_link_many (src, parse, sink, NULL)) {
            printf("** failed to link gst elements for WAV record (%p, %p, %p)\n", src, parse, sink);
            retval = -1;
        }
    } else {
        gst_bin_add_many(GST_BIN(g_pipeline), src, filter, sink, NULL);

        if ( !gst_element_link_many (src, filter, sink, NULL)) {
            printf("** failed to link gst elements for PCM record (%p, %p, %p)\n", src, filter, sink);
            retval = -1;
        }
        else printf("ok to record PCM\n");
    }
    
    if (retval == 0) {
        //gst_bus_add_watch (gst_pipeline_get_bus (GST_PIPELINE (g_pipeline)),
        //                   gstBusWatchCallback, NULL);

    
        GstBus *bus;    
        
        bus = gst_pipeline_get_bus (GST_PIPELINE (g_pipeline));
        gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
        g_signal_connect (bus, "message::eos", (GCallback) busEOSMessageReceived, NULL);
    
        gst_element_set_state(GST_ELEMENT(g_pipeline), GST_STATE_PLAYING);
        
        g_pipelineMode = TWTW_AUDIOSTATUS_REC;
        
        g_recPath = g_strdup((newPath) ? newPath : soundPath);
        
        // add timer to update the UI
        double *timerData = g_malloc(sizeof(double));
        *timerData = twtw_absolute_time_get_current();
        g_timeout_add(500, timerFunc, timerData);
    }

    if (newPath) g_free(newPath);
    
    if (caps) gst_caps_unref(caps);
    
    return retval;
}


static int wavPlay(const char *soundPath)
{
  GstElement *filesrc;
  GstBus *bus;
  GError *error = NULL;

  //testing path: soundPath = "/home/user/MyDocs/page00.wav";

  /* we're already playing */
  if (g_pipeline) return -1;
  
  
  /* setup pipeline and configure elements */
  g_pipeline = gst_parse_launch (
#ifdef __arm__
        "filesrc name=my_filesrc ! wavparse ! dsppcmsink",
#else
        /* "filesrc name=my_filesrc ! wavparse ! osssink", */
  #ifdef HAVE_ESD
        "filesrc name=my_filesrc ! wavparse ! alsasink",
  #elif HAVE_PULSEAUDIO
        "filesrc name=my_filesrc ! wavparse ! pulsesink",
#endif
#endif
         &error);
   if (error) {
    fprintf (stderr, "Parse error: %s\n", error->message);
    goto error;
  }
  
  filesrc = gst_bin_get_by_name (GST_BIN (g_pipeline), "my_filesrc");
  if (!filesrc) {
    fprintf (stderr, "Parse error: no filesrc\n");
    goto error;
  }

  g_object_set (G_OBJECT (filesrc), "location", soundPath, NULL);

  /* setup message handling */
  bus = gst_pipeline_get_bus (GST_PIPELINE (g_pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  g_signal_connect (bus, "message::eos", (GCallback) busEOSMessageReceived, NULL);
  //gst_object_unref (GST_OBJECT(bus));
  
  /* start playback */
  gst_element_set_state (g_pipeline, GST_STATE_PLAYING);
  
  g_pipelineMode = TWTW_AUDIOSTATUS_PLAY;

    // add timer to update the UI
    double *timerData = g_malloc(sizeof(double));
    *timerData = twtw_absolute_time_get_current();
    g_timeout_add(500, timerFunc, timerData);
  
  return 0;

error:
  gst_object_unref (GST_OBJECT(g_pipeline));
  g_pipeline = NULL;
  return -1;  
}


void twtw_audio_init ()
{
    g_pipelineMode = 0;
}

void twtw_audio_deinit ()
{
    if (g_pipelineMode != 0)
        twtw_audio_pcm_stop ();
}


int twtw_audio_pcm_record_to_path_utf8 (const char *path, size_t pathLen, int seconds, TwtwAudioCallbacks callbacks, void *cbData)
{
    if (g_pipelineMode != 0) {
        twtw_audio_pcm_stop ();
    }
    
    g_audioCallbacks = callbacks;
    g_audioCbData = cbData;
    
    wavRecord(path);
    
    return 0;
}

int twtw_audio_pcm_play_from_path_utf8 (const char *path, size_t pathLen, TwtwAudioCallbacks callbacks, void *cbData)
{
    if (g_pipelineMode != 0) {
        twtw_audio_pcm_stop ();
    }
    
    g_audioCallbacks = callbacks;
    g_audioCbData = cbData;
    
    wavPlay(path);
        
    return 0;
}

void twtw_audio_pcm_stop ()
{
    stopPipeline();

    memset(&g_audioCallbacks, 0, sizeof(g_audioCallbacks));
    g_audioCbData = NULL;
}


