/*
 *  twtw-maemo.c
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

#include <time.h>
#include <sys/time.h>
#include <locale.h>
#include <glib.h>

#include "twtw-maemo.h"
#include "twtw-document.h"
#include "twtw-audio.h"
#include "twtw-camera.h"
#include "twtw-filesystem.h"

#include <hildon/hildon-program.h>
#include <hildon/hildon-note.h>
#include <hildon/hildon-banner.h>
#include <hildon/hildon-defines.h>
#include <hildon/hildon-file-system-model.h>
#include <hildon/hildon-file-chooser-dialog.h>
#include <libosso.h>
#include <gdk/gdkkeysyms.h>

#include <gst/gst.h>
//#include <glib/gi18n-lib.h>
#include <libgnomevfs/gnome-vfs.h>

#include <gtk/gtklabel.h>

#include <ogg/ogg.h>
#include <oggz/oggz.h>


#define TWTW_APP_NAME "20:20"
#define TWTW_APP_VERSION_STR "0.2.0"
#define TWTW_OSSO_SERVICE_NAME "com.anioni.twentytwenty"
#define TWTW_OSSO_SERVICE_OBJECT "/com/anioni/twentytwenty"
#define TWTW_OSSO_SERVICE_IFACE "com.anioni.twentytwenty"


#define TWTW_FILE_EXT_INCL_DOT  ".oggtw"
#define TWTW_FILE_EXT_INCL_DOT_LEN  6


typedef enum {
  MENU_FILE_OPEN = 1,
  MENU_FILE_SAVE = 2,
  MENU_FILE_QUIT = 3,
  MENU_PAGE_CLEAR = 4
} MenuActionCode;


static gboolean g_mouseDown = FALSE;
static GtkWindow *g_mainWindow = NULL;



double twtw_absolute_time_get_current()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    long long secs = tv.tv_sec;
    long usecs = tv.tv_usec;
    double f = (double)secs + ((double)usecs / 1000000.0);
    
    //printf("%s: %lld / %ld  -> %.3f\n", __func__, secs, usecs, f);
    return f;
}



static void runOpenDialog(gpointer data)
{
    GtkWidget* dialog = NULL;
    gchar *tmpfile = NULL;
    gchar *selected = NULL;
    gchar *basename;
    gdouble len = -1.0;

    dialog = hildon_file_chooser_dialog_new_with_properties(
              GTK_WINDOW(g_mainWindow), 
              "action", GTK_FILE_CHOOSER_ACTION_OPEN,
              "file-system-model", NULL,
              "local-only", TRUE,
              NULL
              );

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        selected = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }

    if (dialog != NULL && GTK_IS_WIDGET(dialog)) {
        gtk_widget_destroy(dialog);
    }

    if (NULL == selected) /* no file selected */
        return;

    printf("going to open path: %s\n", selected);
    
    TwtwBook *book = NULL;
    int result = twtw_book_create_from_path_utf8 (selected, strlen(selected), &book);
    if (result == 0) {
        printf("OK! got book, going to set as active\n");
        twtw_set_active_document (book);
        
        
    }
    else {
        printf("** failed to open file (path %s; result %i)\n", selected, result);
    }

    g_free(selected);
}

static void runSaveDialog(gpointer data)
{
    GtkWidget* dialog = NULL;
    gchar *tmpfile = NULL;
    gchar *selected = NULL;
    gchar *basename;
    gdouble len = -1.0;

    dialog = hildon_file_chooser_dialog_new_with_properties(
                  GTK_WINDOW(g_mainWindow), 
                  "action", GTK_FILE_CHOOSER_ACTION_SAVE,
                  "file-system-model", NULL,
                  "local-only", TRUE,
                  NULL
                  );

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        selected = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }

    if (dialog != NULL && GTK_IS_WIDGET(dialog)) {
        gtk_widget_destroy(dialog);
    }

    if (NULL == selected) /* no file selected */
        return;
        
    // append file extension
    size_t pathLen = strlen(selected);
    size_t extLen = TWTW_FILE_EXT_INCL_DOT_LEN;
    if (pathLen < extLen || 0 != memcmp(selected+pathLen-extLen, TWTW_FILE_EXT_INCL_DOT, extLen)) {
        gchar *newPath = g_strconcat(selected, TWTW_FILE_EXT_INCL_DOT, NULL);
        g_free(selected);
        selected = newPath;
    }

    TwtwBook *book = twtw_active_document ();
    
    printf("going to save to path: %s (book %p)\n", selected, book);
    
    int result = twtw_book_write_to_path_utf8 (book, selected, strlen(selected));
    
    if (result == 0) {
        printf("save OK!\n");
    }
    else {
        printf("** failed to save file (path %s; result %i)\n", selected, result);
    }

    g_free(selected);
}


