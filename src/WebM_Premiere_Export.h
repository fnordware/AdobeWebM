

#ifndef WEBM_PREMIERE_EXPORT_H
#define WEBM_PREMIERE_EXPORT_H

#include	"PrSDKStructs.h"
#include	"PrSDKExport.h"
#include	"PrSDKExportFileSuite.h"
#include	"PrSDKExportInfoSuite.h"
#include	"PrSDKExportParamSuite.h"
#include	"PrSDKSequenceRenderSuite.h"
#include	"PrSDKExportProgressSuite.h"
#include	"PrSDKPPix2Suite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKMemoryManagerSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKAppInfoSuite.h"
#ifdef		PRMAC_ENV
#include	<wchar.h>
#endif

extern "C" {
DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParms, 
	void			*param1, 
	void			*param2);
}


#endif // WEBM_PREMIERE_EXPORT_H