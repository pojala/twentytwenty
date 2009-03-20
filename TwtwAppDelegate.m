//
//  TwtwAppDelegate.m
//  TwentyTwenty
//
//  Created by Pauli Ojala on 25.11.2008.
//  Copyright 2008 Pauli Olavi Ojala. All rights reserved.
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

#import "TwtwAppDelegate.h"
#import "TwtwInfoPanelController.h"

#import "twtw-units.h"
#import "twtw-curves.h"
#import "twtw-document.h"

#include <oggz/oggz.h>
#include "skeleton.h"



@implementation TwtwAppDelegate


- (void)_test_oggWrite
{
    long serialno = 0;
    ogg_int64_t granulepos = 0;
    ogg_int64_t packetno = 0;
    
    NSString *path = @"/twtw-test.ogg";
    
    OGGZ *oggz;
    ogg_packet op;
    
    oggz = oggz_open([path UTF8String], OGGZ_WRITE);
    
    serialno = oggz_serialno_new(oggz);
    long skeletonSerialno = oggz_serialno_new(oggz);
    
    NSLog(@"%s: oggz %p; primary stream serialno %i; skeleton serial %i", __func__, oggz, serialno, skeletonSerialno);


    ogg_packet skeletonPacket;
    memset(&skeletonPacket, 0, sizeof(ogg_packet));
    {
        fishead_packet fp;
        memset(&fp, 0, sizeof(fp));
        fp.ptime_n = 419 * 1000;
        fp.ptime_d = 1000;
        fp.btime_n = 0;
        fp.btime_d = 1000;
        
        ogg_from_fishead(&fp, &skeletonPacket);
    }
    oggz_write_feed(oggz, &skeletonPacket, skeletonSerialno, OGGZ_FLUSH_AFTER, NULL);
    while ((oggz_write (oggz, 32)) > 0);
    
    _ogg_free(skeletonPacket.packet);
    memset(&skeletonPacket, 0, sizeof(ogg_packet));
    
    
    float fakeHeader[4] = { 0.1, 0.2, 155.0, 156.0 };
    op.packet = (unsigned char *)fakeHeader;
    op.bytes = 4 * sizeof(float);
    op.granulepos = granulepos;
    op.packetno = packetno;
    granulepos += 100;
    packetno++;
    op.b_o_s = 1;
    op.e_o_s = 0;
    
    oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
    while ((oggz_write (oggz, 32)) > 0);
    

    {
        fisbone_packet fp;
        memset(&fp, 0, sizeof(fp));
        fp.serial_no = serialno;
        fp.nr_header_packet = 1;
        fp.granule_rate_n = 100;
        fp.granule_rate_d = 1;
        fp.start_granule = 100;
        fp.preroll = 0;
        fp.granule_shift = 0;

        fisbone_add_message_header_field(&fp, "Content-Type", "application/twentytwenty");
        ogg_from_fisbone(&fp, &skeletonPacket);
    }    
    skeletonPacket.packetno = 1;
    skeletonPacket.e_o_s = 0;
       
    int writeRet = oggz_write_feed(oggz, &skeletonPacket, skeletonSerialno, OGGZ_FLUSH_AFTER, NULL);
    while ((oggz_write (oggz, 32)) > 0);
    
    NSLog(@"%s: skeleton fisbone writeret: %i", __func__, writeRet);
    
    _ogg_free(skeletonPacket.packet);
    memset(&skeletonPacket, 0, sizeof(ogg_packet));
    
    
    size_t bufSize = 90000;
    unsigned char buf[bufSize];
    memset(buf, 0, bufSize);
    
    int numPackets = 4;
    for (packetno = 1; packetno < numPackets; packetno++) {
        long n = 0;
    
        buf[0] = 'A' + (char)packetno;
        buf[1] = 'B' + (char)packetno;
        buf[2] = 'C' + (char)packetno;
        
        op.packet = buf;
        op.bytes = bufSize;
        op.granulepos = granulepos;
        op.packetno = packetno;
        
        op.b_o_s = (packetno == 0) ? 1 : 0;
        op.e_o_s = (packetno == numPackets-1) ? 1 : 0;
        
        oggz_write_feed(oggz, &op, serialno, OGGZ_FLUSH_AFTER, NULL);
        
        granulepos += 100;
        
        while ((n = oggz_write (oggz, 512)) > 0);
    }
    
    oggz_close(oggz);
}

