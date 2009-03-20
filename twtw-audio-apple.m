/*
 *  twtw-audio-apple.c
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

/*
    Based on Mac OS X 10.5 SDK code samples (AQRecorder / AQPlayer).
    
    This implementation requires the 10.5 Audio Queue API, so Tiger is unfortunately excluded.
    A rewrite into raw CoreAudio would be possible but seems like a very unpleasant endeavour.
*/

#include "twtw-audio.h"
#include <glib.h>

#import <AudioToolbox/AudioToolbox.h>  // Audio Queue (Leopard only)
#import <Foundation/Foundation.h> 


#define kNumberRecordBuffers    3

#define kNumberPlayBuffers      3


typedef struct {
	AudioFileID					recordFile;
	AudioQueueRef				queue;
    
	CFAbsoluteTime				queueStartTime;	
	CFAbsoluteTime				queueStopTime;
	SInt64						recordPacket; // current packet number in record file
	Boolean						running;
	Boolean						verbose;
} TwtwAQRecorder;


typedef struct {
    AudioFileID                 playFile;
    AudioQueueRef               queue;

	AudioQueueBufferRef			buffers[kNumberPlayBuffers];
        
	AudioStreamBasicDescription dataFormat;
    
	CFAbsoluteTime				queueStartTime;
    CFAbsoluteTime				queueStopTime;
    SInt64                      currentPacket;
    UInt32                      numPacketsToRead;
} TwtwAQPlayer;


enum {
    TwtwPCMIsIdle = 0,
    TwtwPCMIsRecording,
    TwtwPCMIsPlaying,
    TwtwPCMIsPriming
};

typedef struct _TwtwPCMState {
    int state;
    int didComplete;
    TwtwAudioCallbacks callbacks;
    void *cbData;
    
    CFRunLoopTimerRef timer;
    
    TwtwAQRecorder *aqRecorder;
    TwtwAQPlayer *aqPlayer;
} TwtwPCMState;


static TwtwPCMState g_pcmState = { 0, 0, NULL, NULL, NULL, NULL };

static NSRecursiveLock *g_pcmStateLock = nil;


#pragma mark --- init ---

void twtw_audio_init ()
{
    memset(&g_pcmState, 0, sizeof(TwtwPCMState));
    
    if ( !g_pcmStateLock)
        g_pcmStateLock = [[NSRecursiveLock alloc] init];
}

void twtw_audio_deinit ()
{
    if (g_pcmState.state != TwtwPCMIsIdle)
        twtw_audio_pcm_stop ();

    [g_pcmStateLock release];
    g_pcmStateLock = nil;
}


#pragma mark --- recording ---

// Determine the size, in bytes, of a buffer necessary to represent the supplied number
// of seconds of audio data.
static int aqRecord_ComputeRecordBufferSize(const AudioStreamBasicDescription *format, AudioQueueRef queue, float seconds)
{
	int packets, frames, bytes;
    OSStatus err;
	
	frames = (int)ceil(seconds * format->mSampleRate);
	
	if (format->mBytesPerFrame > 0)
		bytes = frames * format->mBytesPerFrame;
	else {
		UInt32 maxPacketSize;
		if (format->mBytesPerPacket > 0)
			maxPacketSize = format->mBytesPerPacket;	// constant packet size
		else {
			UInt32 propertySize = sizeof(maxPacketSize);
			err = AudioQueueGetProperty(queue, kAudioConverterPropertyMaximumOutputPacketSize, &maxPacketSize, &propertySize);
            if (err != noErr)
                NSLog(@"** %s: couldn't get max output packet size", __func__);
		}
		if (format->mFramesPerPacket > 0)
			packets = frames / format->mFramesPerPacket;
		else
			packets = frames;	// worst-case scenario: 1 frame in a packet
		if (packets == 0)		// sanity check
			packets = 1;
		bytes = packets * maxPacketSize;
	}
	return bytes;
}

