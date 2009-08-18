//
//  TwtwCanvasView.m
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

#import "TwtwCanvasView.h"
#import "TwtwInfoPanelController.h"
#import "twtw-editing.h"
#import "twtw-document.h"
#import "twtw-graphicscache.h"
#import "twtw-graphicscache-priv.h"
#import "twtw-audio.h"
#import "twtw-filesystem.h"


// private function for notifying page about new audio recording
extern void twtw_page_ui_did_record_pcm_with_file_size (TwtwPage *page, gint fileSize);

// implemented in twtw-document.c
void twtw_set_default_color_index (gint index);



static void myAudioCompletedCallback(int status, void *userData)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [(id)userData performSelectorOnMainThread:@selector(audioDidCompleteWithRetainedInfo:)
                    withObject:[[NSDictionary alloc] initWithObjectsAndKeys:
                                        [NSNumber numberWithInt:status], @"statusID",
                                        nil]
                    waitUntilDone:NO];
                    
    [pool release];
}

static void myAudioInProgressCallback(int status, double timeInSecs, void *userData)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [(id)userData performSelectorOnMainThread:@selector(audioInProgressWithRetainedInfo:)
                    withObject:[[NSDictionary alloc] initWithObjectsAndKeys:
                                        [NSNumber numberWithInt:status], @"statusID",
                                        [NSNumber numberWithDouble:timeInSecs], @"timeInSecs",
                                        nil]
                    waitUntilDone:NO];
                    
    [pool release];
}




static void twtwDocChanged(gint notifID, void *userData)
{
    if (notifID == TWTW_NOTIF_DOCUMENT_REPLACED) {
        twtw_set_active_document_page_index (0);  // go to first page
    }

    [(id)userData reloadDocument];
}


@implementation TwtwCanvasView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        twtw_audio_init ();    
    
        twtw_add_active_document_notif_callback (twtwDocChanged, self);
        
        twtw_set_size_for_shared_canvas_cache_surface (round(frame.size.width), round(frame.size.height));
        
        ///NSLog(@"canvas size is: %@", NSStringFromSize(frame.size));
        
        _bgCacheIsDirty = YES;

        _im_pens = [[NSImage imageNamed:@"KYNAT_640"] retain];
        _im_boombox = [[NSImage imageNamed:@"MANKKA_640"] retain];
        _im_cam = [[NSImage imageNamed:@"KAMERA_640"] retain];
        _im_leaf = [[NSImage imageNamed:@"PAPERIARKKI_640"] retain];
        _im_envelope = [[NSImage imageNamed:@"LAHETYS_640"] retain];
        
        _im_buttonglow = [[NSImage imageNamed:@"buttonglow"] retain];
    }
    return self;
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldBoundsSize
{
    [super resizeWithOldSuperviewSize:oldBoundsSize];
    
    NSSize canvasSize = NSMakeSize(TWTW_CANONICAL_CANVAS_WIDTH, round(TWTW_CANONICAL_CANVAS_WIDTH * 9.0 / 16.0));
    NSSize frameSize = [self frame].size;
    
    [self setBoundsSize:canvasSize];
    
    _zoomFactor = (double)frameSize.width / canvasSize.width;
    
    twtw_set_size_for_shared_canvas_cache_surface (round(frameSize.width), round(frameSize.height));
    _bgCacheIsDirty = YES;
}


//- (BOOL)isFlipped {
//    return YES; }
    
- (BOOL)acceptsFirstResponder {
    return YES; }
    
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent {
    return YES; }


- (void)_notifyOfUpdate
{
    //twtw_notify_about_doc_change (TWTW_NOTIF_DOCUMENT_MODIFIED);
    [[TwtwInfoPanelController sharedController] updateInfo];
}

- (void)reloadDocument
{
    _bgCacheIsDirty = YES;
    [self setNeedsDisplay:YES];
    [self _notifyOfUpdate];
}


#pragma mark --- audio ---