GstElement *g_pipeline = NULL;

static void busEOSMessageReceived (GstBus * bus, GstMessage * message, gpointer data)
{
  /* stop playback and free pipeline */
  gst_element_set_state (g_pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT(g_pipeline));
  g_pipeline = NULL;
}

static int simpleWAVPlay(const char *soundPath)
{
  GstElement *filesrc;
  GstBus *bus;
  GError *error = NULL;

  //testing: soundPath = "/home/user/MyDocs/page00.wav";

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
  
  printf("%s: ok!\n", __func__);
  
  /* start playback */
  gst_element_set_state (g_pipeline, GST_STATE_PLAYING);
  return 0;

error:
  gst_object_unref (GST_OBJECT(g_pipeline));
  g_pipeline = NULL;
  return -1;  
}



static void twtwDocChanged(gint notifID, TwtwMaemoAppData *appdata)
{
    g_assert(appdata);

    if (notifID == TWTW_NOTIF_DOCUMENT_REPLACED) {
        twtw_set_active_document_page_index (0);  // go to first page
    }

    twtw_canvas_queue_full_redraw(appdata->drawingArea);
    
    if (appdata) {
        TwtwPage *page = twtw_active_document_page ();
        gint soundLen = twtw_page_get_sound_duration_in_seconds (page);
        
        if (0 && appdata->infoLabel) {
            char str[64];
            sprintf(str, "sound duration: %i secs", soundLen);
            
            g_object_set(appdata->infoLabel, "label", str, NULL);
        }
    }
}



// private function for notifying page about new audio recording
extern void twtw_page_ui_did_record_pcm_with_file_size (TwtwPage *page, gint fileSize);



static gboolean g_isRec = FALSE;
static gboolean g_isPlay = FALSE;

static void myAudioCompletedCallback(int status, TwtwMaemoAppData *appdata)
{
    printf("%s: %i, %p\n", __func__, status, appdata);
    g_return_if_fail(appdata);
    
    if (status == TWTW_AUDIOSTATUS_REC) {
        TwtwPage *page = twtw_active_document_page ();
        const char *audioPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
        
        gboolean isFile = FALSE;
        size_t audioFileSize = twtw_filesys_get_file_size_utf8 (audioPath, strlen(audioPath), &isFile);
    
        twtw_page_ui_did_record_pcm_with_file_size (page, audioFileSize);

        printf("did record new audio; data size is %i bytes (isFile %i)\n", (int)audioFileSize, isFile);
    }
    
    g_isRec = FALSE;
    g_isPlay = FALSE;
    twtw_canvas_set_active_audio_state(appdata->drawingArea, 0);
    
    twtw_canvas_queue_audio_status_redraw(appdata->drawingArea);
}

static void myAudioInProgressCallback(int status, double timeInSecs, TwtwMaemoAppData *appdata)
{
    g_return_if_fail(appdata);
    printf("%s: %i,  %.3f\n", __func__, status, timeInSecs);
    
    twtw_canvas_set_active_audio_position(ceil(timeInSecs));
    
    twtw_canvas_queue_audio_status_redraw(appdata->drawingArea);
}


static void stopAllAudioAction(GtkWidget *widget, TwtwMaemoAppData *appdata)
{
    twtw_audio_pcm_stop ();
    g_isRec = FALSE;
    g_isPlay = FALSE;
    twtw_canvas_set_active_audio_state(appdata->drawingArea, 0);
}