// Copy a queue's encoder's magic cookie to an audio file.
static void	aqRecord_CopyEncoderCookieToFile(AudioQueueRef theQueue, AudioFileID theFile)
{
	OSStatus err;
	UInt32 propertySize = 0;
	// get the magic cookie, if any, from the converter		
	err = AudioQueueGetPropertySize(theQueue, kAudioQueueProperty_MagicCookie, &propertySize);
	
	// we can get a noErr result and also a propertySize == 0
	// -- if the file format does support magic cookies, but this file doesn't have one.
	if (err == noErr && propertySize > 0) {
		Byte magicCookie[propertySize];

        err = AudioQueueGetProperty(theQueue, kAudioConverterCompressionMagicCookie, magicCookie, &propertySize);
              
        if (err == noErr)
            err = AudioFileSetProperty(theFile, kAudioFilePropertyMagicCookieData, propertySize, magicCookie);
	}
}

// AudioQueue callback function, called when a property changes.
static void aqRecord_PropertyListener(void *userData, AudioQueueRef queue, AudioQueuePropertyID propertyID)
{
    [g_pcmStateLock lock];
	TwtwAQRecorder *aqr = g_pcmState.aqRecorder;
    
	if (propertyID == kAudioQueueProperty_IsRunning) {
        aqr->queueStartTime = CFAbsoluteTimeGetCurrent();
    }
        
    [g_pcmStateLock unlock];
}

// AudioQueue callback function, called when an input buffers has been filled.
static void aqRecord_InputBufferHandler(
                                    void *                          inUserData,
                                    AudioQueueRef                   inAQ,
									AudioQueueBufferRef             inBuffer,
									const AudioTimeStamp *          inStartTime,
									UInt32							inNumPackets,
									const AudioStreamPacketDescription *inPacketDesc)
{
    [g_pcmStateLock lock];
    if (g_pcmState.aqRecorder) {
        TwtwAQRecorder *aqr = g_pcmState.aqRecorder;
        OSStatus err;
                
        if (aqr->verbose) {
            printf("%s: buf data %p, %i bytes, %i packets\n", __func__, inBuffer->mAudioData,
                                        (int)inBuffer->mAudioDataByteSize, (int)inNumPackets);
                                        
            printf("    record packet: %i; record file: %p\n", (int)aqr->recordPacket, aqr->recordFile);
        }
        
        if (inNumPackets > 0) {
            // write packets to file
            err = AudioFileWritePackets(aqr->recordFile, FALSE, inBuffer->mAudioDataByteSize,
                                        inPacketDesc, aqr->recordPacket, &inNumPackets, inBuffer->mAudioData);

            if (err != noErr) {
                NSString *desc = (err == paramErr) ? @"paramErr" : [[NSString alloc] initWithFormat:@"err %i", err];
                
                NSLog(@"*** %s: AudioFileWritePackets failed (%@); %p, audioDataByteSize %i, inPacketDesc %p, recPacket %i, numPackets %i, outdata %p", __func__, desc,
                                                               aqr->recordFile, inBuffer->mAudioDataByteSize, inPacketDesc, (int)aqr->recordPacket,
                                                               (int)inNumPackets, inBuffer->mAudioData);
            }

            aqr->recordPacket += inNumPackets;
        }

        if (g_pcmState.callbacks.audioInProgressFunc && aqr->queueStartTime > 0.0) {
            double elapsedTime = CFAbsoluteTimeGetCurrent() - aqr->queueStartTime;
            g_pcmState.callbacks.audioInProgressFunc(TWTW_AUDIOSTATUS_REC, elapsedTime, g_pcmState.cbData);
        }


        // if we're not stopping, re-enqueue the buffer so that it gets filled again
        if (aqr->running) {
            err = AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
            
            if ( err != noErr) NSLog(@"** %s: AudioQueueEnqueueBuffer failed", __func__);
        }
    }
    [g_pcmStateLock unlock];
}

