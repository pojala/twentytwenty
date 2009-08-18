//
//  TwtwCanvasView.h
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

#import <Cocoa/Cocoa.h>
#import "twtw-curves.h"


// equivalent struct is found in twtw-maemo-canvas.c for the Maemo/GTK+ version
typedef struct twtwCanvasElementInfo_Cocoa {
    NSRect pencilsRect;
    
    NSRect boomboxRect;
    NSRect recButtonRect;
    NSRect playButtonRect;
    
    NSRect cameraRect;
    
    NSRect leftPaperStackRect;
    NSRect rightPaperStackRect;
    double paperSheetH;  // height of one paper sheet
    
    NSRect envelopeRect;
    
    NSRect closeRect;
} TwtwCanvasElementInfo;


@interface TwtwCanvasView : NSView {

    BOOL _bgCacheIsDirty;

    TwtwCurveList *_editedCL;
    
    NSImage *_im_pens;
    NSImage *_im_boombox;
    NSImage *_im_cam;
    NSImage *_im_leaf;
    NSImage *_im_envelope;
    NSImage *_im_buttonglow;
    
    double _currentAudioTime;
    int _audioState;
    
    TwtwCanvasElementInfo _elemInfo;
    
    double _zoomFactor;
}

- (void)reloadDocument;

- (void)audioDidCompleteWithRetainedInfo:(id)info;
- (void)audioInProgressWithRetainedInfo:(id)info;

- (BOOL)audioIsBusy;

- (CGImageRef)copyActivePageAsCGImage;

@end