static void recButtonClicked(GtkButton *button, TwtwMaemoAppData *appdata)
{
    TwtwPage *page = twtw_active_document_page ();
    const char *soundPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
    //const char *soundPath = "/home/user/testrec.wav";
    
    TwtwAudioCallbacks callbacks = { myAudioCompletedCallback, myAudioInProgressCallback };
    
    printf("%s: %i, rec dst path is %s\n", __func__, g_isRec, soundPath);
    
    if ( !g_isRec && soundPath) {
        twtw_audio_pcm_record_to_path_utf8 (soundPath, strlen(soundPath), 20, callbacks, appdata);
        twtw_canvas_set_active_audio_state(appdata->drawingArea, TWTW_AUDIOSTATUS_REC);
        g_isRec = TRUE;
    } else {
        stopAllAudioAction (button, appdata);
    }
}

static void playButtonClicked(GtkButton *button, TwtwMaemoAppData *appdata)
{
    //const char *soundPath = "/home/user/testrec.wav";
    const char *soundPath = NULL;
    
    TwtwAudioCallbacks callbacks = { myAudioCompletedCallback, myAudioInProgressCallback };
    
    TwtwPage *page = twtw_active_document_page ();
    gint soundLen = twtw_page_get_sound_duration_in_seconds (page);

    if (soundLen > 0) {
        soundPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
    }
    
    printf("%s (appdata %p): %i; path %s\n", __func__, appdata, g_isPlay, soundPath);
    
    if (soundPath && strlen(soundPath) > 1 && !g_isPlay) {
        twtw_audio_pcm_play_from_path_utf8 (soundPath, strlen(soundPath), callbacks, appdata);
        g_isPlay = TRUE;
        twtw_canvas_set_active_audio_state(appdata->drawingArea, TWTW_AUDIOSTATUS_PLAY);
    } else {
        stopAllAudioAction (button, appdata);
    }
}


static void saveAction(GtkWidget *widget, TwtwMaemoAppData *appdata)
{
    runSaveDialog(NULL);
}

static void openAction(GtkWidget *widget, TwtwMaemoAppData *appdata)
{
    runOpenDialog(NULL);
}


static void cameraFinishedCallback(int status, TwtwYUVImage *yuvImage, TwtwMaemoAppData *appdata)
{
    // a new photo was taken, so apply it on this document page
    
    TwtwPage *page = twtw_active_document_page ();
    twtw_page_copy_yuv_photo(page, yuvImage);

    printf("%s: %i, image %p, appdata %p, docpage %p\n", __func__, status, yuvImage, appdata, page);
        
    twtw_yuv_image_destroy(yuvImage);
    
    // redraw the canvas
    twtw_canvas_queue_full_redraw(appdata->drawingArea);
}

static void cameraAction(GtkWidget *widget, TwtwMaemoAppData *appdata)
{
    g_return_if_fail(appdata);
    g_return_if_fail(appdata->drawingArea);
    
    stopAllAudioAction(widget, appdata);
    
    TwtwCameraCallbacks camCallbacks;
    memset(&camCallbacks, 0, sizeof(camCallbacks));
    
    camCallbacks.cameraFinishedFunc = cameraFinishedCallback;
    camCallbacks.widgetButtonPressCallbackID = appdata->drawingAreaButtonPressCallbackID;
    camCallbacks.widgetButtonReleaseCallbackID = appdata->drawingAreaButtonReleaseCallbackID;
    
    twtw_camera_start_preview (appdata->drawingArea, camCallbacks, appdata);
}

static void clearPageAction(GtkWidget *widget, TwtwMaemoAppData *appdata)
{
    g_return_if_fail(appdata);
    
    stopAllAudioAction(widget, appdata);
    
    TwtwPage *page = twtw_active_document_page ();
    twtw_page_clear_all_data (page);
    
    twtw_canvas_queue_full_redraw(appdata->drawingArea);
}

typedef struct {
    MenuActionCode itemCode;
    TwtwMaemoAppData *appdata;
} MenuItemData;

