
//
// WebP
//
// by Brendan Bolles
//

#include "WebP_UI.h"

//#import "SuperPNG_InUI_Controller.h"
#import "WebP_Out_Controller.h"
//#import "SuperPNG_About_Controller.h"

#include "WebP_version.h"

//#include <Carbon/Carbon.h>
//#include <AppKit/AppKit.h>

// ==========
// Only building this on 64-bit (Cocoa) architectures
// ==========
#if __LP64__


bool
WebP_InUI(
	WebP_InUI_Data		*params,
	const void			*plugHndl,
	const void			*mwnd)
{
	params->alpha = DIALOG_ALPHA_TRANSPARENCY;

	bool result = true;
/*	
	params->alpha = DIALOG_ALPHA_TRANSPARENCY;
	params->mult = false;
	
	// get the prefs
	BOOL always_dialog = FALSE;
	
	CFPropertyListRef alphaMode_val = CFPreferencesCopyAppValue(CFSTR(SUPERPNG_PREFS_ALPHA), CFSTR(SUPERPNG_PREFS_ID));
	CFPropertyListRef mult_val = CFPreferencesCopyAppValue(CFSTR(SUPERPNG_PREFS_MULT), CFSTR(SUPERPNG_PREFS_ID));
	CFPropertyListRef always_val = CFPreferencesCopyAppValue(CFSTR(SUPERPNG_PREFS_ALWAYS), CFSTR(SUPERPNG_PREFS_ID));

	if(alphaMode_val)
	{
	   char alphaMode_char;
		
		if( CFNumberGetValue((CFNumberRef)alphaMode_val, kCFNumberCharType, &alphaMode_char) )
		{
			params->alpha = (DialogAlpha)alphaMode_char;
		}
		
		CFRelease(alphaMode_val);
	}

	if(mult_val)
	{
		params->mult = CFBooleanGetValue((CFBooleanRef)mult_val);
		
		CFRelease(mult_val);
	}

	if(always_val)
	{
		always_dialog = CFBooleanGetValue((CFBooleanRef)always_val);
		
		CFRelease(always_val);
	}

	// user can force dialog open buy holding shift or option
    UInt32 keys = GetCurrentEventKeyModifiers();
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );


	if(always_dialog || option_key)
	{
		// do the dialog (or maybe not (but we still load the object to get the prefs)
		NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

		Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
										classNamed:@"SuperPNG_InUI_Controller"];

		if(ui_controller_class)
		{
			SuperPNG_InUI_Controller *ui_controller = [[ui_controller_class alloc] init:params->alpha
														multiply:params->mult
														alwaysDialog:always_dialog];
			
			if(ui_controller)
			{
				NSWindow *my_window = [ui_controller getWindow];
				
				if(my_window)
				{
					NSInteger modal_result = [NSApp runModalForWindow:my_window];
					
					if(modal_result == NSRunStoppedResponse)
					{
						params->alpha	= [ui_controller getAlpha];
						params->mult	= [ui_controller getMult];
						always_dialog	= [ui_controller getAlways];
						
						// record the always pref every time
						CFBooleanRef always =  (always_dialog ? kCFBooleanTrue : kCFBooleanFalse);
						CFPreferencesSetAppValue(CFSTR(SUPERPNG_PREFS_ALWAYS), always, CFSTR(SUPERPNG_PREFS_ID));
						CFPreferencesAppSynchronize(CFSTR(SUPERPNG_PREFS_ID));
						CFRelease(always);
						
						result = true;
					}
					else
						result = false;
						
					[my_window close];
				}

				[ui_controller release];
			}
		}
	}

*/
	return result;
}


bool
WebP_OutUI(
	WebP_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;

	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

	Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"WebP_Out_Controller"];

	if(ui_controller_class)
	{
		WebP_Out_Controller *ui_controller = [[ui_controller_class alloc] init:params->lossless
														quality:params->quality
														alpha:params->alpha
														have_transparency:have_transparency
														alpha_name:alpha_name];
		if(ui_controller)
		{
			NSWindow *my_window = [ui_controller getWindow];
			
			if(my_window)
			{
				NSInteger modal_result = [NSApp runModalForWindow:my_window];
				
				if(modal_result == NSRunStoppedResponse)
				{
					params->lossless	= [ui_controller getLossless];
					params->quality		= [ui_controller getQuality];
					params->alpha		= [ui_controller getAlpha];
					
					result = true;
				}
				else
					result = false;
					
				[my_window close];
			}
			
			[ui_controller release];
		}
	}
	
	
	return result;
}


void
WebP_About(
	const void		*plugHndl,
	const void		*mwnd)
{
	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];
/*
	Class about_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"SuperPNG_About_Controller"];
	
	if(about_controller_class)
	{
		SuperPNG_About_Controller *about_controller = [[about_controller_class alloc] init:"v" SuperPNG_Version_String " - " SuperPNG_Build_Date dummy:"you dumb"];
		
		if(about_controller)
		{
			NSWindow *the_window = [about_controller getWindow];
			
			if(the_window)
			{
				[NSApp runModalForWindow:the_window];
				
				[the_window close];
			}
			
			[about_controller release];
		}
	}*/
}

#endif // __LP64__