- (void)audioDidCompleteWithRetainedInfo:(id)info
{
    [info autorelease];    
    
    int status = [[info objectForKey:@"statusID"] intValue];
    
    if (status == TWTW_AUDIOSTATUS_REC) {
        TwtwPage *page = twtw_active_document_page ();
        const char *audioPath = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
        
        gboolean isFile = FALSE;
        size_t audioFileSize = twtw_filesys_get_file_size_utf8 (audioPath, strlen(audioPath), &isFile);
    
        twtw_page_ui_did_record_pcm_with_file_size (page, audioFileSize);

        NSLog(@"did record new audio; data size is %i bytes", (int)audioFileSize);
    }
    
    _currentAudioTime = 0;
    _audioState = 0;
    [self setNeedsDisplay:YES];
    [self _notifyOfUpdate];
}

- (void)audioInProgressWithRetainedInfo:(id)info
{
    [info autorelease];
    
    int status = [[info objectForKey:@"statusID"] intValue];
    #pragma unused(status)
    double timeInSecs = [[info objectForKey:@"timeInSecs"] doubleValue];
    
    _currentAudioTime = timeInSecs;
    [self setNeedsDisplay:YES];
}


- (void)recordAction:(id)sender
{
    if (_audioState == TWTW_AUDIOSTATUS_REC) {
        twtw_audio_pcm_stop ();
        _audioState = 0;
        return;  
    } else if (_audioState != 0) {
        return;
    }

    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    const char *path = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);
    NSAssert(path && strlen(path) > 0, @"path is null");

    // make a copy of previous audio and push it on the undo stack
    char *copiedAudioPath = NULL;
    twtw_filesys_make_uniquely_named_copy_of_file_at_path (path, strlen(path), &copiedAudioPath, NULL);
    TwtwAction undoAction = { TWTW_ACTION_SET_PCM_SOUND, pageIndex, NULL,  copiedAudioPath, NULL };
    twtw_undo_push_action (&undoAction);
    
    
    NSLog(@"starting audio recording, temp path is:\n    %s", path);
    
    TwtwAudioCallbacks callbacks;
    callbacks.audioCompletedFunc = myAudioCompletedCallback;
    callbacks.audioInProgressFunc = myAudioInProgressCallback;
        
    int secsToRecord = 20;
        
    if (0 == twtw_audio_pcm_record_to_path_utf8 (path, strlen(path), secsToRecord, callbacks, self))
        _audioState = TWTW_AUDIOSTATUS_REC;
}

- (void)playAction:(id)sender
{
    if (_audioState == TWTW_AUDIOSTATUS_PLAY) {
        twtw_audio_pcm_stop ();
        _audioState = 0;
        return;
    } else if (_audioState != 0) {
        return;
    }

    TwtwPage *page = twtw_active_document_page ();
    const char *path = twtw_page_get_temp_path_for_pcm_sound_utf8 (page);

    if ( !path || strlen(path) < 1)
        return;

    TwtwAudioCallbacks callbacks;
    callbacks.audioCompletedFunc = myAudioCompletedCallback;
    callbacks.audioInProgressFunc = myAudioInProgressCallback;
    
    if (0 == twtw_audio_pcm_play_from_path_utf8 (path, strlen(path), callbacks, self)) {
        _audioState = TWTW_AUDIOSTATUS_PLAY;
        
        NSLog(@"now playing PCM sound, temp path is:\n    %s", __func__, path);
    }
}

- (BOOL)audioIsBusy
{
    return (_audioState != 0) ? YES : NO;
}


#pragma mark --- image loading (camera) ---