// signal handler for the menu item selections
static void menuItemActivated(GtkMenuItem *mi, gpointer data)
{
    MenuItemData *menuData = (MenuItemData *)data;
    g_return_if_fail(menuData);
    g_return_if_fail(menuData->appdata);
    MenuActionCode aCode = menuData->itemCode;

    switch(aCode) {
    case MENU_FILE_OPEN:
      g_print("Selected open\n");
      openAction(mi, menuData->appdata);
      break;
    case MENU_FILE_SAVE:
      g_print("Selected save\n");
      saveAction(mi, menuData->appdata);
      break;
    case MENU_FILE_QUIT:
      g_print("Selected quit\n");
      gtk_main_quit();
      break;
    case MENU_PAGE_CLEAR:
      clearPageAction(mi, menuData->appdata);
      break;
    default:
      g_warning("unknown menu action code %i\n", aCode);
    }
}

static GtkMenuItem* buildMenuItem(const gchar *labelText)
{
    GtkLabel* label;
    GtkMenuItem* mi;
    label = g_object_new(GTK_TYPE_LABEL,
        "label", labelText,     /* GtkLabel property */
        "xalign", (gfloat)0.0,  /* GtkMisc property */
        NULL);

    mi = g_object_new(GTK_TYPE_MENU_ITEM, "child", label, NULL);
    return mi;
}

static void buildMenu(HildonProgram *program, TwtwMaemoAppData *appdata)
{
  GtkMenu* menu;
  GtkMenuItem* miOpen;
  GtkMenuItem* miSave;
  GtkMenuItem* miSep, *miSep2;
  GtkMenuItem* miQuit;
  GtkMenuItem *miClear;

  miOpen = buildMenuItem("Open");
  miSave = buildMenuItem("Save");
  miQuit = buildMenuItem("Quit");
  miClear = buildMenuItem("Clear This Page");
  miSep = g_object_new(GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);
  miSep2 = g_object_new(GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);

  menu = g_object_new(GTK_TYPE_MENU, NULL);

  g_object_set(menu,
    "child", miClear,
    "child", miSep2,
    "child", miOpen,
    "child", miSave,
    "child", miSep,
    "child", miQuit,
    NULL);

  hildon_program_set_common_menu(program, menu);

  MenuItemData *mdata;
  
  mdata = g_malloc0(sizeof(MenuItemData));
  mdata->appdata = appdata;
  mdata->itemCode = MENU_FILE_OPEN;
  g_signal_connect(G_OBJECT(miOpen), "activate", G_CALLBACK(menuItemActivated), mdata);
  
  mdata = g_malloc0(sizeof(MenuItemData));
  mdata->appdata = appdata;
  mdata->itemCode = MENU_FILE_SAVE;
  g_signal_connect(G_OBJECT(miSave), "activate", G_CALLBACK(menuItemActivated), mdata);

  mdata = g_malloc0(sizeof(MenuItemData));
  mdata->appdata = appdata;
  mdata->itemCode = MENU_FILE_QUIT;  
  g_signal_connect(G_OBJECT(miQuit), "activate", G_CALLBACK(menuItemActivated), mdata);

  mdata = g_malloc0(sizeof(MenuItemData));
  mdata->appdata = appdata;
  mdata->itemCode = MENU_PAGE_CLEAR;
  g_signal_connect(G_OBJECT(miClear), "activate", G_CALLBACK(menuItemActivated), mdata);

  gtk_widget_show_all(GTK_WIDGET(menu));
}


static gboolean drawingAreaButtonPressEvent(GtkWidget *widget, GdkEventButton *event, TwtwMaemoAppData *appdata)
{
    if (event->button == 1) {
	    char text[64]="";

        g_mouseDown = TRUE;

        //GdkRectangle redrawRect = { 0, 0, 0, 0 };

        twtw_canvas_mousedown(widget, event->x, event->y);  //, &redrawRect);

        /*
        if (redrawRect.width > 0 && redrawRect.height > 0) {
            gtk_widget_queue_draw_area (widget,
                               redrawRect.x, redrawRect.y,  redrawRect.width, redrawRect.height);
                                //0, 0, widget->allocation.width, widget->allocation.height);
	    }*/
	    
	    if (0 && appdata->infoLabel) {
        	g_snprintf(text, 64, "Clicked point (%d,%d)", (int)event->x, (int)event->y);
        	g_object_set(appdata->infoLabel, "label", text, NULL);
        }
    }
    return TRUE;
}

