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


#include "WebP_UI.h"



// ==============
// Only compiled on 32-bit
// ==============
#if !__LP64__

#include <Carbon/Carbon.h>



static UInt32 g_item_clicked = 0;

static void SetControlVal(WindowRef window, OSType sig, SInt32 id, SInt32 val)
{
	ControlID cid = {sig, id};
	ControlRef ref;
	
	OSStatus result = GetControlByID(window, &cid, &ref);
	
	SetControl32BitValue(ref, val);
}

static SInt32 GetControlVal(WindowRef window, OSType sig, SInt32 id)
{
	ControlID cid = {sig, id};
	ControlRef ref;
	
	OSStatus result = GetControlByID(window, &cid, &ref);
	
	return GetControl32BitValue(ref);
}


static void TrackAlpha(WindowRef window)
{
	ControlID cid = {'Prem', 0};
	ControlRef cref;
	
	GetControlByID(window, &cid, &cref);
	
	if( GetControlVal(window, 'Alph', 0) == 2 )
		EnableControl(cref);
	else
		DisableControl(cref);
}

static DialogAlpha g_default_alpha;
static bool g_default_mult;

static pascal OSStatus
In_WindowEventHandler( EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon )
{
#pragma unused( inCaller )

  OSStatus  result = eventNotHandledErr;
  
	switch ( GetEventClass( inEvent ) )
	{
		case kEventClassCommand:
		{
			HICommand  cmd;

			GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof( cmd ), NULL, &cmd );
			
			switch ( GetEventKind( inEvent ) )
			{
				case kEventCommandProcess:
					switch ( cmd.commandID )
					{
						case kHICommandOK:
						case kHICommandCancel:
							QuitAppModalLoopForWindow( (WindowRef)inRefcon );
							g_item_clicked = cmd.commandID;
							result = noErr;
						break;

						case 'Alph':
							TrackAlpha((WindowRef)inRefcon);
						break;
						
						case 'Dflt':
							g_default_alpha = (DialogAlpha)GetControlVal((WindowRef)inRefcon, 'Alph', 0);
							g_default_mult = GetControlVal((WindowRef)inRefcon, 'Prem', 0);
						break;
						
						default:
						break;
					}
				break;

				default:
				break;
			}
			break;
		}

		default:
		break;
	}
  
  return result;
}

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
	bool auto_dialog = FALSE;

	CFStringRef prefs_id = CFStringCreateWithCString(NULL, WEBP_PREFS_ID, kCFStringEncodingASCII);
	
	CFStringRef alpha_id = CFStringCreateWithCString(NULL, WEBP_PREFS_ALPHA, kCFStringEncodingASCII);
	CFStringRef mult_id = CFStringCreateWithCString(NULL, WEBP_PREFS_MULT, kCFStringEncodingASCII);
	CFStringRef auto_id = CFStringCreateWithCString(NULL, WEBP_PREFS_AUTO, kCFStringEncodingASCII);
	
	CFPropertyListRef alphaMode_val = CFPreferencesCopyAppValue(alpha_id, prefs_id);
	CFPropertyListRef mult_val = CFPreferencesCopyAppValue(mult_id, prefs_id);
	CFPropertyListRef auto_val = CFPreferencesCopyAppValue(auto_id, prefs_id);

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
	
	g_default_alpha = params->alpha;
	g_default_mult = params->mult;
	
	// user can force dialog open buy holding shift or option
    UInt32 keys = GetCurrentEventKeyModifiers();
	bool option_key = ( (keys & shiftKey) || (keys & rightShiftKey) || (keys & optionKey) || (keys & rightOptionKey) );


	if((auto_dialog && have_alpha) || option_key)
	{
		CFStringRef bundle_id = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
																(const char *)plugHndl, 
																kCFStringEncodingUTF8,
																 kCFAllocatorNull);
		
		CFBundleRef bundle_ref = CFBundleGetBundleWithIdentifier(bundle_id);
		
		if(bundle_ref)
		{
			OSStatus err;
		
			CFRetain(bundle_ref);
			
			IBNibRef nib_ref = NULL;
			
			err = CreateNibReferenceWithCFBundle(bundle_ref, CFSTR("WebP_UI_Carbon"), &nib_ref);
			
			if(nib_ref)
			{
				WindowRef window = NULL;
				
				err = CreateWindowFromNib(nib_ref, CFSTR("In Dialog"), &window);
				
				if(window)
				{
					// put image in HIImageView
					CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebP_banner.png"), NULL, NULL);
					
					HIViewRef banner_view = NULL;
					HIViewID  hiViewID = {'Banr', 0};
					
					err = HIViewFindByID(HIViewGetRoot(window), hiViewID, &banner_view);
					
					if(png_url && banner_view)
					{
						CGDataProviderRef png_provider = CGDataProviderCreateWithURL(png_url);
						
						CGImageRef png_image = CGImageCreateWithPNGDataProvider(png_provider, NULL, FALSE, kCGRenderingIntentDefault);
						
						err = HIImageViewSetImage(banner_view, png_image);
						err = HIViewSetVisible(banner_view, true);
						err = HIViewSetNeedsDisplay(banner_view, true);
						
						CGImageRelease(png_image);
						CFRelease(png_url);
					}
					
					SetControlVal(window, 'Alph', 0, params->alpha);
					SetControlVal(window, 'Prem', 0, params->mult);
					SetControlVal(window, 'Auto', 0, auto_dialog);
					
					TrackAlpha(window);
					
					

					// event handler
					EventTypeSpec  kWindowEvents[] =  {  { kEventClassCommand, kEventCommandProcess } };
					EventHandlerUPP windowUPP = NewEventHandlerUPP( In_WindowEventHandler );
					
					InstallWindowEventHandler(window, windowUPP, GetEventTypeCount( kWindowEvents ), kWindowEvents, window, NULL );
					
					
					// show the window
					RepositionWindow(window, NULL, kWindowCenterOnMainScreen);
					ShowWindow(window);
					
					SetThemeCursor(kThemeArrowCursor); // Photoshop will have set the cursor to the watch
					
					// event loop
					RunAppModalLoopForWindow(window);
					
					
					if(g_item_clicked == kHICommandOK)
					{
						params->alpha = (DialogAlpha)GetControlVal(window, 'Alph', 0);
						params->mult = GetControlVal(window, 'Prem', 0);
					
						result = true;
					}
					else
						result = false;
						
					
					// record the prefs every time
					char val = g_default_alpha;;
					CFNumberRef alpha_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberCharType, &val);
					CFPreferencesSetAppValue(alpha_id, alpha_ref, prefs_id);
					
					CFBooleanRef multRef =  (g_default_mult ? kCFBooleanTrue : kCFBooleanFalse);
					CFPreferencesSetAppValue(mult_id, multRef, prefs_id);
					
					auto_dialog = GetControlVal(window, 'Auto', 0);
					CFBooleanRef autoRef =  (auto_dialog ? kCFBooleanTrue : kCFBooleanFalse);
					CFPreferencesSetAppValue(auto_id, autoRef, prefs_id);
					
					CFPreferencesAppSynchronize(prefs_id);
					
					CFRelease(alpha_ref);
					
						
					DisposeWindow(window);
				}
				
				DisposeNibReference(nib_ref);
			}
		
			CFRelease(bundle_ref);
		}
		
		CFRelease(bundle_id);
	}
	
	CFRelease(prefs_id);
	CFRelease(alpha_id);
	CFRelease(mult_id);
	CFRelease(auto_id);
	
	return result;
}