static void stopAQRecording()
{
    NSCAssert(g_pcmState.aqRecorder, @"no recorder state");
    [g_pcmStateLock lock];
    
    TwtwAQRecorder *aqr = g_pcmState.aqRecorder;
    OSStatus err;

    aqr->running = FALSE;
    g_pcmState.state = TwtwPCMIsIdle;
    [g_pcmStateLock unlock];

    if ((err = AudioQueueStop(aqr->queue, TRUE)) != noErr) {
        NSLog(@"** %s: AudioQueueStop failed (%i)", __func__, err);
    }
    
    // a codec may update its cookie at the end of an encoding session, so reapply it to the file now
    aqRecord_CopyEncoderCookieToFile(aqr->queue, aqr->recordFile);
    
    AudioQueueDispose(aqr->queue, TRUE);
	AudioFileClose(aqr->recordFile);
    
    free(g_pcmState.aqRecorder);
    g_pcmState.aqRecorder = NULL;
    
    if (g_pcmState.timer) {
        CFRunLoopTimerInvalidate(g_pcmState.timer);
        CFRelease(g_pcmState.timer);
    }
    
    if (g_pcmState.callbacks.audioCompletedFunc) {
        g_pcmState.callbacks.audioCompletedFunc(TWTW_AUDIOSTATUS_REC, g_pcmState.cbData);
    }
    
    memset(&g_pcmState, 0, sizeof(TwtwPCMState));
}


static void aqRecord_TimerCallback (CFRunLoopTimerRef timer, void *userdata)
{
    [g_pcmStateLock lock];

    BOOL doStop = YES;
    if (g_pcmState.state != TwtwPCMIsRecording) {
        NSLog(@"** %s: not in recording state (this timer should have been invalidated)", __func__);
        doStop = NO;
    }
    
    if (doStop) g_pcmState.didComplete = TRUE;
    
    [g_pcmStateLock unlock];
    
    if (doStop) stopAQRecording();
}


static NSString *printableOSStatusError(OSStatus error, const char *operation)
{
	if (error == noErr) return @"(no error)";
	
	char str[20];
	// see if it appears to be a 4-char-code
	*(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
	if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
		str[0] = str[5] = '\'';
		str[6] = '\0';
	} else
		// no, format it as an integer
		sprintf(str, "%d", (int)error);

    return [NSString stringWithFormat:@"%s (%s)", operation, str];
}