- (void)_loadImageAsBGPhotoForCurrentPageFromPath:(NSString *)path
{
    NSImage *im = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
    if ( !im) return;
    
    // for RGB conversion, we'll draw the image into a CG bitmap context
    
    const int w = TWTW_CAM_IMAGEWIDTH;
    const int h = TWTW_CAM_IMAGEHEIGHT;
    const size_t rowBytes = w * 4;
    uint8_t *bitmapData = malloc(rowBytes * h);

    CGColorSpaceRef cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGContextRef tempContext = CGBitmapContextCreate(bitmapData, w, h,
                                                 8,  // bits per component
                                                 rowBytes,
                                                 cspace,
                                                 kCGImageAlphaNoneSkipLast);  // CG only supports 32-bit pixels, not 24
    CGColorSpaceRelease(cspace);
    
    CGContextSetInterpolationQuality(tempContext, kCGInterpolationHigh);
    
    NSGraphicsContext *tempNSCtx = [NSGraphicsContext graphicsContextWithGraphicsPort:tempContext flipped:NO];
    NSGraphicsContext *prevCtx = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:tempNSCtx];
    {
        double srcW = [im size].width;
        double srcH = [im size].height;
        double srcAsp = (double)srcW / srcH;
        double dstAsp = (double)w / h;
        NSRect srcRect;
        
        if (srcAsp >= dstAsp) {  // image is wider than destination, so crop at sides
            double d = srcAsp - dstAsp;
            srcRect = NSMakeRect(0.5*d*srcW,    0.0,
                                 (1.0-d)*srcW,  srcH);
        } else {  // image is taller than destination, so crop at top+bottom
            double d = dstAsp - srcAsp;
            srcRect = NSMakeRect(0.0,   0.5*d*srcH,
                                 srcW,  (1.0-d)*srcH);
        }
        
        [im drawInRect:NSMakeRect(0, 0, w, h)
              fromRect:srcRect
              operation:NSCompositeCopy
              fraction:1.0];
              
        // the bottom of the image currently gets cropped by the UI, so we can paint it with a solid color.
        // this is just a quick hack.
        [[NSColor colorWithDeviceRed:1.0 green:1.0 blue:1.0 alpha:1.0] set];
        [[NSBezierPath bezierPathWithRect:NSMakeRect(0, 0,  w, 0.1*h)] fill];
    }
    [NSGraphicsContext setCurrentContext:prevCtx];
    
    ///NSLog(@"did convert, orig size %@; corner pixel %i / %i / %i", NSStringFromSize([im size]), bitmapData[0], bitmapData[1], bitmapData[2]);
    
    TwtwYUVImage *yuvImage = twtw_yuv_image_create_from_rgb_with_default_size (bitmapData, rowBytes, TRUE);

    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    
    TwtwYUVImage *prevValue = twtw_yuv_image_copy (twtw_page_get_yuv_photo (page));
    TwtwAction undoAction = { TWTW_ACTION_SET_BG_PHOTO, pageIndex, NULL,  prevValue, (TwtwActionDestructorFuncPtr)twtw_yuv_image_destroy };
    twtw_undo_push_action (&undoAction);
    
    twtw_page_set_yuv_photo_copy (page, yuvImage);

    // done, clean up
    twtw_yuv_image_destroy(yuvImage);    
    CGContextRelease(tempContext);
    free(bitmapData);
}

- (void)cameraAction:(id)sender
{
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];
    [oPanel setAllowsMultipleSelection:NO];
    
    NSTextField *infoView = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 400, 36)];
    [infoView setDrawsBackground:NO];
    [infoView setBordered:NO];
    [infoView setBezeled:NO];
    [infoView setFont:[NSFont systemFontOfSize:11.0]];
    [infoView setStringValue:@"Choose an image file to be used as the background for this page.\n(The image will be cropped and converted to 20:20's internal format.)"];
    
    [oPanel setAccessoryView:[infoView autorelease]];
    
    int result = [oPanel runModalForDirectory:nil file:nil types:[NSImage imageTypes]];
	
    if (result == NSOKButton) {
        NSArray *filesToOpen = [oPanel filenames];
        int count = [filesToOpen count];
        if (count > 0) {
            NSString *file = [filesToOpen objectAtIndex:0];
            [self _loadImageAsBGPhotoForCurrentPageFromPath:file];
            
            [self reloadDocument];
        }
    }
}


#pragma mark --- events ---

- (void)moveToPreviousPageAction:(id)sender
{
    int page = twtw_active_document_page_index (index);

    if (page > 0)
        twtw_set_active_document_page_index (page - 1);
}

- (void)moveToNextPageAction:(id)sender
{
    int page = twtw_active_document_page_index (index);

    if (page < 19)
        twtw_set_active_document_page_index (page + 1);
}

- (void)sendDocumentAction:(id)sender
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert addButtonWithTitle:@"OK"];
    [alert setMessageText:@"Feature not available"];
    [alert setInformativeText:@"Sorry, direct email functionality is not currently implemented in this desktop version of 20:20. To save the document, choose Save from the File menu."];
    [alert runModal];
    [alert release];        
    
}

