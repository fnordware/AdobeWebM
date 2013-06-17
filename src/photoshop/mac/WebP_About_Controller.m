//
//  WebP_About_Controller.m
//
//  Created by Brendan Bolles on 6/17/13.
//  Copyright 2013 fnord. All rights reserved.
//

#import "WebP_About_Controller.h"

@implementation WebP_About_Controller

- (id)init:(const char *)pluginVersion
	webpVersion:(const char *)webpVersion
{
	self = [super init];
	
	if(!([NSBundle loadNibNamed:@"WebP_About_Dialog" owner:self]))
		return nil;

	[pluginVersionString setStringValue:[NSString stringWithUTF8String:pluginVersion]];
	[webpVersionString setStringValue:[NSString stringWithUTF8String:webpVersion]];
	
	[theWindow center];
	
	return self;
}

- (IBAction)clickedOK:(id)sender {
	[NSApp stopModal];
}

- (NSWindow *)getWindow {
	return theWindow;
}

@end
