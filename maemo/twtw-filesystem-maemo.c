/*
 *  twtw-filesystem-maemo.c
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

#include "twtw-filesystem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <math.h>


static gchar *file2uri(const gchar *filename)
{
    if ( !filename)
        return NULL;

    if (g_str_has_prefix(filename, "file://"))
        return g_strdup(filename);

    //return g_strconcat("file://", filename, NULL);
    return g_filename_to_uri(filename, NULL, NULL);
}


size_t twtw_filesys_get_file_size_utf8 (const char *path, size_t pathLen, gboolean *outIsValidFile)
{
    FILE *file = fopen(path, "rb");
    size_t size = 0;
    gboolean isValid = FALSE;
    
    if (file) {
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        isValid = TRUE;

        fclose(file);
    }
    if (outIsValidFile) *outIsValidFile = isValid;
    return size;
}

FILE *twtw_open_readb_utf8(const char *path, size_t pathLen)
{
    FILE *file = fopen(path, "rb");
    return file;
}

FILE *twtw_open_writeb_utf8(const char *path, size_t pathLen)
{
    GnomeVFSFileSize len = 0;
    guint32 encoding = 0;
    gint written = -1;
    GnomeVFSURI *uri = NULL;
    gchar *text_uri;
    GnomeVFSResult res;
    GnomeVFSHandle *filehandle;

    text_uri = file2uri(path);
    uri = gnome_vfs_uri_new(text_uri);

    res = gnome_vfs_create_uri(&filehandle, uri, 
                    GNOME_VFS_OPEN_WRITE, 
                    0 /* exclusive */,
                    S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH  /* perm */);

    FILE *file = NULL;

    if (res != GNOME_VFS_OK) {
        printf("** %s() - gnome_vfs_create() failed for '%s': %s\n", G_STRFUNC, gnome_vfs_uri_to_string(uri, GNOME_VFS_URI_HIDE_NONE), gnome_vfs_result_to_string(res));
    } else {
        gnome_vfs_close(filehandle);
        
        file = fopen(path, "w+b");

        printf("did create file at %s (ptr %p)\n", text_uri, file);
    }
    
    gnome_vfs_uri_unref(uri);
    g_free(text_uri);

    return file;
}



void twtw_filesys_generate_temp_dir_path_for_book (gint32 serialNo, char **outPath, size_t *outPathLen)
{
    g_return_if_fail (outPath && outPathLen);

    char buf[64];
    if (serialNo > 0)
        g_snprintf(buf, 64, "book_%d", (int)serialNo);
    else
        g_snprintf(buf, 64, "book_m%d", (int)abs(serialNo));

    const char *tmpRootPath = "/home/user/MyDocs/.twbook-temp";  // I can't seem to figure out how to get /tmp to play nice with gstreamer's playback, so use this instead

    char *path = g_strconcat(tmpRootPath,  //"/tmp/twbook-temp/",
                                   "/", buf, NULL);

    *outPath = path;
    *outPathLen = strlen(path);

    gchar *text_uri;
    GnomeVFSResult res;
    GnomeVFSHandle *filehandle;

    guint perm = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH;

    text_uri = file2uri(tmpRootPath);
    
    res = gnome_vfs_make_directory(text_uri, perm);
    if (res != GNOME_VFS_OK) {
        // no problem here, the directory is expected to exist
        //printf("** failed to create temp root path: %s\n", gnome_vfs_result_to_string(res));
    }
    g_free(text_uri);

    
    text_uri = file2uri(path);
    
    res = gnome_vfs_make_directory(text_uri, perm);
    if (res != GNOME_VFS_OK) {
        printf("** failed to create temp path for book: %s\n(path: %s)\n", gnome_vfs_result_to_string(res), text_uri);
    }
    g_free(text_uri);

}


char *twtw_filesys_append_path_component (const char *basePath, const char *comp)
{
    g_return_val_if_fail (basePath, NULL);
    g_return_val_if_fail (comp, NULL);
    
    return g_strconcat(basePath, "/", comp, NULL);
}


typedef struct {
    gchar *baseURI;
    
    gchar **relFilePaths;
    gint relFilePathCount;
} VisitData;

static gboolean dirVFSVisitCallback(const gchar *rel_path, GnomeVFSFileInfo *info,
                                                           gboolean recursingWillLoop,
                                                           gpointer userdata,
                                                           gboolean *recurse)
{
    g_return_val_if_fail(userdata, FALSE);
    VisitData *myVisitData = (VisitData *)userdata;

    if (recursingWillLoop)
        return TRUE;  // file is in parent directory, skip it

    *recurse = TRUE;
    
    if (info->type == GNOME_VFS_FILE_TYPE_REGULAR) {
        int n = myVisitData->relFilePathCount;
        myVisitData->relFilePathCount++;
        myVisitData->relFilePaths = (myVisitData->relFilePaths == NULL) ? g_malloc(myVisitData->relFilePathCount * sizeof(void *))
                                                                        : g_realloc(myVisitData->relFilePaths, myVisitData->relFilePathCount * sizeof(void *));
                                                                        
        myVisitData->relFilePaths[n] = g_strdup(rel_path);
    }
    
    return TRUE;
}

void twtw_filesys_clean_temp_files_at_path (const char *path, size_t pathLen)
{
    g_return_if_fail(path);
    g_return_if_fail(pathLen > 0);
    
    gchar *text_uri;
    GnomeVFSResult res;
    
    text_uri = file2uri(path);
    
    printf("cleaning temp files - base uri is %s\n", text_uri);
    
    VisitData visitData;
    memset(&visitData, 0, sizeof(visitData));
    visitData.baseURI = text_uri;
    
    res = gnome_vfs_directory_visit (text_uri, 
                                     0 /*GNOME_VFS_FILE_INFO_NAME_ONLY*/,
                                     GNOME_VFS_DIRECTORY_VISIT_DEFAULT,
                                     dirVFSVisitCallback,
                                     &visitData);

    int i;
    for (i = 0; i < visitData.relFilePathCount; i++) {
        gchar *path = visitData.relFilePaths[i];
        
        gchar *fullURI = g_strconcat(visitData.baseURI, "/", path, NULL);
        
        gnome_vfs_unlink(fullURI);  // delete the file
        
        g_free(fullURI);
    }
    
    res = gnome_vfs_remove_directory (visitData.baseURI);
    if (res != GNOME_VFS_OK) {
        printf("** unable to remove temp dir: %s\n", visitData.baseURI);
    }
    
    g_free(text_uri);
}



