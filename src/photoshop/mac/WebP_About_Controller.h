//
//  WebP_About_Controller.h
//
//  Created by Brendan Bolles on 6/17/13.
//  Copyright 2013 fnord. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface WebP_About_Controller : NSObject {
	IBOutlet NSTextField *pluginVersionString;
	IBOutlet NSTextField *webpVersionString;
	IBOutlet NSWindow *theWindow;
}

- (id)init:(const char *)pluginVersion
	webpVersion:(const char *)webpVersion;

- (IBAction)clickedOK:(id)sender;

- (NSWindow *)getWindow;

@end
