//
//  WebP_In_Controller.h
//
//  Created by Brendan Bolles on 6/16/13.
//  Copyright 2013 fnord. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#include "WebP_UI.h"

@interface WebP_In_Controller : NSObject {
	IBOutlet NSButton *transparencyRadio;
	IBOutlet NSButton *channelRadio;
	IBOutlet NSButton *premultCheck;
	IBOutlet NSButton *autoCheck;
	IBOutlet NSWindow *theWindow;
	
	DialogAlpha defaultAlpha;
	BOOL defaultPremult;
}
- (id)init:(DialogAlpha)alpha
	premultiply:(BOOL)premultiply
	autoDialog:(BOOL)autoDialog;

- (IBAction)clickedOK:(id)sender;
- (IBAction)clickedCancel:(id)sender;

- (IBAction)clickedSetDefaults:(id)sender;

- (IBAction)trackAlpha:(id)sender;

- (NSWindow *)getWindow;

- (DialogAlpha)getAlpha;
- (BOOL)getPremult;

- (DialogAlpha)getDefaultAlpha;
- (BOOL)getDefaultPremult;

- (BOOL)getAuto;

@end