int twtw_audio_pcm_record_to_path_utf8 (const char *path, size_t pathLen, int seconds, TwtwAudioCallbacks callbacks, void *cbData)
{
    if ( !path || pathLen < 1 || seconds < 1)
        return -1;
 
    [g_pcmStateLock lock];
    BOOL stateIsOK = (g_pcmState.state == TwtwPCMIsIdle);
    [g_pcmStateLock unlock];
    
    if ( !stateIsOK) {
        NSLog(@"** %s: can't start recording, system is not idle (state: %i)", __func__, g_pcmState.state);
        return -1;
    }
    
    memset(&g_pcmState, 0, sizeof(TwtwPCMState));
    
    AudioStreamBasicDescription recordFormat;
    TwtwAQRecorder aqr;
    OSStatus err;
        
    memset(&recordFormat, 0, sizeof(recordFormat));
	memset(&aqr, 0, sizeof(aqr));

    recordFormat.mChannelsPerFrame = 1;
    recordFormat.mSampleRate = TWTW_PCM_SAMPLERATE;
    recordFormat.mBitsPerChannel = TWTW_PCM_SAMPLEBITS;
    recordFormat.mFormatID = kAudioFormatLinearPCM;
    recordFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;

#ifndef TWTW_PCM_LITTLE_ENDIAN
    recordFormat.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif

    recordFormat.mBytesPerPacket = recordFormat.mBytesPerFrame =
            (recordFormat.mBitsPerChannel / 8) * recordFormat.mChannelsPerFrame;
            
    recordFormat.mFramesPerPacket = 1;
    recordFormat.mReserved = 0;
    
    if ((err = AudioQueueNewInput(&recordFormat,
                                  aqRecord_InputBufferHandler,
                                  NULL,
                                  NULL /* run loop */, NULL /* run loop mode */,
                                  0 /* flags */, &aqr.queue)) != noErr) {
        NSLog(@"** AudioQueueNewInput failed");
        return err;
    }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (Byte *)path, pathLen, FALSE);
    AudioFileTypeID audioFileType = kAudioFileWAVEType;
        
    if ((err = AudioFileCreateWithURL(url, audioFileType, &recordFormat, kAudioFileFlags_EraseFile,
                                      &aqr.recordFile)) != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "AudioFileCreateWithURL"));
        return err;
    }
    
    CFRelease(url);
    url = NULL;
    
    
    // copy the cookie first to give the file object as much info as we can about the data going in
    aqRecord_CopyEncoderCookieToFile(aqr.queue, aqr.recordFile);
    
	// allocate and enqueue buffers
	int bufferByteSize = aqRecord_ComputeRecordBufferSize(&recordFormat, aqr.queue, 0.5);	// enough bytes for half a second
    int i;
	for (i = 0; i < kNumberRecordBuffers; ++i) {
		AudioQueueBufferRef buffer;
        AudioQueueAllocateBuffer(aqr.queue, bufferByteSize, &buffer);
        
        AudioQueueEnqueueBuffer(aqr.queue, buffer, 0, NULL);
	}

    aqr.running = TRUE;
    aqr.verbose = FALSE;
    
    g_pcmState.state = TwtwPCMIsRecording;
    g_pcmState.callbacks = callbacks;
    g_pcmState.cbData = cbData;
    
    g_pcmState.aqRecorder = malloc(sizeof(TwtwAQRecorder));
    memcpy(g_pcmState.aqRecorder, &aqr, sizeof(TwtwAQRecorder));

    // add listener to time the recording more accurately
    g_pcmState.aqRecorder->queueStartTime = 0.0;
    AudioQueueAddPropertyListener(aqr.queue, kAudioQueueProperty_IsRunning, aqRecord_PropertyListener, NULL);
    
    if ((err = AudioQueueStart(aqr.queue, NULL)) != noErr) {
        NSLog(@"** AudioQueueStart failed");
        stopAQRecording();
        return 16452;
    }
    
    CFAbsoluteTime waitForStartUntil = CFAbsoluteTimeGetCurrent() + 10;

    // wait for the started notification
    while (g_pcmState.aqRecorder->queueStartTime == 0.0) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.010, FALSE);
        if (CFAbsoluteTimeGetCurrent() >= waitForStartUntil) {
            fprintf(stderr, "Timeout waiting for the queue's IsRunning notification\n");
            
            stopAQRecording();
            return 16455;
        }
    }

    g_pcmState.aqRecorder->queueStopTime = g_pcmState.aqRecorder->queueStartTime + seconds;
    CFAbsoluteTime stopTime = g_pcmState.aqRecorder->queueStopTime;
    //CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

    ///NSLog(@"%s: got callback notif, now recording for %i seconds", __func__, seconds);

    CFRunLoopTimerRef recTimer = CFRunLoopTimerCreate(NULL,
                                                      stopTime,
                                                      0.0,  // interval
                                                      0,    // flags (ignored)
                                                      0,    // order (ignored)
                                                      aqRecord_TimerCallback,
                                                      NULL);
                                                          
    g_pcmState.timer = recTimer;
    CFRunLoopAddTimer(CFRunLoopGetMain(), recTimer, kCFRunLoopCommonModes);
    
    return 0;
}


#pragma mark --- playback ---


static void stopAQPlayback()
{
    NSCAssert(g_pcmState.aqPlayer, @"no recorder state");
    [g_pcmStateLock lock];
    
    TwtwAQPlayer *aqp = g_pcmState.aqPlayer;
    OSStatus err;

    g_pcmState.state = TwtwPCMIsIdle;
    [g_pcmStateLock unlock];

    if ((err = AudioQueueStop(aqp->queue, FALSE)) != noErr) {
        NSLog(@"** %s: AudioQueueStop failed (%i)", __func__, err);
    }
    
    AudioQueueDispose(aqp->queue, TRUE);
	AudioFileClose(aqp->playFile);
    
    free(g_pcmState.aqPlayer);
    
    if (g_pcmState.timer) {
        CFRunLoopTimerInvalidate(g_pcmState.timer);
        CFRelease(g_pcmState.timer);
    }
    
    if (g_pcmState.callbacks.audioCompletedFunc) {
        g_pcmState.callbacks.audioCompletedFunc(TWTW_AUDIOSTATUS_PLAY, g_pcmState.cbData);
    }
    
    memset(&g_pcmState, 0, sizeof(TwtwPCMState));
}

