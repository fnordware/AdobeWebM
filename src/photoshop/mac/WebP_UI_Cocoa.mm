
//
// WebP
//
// by Brendan Bolles
//

#include "WebP_UI.h"

#import "WebP_In_Controller.h"
#import "WebP_Out_Controller.h"
#import "WebP_About_Controller.h"

#include "WebP_version.h"

#include <Carbon/Carbon.h> // for GetCurrentEventKeyModifiers()

// ==========
// Only building this on 64-bit (Cocoa) architectures
// ==========
#if __LP64__


bool
WebP_InUI(
	WebP_InUI_Data		*params,
	bool				have_alpha,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;
	
	params->alpha = DIALOG_ALPHA_TRANSPARENCY;
	params->mult = false;
	
	// get the prefs
	BOOL auto_dialog = FALSE;
	
	CFPropertyListRef alphaMode_val = CFPreferencesCopyAppValue(CFSTR(WEBP_PREFS_ALPHA), CFSTR(WEBP_PREFS_ID));
	CFPropertyListRef mult_val = CFPreferencesCopyAppValue(CFSTR(WEBP_PREFS_MULT), CFSTR(WEBP_PREFS_ID));
	CFPropertyListRef auto_val = CFPreferencesCopyAppValue(CFSTR(WEBP_PREFS_AUTO), CFSTR(WEBP_PREFS_ID));

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

	if(auto_val)
	{
		auto_dialog = CFBooleanGetValue((CFBooleanRef)auto_val);
		
		CFRelease(auto_val);
	}
	
	
	// user can force dialog open buy holding shift or option
    UInt32 keys = GetCurrentEventKeyModifiers();
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );


	if((auto_dialog && have_alpha) || option_key)
	{
		// do the dialog (or maybe not (but we still load the object to get the prefs)
		NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

		Class ui_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
										classNamed:@"WebP_In_Controller"];

		if(ui_controller_class)
		{
			WebP_In_Controller *ui_controller = [[ui_controller_class alloc] init:params->alpha
														premultiply:params->mult
														autoDialog:auto_dialog];
			
			if(ui_controller)
			{
				NSWindow *my_window = [ui_controller getWindow];
				
				if(my_window)
				{
					NSInteger modal_result = [NSApp runModalForWindow:my_window];
					
					if(modal_result == NSRunStoppedResponse)
					{
						params->alpha	= [ui_controller getAlpha];
						params->mult	= [ui_controller getPremult];
						
						result = true;
					}
					else
						result = false;
					
					
					// record the prefs every time
					char val = [ui_controller getDefaultAlpha];
					CFNumberRef alpha_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberCharType, &val);
					CFPreferencesSetAppValue(CFSTR(WEBP_PREFS_ALPHA), alpha_ref, CFSTR(WEBP_PREFS_ID));
					
					CFBooleanRef multRef =  ([ui_controller getDefaultPremult] ? kCFBooleanTrue : kCFBooleanFalse);
					CFPreferencesSetAppValue(CFSTR(WEBP_PREFS_MULT), multRef, CFSTR(WEBP_PREFS_ID));
					
					CFBooleanRef autoRef =  ([ui_controller getAuto] ? kCFBooleanTrue : kCFBooleanFalse);
					CFPreferencesSetAppValue(CFSTR(WEBP_PREFS_AUTO), autoRef, CFSTR(WEBP_PREFS_ID));
					
					CFPreferencesAppSynchronize(CFSTR(WEBP_PREFS_ID));
					
					CFRelease(alpha_ref);
					
					
					[my_window close];
				}

				[ui_controller release];
			}
		}
	}


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
	const char		*plugin_version_string,
	const char		*WebP_version_string,
	const void		*plugHndl,
	const void		*mwnd)
{
	NSString *bundle_id = [NSString stringWithUTF8String:(const char *)plugHndl];

	Class about_controller_class = [[NSBundle bundleWithIdentifier:bundle_id]
									classNamed:@"WebP_About_Controller"];
	
	if(about_controller_class)
	{
		WebP_About_Controller *about_controller = [[about_controller_class alloc] init:plugin_version_string
													webpVersion:WebP_version_string];
		
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
	}
}

#endif // __LP64__