static gboolean drawingAreaMotionNotifyEvent(GtkWidget *widget, GdkEventMotion *event, TwtwMaemoAppData *appdata)
{
    if (g_mouseDown) {
        twtw_canvas_mousedragged(widget, event->x, event->y);
    }
    return TRUE;
}

static gboolean drawingAreaButtonReleaseEvent(GtkWidget *widget, GdkEventButton *event, TwtwMaemoAppData *appdata)
{
    if (event->button == 1) {
        g_mouseDown = FALSE;

        twtw_canvas_mouseup(widget, event->x, event->y);
    }
    return TRUE;
}



static gboolean drawingAreaExposeEvent(GtkWidget *widget, GdkEventExpose *event, TwtwMaemoAppData *appdata)
{
    GdkWindow *gdkWindow = event->window;
    cairo_t *cr = gdk_cairo_create(gdkWindow);
    
    if (widget->allocation.width > 0 && widget->allocation.height > 0) {
        twtw_canvas_set_rect(0, 0, widget->allocation.width, widget->allocation.height);
    }

    ///printf("%s: (%i, %i) - (%i, %i)\n", __func__, event->area.x, event->area.y, event->area.width, event->area.height);

    twtw_canvas_draw(cr, event->area.x, event->area.y, event->area.width, event->area.height);

    //cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    //cairo_set_source_rgba(cr, 0.9, 0.8, 0.0, 1.0);
    //cairo_paint(cr);

    cairo_destroy(cr);
    
    return TRUE;
}


static gboolean drawingAreaConfigureEvent(GtkWidget *widget, GdkEventConfigure *event, TwtwMaemoAppData *appdata)
{
    GdkWindow *gdkWindow = widget->window;
    g_return_val_if_fail(gdkWindow != NULL, TRUE);

    //printf("%s: %i, %i, %p\n", __func__, widget->allocation.width, widget->allocation.height, gdkWindow);

    // set up canvas stuff now that the widget is known to be valid

    twtw_canvas_did_acquire_drawable(widget->allocation.width, widget->allocation.height, GDK_DRAWABLE(gdkWindow) );

    TwtwCanvasActionCallbacks actionCb;
    memset(&actionCb, 0, sizeof(actionCb));
    actionCb.recSoundAction = recButtonClicked;
    actionCb.playSoundAction = playButtonClicked;
    actionCb.cameraAction = cameraAction;
    actionCb.sendDocumentAction = saveAction;
    actionCb.closeAppAction = gtk_main_quit;

    twtw_canvas_set_action_callbacks(widget, &actionCb, appdata);

    return TRUE;
}


// hardware keys
static gboolean keyPressedEvent(GtkWidget *widget, GdkEventKey *event, TwtwMaemoAppData *appdata)
{
    // all buttons should stop audio
    stopAllAudioAction(widget, appdata);

    // in camera preview mode, all hardware buttons should be handled by the camera functions
    if (twtw_camera_hw_button_pressed (event->keyval)) {
        return TRUE;
    }

    int page = twtw_active_document_page_index ();

    switch (event->keyval) {
/*    case HILDON_HARDKEY_UP:
        hildon_banner_show_information(GTK_WIDGET(window), NULL, "Navigation Key Up");
        return TRUE;

    case HILDON_HARDKEY_DOWN:
        hildon_banner_show_information(GTK_WIDGET(window), NULL, "Navigation Key Down");
        return TRUE;
*/

    case HILDON_HARDKEY_LEFT:
    case HILDON_HARDKEY_DECREASE:
        
        if (page > 0) {
            twtw_set_active_document_page_index (page - 1);
        }
        return TRUE;

    case HILDON_HARDKEY_RIGHT:
    case HILDON_HARDKEY_INCREASE:
    
        if (page < 19) {
            twtw_set_active_document_page_index (page + 1);
        }
        return TRUE;
                
/*
    case HILDON_HARDKEY_SELECT:
        hildon_banner_show_information(GTK_WIDGET(window), NULL, "Navigation Key select");
        return TRUE;

    case HILDON_HARDKEY_FULLSCREEN:
        hildon_banner_show_information(GTK_WIDGET(window), NULL, "Full screen");
        return TRUE;

    case HILDON_HARDKEY_ESC:
        hildon_banner_show_information(GTK_WIDGET(window), NULL, "Cancel/Close");
        return TRUE;
*/
    }
    return FALSE;
}