static void aqPlayback_TimerCallback(CFRunLoopTimerRef timer, void *userdata)
{
    [g_pcmStateLock lock];

    double time = CFAbsoluteTimeGetCurrent();

    BOOL doStop = (time >= g_pcmState.aqPlayer->queueStopTime) ? YES : NO;
    
    if (g_pcmState.state != TwtwPCMIsPlaying) {
        NSLog(@"** %s: not in playback state (this timer should have been invalidated)", __func__);
        doStop = NO;
    } else {
        if (g_pcmState.callbacks.audioInProgressFunc) {
            double elapsedTime = time - g_pcmState.aqPlayer->queueStartTime;
            g_pcmState.callbacks.audioInProgressFunc(TWTW_AUDIOSTATUS_PLAY, elapsedTime, g_pcmState.cbData);
        }    
    }
    
    if (doStop) g_pcmState.didComplete = TRUE;
    
    [g_pcmStateLock unlock];
    
    if (doStop) {
        //NSLog(@"going to stop playback timer");
        stopAQPlayback();
    }
}


static void aqPlay_BufferHandler(void *userData, AudioQueueRef inAQ, AudioQueueBufferRef inCompleteAQBuffer)
{
    BOOL shouldStop = NO;
    
    [g_pcmStateLock lock];
    
    if ((g_pcmState.state == TwtwPCMIsPlaying || g_pcmState.state == TwtwPCMIsPriming) && g_pcmState.aqPlayer) {
        TwtwAQPlayer *aqp = g_pcmState.aqPlayer;    
        UInt32 numBytes = 0;
        UInt32 nPackets = aqp->numPacketsToRead;
        
        OSStatus err = AudioFileReadPackets(aqp->playFile, FALSE, &numBytes, NULL, aqp->currentPacket, &nPackets,
                                            inCompleteAQBuffer->mAudioData);

        if (err != noErr) {
            NSLog(@"** %s failed: %@", __func__, printableOSStatusError(err, "AudioFileReadPackets"));
        }
        //else NSLog(@"did read %i packets from audiofile", (int)nPackets);
        
        if (nPackets > 0) {
            inCompleteAQBuffer->mAudioDataByteSize = numBytes;

            AudioQueueEnqueueBuffer(inAQ, inCompleteAQBuffer, 0, NULL);
		
            aqp->currentPacket += nPackets;            
        } else {
            shouldStop = YES;
        }        
    }
    [g_pcmStateLock unlock];
    
    //if (shouldStop && g_pcmState.state != TwtwPCMIsPriming)
    //    stopAQPlayback();
    if (shouldStop)
        g_pcmState.didComplete = YES;
}


static void aqPlay_PropertyListener(void *userData, AudioQueueRef queue, AudioQueuePropertyID propertyID)
{
    UInt32 isRunning = 0;
    UInt32 size = sizeof(isRunning);
    AudioQueueGetProperty (queue, kAudioQueueProperty_IsRunning, &isRunning, &size);
}


static void calculateBytesForTime(AudioStreamBasicDescription *inDesc, UInt32 inMaxPacketSize, Float64 inSeconds, UInt32 *outBufferSize, UInt32 *outNumPackets)
{
	static const int maxBufferSize = 0x10000; // limit size to 64K
	static const int minBufferSize = 0x4000; // limit size to 16K

	if (inDesc->mFramesPerPacket) {
		Float64 numPacketsForTime = inDesc->mSampleRate / inDesc->mFramesPerPacket * inSeconds;
		*outBufferSize = numPacketsForTime * inMaxPacketSize;
	} else {
		// if frames per packet is zero, then the codec has no predictable packet == time
		// so we can't tailor this (we don't know how many Packets represent a time period
		// we'll just return a default buffer size
		*outBufferSize = maxBufferSize > inMaxPacketSize ? maxBufferSize : inMaxPacketSize;
	}
	
		// we're going to limit our size to our default
	if (*outBufferSize > maxBufferSize && *outBufferSize > inMaxPacketSize)
		*outBufferSize = maxBufferSize;
	else {
		// also make sure we're not too small - we don't want to go the disk for too small chunks
		if (*outBufferSize < minBufferSize)
			*outBufferSize = minBufferSize;
	}
	*outNumPackets = *outBufferSize / inMaxPacketSize;
}


