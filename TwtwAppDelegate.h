//
//  TwtwAppDelegate.h
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
@class TwtwCanvasView;


@interface TwtwAppDelegate : NSObject {
    
    IBOutlet NSWindow *_editorWindow;
    IBOutlet TwtwCanvasView *_canvasView;
    
    BOOL _docHasChanges;
    NSString *_prevSavePath;
    
    NSString *_authToken;
}

- (IBAction)newDocument:(id)sender;
- (IBAction)openDocument:(id)sender;
- (IBAction)saveDocument:(id)sender;
- (IBAction)saveDocumentAs:(id)sender;

- (IBAction)undo:(id)sender;

- (IBAction)zoomNormalSizeAction:(id)sender;
- (IBAction)zoom2xAction:(id)sender;
- (IBAction)zoom1_5xAction:(id)sender;
- (IBAction)zoom0_75xAction:(id)sender;

- (void)documentWasModified;

@end
