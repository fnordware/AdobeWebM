//
//  WebP_Out_Controller.h
//
//  Created by Brendan Bolles on 6/16/13.
//  Copyright 2013 fnord. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#include "WebP_UI.h"


@interface WebP_Out_Controller : NSObject {
	IBOutlet NSButton *losslessCheck;
	IBOutlet NSSlider *qualitySlider;
	IBOutlet NSMatrix *alphaMatrix;
	IBOutlet NSWindow *theWindow;
}
- (id)init:(BOOL)lossless
	quality:(int)quality
	alpha:(DialogAlpha)alpha
	have_transparency:(BOOL)has_transparency
	alpha_name:(const char *)alphaName;

- (IBAction)clickedOK:(id)sender;
- (IBAction)clickedCancel:(id)sender;

- (IBAction)trackLossless:(id)sender;

- (NSWindow *)getWindow;

- (BOOL)getLossless;
- (int)getQuality;
- (DialogAlpha)getAlpha;

@end
