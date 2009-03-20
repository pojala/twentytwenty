/*
 *  twtw-camera.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 29.11.2008.
 *  Copyright 2008 Lacquer oy/ltd. All rights reserved.
 *
 */

#ifndef _TWTW_CAMERA_H_
#define _TWTW_CAMERA_H_


#include <stdlib.h>
#include <string.h>
#include "twtw-document.h"



enum {
    TWTW_CAMSTATUS_OFF = 0,
    TWTW_CAMSTATUS_PREVIEW = 1,
    TWTW_CAMSTATUS_SHOT_TAKEN = 2,
    TWTW_CAMSTATUS_CANCELLED = 3
}; 


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TwtwCameraCallbacks {
    // the YUVImage returned here is owned by the callback and needs to be freed with twtw_yuv_image_destroy
    void (*cameraFinishedFunc) (int, TwtwYUVImage *, void *);
    
    // this is not currently called
    void (*cameraPreviewFunc) (int, void *, void *);
    
    // gtk signal IDs.
    // these are currently needed by the camera preview
    // so that these signals can be reconnected after the preview is complete
    unsigned int widgetButtonPressCallbackID;
    unsigned int widgetButtonReleaseCallbackID;
} TwtwCameraCallbacks;


void twtw_camera_init ();
void twtw_camera_deinit ();

void twtw_camera_start_preview (void *presentationObj,   // preview surface, i.e. the drawing area GtkWidget on Maemo
                                TwtwCameraCallbacks callbacks, void *userData);

// hw buttons should take a photo in camera mode, so this call
// returns TRUE if a photo was taken
gboolean twtw_camera_hw_button_pressed (unsigned int buttonID);

#ifdef __cplusplus
}
#endif

#endif