- (void)keyDown:(NSEvent *)event
{
    NSString *characters = [event characters];
    unichar character = [characters characterAtIndex:0];

    if (character == NSLeftArrowFunctionKey) {
        [self moveToPreviousPageAction:self];
    } else if (character == NSRightArrowFunctionKey) {
        [self moveToNextPageAction:self];
    }
}

static int colorIDFromPointInPencilsBox(int x, int y)
{
    int row = 0;
    int col = 0;
    const int pencilW = 18;
    if (y > 22) {
        row = 1;
        if (x <= 6) col = 0;
        else col = (x-6) / pencilW;
    } else {
        if (x <= 15) col = 0;
        else col = (x-15) / pencilW;
    }
    col = MIN(9, col);
    //printf("(%i, %i) --> col %i, row %i\n", x, y, col, row);
    return row*10 + col;
}

- (void)mouseDown:(NSEvent *)event
{
    NSRect bounds = [self bounds];
    NSPoint pos = [self convertPoint:[event locationInWindow] fromView:nil];
    
    ///NSLog(@"pos %@ -- recrect %@", NSStringFromPoint(pos), NSStringFromRect(_elemInfo.recButtonRect));
    
    if (NSMouseInRect(pos, _elemInfo.pencilsRect, NO)) {
        int x = round(pos.x - _elemInfo.pencilsRect.origin.x);
        int y = round(pos.y - _elemInfo.pencilsRect.origin.y);
        
        y = _elemInfo.pencilsRect.size.height - y;  // flip Y
        int colorID = colorIDFromPointInPencilsBox(x, y);
        
        twtw_set_default_color_index (colorID);
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.recButtonRect, NO)) {
        [self recordAction:self];
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.playButtonRect, NO)) {
        [self playAction:self];
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.cameraRect, NO)) {
        [self cameraAction:self];
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.leftPaperStackRect, NO)) {
        [self moveToPreviousPageAction:self];
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.rightPaperStackRect, NO)) {
        [self moveToNextPageAction:self];
        return;
    }
    else if (NSMouseInRect(pos, _elemInfo.envelopeRect, NO)) {
        [self sendDocumentAction:self];
        return;
    }
    
    // not on a button, so start drawing a curve.
    // view origin is bottom-left, so must flip Y for curves.
    pos.y = bounds.size.height - 1 - pos.y;

    TwtwCurveList *newCL = twtw_editing_curvelist_create_with_start_cursor_point (TwtwMakeFloatPoint(pos.x, pos.y));
    _editedCL = newCL;

    while (1) {
        event = [[self window] nextEventMatchingMask:(NSLeftMouseDraggedMask | NSLeftMouseUpMask)];
        if ( !event)
            break;
        
        pos = [self convertPoint:[event locationInWindow] fromView:nil];
        pos.y = bounds.size.height - 1 - pos.y;

        if ([event type] == NSLeftMouseUp)
            break;
            
        twtw_editing_curvelist_add_cursor_point (newCL, TwtwMakeFloatPoint(pos.x, pos.y));
        [self setNeedsDisplay:YES];
    }
    
    twtw_editing_curvelist_finish_at_cursor_point (newCL, TwtwMakeFloatPoint(pos.x, pos.y));
    
    // add created curve to active document
    TwtwPage *page = twtw_active_document_page ();
    gint pageIndex = twtw_active_document_page_index ();
    
    twtw_page_add_curve (page, newCL);
    
    TwtwAction undoAction = { TWTW_ACTION_DELETE_LAST_CURVE, pageIndex, NULL,  NULL, NULL };
    twtw_undo_push_action (&undoAction);
    
    _editedCL = nil;
    twtw_curvelist_destroy (newCL);
    
    _bgCacheIsDirty = YES;
    [self setNeedsDisplay:YES];
    [self _notifyOfUpdate];
}


#pragma mark --- drawing ---

