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
