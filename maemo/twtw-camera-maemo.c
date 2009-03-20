/*
 *  twtw-camera-maemo.c
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

#include "twtw-camera.h"

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/interfaces/xoverlay.h>
//#include <hildon/hildon-program.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>


#ifdef __arm__
/* The device by default supports only
 * vl4l2src for camera and xvimagesink
 * for screen */
 #define VIDEO_SRC "v4l2src"
 #define VIDEO_SINK "xvimagesink"
#else
/* These are for the X86 SDK. Xephyr doesn't
 * support XVideo extension, so the application
 * must use ximagesink. The video source depends
 * on driver of your Video4Linux device so this
 * may have to be changed */
 #define VIDEO_SRC "v4lsrc"
 #define VIDEO_SINK "ximagesink"
#endif

#define VIDEO_SRC_WIDTH  320
#define VIDEO_SRC_HEIGHT 240




typedef struct _TwtwCamData {
    int status;

    GstElement *pipeline;
    GstElement *screenSink;
    guint bufferCallbackID;
    
    GtkWidget *widget;
    
    TwtwCameraCallbacks cbFuncs;
    void *cbData;
    
    guint widgetCameraPressCallbackID;
    
    TwtwYUVImage *yuvPhoto;
} TwtwCamData;

static TwtwCamData g_camData = { 0, NULL, NULL };


static void destroyPipeline(TwtwCamData *camData);
static void exitCameraPreview(TwtwCamData *camData);





// ----- callbacks / bus messages ------


TwtwYUVImage *twtw_yuv_image_create_from_gst_buffer(GstBuffer *gstBuffer)
{
    if ( !gstBuffer) return NULL;
    
    unsigned char *imageData = (unsigned char *) GST_BUFFER_DATA(gstBuffer);
    int dataSize = GST_BUFFER_SIZE(gstBuffer);
    
    g_return_val_if_fail(imageData, NULL);
    g_return_val_if_fail(dataSize > 0, NULL);
    
    //GstCaps *caps = gst_buffer_get_caps(gstBuffer);
    //printf("gst buffer caps: %s\n", gst_caps_to_string(caps));
    
    // on Maemo, the default pixel format is UYVY,
    // so assume that throughout
    
    /*unsigned short *yuvData = (unsigned short *)imageData;
    int i;
    for (i = 0; i < 10; i++) {
        int v1 = *(yuvData + 320*10 + i*16);
        int v2 = *(yuvData + 320*10 + i*16 + 1);
        printf(".. %i: %x / %x\n", i, v1, v2);
    }*/
    
    TwtwYUVImage *newimg = g_malloc0(sizeof(TwtwYUVImage));
    
    newimg->w = TWTW_CAM_IMAGEWIDTH;
    newimg->h = TWTW_CAM_IMAGEHEIGHT;
    newimg->rowBytes = newimg->w * 2;
    newimg->pixelFormat = GST_MAKE_FOURCC('U', 'Y', 'V', 'Y');

    newimg->buffer = g_malloc(newimg->rowBytes * newimg->h);
    
    // clear with black if necessary
    int srcYOff = 0;
    int dstYOff = 0;
    int rowsToCopy;
    if (newimg->h > VIDEO_SRC_HEIGHT) {
        memset(newimg->buffer, 0, newimg->rowBytes * newimg->h);
        
        dstYOff = (newimg->h - VIDEO_SRC_HEIGHT) / 2;
        rowsToCopy = VIDEO_SRC_HEIGHT;
    }
    else {
        srcYOff = (VIDEO_SRC_HEIGHT - newimg->h) / 2;
        rowsToCopy = newimg->h;
    }
        
    unsigned char *srcBuf = imageData + newimg->rowBytes * srcYOff;
    unsigned char *dstBuf = newimg->buffer + newimg->rowBytes * dstYOff;
    
    memcpy(dstBuf, srcBuf, newimg->rowBytes * rowsToCopy);

    printf("yuv buffer: datasize %i, h %i -> rowbytes %i\n", dataSize, newimg->h, newimg->rowBytes);
    
    return newimg;
}


/* This callback will be registered to the image sink
 * after user requests a photo */
