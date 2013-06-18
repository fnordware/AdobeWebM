///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2013, Brendan Bolles
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *	   Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *	   Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
//
// WebP Photoshop plug-in
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------

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