static void drawCurveList(CGContextRef ctx, TwtwCurveList *curve)
{
    int segCount = twtw_curvelist_get_segment_count (curve);
    TwtwCurveSegment *segs = twtw_curvelist_get_segment_array (curve);

    unsigned char *rgbPalette = twtw_default_color_palette_rgb_array (NULL);
    float *paletteLineWeights = twtw_default_color_palette_line_weight_array (NULL);
    NSCAssert(rgbPalette, @"palette missing");
    NSCAssert(paletteLineWeights, @"line weights missing");

    const int colorID = twtw_curvelist_get_color_id (curve);
    float lineWMul = (paletteLineWeights[colorID] > 0.0f) ? paletteLineWeights[colorID] : 1.0;
    
    // lines on the Mac shouldn't be quite as thick as on Maemo, hence this hack...
    lineWMul *= 0.84;
    
    const double fColorMul = 1.0 / 255.0;
    float rgbaColor[4] = { rgbPalette[colorID*3] * fColorMul, rgbPalette[colorID*3+1] * fColorMul, rgbPalette[colorID*3+2] * fColorMul,  1.0 };
    
    CGContextSetStrokeColor(ctx, rgbaColor);
    
    int i;
    for (i = 0; i < segCount; i++) {
        TwtwCurveSegment *seg = segs + i;
    
        float startX = TWTW_UNITS_TO_FLOAT(seg->startPoint.x);
        float startY = TWTW_UNITS_TO_FLOAT(seg->startPoint.y);
        
        float endX = TWTW_UNITS_TO_FLOAT(seg->endPoint.x);
        float endY = TWTW_UNITS_TO_FLOAT(seg->endPoint.y);
    
        CGContextMoveToPoint(ctx, startX, startY);
        
        float startW = TWTW_UNITS_TO_FLOAT(seg->startWeight);
        float endW = TWTW_UNITS_TO_FLOAT(seg->endWeight);
        if (startW < 0.001) startW = 0.7;
        if (endW < 0.001) endW = 0.7;
        CGContextSetLineWidth(ctx, startW * lineWMul);
        
        if (seg->segmentType == TWTW_SEG_CATMULLROM &&
                !twtw_is_invalid_point(seg->controlPoint1) &&
                !twtw_is_invalid_point(seg->controlPoint2)) {
                
            TwtwUnit segLen = twtw_point_distance (seg->startPoint, seg->endPoint);
            
                if (segLen > TWTW_UNITS_FROM_INT(1)) {
                    const int steps = MIN(16, TWTW_UNITS_TO_INT(segLen) * 2);
                    TwtwPoint twarr[steps];
                    
                    twtw_calc_catmullrom_curve (seg, steps, twarr);

                    int j;
                    for (j = 0; j < steps; j++) {
                        double x = TWTW_UNITS_TO_FLOAT(twarr[j].x);
                        double y = TWTW_UNITS_TO_FLOAT(twarr[j].y);

                        CGContextAddLineToPoint(ctx, x, y);
                        
                        CGContextStrokePath(ctx);
                        CGContextMoveToPoint(ctx, x, y);
                        
                        double u = (double)(j+1) / steps;
                        CGContextSetLineWidth(ctx, (startW + u*(endW-startW)) * lineWMul);
                    }
                }
        }
        
        CGContextAddLineToPoint(ctx, endX, endY);
        CGContextStrokePath(ctx);
    }
}


static CGAffineTransform CGAffineTransformMakeXShear(CGFloat proportion) {
 return CGAffineTransformMake(1.0, 0.0, proportion, 1.0, 0.0, 0.0);
}

static CGAffineTransform CGAffineTransformXShear(CGAffineTransform src, CGFloat proportion) {
 return CGAffineTransformConcat(src, CGAffineTransformMakeXShear(proportion));
}

static void drawPageThumbnail(CGContextRef ctx, TwtwPageThumb *thumb, NSRect outRect)
{
    g_return_if_fail(thumb);
    g_return_if_fail(thumb->rgbHasAlpha == TRUE);
    
    int w = thumb->w;
    int h = thumb->h;
    
    CGColorSpaceRef cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);  //CGColorSpaceCreateDeviceRGB();
    CGContextRef thumbImageCtx = CGBitmapContextCreate(thumb->rgbPixels,
                                                 w, h,
                                                 8,  // bits per component
                                                 thumb->rgbRowBytes,
                                                 cspace,
                                                 kCGImageAlphaPremultipliedLast);  // CG only supports premultiplied alpha
    CGColorSpaceRelease(cspace);
    cspace = NULL;

    CGImageRef cgImage = CGBitmapContextCreateImage(thumbImageCtx);
    
    CGContextSaveGState(ctx);
    
    //CGContextTranslateCTM(ctx, 0, h - 1);
    //CGContextScaleCTM(ctx, 1, -1);
    
    CGContextTranslateCTM(ctx, outRect.origin.x + 2.0, outRect.origin.y + 1.5);
    //CGContextScaleCTM(ctx, (outRect.size.width - 2) / outRect.size.width, (outRect.size.height - 2) / outRect.size.height);
    
    CGAffineTransform trs = CGAffineTransformMakeXShear(1.0);
    CGContextConcatCTM(ctx, trs);
    
    CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    
    CGContextDrawImage(ctx, CGRectMake(0, 0,  outRect.size.width - 4, outRect.size.height - 3), cgImage);
    
    CGContextRestoreGState(ctx);
    
    CGImageRelease(cgImage);
    CGContextRelease(thumbImageCtx);
}