static gboolean bufferProbeCallback(
		GstElement *image_sink,
		GstBuffer *buffer, GstPad *pad, TwtwCamData *camData)
{
	GstMessage *message;
	gchar *messageName = "photo-taken";
	/* This is the raw RGB-data that image sink is about
	 * to discard */
	//unsigned char *imageBuffer = (unsigned char *) GST_BUFFER_DATA(buffer);

    if (camData->yuvPhoto)
        twtw_yuv_image_destroy(camData->yuvPhoto);
    
    camData->yuvPhoto = twtw_yuv_image_create_from_gst_buffer(buffer);

    printf("%s: got image buffer %p\n", __func__, camData->yuvPhoto);
    
	/*
	if(!create_jpeg(imageBuffer))
		message_name = "photo-failed";
	else
		message_name = "photo-taken";
	*/
	
	/* Disconnect the handler so no more photos
	 * are taken */
	g_signal_handler_disconnect(G_OBJECT(image_sink), camData->bufferCallbackID);
	
	/* Create and send an application message which will be
	 * catched in the bus watcher function. This has to be
	 * sent as a message because this callback is called in
	 * a gstreamer thread and calling GUI-functions here would
	 * lead to X-server synchronization problems */
	message = gst_message_new_application(GST_OBJECT(camData->pipeline),
			                                gst_structure_new(messageName, NULL));
			
	gst_element_post_message(camData->pipeline, message);
	
	/* Returning TRUE means that the buffer can is OK to be
	 * sent forward. When using fakesink this doesn't really
	 * matter because the data is discarded anyway */
	return TRUE;
}

/* Callback that gets called when user clicks the "Take photo" button */
static void takePhotoAction(GtkWidget *widget, TwtwCamData *camData)
{
	GstElement *imageSink;
	
	/* Get the image sink element from the pipeline */
	imageSink = gst_bin_get_by_name(GST_BIN(camData->pipeline), "image_sink");
			
	/* Display a note to the user */
	//hildon_banner_show_information(GTK_WIDGET(camData->window), NULL, "Taking Photo");

    // disconnect click from drawing area
    g_signal_handler_disconnect (G_OBJECT(camData->widget), camData->widgetCameraPressCallbackID);
    camData->widgetCameraPressCallbackID = 0;

	/* Connect the "handoff"-signal of the image sink to the
	 * callback. This gets called whenever the sink gets a
	 * buffer it's ready to pass forward on the pipeline */
	camData->bufferCallbackID = g_signal_connect( G_OBJECT(imageSink), "handoff",
                                			      G_CALLBACK(bufferProbeCallback), camData);
                                			      
    camData->status = TWTW_CAMSTATUS_SHOT_TAKEN;
}

static gboolean cameraButtonPressEvent(GtkWidget *widget, GdkEventButton *event, TwtwCamData *camData)
{
    takePhotoAction(widget, camData);

    return TRUE;
}


/* Callback that gets called whenever pipeline's message bus has
 * a message */