- (void)applicationDidFinishLaunching:(NSNotification *)notif
{
    /*TwtwFixedNum u1b = FIXD_FROM_INT(162);
    TwtwFixedNum u2b = FIXD_FROM_FLOAT(100.17);
    TwtwFixedNum mulb = FIXD_QMUL(u1b, u2b);
    TwtwFixedNum sqrtresb = twtw_fixed_sqrt(u2b);
    TwtwFixedNum sqrtres2b = twtw_fixed_sqrt(FIXD_FROM_FLOAT(270.0));
    NSLog(@"  .. ver2: mulf: %.8f, sqrtx: %.8f; second: %8.f", TWTW_UNITS_TO_FLOAT(mulb), TWTW_UNITS_TO_FLOAT(sqrtresb), TWTW_UNITS_TO_FLOAT(sqrtres2b));
    */
    
    // create the initial document
    twtw_active_document ();
    
    [[TwtwInfoPanelController sharedController] showWindow:self];
    
    [_editorWindow makeKeyAndOrderFront:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    twtw_book_clean_temp_files (twtw_active_document());
}


- (BOOL)application:(NSApplication *)app openFile:(NSString *)path
{
    const char *utf8Path = [path UTF8String];
    size_t utf8Len = strlen(utf8Path);
    
    TwtwBook *book = NULL;
    
    int result = twtw_book_create_from_path_utf8 (utf8Path, utf8Len, &book);
    if (result == 0) {
        twtw_set_active_document (book);
        return YES;
    }
    
    return NO;
}


#pragma mark --- actions ---

- (IBAction)newDocument:(id)sender
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSWarningAlertStyle];
    [alert addButtonWithTitle:@"Create New"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:@"Warning"];
    [alert setInformativeText:@"The current document will be cleared out. Any unsaved changes will be lost. Are you sure?"];
    
    int alertReturn = [alert runModal];
    [alert release];
    
    if (alertReturn != NSAlertFirstButtonReturn)
        return;

    twtw_set_active_document (twtw_book_create ());
}


#define TWTW_FILE_EXTENSION @"oggtw"


- (IBAction)openDocument:(id)sender
{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    [oPanel setAllowsMultipleSelection:NO];
    int result = [oPanel runModalForDirectory:nil file:nil types:[NSArray arrayWithObjects:TWTW_FILE_EXTENSION, @"ogtw", @"ogx", nil]];
	
    if (result == NSOKButton) {
        NSArray *filesToOpen = [oPanel filenames];
        int count = [filesToOpen count];
        if (count > 0) {
            NSString *file = [filesToOpen objectAtIndex:0];
            [self application:NSApp openFile:file];
        }
    }
}

- (void)saveDocumentToPath:(NSString *)path
{
    if ([path length] < 1) return;

    ///NSLog(@"%s: path %@", __func__, path);
    
    const char *utf8Path = [path UTF8String];
    size_t utf8Len = strlen(utf8Path);
    
    int result = twtw_book_write_to_path_utf8 (twtw_active_document(), utf8Path, utf8Len);
    
    if (result != 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSWarningAlertStyle];
        [alert addButtonWithTitle:@"OK"];
        [alert setMessageText:@"File not saved"];
        [alert setInformativeText:[NSString stringWithFormat:@"There was an error saving the file.\n\n(Error code: %i)", result]];
    
        [alert runModal];
        [alert release];        
    }
}

- (IBAction)saveDocumentAs:(id)sender
{
	NSSavePanel *sp;
	int runResult;
	sp = [NSSavePanel savePanel];
	[sp setRequiredFileType:TWTW_FILE_EXTENSION];
	runResult = [sp runModalForDirectory:nil file:nil];
	
	if (runResult == NSOKButton) {
        [self saveDocumentToPath:[sp filename]];
    }
}

- (IBAction)saveDocument:(id)sender
{
    if (_prevSavePath) {
        [self saveDocumentToPath:_prevSavePath];
    } else {
        [self saveDocumentAs:sender];
    }
}



@end
