//
//  WebP_In_Controller.m
//
//  Created by Brendan Bolles on 6/16/13.
//  Copyright 2013 fnord. All rights reserved.
//

#import "WebP_In_Controller.h"

@implementation WebP_In_Controller

- (id)init:(DialogAlpha)alpha
	premultiply:(BOOL)premultiply
	autoDialog:(BOOL)autoDialog
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"WebP_In_Dialog" owner:self]))
		return nil;
	
	[transparencyRadio setState:(alpha == DIALOG_ALPHA_TRANSPARENCY ? NSOnState : NSOffState)];
	[channelRadio setState:(alpha == DIALOG_ALPHA_CHANNEL ? NSOnState : NSOffState)];
	
	[premultCheck setState:(premultiply ? NSOnState : NSOffState)];
	[autoCheck setState:(autoDialog ? NSOnState : NSOffState)];
	
	[premultCheck setEnabled:(alpha == DIALOG_ALPHA_CHANNEL)];
	
	defaultAlpha = alpha;
	defaultPremult = premultiply;
	
	[theWindow center];
	
	return self;
}

- (IBAction)clickedOK:(id)sender {
	[NSApp stopModal];
}

- (IBAction)clickedCancel:(id)sender {
    [NSApp abortModal];
}

- (IBAction)clickedSetDefaults:(id)sender {
	defaultAlpha = [self getAlpha];
	defaultPremult = [self getPremult];
}

- (IBAction)trackAlpha:(id)sender {
	if(sender == transparencyRadio)
	{
		[channelRadio setState:NSOffState];
		[premultCheck setEnabled:FALSE];
	}
	else
	{
		[transparencyRadio setState:NSOffState];
		[premultCheck setEnabled:TRUE];
	}
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (DialogAlpha)getAlpha {
	if([transparencyRadio state] == NSOnState)
		return DIALOG_ALPHA_TRANSPARENCY;
	else
		return DIALOG_ALPHA_CHANNEL;
}

- (BOOL)getPremult {
	return ([premultCheck state] == NSOnState);
}

- (DialogAlpha)getDefaultAlpha {
	return defaultAlpha;
}

- (BOOL)getDefaultPremult {
	return defaultPremult;
}

- (BOOL)getAuto {
	return ([autoCheck state] == NSOnState);
}

@end
