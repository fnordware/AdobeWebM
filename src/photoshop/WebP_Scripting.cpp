
//
// SuperPNG
//
// by Brendan Bolles
//

#include "PIDefines.h"
#include "WebP.h"

		

static WebP_Alpha KeyToAlpha(OSType key)
{
	return	(key == alphaChannelNone)			? WEBP_ALPHA_NONE :
			(key == alphaChannelTransparency)	? WEBP_ALPHA_TRANSPARENCY :
			(key == alphaChannelChannel)		? WEBP_ALPHA_CHANNEL :
			WEBP_ALPHA_TRANSPARENCY;
}

Boolean ReadScriptParamsOnWrite(GPtr globals)
{
	PIReadDescriptor			token = NULL;
	DescriptorKeyID				key = 0;
	DescriptorTypeID			type = 0;
	OSType						shape = 0, create = 0;
	DescriptorKeyIDArray		array = { NULLID };
	int32						flags = 0;
	OSErr						gotErr = noErr, stickyError = noErr;
	Boolean						returnValue = true;
	int32						storeValue;
	DescriptorEnumID			ostypeStoreValue;
	Boolean						boolStoreValue;
	
	if (DescriptorAvailable(NULL))
	{
		token = OpenReader(array);
		if (token)
		{
			while (PIGetKey(token, &key, &type, &flags))
			{
				switch (key)
				{
/*					case keyPNGcompression:
							PIGetInt(token, &storeValue);
							gOptions.compression = storeValue;
							break;
					
					case keyPNGfilter:
							PIGetInt(token, &storeValue);
							gOptions.filter = storeValue;
							break;

					case keyPNGstrategy:
							PIGetInt(token, &storeValue);
							gOptions.strategy = storeValue;
							break;

					case keyPNGinterlace:
							PIGetBool(token, &boolStoreValue);
							gOptions.interlace = boolStoreValue;
							break;
					
					case keyPNGmeta:
							PIGetBool(token, &boolStoreValue);
							gOptions.metadata = boolStoreValue;
							break;
*/					
					case keyWebPalpha:
							PIGetEnum(token, &ostypeStoreValue);
							gOptions.alpha = KeyToAlpha(ostypeStoreValue);
							break;
				}
			}

			stickyError = CloseReader(&token); // closes & disposes.
				
			if (stickyError)
			{
				if (stickyError == errMissingParameter) // missedParamErr == -1715
					;
					/* (descriptorKeyIDArray != NULL)
					   missing parameter somewhere.  Walk IDarray to find which one. */
				else
					gResult = stickyError;
			}
		}
		
		returnValue = PlayDialog();
		// return TRUE if want to show our Dialog
	}
	
	return returnValue;
}

		

static OSType AlphaToKey(WebP_Alpha alpha)
{
	return	(alpha == WEBP_ALPHA_NONE)			? alphaChannelNone :
			(alpha == WEBP_ALPHA_TRANSPARENCY)	? alphaChannelTransparency :
			(alpha == WEBP_ALPHA_CHANNEL)		? alphaChannelChannel :
			alphaChannelTransparency;
}

OSErr WriteScriptParamsOnWrite(GPtr globals)
{
	PIWriteDescriptor			token = nil;
	OSErr						gotErr = noErr;
			
	if (DescriptorAvailable(NULL))
	{
		token = OpenWriter();
		if (token)
		{
			// write keys here
/*			PIPutInt(token, keyPNGcompression, gOptions.compression);
			PIPutInt(token, keyPNGfilter, gOptions.filter);
			PIPutInt(token, keyPNGstrategy, gOptions.strategy);
			PIPutBool(token, keyPNGinterlace, gOptions.interlace);
			PIPutBool(token, keyPNGmeta, gOptions.metadata);
*/			PIPutEnum(token, keyWebPalpha, typeAlphaChannel, AlphaToKey(gOptions.alpha));
			gotErr = CloseWriter(&token); /* closes and sets dialog optional */
			/* done.  Now pass handle on to Photoshop */
		}
	}
	return gotErr;
}