/* Callback for normal D-BUS messages */
static gint dbusReqHandler(const gchar *interface, const gchar *method,
                           GArray *arguments, gpointer data,
                           osso_rpc_t *retval)
{
    TwtwMaemoAppData *appdata = (TwtwMaemoAppData *)data;
    osso_rpc_t val;

    g_return_val_if_fail(method != NULL, OSSO_ERROR);
    
    //osso_system_note_infoprint(appdata->osso_context, method, retval);
    
    printf("twtw dbus req handler: method '%s'\n", method);

	if (appdata->infoLabel) {
	    char text[65];
        g_snprintf(text, 64, "dbus: %s", method);
        g_object_set(appdata->infoLabel, "label", text, NULL);
    }

    if (g_ascii_strcasecmp(method, "mime_open") == 0) {
        if (arguments == NULL) {
            fprintf(stderr, "dbus req handler arguments == NULL\n");
            return OSSO_ERROR;
        }
        val = g_array_index(arguments, osso_rpc_t, 0);
        if ((val.type == DBUS_TYPE_STRING) && (val.value.s != NULL)) {
            gchar *documentPath = val.value.s;
            
            char infoText[128];
            g_snprintf(infoText, 128, "open: %s", documentPath);

            int len = strlen(documentPath);
            if (strlen(documentPath) > 7 && 0 == memcmp(documentPath, "file://", 7)) {
                gchar *dirPath = g_malloc0(len-5);
                memcpy(dirPath, documentPath+7, len-7);
                
                TwtwBook *book = NULL;
                gint result = twtw_book_create_from_path_utf8 (dirPath, strlen(dirPath), &book);

                g_free(dirPath);

                if (result == 0 && book) {
                    twtw_set_active_document (book);

                    g_snprintf(infoText, 128, "open success! (%p)", book);

                    // success!
                    retval->type = DBUS_TYPE_BOOLEAN;
                    retval->value.b = TRUE;
                    return OSSO_OK;                                
                } else {
                    printf("** failed to load document: path '%s'", val.value.s);
                    g_snprintf(infoText, 128, "err %i (%s)", result, dirPath);
                }            
            }
            else {
                fprintf(stderr, "unsupported URI type for mime_open: %s", documentPath);
                g_snprintf(infoText, 128, "unsupported URI: %s", documentPath);
            }
            
        	if (appdata->infoLabel) {
                g_object_set(appdata->infoLabel, "label", infoText, NULL);
            }            
        }
    }

    else if (g_ascii_strcasecmp(method, "top_application") == 0) {
        ///gtk_window_present(GTK_WINDOW(mainview->data->program));

        /* success */
        retval->type = DBUS_TYPE_BOOLEAN;
        retval->value.b = TRUE;
        return OSSO_OK;
    }
    
    osso_rpc_free_val(retval);
    return OSSO_OK;
}


