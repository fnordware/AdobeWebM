//
//  WebP_Out_Controller.m
//
//  Created by Brendan Bolles on 6/16/13.
//  Copyright 2013 fnord. All rights reserved.
//

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