static void gstBusCallback(GstBus *bus, GstMessage *message, TwtwCamData *camData)
{
	gchar *message_str;
	const gchar *message_name;
	GError *error;
	
	/* Report errors to the console */
	if(GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
	{	
		gst_message_parse_error(message, &error, &message_str);
		g_error("GST error: %s\n", message_str);
		g_free(error);
		g_free(message_str);
		
		exitCameraPreview(camData);
	}
	
	/* Report warnings to the console */
	if(GST_MESSAGE_TYPE(message) == GST_MESSAGE_WARNING)
	{	
		gst_message_parse_warning(message, &error, &message_str);
		g_warning("GST warning: %s\n", message_str);
		g_free(error);
		g_free(message_str);
	}

	/* See if the message type is GST_MESSAGE_APPLICATION which means
	 * thet the message is sent by the client code (this program) and
	 * not by gstreamer. */
	if(GST_MESSAGE_TYPE(message) == GST_MESSAGE_APPLICATION)
	{
		/* Get name of the message's structure */
		message_name = gst_structure_get_name(gst_message_get_structure(message));
		
		/* The hildon banner must be shown in here, because the bus callback is
		 * called in the main thread and calling GUI-functions in gstreamer threads
		 * usually leads to problems with X-server */
		
		/* "photo-taken" message means that the photo was succefully taken
		 * and saved and message is shown to user */
		if(0 == strcmp(message_name, "photo-taken")) {
			//hildon_banner_show_information(GTK_WIDGET(camData->window), NULL, "Photo taken");
		}
		printf("%s: message is %s\n", __func__, message_name);		
		
		exitCameraPreview(camData);
	}
}




// ----- GST pipeline ----

static gboolean createPipeline(TwtwCamData *camData)
{
	GstElement *pipeline, *camera_src, *screen_sink, *image_sink;
	GstElement *screen_queue, *image_queue;
	GstElement *csp_filter, *image_filter, *tee;
	GstCaps *caps;
	GstBus *bus;
	
	/* Create pipeline and attach a callback to it's
	 * message bus */
	pipeline = gst_pipeline_new("test-camera");

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_watch(bus, (GstBusFunc)gstBusCallback, camData);
	gst_object_unref(GST_OBJECT(bus));
	
	/* Save pipeline to the TwtwCamData structure */
	camData->pipeline = pipeline;
	
	/* Create elements */
	/* Camera video stream comes from a Video4Linux driver */
	camera_src = gst_element_factory_make(VIDEO_SRC, "camera_src");
	/* Colorspace filter is needed to make sure that sinks understands
	 * the stream coming from the camera */
	csp_filter = gst_element_factory_make("ffmpegcolorspace", "csp_filter");
	/* Tee that copies the stream to multiple outputs */
	tee = gst_element_factory_make("tee", "tee");
	/* Queue creates new thread for the stream */
	screen_queue = gst_element_factory_make("queue", "screen_queue");
	/* Sink that shows the image on screen. Xephyr doesn't support XVideo
	 * extension, so it needs to use ximagesink, but the device uses
	 * xvimagesink */
	screen_sink = gst_element_factory_make(VIDEO_SINK, "screen_sink");
	/* Creates separate thread for the stream from which the image
	 * is captured */
	image_queue = gst_element_factory_make("queue", "image_queue");
	/* Filter to convert stream to use format that the gdkpixbuf library
	 * can use */
	image_filter = gst_element_factory_make("ffmpegcolorspace", "image_filter");
	/* A dummy sink for the image stream. Goes to bitheaven */
	image_sink = gst_element_factory_make("fakesink", "image_sink");

	/* Check that elements are correctly initialized */
	if(!(pipeline && camera_src && screen_sink && csp_filter && screen_queue
		&& image_queue && image_filter && image_sink))
	{
		g_critical("Couldn't create pipeline elements");
		return FALSE;
	}

	/* Set image sink to emit handoff-signal before throwing away
	 * it's buffer */
	g_object_set(G_OBJECT(image_sink),
			"signal-handoffs", TRUE, NULL);
	
	/* Add elements to the pipeline. This has to be done prior to
	 * linking them */
	gst_bin_add_many(GST_BIN(pipeline), camera_src, csp_filter,
			tee, screen_queue, screen_sink, image_queue,
			image_filter, image_sink, NULL);
	
	/* Specify what kind of video is wanted from the camera */
	caps = gst_caps_new_simple("video/x-raw-yuv",
			"width", G_TYPE_INT,  VIDEO_SRC_WIDTH,
			"height", G_TYPE_INT, VIDEO_SRC_HEIGHT,
			NULL);
			

	/* Link the camera source and colorspace filter using capabilities
	 * specified */
	if(!gst_element_link_filtered(camera_src, csp_filter, caps))
	{
		return FALSE;
	}
	gst_caps_unref(caps);
	
	/* Connect Colorspace Filter -> Tee -> Screen Queue -> Screen Sink
	 * This finalizes the initialization of the screen-part of the pipeline */
	if(!gst_element_link_many(csp_filter, tee, screen_queue, screen_sink, NULL))
	{
		return FALSE;
	}

	/* gdkpixbuf requires 8 bits per sample which is 24 bits per
	 * pixel */
	caps = gst_caps_new_simple("video/x-raw-yuv",
			"width", G_TYPE_INT,  VIDEO_SRC_WIDTH,
			"height", G_TYPE_INT, VIDEO_SRC_HEIGHT,
			//"bpp", G_TYPE_INT, 24,
			//"depth", G_TYPE_INT, 24,
			"framerate", GST_TYPE_FRACTION, 15, 1,
			NULL);
			
	/* Link the image-branch of the pipeline. The pipeline is
	 * ready after this */
	if(!gst_element_link_many(tee, image_queue, image_filter, NULL)) return FALSE;
	if(!gst_element_link_filtered(image_filter, image_sink, caps)) return FALSE;

	gst_caps_unref(caps);
	
	/* As soon as screen is exposed, window ID will be advised to the sink */
	//g_signal_connect(camData->screen, "expose-event", G_CALLBACK(expose_cb),
	//		 screen_sink);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

    camData->screenSink = screen_sink;

	return TRUE;
}

/* Destroy the pipeline on exit */
static void destroyPipeline(TwtwCamData *camData)
{
	/* Free the pipeline. This automatically also unrefs all elements
	 * added to the pipeline */
	gst_element_set_state(camData->pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(camData->pipeline));
}


static void exitCameraPreview(TwtwCamData *camData)
{
    if (camData->status != 0) {
        destroyPipeline(camData);
        
        if (camData->cbFuncs.widgetButtonPressCallbackID) {
            g_signal_handler_unblock (G_OBJECT(camData->widget), camData->cbFuncs.widgetButtonPressCallbackID);
            camData->cbFuncs.widgetButtonPressCallbackID = 0;
        }
        if (camData->cbFuncs.widgetButtonReleaseCallbackID) {
            g_signal_handler_unblock (G_OBJECT(camData->widget), camData->cbFuncs.widgetButtonReleaseCallbackID);
            camData->cbFuncs.widgetButtonReleaseCallbackID = 0;
        }

        if (camData->cbFuncs.cameraFinishedFunc) {
            camData->cbFuncs.cameraFinishedFunc(TWTW_CAMSTATUS_SHOT_TAKEN, camData->yuvPhoto, camData->cbData);
        } else
            twtw_yuv_image_destroy(camData->yuvPhoto);

        memset(camData, 0, sizeof(*camData));
    }
}


// ------- public API ------------

void twtw_camera_init ()
{

}

void twtw_camera_deinit ()
{
    if (g_camData.status != 0) {
        destroyPipeline(&g_camData);
    }
    memset(&g_camData, 0, sizeof(g_camData));
}

void twtw_camera_start_preview (void *presentationObj, TwtwCameraCallbacks callbacks, void *userData)
{
    GtkWidget *theWidget = GTK_WIDGET(presentationObj);
    g_return_if_fail(theWidget);

    if (g_camData.status != 0) {
        printf("camera is still busy, can't start\n");
        return;
    }
    
    memset(&g_camData, 0, sizeof(g_camData));
    g_camData.widget = theWidget;
    g_camData.cbFuncs = callbacks;
    g_camData.cbData = userData;
    
    if ( !createPipeline(&g_camData)) {
        printf("*** failed to create cam pipeline\n");
        memset(&g_camData, 0, sizeof(g_camData));
    }
    else {
        g_camData.status = TWTW_CAMSTATUS_PREVIEW;
    
        printf("%s: widget is %p, screenSink %p, window %p, XID %i\n", __func__, theWidget, g_camData.screenSink, theWidget->window, (int)GDK_WINDOW_XWINDOW(theWidget->window));
    
        if (g_camData.cbFuncs.widgetButtonPressCallbackID)
            g_signal_handler_block (G_OBJECT(g_camData.widget), g_camData.cbFuncs.widgetButtonPressCallbackID);
            
        if (g_camData.cbFuncs.widgetButtonReleaseCallbackID)
            g_signal_handler_block (G_OBJECT(g_camData.widget), g_camData.cbFuncs.widgetButtonReleaseCallbackID);

        g_camData.widgetCameraPressCallbackID = g_signal_connect (G_OBJECT (g_camData.widget), "button-press-event", G_CALLBACK(cameraButtonPressEvent), &g_camData);

    
        gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(g_camData.screenSink),
            				         GDK_DRAWABLE_XID(g_camData.widget->window));
    }
}


gboolean twtw_camera_hw_button_pressed (unsigned int buttonID)
{
    if (g_camData.status == TWTW_CAMSTATUS_PREVIEW) {
        takePhotoAction(NULL, &g_camData);
        return TRUE;
    }
    else
        return FALSE;
}