static void TrackLossless(WindowRef window)
{
	ControlID slider_id = {'Qual', 0};
	ControlID field_id = {'Qual', 1};
	ControlID lossyA_id = {'ALos', 0};
	
	ControlRef slider_ref, field_ref, lossyA_ref;
	
	GetControlByID(window, &slider_id, &slider_ref);
	GetControlByID(window, &field_id, &field_ref);
	GetControlByID(window, &lossyA_id, &lossyA_ref);
	
	if( GetControlVal(window, 'Loss', 0) == 1 )
	{
		// Lossless
		DisableControl(slider_ref);
		DisableControl(field_ref);
		DisableControl(lossyA_ref);
	}
	else
	{
		EnableControl(slider_ref);
		DisableControl(field_ref); // can't figure out how to track typing, so I'll keep it disabled
		
		if( GetControlVal(window, 'Alph', 0) != 1 )
		{
			EnableControl(lossyA_ref);
		}
	}
}


static void TrackAlphaOut(WindowRef window)
{
	ControlID cleanup_id = {'ACln', 0};
	ControlID lossyA_id = {'ALos', 0};
	
	ControlRef cleanup_ref, lossyA_ref;
	
	GetControlByID(window, &cleanup_id, &cleanup_ref);
	GetControlByID(window, &lossyA_id, &lossyA_ref);
	
	DialogAlpha alpha = (DialogAlpha)(GetControlVal(window, 'Alph', 0) - 1);
	
	if(alpha == DIALOG_ALPHA_NONE)
	{
		DisableControl(cleanup_ref);
		DisableControl(lossyA_ref);
	}
	else
	{
		if(GetControlVal(window, 'Loss', 0) == 1)
		{
			// lossless
			DisableControl(lossyA_ref);
		}
		else
		{
			EnableControl(lossyA_ref);
		}
		
		if(alpha == DIALOG_ALPHA_TRANSPARENCY)
		{
			EnableControl(cleanup_ref);
		}
		else
		{
			DisableControl(cleanup_ref);
		}
	}
}


