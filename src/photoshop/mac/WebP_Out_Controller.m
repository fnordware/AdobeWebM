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

#import "WebP_Out_Controller.h"

@implementation WebP_Out_Controller

- (id)init:(BOOL)lossless
	quality:(int)quality
	alpha:(DialogAlpha)alpha
	have_transparency:(BOOL)has_transparency
	alpha_name:(const char *)alphaName
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"WebP_Out_Dialog" owner:self]))
		return nil;
		
	[losslessCheck setState:(lossless ? NSOnState : NSOffState)];
	[qualitySlider setIntValue:quality];
	
	[self trackLossless:self];
	
	
	if(!has_transparency)
	{
		[[alphaMatrix cellAtRow:1 column:0] setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_TRANSPARENCY)
		{
			alpha = (alphaName ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
		}
	}
	
	if(alphaName)
	{
		[[alphaMatrix cellAtRow:2 column:0] setTitle:[NSString stringWithUTF8String:alphaName]];
	}
	else
	{
		[[alphaMatrix cellAtRow:2 column:0] setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_CHANNEL)
		{
			alpha = (has_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
		}
	}

	[alphaMatrix selectCellAtRow:(NSInteger)alpha column:0];


	[theWindow center];
	
	return self;
}


- (IBAction)clickedOK:(id)sender {
	[NSApp stopModal];
}

- (IBAction)clickedCancel:(id)sender {
    [NSApp abortModal];
}

- (IBAction)trackLossless:(id)sender {
	[qualitySlider setEnabled:([losslessCheck state] == NSOffState)];
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (BOOL)getLossless {
	return ([losslessCheck state] == NSOnState);
}

- (int)getQuality {
	return [qualitySlider intValue];
}

- (DialogAlpha)getAlpha {
	// got to be a better way to do this, right?
	if([[alphaMatrix cellAtRow:0 column:0] state] == NSOnState)
		return DIALOG_ALPHA_NONE;
	else if([[alphaMatrix cellAtRow:1 column:0] state] == NSOnState)
		return DIALOG_ALPHA_TRANSPARENCY;
	else if([[alphaMatrix cellAtRow:2 column:0] state] == NSOnState)
		return DIALOG_ALPHA_CHANNEL;
	else
		return DIALOG_ALPHA_NONE;
}

@end