int twtw_audio_pcm_play_from_path_utf8 (const char *path, size_t pathLen, TwtwAudioCallbacks callbacks, void *cbData)
{
    [g_pcmStateLock lock];
    BOOL stateIsOK = (g_pcmState.state == TwtwPCMIsIdle);
    [g_pcmStateLock unlock];
    
    if ( !stateIsOK) {
        NSLog(@"** %s: can't start recording, system is not idle (state: %i)", __func__, g_pcmState.state);
        return -1;
    }
    
    memset(&g_pcmState, 0, sizeof(TwtwPCMState));


    TwtwAQPlayer aqp;
    memset(&aqp, 0, sizeof(aqp));
    OSStatus err;
    
    CFURLRef sndFile = CFURLCreateFromFileSystemRepresentation (NULL, (const UInt8 *)path, pathLen, FALSE);
    
    if ( !sndFile) {
        NSLog(@"** %s: can't parse path to URL", __func__);
        return -50;
    }
    
    BOOL okToPlay = NO;
    
    err = AudioFileOpenURL(sndFile, 0x1/*fsRdPerm*/, 0/*inFileTypeHint*/, &aqp.playFile);
    
    CFRelease(sndFile);
    sndFile = NULL;
    
    if (err != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "AudioFileOpenURL"));
        return err;
    }
    
    UInt32 size = sizeof(aqp.dataFormat);
    if ((err = AudioFileGetProperty(aqp.playFile, kAudioFilePropertyDataFormat, &size, &aqp.dataFormat)) != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "getDataFormat"));
        goto bail;
    }
    
    Float64 duration = 0;
    size = sizeof(duration);
    if ((err = AudioFileGetProperty(aqp.playFile, kAudioFilePropertyEstimatedDuration, &size, &duration)) != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "getDuration"));
        goto bail;
    }
    
    
    ///NSLog(@"%s: sample rate is %.2f; formatID %i, bitsPerCh %i; duration %.3f",
    ///        __func__, aqp.dataFormat.mSampleRate, aqp.dataFormat.mFormatID, aqp.dataFormat.mBitsPerChannel, duration);
    
    
    if ((err = AudioQueueNewOutput(&aqp.dataFormat, aqPlay_BufferHandler, NULL, 
                                    //NULL, NULL,
                                    CFRunLoopGetCurrent(), kCFRunLoopCommonModes,
                                    0, &aqp.queue)) != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "AudioQueueNewOutput"));
        goto bail;
    }

    UInt32 bufferByteSize = 0;
    UInt32 maxPacketSize = 0;
    size = sizeof(maxPacketSize);
    AudioFileGetProperty(aqp.playFile, kAudioFilePropertyPacketSizeUpperBound, &size, &maxPacketSize);
    
    // adjust buffer size to represent about a half second of audio based on this format
    calculateBytesForTime (&aqp.dataFormat, maxPacketSize, 0.5/*seconds*/, &bufferByteSize, &aqp.numPacketsToRead);

    // not all formats use a cookie
    size = sizeof(UInt32);
    err = AudioFileGetPropertyInfo(aqp.playFile, kAudioFilePropertyMagicCookieData, &size, NULL);
    
    if (err == noErr && size) {
        char cookie[size];
        memset(cookie, 0, size);
        NSLog(@"setting audio cookie, size %i", size);
        err = AudioFileGetProperty(aqp.playFile, kAudioFilePropertyMagicCookieData, &size, cookie);
        err = AudioQueueSetProperty(aqp.queue, kAudioQueueProperty_MagicCookie, cookie, size);
        if (err != noErr)
            NSLog(@"** setting audio queue cookie failed (%i)", err);
    }

    err = AudioFileGetPropertyInfo(aqp.playFile, kAudioFilePropertyChannelLayout, &size, NULL);
    if (err == noErr && size > 0) {
        AudioChannelLayout *acl = (AudioChannelLayout *)malloc(size);
        
        OSStatus err1 = AudioFileGetProperty(aqp.playFile, kAudioFilePropertyChannelLayout, &size, acl);
        OSStatus err2 = AudioQueueSetProperty(aqp.queue, kAudioQueueProperty_ChannelLayout, acl, size);
        free(acl);
        
        if (err1 || err2) NSLog(@"** failed to set channel layout (%i, %i)", err1, err2);
    }

    g_pcmState.state = TwtwPCMIsPriming;
    g_pcmState.didComplete = NO;
    g_pcmState.aqPlayer = malloc(sizeof(TwtwAQPlayer));
    memcpy(g_pcmState.aqPlayer, &aqp, sizeof(TwtwAQPlayer));

    // prime the queue with some data before starting
    g_pcmState.aqPlayer->currentPacket = 0;
    int i;
    for (i = 0; i < kNumberPlayBuffers; i++) {
        AudioQueueAllocateBuffer(aqp.queue, bufferByteSize, &(g_pcmState.aqPlayer->buffers[i]));
        
        //AudioQueueEnqueueBuffer(aqp.queue, g_pcmState.aqPlayer->buffers[i], 0, NULL);
        aqPlay_BufferHandler(NULL, aqp.queue, g_pcmState.aqPlayer->buffers[i]);
    }

    // set volume
    Float32 volume = 1.0;
    AudioQueueSetParameter(aqp.queue, kAudioQueueParam_Volume, volume);


    g_pcmState.callbacks = callbacks;
    g_pcmState.cbData = cbData;

    /*
    UInt32 numFramesPrepared = 0;
    err = AudioQueuePrime(aqp.queue, 0, &numFramesPrepared);
    if (err != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "AudioQueuePrime"));
        goto bail;
    } else
        NSLog(@"frames primed: %i", numFramesPrepared);
    */
        
    AudioQueueAddPropertyListener(aqp.queue, kAudioQueueProperty_IsRunning, aqPlay_PropertyListener, NULL);
    
    
    g_pcmState.state = TwtwPCMIsPlaying;
    g_pcmState.aqPlayer->queueStartTime = CFAbsoluteTimeGetCurrent();
    g_pcmState.aqPlayer->queueStopTime = g_pcmState.aqPlayer->queueStartTime + duration;
    
    err = AudioQueueStart(aqp.queue, NULL);
    
    if (err != noErr) {
        NSLog(@"** failed: %@", printableOSStatusError(err, "AudioQueueStart"));
    } else {
        okToPlay = YES;

        // I can't figure out how to make the queue play for the correct duration without using a timer
        CFRunLoopTimerRef playTimer = CFRunLoopTimerCreate(NULL,
                                                      g_pcmState.aqPlayer->queueStartTime + 0.5,
                                                      0.1,  // interval
                                                      0,    // flags (ignored)
                                                      0,    // order (ignored)
                                                      aqPlayback_TimerCallback,
                                                      NULL);
                                                          
        g_pcmState.timer = playTimer;
        CFRunLoopAddTimer(CFRunLoopGetMain(), playTimer, kCFRunLoopCommonModes);

        /*
		do {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, FALSE);
		} //while (g_pcmState.state == TwtwPCMIsPlaying);	
          while (g_pcmState.didComplete == NO);
        
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 4.0, FALSE);
        
        stopAQPlayback();
        */
    }


bail:
    if ( !okToPlay) {
        AudioQueueDispose(aqp.queue, TRUE);
        AudioFileClose(aqp.playFile);
        
        if (g_pcmState.aqPlayer)
            free(g_pcmState.aqPlayer);
    
        memset(&g_pcmState, 0, sizeof(TwtwPCMState));
        
        return -1;
    }
    else {
        return 0;
    }
}


void twtw_audio_pcm_stop ()
{
    if (g_pcmState.state == TwtwPCMIsRecording) {
        stopAQRecording();
    }
}