static void TrackSlider(WindowRef window)
{
	ControlID text_id = {'Qual', 1};
	
	ControlRef text_ref;
	
	GetControlByID(window, &text_id, &text_ref);


	int val = GetControlVal(window, 'Qual', 0);
	
	CFStringRef text_str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), val);
	
	if(text_str)
	{
		SetControlData(text_ref, kControlEntireControl, kControlEditTextCFStringTag, sizeof(CFStringRef), &text_str);
		
		CFRelease(text_str);
	}
}


static void TrackText(WindowRef window)
{
	ControlID slider_id = {'Qual', 0};
	ControlID text_id = {'Qual', 1};
	
	ControlRef slider_ref, text_ref;
	
	GetControlByID(window, &slider_id, &slider_ref);
	GetControlByID(window, &text_id, &text_ref);
	
	CFStringRef text_str = NULL;
	
	GetControlData(text_ref, kControlEntireControl, kControlEditTextCFStringTag, sizeof(CFStringRef), &text_str, NULL);
	
	if(text_str)
	{
		int val = CFStringGetIntValue(text_str);
		
		if(val >= 0 && val <= 100)	
			SetControlVal(window, 'Qual', 0, val);
		
		CFRelease(text_str);
	}
}


static pascal OSStatus
Out_WindowEventHandler( EventHandlerCallRef inCaller, EventRef inEvent, void* inRefcon )
{
#pragma unused( inCaller )

  OSStatus  result = eventNotHandledErr;
  
	switch ( GetEventClass( inEvent ) )
	{
		case kEventClassCommand:
		{
			HICommand  cmd;

			GetEventParameter( inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof( cmd ), NULL, &cmd );
			
			switch ( GetEventKind( inEvent ) )
			{
				case kEventCommandProcess:
					switch ( cmd.commandID )
					{
						case kHICommandOK:
						case kHICommandCancel:
							QuitAppModalLoopForWindow( (WindowRef)inRefcon );
							g_item_clicked = cmd.commandID;
							result = noErr;
						break;

						case 'Loss':
							TrackLossless((WindowRef)inRefcon);
						break;
						
						case 'Alph':
							TrackAlphaOut((WindowRef)inRefcon);
						break;
						
						case 'Slid':
							TrackSlider((WindowRef)inRefcon);
						break;
						
						case 'Text':
							TrackText((WindowRef)inRefcon);
						break;
						
						default:
						break;
					}
				break;
				
				default:
				break;
			}
			break;
		}
		
		default:
		break;
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
	bool result = false;

	CFStringRef bundle_id = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
															(const char *)plugHndl, 
															kCFStringEncodingUTF8,
															 kCFAllocatorNull);
	
	CFBundleRef bundle_ref = CFBundleGetBundleWithIdentifier(bundle_id);
	
	if(bundle_ref)
	{
		OSStatus err;
	
		CFRetain(bundle_ref);
		
		IBNibRef nib_ref = NULL;
		
		err = CreateNibReferenceWithCFBundle(bundle_ref, CFSTR("WebP_UI_Carbon"), &nib_ref);
		
		if(nib_ref)
		{
			WindowRef window = NULL;
			
			err = CreateWindowFromNib(nib_ref, CFSTR("Out Dialog"), &window);
			
			if(window)
			{
				// put image in HIImageView
				CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebP_banner.png"), NULL, NULL);
				
				HIViewRef banner_view = NULL;
				HIViewID  hiViewID = {'Banr', 0};
				
				err = HIViewFindByID(HIViewGetRoot(window), hiViewID, &banner_view);
				
				if(png_url && banner_view)
				{
					CGDataProviderRef png_provider = CGDataProviderCreateWithURL(png_url);
					
					CGImageRef png_image = CGImageCreateWithPNGDataProvider(png_provider, NULL, FALSE, kCGRenderingIntentDefault);
					
					err = HIImageViewSetImage(banner_view, png_image);
					err = HIViewSetVisible(banner_view, true);
					err = HIViewSetNeedsDisplay(banner_view, true);
					
					CGImageRelease(png_image);
					CFRelease(png_url);
				}
				
				SetControlVal(window, 'Loss', 0, params->lossless);
				SetControlVal(window, 'Qual', 0, params->quality);
				SetControlVal(window, 'Alph', 0, params->alpha + 1);
				SetControlVal(window, 'ACln', 0, params->alpha_cleanup);
				SetControlVal(window, 'ALos', 0, params->lossy_alpha);
				SetControlVal(window, 'Meta', 0, params->save_metadata);
				
				
				// manipulate alpha radio buttons
				ControlID alpha_id = {'Alph', 0};
				ControlRef alpha_control;
				GetControlByID(window, &alpha_id, &alpha_control);

				if(!have_transparency)
				{
					ControlRef transparency_radio;
					GetIndexedSubControl(alpha_control, 2, &transparency_radio);
					
					if(params->alpha == DIALOG_ALPHA_TRANSPARENCY)
					{
						DialogAlpha val = (alpha_name ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
						SetControlVal(window, 'Alph', 0, val + 1);
					}
					
					DisableControl(transparency_radio);
				}
				
				ControlRef channel_radio;
				GetIndexedSubControl(alpha_control, 3, &channel_radio);
				
				if(alpha_name)
				{
					CFStringRef alpha_ref = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, alpha_name, kCFStringEncodingUTF8, kCFAllocatorNull);
					
					SetControlTitleWithCFString(channel_radio, alpha_ref);
					
					CFRelease(alpha_ref);
				}
				else
				{
					if(params->alpha == DIALOG_ALPHA_CHANNEL)
					{
						DialogAlpha val = (have_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
						SetControlVal(window, 'Alph', 0, val + 1);
					}
					
					DisableControl(channel_radio);
				}


				TrackLossless(window);
				TrackSlider(window);
				TrackAlphaOut(window);

				

				// event handler
				EventTypeSpec  kWindowEvents[] =  {  { kEventClassCommand, kEventCommandProcess } };
				EventHandlerUPP windowUPP = NewEventHandlerUPP( Out_WindowEventHandler );
				
				InstallWindowEventHandler(window, windowUPP, GetEventTypeCount( kWindowEvents ), kWindowEvents, window, NULL );
				
				
				// show the window
				RepositionWindow(window, NULL, kWindowCenterOnMainScreen);
				ShowWindow(window);
				
				
				SetThemeCursor(kThemeArrowCursor); // Photoshop will have set the cursor to the watch

				// event loop
				RunAppModalLoopForWindow(window);
				
				if(g_item_clicked == kHICommandOK)
				{
					params->lossless = (GetControlVal(window, 'Loss', 0) == 1);
					params->quality = GetControlVal(window, 'Qual', 0);
					params->alpha = (DialogAlpha)(GetControlVal(window, 'Alph', 0) - 1);
					params->alpha_cleanup = GetControlVal(window, 'ACln', 0);
					params->lossy_alpha = GetControlVal(window, 'ALos', 0);
					params->save_metadata = GetControlVal(window, 'Meta', 0);
				
					result = true;
				}
					
				DisposeWindow(window);
			}
			
			DisposeNibReference(nib_ref);
		}
	
		CFRelease(bundle_ref);
	}
	
	CFRelease(bundle_id);


	return result;
}


void
WebP_About(
	const char		*plugin_version_string,
	const char		*WebP_version_string,
	const void		*plugHndl,
	const void		*mwnd)
{
	CFStringRef bundle_id = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
															(const char *)plugHndl, 
															kCFStringEncodingUTF8,
															 kCFAllocatorNull);
	
	CFBundleRef bundle_ref = CFBundleGetBundleWithIdentifier(bundle_id);
	
	if(bundle_ref)
	{
		OSStatus err;
	
		CFRetain(bundle_ref);
		
		IBNibRef nib_ref = NULL;
		
		err = CreateNibReferenceWithCFBundle(bundle_ref, CFSTR("WebP_UI_Carbon"), &nib_ref);
		
		if(nib_ref)
		{
			WindowRef window = NULL;
			
			err = CreateWindowFromNib(nib_ref, CFSTR("About Dialog"), &window);
			
			if(window)
			{
				// put image in HIImageView
				CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebP_banner.png"), NULL, NULL);
				
				HIViewRef banner_view = NULL;
				HIViewID  hiViewID = {'Banr', 0};
				
				err = HIViewFindByID(HIViewGetRoot(window), hiViewID, &banner_view);
				
				if(png_url && banner_view)
				{
					CGDataProviderRef png_provider = CGDataProviderCreateWithURL(png_url);
					
					CGImageRef png_image = CGImageCreateWithPNGDataProvider(png_provider, NULL, FALSE, kCGRenderingIntentDefault);
					
					err = HIImageViewSetImage(banner_view, png_image);
					err = HIViewSetVisible(banner_view, true);
					err = HIViewSetNeedsDisplay(banner_view, true);
					
					CGImageRelease(png_image);
					CFRelease(png_url);
				}
			
				ControlID plug_vers_id = {'Plug', 0};
				ControlID webp_vers_id = {'WebP', 0};
				
				ControlRef plug_vers_ref;
				ControlRef webp_vers_ref;
				
				GetControlByID(window, &plug_vers_id, &plug_vers_ref);
				GetControlByID(window, &webp_vers_id, &webp_vers_ref);
				
				CFStringRef plug_vers_str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, plugin_version_string, kCFStringEncodingUTF8, kCFAllocatorNull);
				CFStringRef webp_vers_str = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, WebP_version_string, kCFStringEncodingUTF8, kCFAllocatorNull);
				
				SetControlData(plug_vers_ref, kControlEntireControl, kControlEditTextCFStringTag, sizeof(CFStringRef), &plug_vers_str);
				SetControlData(webp_vers_ref, kControlEntireControl, kControlEditTextCFStringTag, sizeof(CFStringRef), &webp_vers_str);
				
				CFRelease(plug_vers_str);
				CFRelease(webp_vers_str);
				

				// event handler
				EventTypeSpec  kWindowEvents[] =  {  { kEventClassCommand, kEventCommandProcess } };
				EventHandlerUPP windowUPP = NewEventHandlerUPP( Out_WindowEventHandler );
				
				InstallWindowEventHandler(window, windowUPP, GetEventTypeCount( kWindowEvents ), kWindowEvents, window, NULL );
				
				
				// show the window
				RepositionWindow(window, NULL, kWindowCenterOnMainScreen);
				ShowWindow(window);
				
				
				// event loop
				RunAppModalLoopForWindow(window);
				
					
				DisposeWindow(window);
			}
			
			DisposeNibReference(nib_ref);
		}
	
		CFRelease(bundle_ref);
	}
	
	CFRelease(bundle_id);
}

#endif //!__LP64__
