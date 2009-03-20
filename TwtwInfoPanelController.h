//
//  TwtwInfoPanelController.h
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

#import <Cocoa/Cocoa.h>


@interface TwtwInfoPanelController : NSWindowController {

    IBOutlet id _authorField;
    IBOutlet id _descField;
    IBOutlet id _lockedCheckbox;
    IBOutlet id _fileSizeField;
    
    IBOutlet id _clearVectorsButton;
    IBOutlet id _clearPhotoButton;
    IBOutlet id _clearAudioButton;
}

+ (id)sharedController;

- (IBAction)computeFileSize:(id)sender;

- (IBAction)setMetadataField:(id)sender;
- (IBAction)setDocumentLocked:(id)sender;

- (IBAction)clearVectors:(id)sender;
- (IBAction)clearPhoto:(id)sender;
- (IBAction)clearAudio:(id)sender;

- (void)updateInfo;

@end