int main(int argc, char *argv[])
{
    TwtwMaemoAppData *appdata;
    GtkWidget *vbox;    
    // version number
    guint major = 0, minor = 0, micro = 0, nano = 0;


    gchar *documentPathToLoad = NULL;

    if (argc > 1) {
        int n;
        for (n = 1; n < argc; n++) {
            char *arg = argv[n];
            printf("arg %i: '%s'\n", n, arg);
            size_t len = strlen(arg);
            size_t extLen = TWTW_FILE_EXT_INCL_DOT_LEN;
            if (len > extLen && 0 == memcmp(arg+len-extLen, TWTW_FILE_EXT_INCL_DOT, extLen)) {
                documentPathToLoad = g_strdup(arg);
            }
        }
    }

    // init locale
    //setlocale(LC_ALL, "");
    //bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    //bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    //textdomain(GETTEXT_PACKAGE);

    // init threading
	g_thread_init(NULL);

    // the app data
    appdata = g_new0(TwtwMaemoAppData, 1);

    /*
    if (osso_mime_set_cb(appData->osso,
             (osso_mime_cb_f *)  (&maemo_recorder_mime_open),
             (gpointer) appData
             ) != OSSO_OK) {
        ULOG_CRIT("osso_mime_set_cb() failed");
    }
    */

    gst_init(&argc, &argv);
    gst_version (&major, &minor, &micro, &nano);
    printf("\ngst version: %u.%u.%u.%u\n", major, minor, micro, nano);
    
    gnome_vfs_init();

	/* Initialize GTK+ */
	gtk_init(&argc, &argv);

	/* Create HildonProgram and set application name */
	appdata->program = HILDON_PROGRAM(hildon_program_get_instance());
	g_set_application_name(TWTW_APP_NAME);


    // osso init
    appdata->osso = osso_initialize(TWTW_OSSO_SERVICE_NAME, TWTW_APP_VERSION_STR, FALSE, NULL);
    if (appdata->osso == NULL) {
        printf("** osso_initialize() failed\n");
        //exit(1);
    }

    // dbus handler
    osso_return_t ossoRes;
    ossoRes = osso_rpc_set_cb_f(appdata->osso, 
                               TWTW_OSSO_SERVICE_NAME, 
                               TWTW_OSSO_SERVICE_OBJECT, 
                               TWTW_OSSO_SERVICE_IFACE,
                               dbusReqHandler, appdata);

    if (ossoRes != OSSO_OK) {
        g_print("** error setting D-BUS callback (%d)\n", ossoRes);
        //return OSSO_ERROR;
    }
    



	/* Create the toplevel HildonWindow */
	appdata->window = HILDON_WINDOW(hildon_window_new());
    hildon_program_add_window(appdata->program, appdata->window);
    
    g_mainWindow = (GtkWindow *)appdata->window;

	/* Connect destroying of the main window to gtk_main_quit */
	g_signal_connect(G_OBJECT(appdata->window), "delete_event", G_CALLBACK(gtk_main_quit), NULL);

    // connect hardware key events
    g_signal_connect(G_OBJECT(appdata->window), "key_press_event", G_CALLBACK(keyPressedEvent), appdata);

    // create main menu
    buildMenu(appdata->program, appdata);


    /* Create the drawing area widget */
    appdata->drawingArea = gtk_drawing_area_new();

    gtk_signal_connect (GTK_OBJECT (appdata->drawingArea), "expose-event", (GtkSignalFunc) drawingAreaExposeEvent, appdata);
    gtk_signal_connect (GTK_OBJECT (appdata->drawingArea), "configure-event", (GtkSignalFunc) drawingAreaConfigureEvent, appdata);
    gtk_signal_connect (GTK_OBJECT (appdata->drawingArea), "motion-notify-event", (GtkSignalFunc) drawingAreaMotionNotifyEvent, appdata);

    appdata->drawingAreaButtonPressCallbackID = g_signal_connect (G_OBJECT (appdata->drawingArea), "button-press-event", G_CALLBACK(drawingAreaButtonPressEvent), appdata);
    appdata->drawingAreaButtonReleaseCallbackID = g_signal_connect (G_OBJECT (appdata->drawingArea), "button-release-event", G_CALLBACK(drawingAreaButtonReleaseEvent), appdata);

    //gtk_widget_set_size_request(appdata->drawingArea, 800, 450);

    gtk_widget_set_events (appdata->drawingArea, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
             | GDK_BUTTON_RELEASE_MASK
             | GDK_POINTER_MOTION_MASK
        );

  
#if 0
  /* Create the Label for showing information */
  appdata->infoLabel = (GtkWidget*)gtk_object_new(GTK_TYPE_LABEL, 
  		"label", "(debug info)", 
		NULL);
		
  GtkWidget *infoBox = gtk_hbox_new(FALSE, 0);
  GtkWidget *recButton = gtk_button_new_with_label("(rec)");
  GtkWidget *playButton = gtk_button_new_with_label("(play)");
  GtkWidget *camButton = gtk_button_new_with_label("(camera)");  
  GtkWidget *openButton = gtk_button_new_with_label("(open)");
  GtkWidget *saveButton = gtk_button_new_with_label("(save)");
  GtkWidget *quitButton = gtk_button_new_with_label("(exit)");
  gtk_box_pack_start(GTK_BOX(infoBox), recButton, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(infoBox), playButton, FALSE, FALSE, 0);  
  gtk_box_pack_start(GTK_BOX(infoBox), camButton, FALSE, FALSE, 0);  
  gtk_box_pack_start(GTK_BOX(infoBox), openButton, FALSE, FALSE, 0);  
  gtk_box_pack_start(GTK_BOX(infoBox), saveButton, FALSE, FALSE, 0);  
  gtk_box_pack_start(GTK_BOX(infoBox), appdata->infoLabel, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(infoBox), quitButton, FALSE, FALSE, 0);
  
  gtk_signal_connect (GTK_OBJECT (recButton), "clicked", (GtkSignalFunc) recButtonClicked, appdata);
  gtk_signal_connect (GTK_OBJECT (playButton), "clicked", (GtkSignalFunc) playButtonClicked, appdata);
  gtk_signal_connect (GTK_OBJECT (camButton), "clicked", (GtkSignalFunc) cameraAction, appdata);
  gtk_signal_connect (GTK_OBJECT (openButton), "clicked", (GtkSignalFunc) openAction, appdata);
  gtk_signal_connect (GTK_OBJECT (saveButton), "clicked", (GtkSignalFunc) saveAction, appdata);
  gtk_signal_connect (GTK_OBJECT (quitButton), "clicked", (GtkSignalFunc) gtk_main_quit, appdata);


  /* Create a layout box for the window since it can only hold one widget */
  vbox = gtk_vbox_new(FALSE, 0);

  /* Add the vbox as a child to the window */
  gtk_container_add(GTK_CONTAINER(appdata->window), vbox); 

  /* Pack the label and drawing_area into the VBox. */
  gtk_box_pack_end(GTK_BOX(vbox), infoBox, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), appdata->drawingArea, TRUE, TRUE, 0);
  
