/*
 *  twtw-filesystem.h
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

#ifndef _TWTW_FILESYS_H_
#define _TWTW_FILESYS_H_ 

#include "twtw-glib.h"
#include <stdio.h>


// all returned paths are allocated with g_malloc

#ifdef __cplusplus
extern "C" {
#endif

void twtw_filesys_generate_temp_dir_path_for_book (gint32 serialNo, char **outPath, size_t *outPathLen);
void twtw_filesys_generate_temp_path_for_single_file (char **outPath, size_t *outPathLen);

// path can be a directory or a single file
void twtw_filesys_clean_temp_files_at_path (const char *path, size_t pathLen);

size_t twtw_filesys_get_file_size_utf8 (const char *path, size_t pathLen, gboolean *outIsValidFile);

FILE *twtw_open_readb_utf8(const char *path, size_t pathLen);
FILE *twtw_open_writeb_utf8(const char *path, size_t pathLen);

// path string util (so that the app doesn't need to know the system path component separator)
char *twtw_filesys_append_path_component (const char *basePath, const char *str);

// generating temp file copies (used by sound undo)
gboolean twtw_filesys_make_uniquely_named_copy_of_file_at_path (const char *path, size_t pathLen,
                                                                char **outPath, size_t *outPathLen);

gboolean twtw_filesys_copy_file (const char *sourcePath, size_t sourcePathLen, const char *dstPath, size_t dstPathLen, gboolean allowReplace);

#ifdef __cplusplus
}
#endif

#endif
