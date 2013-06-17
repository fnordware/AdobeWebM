

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
					CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebM_banner.png"), NULL, NULL);
					
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
	ControlID cid = {'Qual', 0};
	ControlRef cref;
	
	GetControlByID(window, &cid, &cref);
	
	if( GetControlVal(window, 'Loss', 0) )
		DisableControl(cref);
	else
		EnableControl(cref);
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
				CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebM_banner.png"), NULL, NULL);
				
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
				
				TrackLossless(window);
				
				
				// manipulate radio buttons
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

				
				

				// event handler
				EventTypeSpec  kWindowEvents[] =  {  { kEventClassCommand, kEventCommandProcess } };
				EventHandlerUPP windowUPP = NewEventHandlerUPP( Out_WindowEventHandler );
				
				InstallWindowEventHandler(window, windowUPP, GetEventTypeCount( kWindowEvents ), kWindowEvents, window, NULL );
				
				
				// show the window
				RepositionWindow(window, NULL, kWindowCenterOnMainScreen);
				ShowWindow(window);
				
				
				// event loop
				RunAppModalLoopForWindow(window);
				
				if(g_item_clicked == kHICommandOK)
				{
					params->lossless = GetControlVal(window, 'Loss', 0);
					params->quality = GetControlVal(window, 'Qual', 0);
					params->alpha = (DialogAlpha)(GetControlVal(window, 'Alph', 0) - 1);
				
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
				CFURLRef png_url = CFBundleCopyResourceURL(bundle_ref, CFSTR("WebM_banner.png"), NULL, NULL);
				
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