#else
    // add the canvas directly under the window
    gtk_container_add(GTK_CONTAINER(appdata->window), appdata->drawingArea);
#endif

    twtw_audio_init ();
    twtw_camera_init ();

    // create the initial 20:20 document
    gboolean didLoad = FALSE;
    if (documentPathToLoad) {
        TwtwBook *book = NULL;
        gint result = twtw_book_create_from_path_utf8 (documentPathToLoad, strlen(documentPathToLoad), &book);
        if (result == 0 && book) {
            twtw_set_active_document (book);
            didLoad = TRUE;
        } else {
            printf("** failed to load document: path '%s'", documentPathToLoad);
        }
    }
    if ( !didLoad) {
        twtw_active_document ();    
    }
    
    twtw_add_active_document_notif_callback (twtwDocChanged, appdata);

    // run app main loop
    gtk_widget_show_all(GTK_WIDGET(appdata->window));
    
    
    gtk_window_fullscreen(GTK_WINDOW(appdata->window));
    
    gtk_main();
    
    gtk_window_unfullscreen(GTK_WINDOW(appdata->window));
    
    // exit
    if (g_pipeline) {
      gst_element_set_state (g_pipeline, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT(g_pipeline));
    }    
    
    twtw_camera_deinit();
    twtw_audio_deinit();
    twtw_remove_active_document_notif_callback (twtwDocChanged);
    
    twtw_book_clean_temp_files (twtw_active_document ());
    
    if (appdata->osso)
        osso_deinitialize(appdata->osso);

    return 0;
}
