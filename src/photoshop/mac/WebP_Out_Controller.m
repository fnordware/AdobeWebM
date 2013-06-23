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
	lossyAlpha:(BOOL)lossyAlpha
	alphaCleanup:(BOOL)alphaCleanup
	saveMetadata:(BOOL)saveMetadata
	have_transparency:(BOOL)has_transparency
	alpha_name:(const char *)alphaName
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"WebP_Out_Dialog" owner:self]))
		return nil;
	
	
	[qualityField setIntValue:quality];
	[qualitySlider setIntValue:quality];
	
	
	if(!has_transparency)
	{
		[alphaTransparencyRadio setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_TRANSPARENCY)
		{
			alpha = (alphaName ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
		}
	}
	
	if(alphaName)
	{
		[alphaChannelRadio setTitle:[NSString stringWithUTF8String:alphaName]];
	}
	else
	{
		[alphaChannelRadio setEnabled:FALSE];
		
		if(alpha == DIALOG_ALPHA_CHANNEL)
		{
			alpha = (has_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
		}
	}


	[lossyAlphaCheck setState:(lossyAlpha ? NSOnState : NSOffState)];
	[alphaCleanupCheck setState:(alphaCleanup ? NSOnState : NSOffState)];

	id selectedAlpha = (alpha == DIALOG_ALPHA_NONE ? alphaNoneRadio :
						alpha == DIALOG_ALPHA_TRANSPARENCY ? alphaTransparencyRadio :
						alphaChannelRadio);
						
	[selectedAlpha setState:NSOnState];
	[self trackAlpha:selectedAlpha];
	
	
	id selectedLossless = (lossless ? losslessRadio : qualityRadio);
	
	[selectedLossless setState:NSOnState];
	[self trackLossless:(lossless ? losslessRadio : qualityRadio)];
	

	[saveMetadataCheck setState:(saveMetadata ? NSOnState : NSOffState)];
	

	[theWindow center];
	
	result = DIALOG_RESULT_CONTINUE;
	
	return self;
}

- (IBAction)clickedOK:(id)sender {
	result = DIALOG_RESULT_OK;
}

- (IBAction)clickedCancel:(id)sender {
    result = DIALOG_RESULT_CANCEL;
}

- (IBAction)trackLossless:(id)sender {
	BOOL isLossless = (sender == losslessRadio);
	
	[qualityField setEnabled:!isLossless];
	[qualitySlider setEnabled:!isLossless];
	
	BOOL using_alpha = ([self getAlpha] != DIALOG_ALPHA_NONE);
	
	[lossyAlphaCheck setEnabled:(!isLossless && using_alpha)];
	

	id otherButton = (sender == losslessRadio ? qualityRadio : losslessRadio);

	[otherButton setState:NSOffState];
}

- (IBAction)trackAlpha:(id)sender {
	if(sender == alphaNoneRadio)
	{
		[alphaTransparencyRadio setState:NSOffState];
		[alphaChannelRadio setState:NSOffState];
	}
	else if(sender == alphaTransparencyRadio)
	{
		[alphaNoneRadio setState:NSOffState];
		[alphaChannelRadio setState:NSOffState];
	}
	else // alphaChannelRadio
	{
		[alphaNoneRadio setState:NSOffState];
		[alphaTransparencyRadio setState:NSOffState];
	}
	
	[lossyAlphaCheck setEnabled:(![self getLossless] && (sender != alphaNoneRadio))];
	
	[alphaCleanupCheck setEnabled:(sender == alphaTransparencyRadio)];
}

- (NSWindow *)getWindow {
	return theWindow;
}

- (DialogResult)getResult {
	return result;
}

- (BOOL)getLossless {
	return ([losslessRadio state] == NSOnState);
}

- (int)getQuality {
	return [qualitySlider intValue];
}

- (DialogAlpha)getAlpha {
	if([alphaNoneRadio state] == NSOnState)
		return DIALOG_ALPHA_NONE;
	else if([alphaTransparencyRadio state] == NSOnState)
		return DIALOG_ALPHA_TRANSPARENCY;
	else if([alphaChannelRadio state] == NSOnState)
		return DIALOG_ALPHA_CHANNEL;
	else
		return DIALOG_ALPHA_NONE;
}

- (BOOL)getLossyAlpha {
	return ([lossyAlphaCheck state] == NSOnState);
}

- (BOOL)getAlphaCleanup {
	return ([alphaCleanupCheck state] == NSOnState);
}

- (BOOL)getSaveMetadata {
	return ([saveMetadataCheck state] == NSOnState);
}

@end