static CGContextRef createCGContextFromTwtwYUVImage(TwtwYUVImage *yuvImage, uint8_t **outBitmapData)
{
    g_return_val_if_fail(yuvImage, NULL);
    g_return_val_if_fail(yuvImage->buffer, NULL);

    const int w = yuvImage->w;
    const int h = yuvImage->h;
    const size_t rowBytes = w * 4;
    uint8_t *bitmapData = malloc(rowBytes * h);

    twtw_yuv_image_convert_to_rgb_for_display (yuvImage, bitmapData, rowBytes, TRUE, 1, 1);
    
    CGColorSpaceRef cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
    CGContextRef context = CGBitmapContextCreate(bitmapData, w, h,
                                                 8,  // bits per component
                                                 rowBytes,
                                                 cspace,
                                                 kCGImageAlphaNoneSkipLast);  // CG only supports 32-bit pixels, not 24
    CGColorSpaceRelease(cspace);
    
    *outBitmapData = bitmapData;
    return context;
}

static void drawBackgroundPhotoFromPage(CGContextRef ctx, TwtwPage *page, int w, int h)
{
    TwtwYUVImage *image = twtw_page_get_yuv_photo (page);

    if ( !image) return;
    
    uint8_t *bitmapData = NULL;
    CGContextRef bgImageCtx = createCGContextFromTwtwYUVImage(image, &bitmapData);

    ///printf("%s: should draw YUV photo: %p (page %p); size %i * %i -- image size %i * %i\n", __func__, image, page, w, h, image->w, image->h);

    CGImageRef cgImage = CGBitmapContextCreateImage(bgImageCtx);
    
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, 0, h - 1);
    CGContextScaleCTM(ctx, 1, -1);
    
    CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cgImage);
    
    CGContextRestoreGState(ctx);
    
    CGImageRelease(cgImage);
    CGContextRelease(bgImageCtx);
    free(bitmapData);
}

static void drawActivePageInCache(double zoomFactor)
{
    TwtwPage *page = twtw_active_document_page ();
    TwtwCacheSurface *surf = twtw_shared_canvas_cache_surface();
    
    int w = twtw_cache_surface_get_width(surf);
    int h = twtw_cache_surface_get_height(surf);
    
    if (zoomFactor <= 0.0) zoomFactor = 1.0;
    
    twtw_cache_surface_clear_rect (surf, 0, 0, w, h);
    
    {
    CGContextRef cacheCtx = (CGContextRef) twtw_cache_surface_begin_drawing (surf);
        
        drawBackgroundPhotoFromPage(cacheCtx, page, w, h);

        // reset the stroke color space to RGB before drawing curves
        CGColorSpaceRef cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
        CGContextSetStrokeColorSpace(cacheCtx, cspace);
        CGColorSpaceRelease(cspace);

        CGContextSaveGState(cacheCtx);
        if (zoomFactor != 1.0)
            CGContextScaleCTM(cacheCtx, zoomFactor, zoomFactor);

        int curveCount = twtw_page_get_curves_count (page);
        int i;
        for (i = 0; i < curveCount; i++) {
            drawCurveList(cacheCtx, twtw_page_get_curve(page, i));
        }
        
        CGContextRestoreGState(cacheCtx);

    twtw_cache_surface_end_drawing (surf);
    }
}

