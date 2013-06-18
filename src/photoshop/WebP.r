
//
// WebP
//
// by Brendan Bolles
//

//-------------------------------------------------------------------------------
//	Definitions -- Required by include files.
//-------------------------------------------------------------------------------

#include "WebP_version.h"

#define plugInName			"WebP"
#define plugInCopyrightYear	WebP_Copyright_Year
#define plugInDescription WebP_Description
#define VersionString 	WebP_Version_String
#define ReleaseString	WebP_Build_Date_Manual
#define CurrentYear		WebP_Build_Year

//-------------------------------------------------------------------------------
//	Definitions -- Required by other resources in this rez file.
//-------------------------------------------------------------------------------

// Dictionary (aete) resources:

#define vendorName			"fnord"
#define plugInAETEComment 	WebP_Description

#define plugInSuiteID		'sdK4'
#define plugInClassID		'WebP'
#define plugInEventID		typeNull // must be this

//-------------------------------------------------------------------------------
//	Set up included files for Macintosh and Windows.
//-------------------------------------------------------------------------------

#include "PIDefines.h"

#ifdef __PIMac__
	#include "Types.r"
	#include "SysTypes.r"
	#include "PIGeneral.r"
	//#include "PIUtilities.r"
	//#include "DialogUtilities.r"
#elif defined(__PIWin__)
	#include "PIGeneral.h"
	//#include "PIUtilities.r"
	//#include "WinDialogUtils.r"
#endif

#ifndef ResourceID
	#define ResourceID		16000
#endif

#include "PITerminology.h"
#include "PIActions.h"

#include "WebP_Terminology.h"

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (ResourceID, plugInName " PiPL", purgeable)
{
    {
		Kind { ImageFormat },
		Name { plugInName },

		//Category { "WebP" },
		//Priority { 1 }, // Can use this to override a built-in Photoshop plug-in

		Version { (latestFormatVersion << 16) | latestFormatSubVersion },

		#ifdef __PIMac__
			#ifdef BUILDING_FOR_MACH
				#if (defined(__x86_64__))
					CodeMacIntel64 { "PluginMain" },
				#endif
				#if (defined(__i386__))
					CodeMacIntel32 { "PluginMain" },
				#endif
				#if (defined(__ppc__))
					CodeMachOPowerPC { 0, 0, "PluginMain" },
				#endif
			#else
				#if TARGET_CARBON
			        CodeCarbonPowerPC { 0, 0, "" },
			    #else
					CodePowerPC { 0, 0, "" },		
				#endif
			#endif
		#else
			#if defined(_WIN64)
				CodeWin64X86 { "PluginMain" },
			#else
				CodeWin32X86 { "PluginMain" },
			#endif
		#endif
	
		// ClassID, eventID, aete ID, uniqueString:
		HasTerminology { plugInClassID, plugInEventID, ResourceID, vendorName " " plugInName },
		
		SupportedModes
		{
			noBitmap, noGrayScale,
			noIndexedColor, doesSupportRGBColor,
			noCMYKColor, noHSLColor,
			noHSBColor, noMultichannel,
			noDuotone, noLABColor
		},
			
		EnableInfo { "in (PSHOP_ImageMode, RGBMode)" },
	
		FmtFileType { 'WebP', '8BIM' },
		ReadTypes { { 'WebP', '    ' } },
		ReadExtensions { { 'webp' } },
		WriteExtensions { { 'webp' } },
		FilteredExtensions { { 'webp' } },
		FormatFlags { fmtSavesImageResources, //(by saying we do, PS won't store them, thereby avoiding problems)
		              fmtCanRead, 
					  fmtCanWrite, 
					  fmtCanWriteIfRead, 
					  fmtCanWriteTransparency,
					  fmtCannotCreateThumbnail },
		PlugInMaxSize { 16384, 16384 },
		FormatMaxSize { { 16384, 16384 } },
		FormatMaxChannels { {   0, 0, 0, 5, 0, 0, 
							   0, 0, 0, 0, 0, 0 } },
		FormatICCFlags { 	iccCanEmbedGray,
							iccCanEmbedIndexed,
							iccCanEmbedRGB,
							iccCannotEmbedCMYK },
		XMPWrite { },
		XMPRead { }
		},
	};


//-------------------------------------------------------------------------------
//	PiMI resource (kept for backward compatibility)
//-------------------------------------------------------------------------------

resource 'PiMI' (ResourceID, plugInName " PiMI", purgeable)
{
	latestFormatVersion, 	/* Version, subVersion, and priority of the interface */
	latestFormatSubVersion,
	0,

	supportsGrayScale +
	supportsRGBColor,			/* Supported Image Modes */
	'    ',						/* Required host */
	
	{
		canRead,
		cannotReadAll,
		canWrite,
		canWriteIfRead,
		savesResources,
		{  0, 0, 0, 5,		/* Maximum # of channels for each plug-in mode */
		  0, 0, 0, 0,
		  0, 0,  0,  0,
		   0,  0,  0,  0 },
		32767,				/* Maximum rows allowed in document */
		32767,				/* Maximum columns allowed in document */
		'WebM',				/* The file type if we create a file. */
		'8BIM',				/* The creator type if we create a file. */
		{					/* The type-creator pairs supported. */
			'8B1F', '    '
		},
		{					/* The extensions supported. */
		}
	},
	
};

//-------------------------------------------------------------------------------
//	Dictionary (scripting) resource
//-------------------------------------------------------------------------------

resource 'aete' (ResourceID, plugInName " dictionary", purgeable)
{
	1, 0, english, roman,									/* aete version and language specifiers */
	{
		vendorName,											/* vendor suite name */
		"WebP format",							/* optional description */
		plugInSuiteID,										/* suite ID */
		1,													/* suite code, must be 1 */
		1,													/* suite level, must be 1 */
		{},													/* structure for filters */
		{													/* non-filter plug-in class here */
			"WebP",										/* unique class name */
			plugInClassID,									/* class ID, must be unique or Suite ID */
			plugInAETEComment,								/* optional description */
			{												/* define inheritance */
				"$$$/private/AETE/Inheritance=<Inheritance>",							/* must be exactly this */
				keyInherits,								/* must be keyInherits */
				classFormat,								/* parent: Format, Import, Export */
				"parent class format",						/* optional description */
				flagsSingleProperty,						/* if properties, list below */
							
				"Lossless",
				keyWebPlossless,
				typeBoolean,
				"WebP lossless compression used",
				flagsSingleProperty,
				
				"Quality",
				keyWebPquality,
				typeInteger,
				"WebP compression quality",
				flagsSingleProperty,
				
				"Alpha Channel",
				keyWebPalpha,
				typeEnumerated,
				"Source of the alpha channel",
				flagsSingleProperty

				/* no properties */
			},
			{}, /* elements (not supported) */
			/* class descriptions */
		},
		{}, /* comparison ops (not supported) */
		{	/* any enumerations */
			typeAlphaChannel,
			{
                "None",
                alphaChannelNone,
                "No alpha channel",

                "Transparency",
                alphaChannelTransparency,
                "Get alpha from Transparency",

                "Channel",
                alphaChannelChannel,
                "Get alpha from channels palette"
			}
		}
	}
};

#ifdef __PIMac__

//-------------------------------------------------------------------------------
//	Version 'vers' resources.
//-------------------------------------------------------------------------------

resource 'vers' (1, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	VersionString " ©" plugInCopyrightYear " fnord"
};

resource 'vers' (2, plugInName " Version", purgeable)
{
	5, 0x50, final, 0, verUs,
	VersionString,
	"by Brendan Bolles"
};


#endif // __PIMac__


