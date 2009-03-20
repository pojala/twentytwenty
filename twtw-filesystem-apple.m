/*
 *  twtw-filesystem.c
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

#include "twtw-filesystem.h"
#import <Foundation/Foundation.h>
#import <stdio.h>


void twtw_filesys_generate_temp_dir_path_for_book (gint32 serialNo, char **outPath, size_t *outPathLen)
{
    g_return_if_fail (outPath && outPathLen);

    NSString *tempPath = NSTemporaryDirectory();  //@"/tmp/";
    
    if ([tempPath length] < 1)
        tempPath = @"/tmp/";
    else if ([tempPath characterAtIndex:[tempPath length]-1] != '/')
        tempPath = [tempPath stringByAppendingString:@"/"];
    
    tempPath = [NSString stringWithFormat:@"%@twentytwenty/book_%i/", tempPath, serialNo];
    
    const char *utf8Path = [tempPath UTF8String];
    size_t utf8Len = strlen(utf8Path);
    
    *outPath = g_malloc0(utf8Len + 1);
    *outPathLen = utf8Len;
    
    memcpy(*outPath, utf8Path, utf8Len);
    
    ///NSLog(@"%s: book serial is %i -\n  -> temp path '%@'", __func__, serialNo, tempPath);
    
    
    // create the directory
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:tempPath isDirectory:&isDir]) {
        [fileManager removeItemAtPath:tempPath error:NULL];  // Leopard / 10.5 API
    }

    NSError *error = nil;
    if ( ![fileManager createDirectoryAtPath:tempPath withIntermediateDirectories:YES attributes:nil error:&error]) {  // Leopard / 10.5 API
        NSLog(@"*** unable to create temp directory at path '%@':\n    error %@", tempPath, error);
    }
}


void twtw_filesys_clean_temp_files_at_path (const char *path, size_t pathLen)
{
    NSString *tempPath = [NSString stringWithUTF8String:path];
    if ([tempPath length] < 5)
        return;

    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:tempPath isDirectory:&isDir]) {
        [fileManager removeItemAtPath:tempPath error:NULL];  // Leopard / 10.5 API        
    }
    ///NSLog(@"%s: cleaning path %@", __func__, tempPath);
}

size_t twtw_filesys_get_file_size_utf8 (const char *path, size_t pathLen, gboolean *outIsValidFile)
{
    NSString *tempPath = [NSString stringWithUTF8String:path];
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if ( ![fileManager fileExistsAtPath:tempPath isDirectory:&isDir] || isDir) {
        if (outIsValidFile) *outIsValidFile = FALSE;
        return 0;
    }
    
    if (outIsValidFile) *outIsValidFile = TRUE;
    
    NSDictionary *fileAttributes = [fileManager fileAttributesAtPath:tempPath traverseLink:YES];
    NSNumber *fileSize = [fileAttributes objectForKey:NSFileSize];
    if (fileSize)
        return [fileSize longLongValue];
    else
        return 0;
}


FILE *twtw_open_readb_utf8(const char *path, size_t pathLen)
{
    FILE *file = fopen(path, "rb");
    return file;
}

FILE *twtw_open_writeb_utf8(const char *path, size_t pathLen)
{
    FILE *file = fopen(path, "wb");
    return file;
}


char *twtw_filesys_append_path_component (const char *basePath, const char *comp)
{
    g_return_val_if_fail (basePath, NULL);
    g_return_val_if_fail (comp, NULL);
    
    size_t baseLen = strlen(basePath);
    size_t compLen = strlen(comp);
    size_t newLen = baseLen + compLen + 1;
    
    char *newPath = g_malloc0(newLen + 1);
    
    memcpy(newPath, basePath, baseLen);
    
    int sepLen = (newPath[baseLen-1] != '/') ? 1 : 0;
    if (sepLen)
        newPath[baseLen] = '/';
    
    memcpy(newPath+baseLen+sepLen, comp, compLen);
    
    return newPath;
}