- (CGImageRef)copyActivePageAsCGImage
{
    TwtwCacheSurface *surf = twtw_shared_canvas_cache_surface();
    CGContextRef cacheCtx = (CGContextRef) twtw_cache_surface_begin_drawing (surf);
    
    CGImageRef cgImage = CGBitmapContextCreateImage(cacheCtx);

    twtw_cache_surface_end_drawing (surf);
    
    return cgImage;
}


- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    NSRect canvasRect = NSMakeRect(4.5, 4.5, 610, 304);

    CGContextRef cgCtx = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];

    CGContextSaveGState(cgCtx);
    CGContextTranslateCTM(cgCtx, 0, bounds.size.height - 1);
    CGContextScaleCTM(cgCtx, 1, -1);

    [[NSColor whiteColor] set];
    NSRectFill(rect);
    
    if (_bgCacheIsDirty) {
        drawActivePageInCache(_zoomFactor);
        _bgCacheIsDirty = NO;
    }
    
    // copy cached strokes into the view
    {
    TwtwCacheSurface *cacheSurf = twtw_shared_canvas_cache_surface();
    CGContextRef cacheCtx = (CGContextRef) twtw_cache_surface_get_sourceable (cacheSurf);
    CGImageRef cgImage = CGBitmapContextCreateImage(cacheCtx);
    
    CGContextDrawImage(cgCtx, NSRectToCGRect(bounds), cgImage);
    CGImageRelease(cgImage);
    }

    [NSBezierPath setDefaultLineWidth:0.66];
    [[NSColor blackColor] set];

    if (_editedCL) {
        CGColorSpaceRef cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
        CGContextSetStrokeColorSpace(cgCtx, cspace);
        CGColorSpaceRelease(cspace);
    
        drawCurveList(cgCtx, _editedCL);
    }

    // --- draw canvas borders ---
    {    
    // draw around canvas
    [[NSColor colorWithDeviceRed:0.92 green:0.92 blue:0.92 alpha:1.0] set];
    NSRectFill(NSMakeRect(0, 0, canvasRect.origin.x, bounds.size.height));    
    NSRectFill(NSMakeRect(0, 0, bounds.size.width, canvasRect.origin.y));
    
    double rx = canvasRect.origin.x + canvasRect.size.width;
    NSRectFill(NSMakeRect(rx, 0, bounds.size.width - rx, bounds.size.height));

    double ry = canvasRect.origin.y + canvasRect.size.height;
    NSRectFill(NSMakeRect(0, ry, bounds.size.width, bounds.size.height - ry));
    
    [[NSColor blackColor] set];
    NSBezierPath *path = [NSBezierPath bezierPathWithRect:canvasRect];
    [path setLineWidth:0.8];
    [path stroke];
    }
    
    CGContextRestoreGState(cgCtx);
    
    
    // --- draw overlay graphics ---
    {
    NSSize imSize = [_im_pens size];
    double penboxXMargin = 12;
    double penboxX = 4;
    _elemInfo.pencilsRect = NSMakeRect(penboxX, -1,  imSize.width-penboxXMargin, imSize.height);
    
    [_im_pens drawAtPoint:_elemInfo.pencilsRect.origin
                fromRect:NSMakeRect(penboxXMargin, 0,  _elemInfo.pencilsRect.size.width, _elemInfo.pencilsRect.size.height)
                operation:NSCompositeSourceOver
                fraction:1.0
                ];
                
    double x = penboxX + imSize.width - penboxXMargin;
    
    imSize = [_im_boombox size];
    [_im_boombox drawAtPoint:NSMakePoint(x, 0)
                fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                operation:NSCompositeSourceOver
                fraction:1.0
                ];

    _elemInfo.recButtonRect = NSMakeRect(x+47, 7, 24, 24);
    _elemInfo.playButtonRect = NSMakeRect(x+84, 7, 24, 24);
    
    x += imSize.width - 10;
    
    imSize = [_im_buttonglow size];
    switch (_audioState) {
        default:  break;
        case TWTW_AUDIOSTATUS_REC:
            [_im_buttonglow drawAtPoint:NSMakePoint(_elemInfo.recButtonRect.origin.x - 10, _elemInfo.recButtonRect.origin.y - 8)
                            fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                            operation:NSCompositePlusDarker
                            fraction:0.72];
            break;
        case TWTW_AUDIOSTATUS_PLAY:
            [_im_buttonglow drawAtPoint:NSMakePoint(_elemInfo.playButtonRect.origin.x - 10, _elemInfo.playButtonRect.origin.y - 8)
                            fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                            operation:NSCompositePlusDarker
                            fraction:0.72];
            break;
    }
                
                
    imSize = [_im_cam size];
    _elemInfo.cameraRect = NSMakeRect(x, 1, imSize.width, imSize.height);
    
    [_im_cam drawAtPoint:_elemInfo.cameraRect.origin
                fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                operation:NSCompositeSourceOver
                fraction:1.0
                ];
    x += imSize.width;
    
    
    // -- two stacks of papers --
    int currentPage = twtw_active_document_page_index();
    imSize = [_im_leaf size];
    int i;
    double y = 1;
    for (i = 0; i <= currentPage; i++) {
        [_im_leaf drawAtPoint:NSMakePoint(x, y)
                    fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                    operation:NSCompositeSourceOver
                    fraction:1.0
                    ];
                    
        TwtwPage *page = twtw_book_get_page (twtw_active_document(), i);
        TwtwPageThumb *thumb = twtw_page_get_thumb (page);
        drawPageThumbnail(cgCtx, thumb, NSMakeRect(x, y,
                                                   imSize.width - 24, imSize.height));

        y += 3;
    }
    _elemInfo.leftPaperStackRect = NSMakeRect(x, 1, imSize.width, imSize.height + y - 2);
    
    x += imSize.width - 22;
    y = 1;
    for (; i < 20; i++) {
        [_im_leaf drawAtPoint:NSMakePoint(x, y)
                    fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                    operation:NSCompositeSourceOver
                    fraction:1.0
                    ];
                    
        TwtwPage *page = twtw_book_get_page (twtw_active_document(), 20 - (i - currentPage));
        TwtwPageThumb *thumb = twtw_page_get_thumb (page);
        drawPageThumbnail(cgCtx, thumb, NSMakeRect(x, y,
                                                   imSize.width - 24, imSize.height));

        y += 3;
    }
    _elemInfo.rightPaperStackRect = NSMakeRect(x, 1, imSize.width, imSize.height + y - 2);
    
    x += imSize.width - 17;
    
    imSize = [_im_envelope size];
    _elemInfo.envelopeRect = NSMakeRect(x, 2, imSize.width, imSize.height);
    
    [_im_envelope drawAtPoint:_elemInfo.envelopeRect.origin
                fromRect:NSMakeRect(0, 0, imSize.width, imSize.height)
                operation:NSCompositeSourceOver
                fraction:1.0
                ];
    }

    // -- draw time markers --
    {
    NSRect markerRect = NSMakeRect(bounds.origin.x + bounds.size.width - 20,
                                   bounds.origin.y + 60,
                                   bounds.size.width - 24,
                                   bounds.size.height - 40 - 24);
                                   
    double d = (markerRect.size.height / 20.0);

    int pos = (_audioState != 0) ? ceil(_currentAudioTime) : 0;
    int soundDuration = twtw_page_get_sound_duration_in_seconds (twtw_active_document_page());

    int i;
    for (i = 0; i < 20; i++) {
        NSRect circleRect = NSMakeRect(markerRect.origin.x,
                                       markerRect.origin.y + i*d,  //markerRect.origin.y + markerRect.size.height - i*d - d,
                                       d, d);
        circleRect = NSInsetRect(circleRect, 2, 2);
        
        NSBezierPath *path = [NSBezierPath bezierPathWithOvalInRect:circleRect];

        if (i < soundDuration || _audioState == TWTW_AUDIOSTATUS_REC) {
            NSColor *color = [NSColor blueColor];
            if (i < pos) {
                switch (_audioState) {
                    case TWTW_AUDIOSTATUS_REC:  color = [NSColor redColor]; break;
                    case TWTW_AUDIOSTATUS_PLAY: color = [NSColor greenColor]; break;
                    default: break;
                }
            }
        
            if (_audioState == TWTW_AUDIOSTATUS_REC && i >= pos) {
                color = [NSColor lightGrayColor];
            }
            [color set];
            [path fill];
            
            [[NSColor blackColor] set];
            [path stroke];
        } else {
            [[NSColor grayColor] set];
            [path stroke];
        }
    }    
    }
}

@end
