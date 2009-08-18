//
//  TwtwInfoPanelController.m
//  TwentyTwenty
//
//  Created by Pauli Ojala on 15.3.2009.
//  Copyright 2009 Pauli Olavi Ojala. All rights reserved.
//
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

#import "TwtwInfoPanelController.h"
#import "twtw-document.h"
#import "twtw-filesystem.h"
#import "twtw-editing.h"
#import "twtw-photo.h"


extern void twtw_notify_about_doc_change (const gint notifID);

extern const char *twtw_book_get_temp_path (TwtwBook *book);


static void twtwDocChanged(gint notifID, void *userData)
{
    [(id)userData updateInfo];
}


@implementation TwtwInfoPanelController

+ (id)sharedController
{
    static id s_ctrl = nil;
    if ( !s_ctrl) {
        s_ctrl = [[[self class] alloc] initWithWindowNibName:@"InfoPanel"];
    }
    return s_ctrl;
}

- (void)awakeFromNib
{
    [_clearVectorsButton setEnabled:NO];
    [_clearAudioButton setEnabled:NO];
    [_clearPhotoButton setEnabled:NO];
    
    twtw_add_active_document_notif_callback (twtwDocChanged, self);
    
    [self updateInfo];
}


- (IBAction)computeFileSize:(id)sender
{
    const char *dirPath = twtw_book_get_temp_path (twtw_active_document());
    NSAssert(dirPath && strlen(dirPath) > 0, @"no temp path");
    
    NSString *tempFilePath = [[NSString stringWithUTF8String:dirPath] stringByAppendingPathComponent:@"__infoPanelTestSave.oggtw"];
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if ([fileManager fileExistsAtPath:tempFilePath isDirectory:&isDir]) {
        [fileManager removeItemAtPath:tempFilePath error:NULL];  // Leopard / 10.5 API        
    }

    ///NSLog(@"%s: path %@", __func__, tempFilePath);
    
    const char *utf8Path = [tempFilePath UTF8String];
    size_t utf8Len = strlen(utf8Path);
        
    NSString *sizeInfoStr;
    int result = twtw_book_write_to_path_utf8 (twtw_active_document(), utf8Path, utf8Len);
        
    if (result != 0) {
        sizeInfoStr = [NSString stringWithFormat:@"(Unable to compute file size; err %i)", result];
    } else {
        NSDictionary *fileAttrs = [fileManager fileAttributesAtPath:tempFilePath traverseLink:YES];
        unsigned long long fileSize = [[fileAttrs objectForKey:NSFileSize] unsignedLongLongValue];
        
        sizeInfoStr = [NSString stringWithFormat:@"%llu bytes", fileSize];
        
        [fileManager removeItemAtPath:tempFilePath error:NULL];  // Leopard / 10.5 API        
    }
    
    [_fileSizeField setStringValue:sizeInfoStr];
}

- (IBAction)clearVectors:(id)sender
{
    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    
    TwtwCurveListArray *prevValue = g_malloc(sizeof(TwtwCurveListArray));
    prevValue->count = twtw_page_get_curves_count (page);
    prevValue->array = twtw_page_copy_all_curves (page);
    
    TwtwAction undoAction = { TWTW_ACTION_SET_CURVES, pageIndex, NULL,  prevValue, (TwtwActionDestructorFuncPtr)twtw_destroy_curvelist_array };
    twtw_undo_push_action (&undoAction);
    
    twtw_page_clear_curves (page);
    
    twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_PAGE_MODIFIED);
}

- (IBAction)clearPhoto:(id)sender
{
    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    
    TwtwYUVImage *prevValue = twtw_yuv_image_copy (twtw_page_get_yuv_photo (page));
    
    TwtwAction undoAction = { TWTW_ACTION_SET_BG_PHOTO, pageIndex, NULL,  prevValue, (TwtwActionDestructorFuncPtr)twtw_yuv_image_destroy };
    twtw_undo_push_action (&undoAction);

    twtw_page_clear_photo (page);
    
    twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_PAGE_MODIFIED);
}

- (IBAction)clearAudio:(id)sender
{
    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    
    const char *currAudioPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
    char *copiedAudioPath = NULL;
    twtw_filesys_make_uniquely_named_copy_of_file_at_path (currAudioPath, strlen(currAudioPath), &copiedAudioPath, NULL);
    
    TwtwAction undoAction = { TWTW_ACTION_SET_PCM_SOUND, pageIndex, NULL,  copiedAudioPath, NULL };
    twtw_undo_push_action (&undoAction);
    
    twtw_page_clear_audio (page);
    
    twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_PAGE_MODIFIED);
}

- (IBAction)setMetadataField:(id)sender
{
    [[sender window] makeFirstResponder:nil];

    TwtwBook *book = twtw_active_document ();
    
    NSString *str = [sender stringValue];
    if ([str length] < 1)
        str = nil;
    
    switch ([sender tag]) {
        case 1: {
            char *prevValue = g_strdup(twtw_book_get_author (book));
        
            TwtwAction undoAction = { TWTW_ACTION_SET_AUTHOR, -1, NULL,  prevValue, NULL };
            twtw_undo_push_action (&undoAction);

            twtw_book_set_author (book, [str UTF8String]);            
            break;
        }
            
        case 2: {
            char *prevValue = g_strdup(twtw_book_get_title (book));
        
            TwtwAction undoAction = { TWTW_ACTION_SET_TITLE, -1, NULL,  prevValue, NULL };
            twtw_undo_push_action (&undoAction);

            twtw_book_set_title (book, [str UTF8String]);            
            break;
        }
    }
    
    twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_MODIFIED);
}

- (IBAction)setDocumentLocked:(id)sender
{
    TwtwBook *book = twtw_active_document ();
    gint32 flags = twtw_book_get_flags (book);
    
    gint32 *prevValue = g_malloc(sizeof(gint32));
    *prevValue = flags;
    
    TwtwAction undoAction = { TWTW_ACTION_SET_DOCFLAGS, -1, NULL,  prevValue, NULL };
    twtw_undo_push_action (&undoAction);

    BOOL isLocked = ([sender state] == NSOnState);
    uint32_t lockFlag = 0x01;
    
    if (isLocked)
        flags |= lockFlag;
    else
        flags &= ~lockFlag;
        
    twtw_book_set_flags (book, flags);
    
    twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_MODIFIED);
}


- (void)updateInfo
{
    TwtwPage *page = twtw_active_document_page ();
    
    [_fileSizeField setStringValue:@"(Not yet computed)"];
    
    [_clearPhotoButton setEnabled:(twtw_page_get_yuv_photo(page)) ? YES : NO];
    [_clearVectorsButton setEnabled:(twtw_page_get_curves_count(page) > 0) ? YES : NO];
    [_clearAudioButton setEnabled:(twtw_page_get_sound_duration_in_seconds(page) > 0) ? YES : NO];
    
    const char *authorStr = twtw_book_get_author (twtw_active_document());
    const char *titleStr = twtw_book_get_title (twtw_active_document());
    
    [_authorField setStringValue:(authorStr) ? [NSString stringWithUTF8String:authorStr] : @""];
    [_descField setStringValue:(titleStr) ? [NSString stringWithUTF8String:titleStr] : @""];
    
    uint32_t flags = twtw_book_get_flags(twtw_active_document());
    uint32_t lockFlag = 0x01;
    [_lockedCheckbox setState:((flags & lockFlag) ? NSOnState : NSOffState)];
    [_lockedCheckbox setNeedsDisplay:YES];
}

@end
