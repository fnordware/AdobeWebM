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
// WebM plug-in for Premiere
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------




#ifndef WEBM_PREMIERE_IMPORT_H
#define WEBM_PREMIERE_IMPORT_H

#include	"PrSDKStructs.h"
#include	"PrSDKImport.h"
#include	"PrSDKExport.h"
#include	"PrSDKExportFileSuite.h"
#include	"PrSDKExportInfoSuite.h"
#include	"PrSDKExportParamSuite.h"
#include	"PrSDKExportProgressSuite.h"
#include	"PrSDKErrorSuite.h"
#include	"PrSDKMALErrors.h"
#include	"PrSDKMarkerSuite.h"
#include	"PrSDKSequenceRenderSuite.h"
#include	"PrSDKSequenceAudioSuite.h"
#include	"PrSDKClipRenderSuite.h"
#include	"PrSDKPPix2Suite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKImporterFileManagerSuite.h"
#include	"PrSDKMemoryManagerSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKAppInfoSuite.h"
#include	"SDK_Segment_Utils.h"
#ifdef		PRMAC_ENV
#include	<wchar.h>
#endif



#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef long PrivateDataPtr;

typedef int csSDK_int32;
typedef int csSDK_size_t;
typedef long int RowbyteType;

#define CAST_REFNUM(REFNUM)		(REFNUM)
#define CAST_FILEREF(FILEREF)	(FILEREF)

#ifdef PRMAC_ENV
typedef SInt16 FSIORefNum;
#endif

#else
typedef void * PrivateDataPtr;
typedef csSDK_int32 RowbyteType;

#ifdef PRMAC_ENV
#define CAST_REFNUM(REFNUM)		reinterpret_cast<intptr_t>(REFNUM)
#define CAST_FILEREF(FILEREF)	reinterpret_cast<imFileRef>(FILEREF)
#else
#define CAST_REFNUM(REFNUM)		(REFNUM)
#define CAST_FILEREF(FILEREF)	(FILEREF)
#endif

#endif

// Declare plug-in entry point with C linkage
extern "C" {
PREMPLUGENTRY DllExport xImportEntry (csSDK_int32	selector, 
									  imStdParms	*stdParms, 
									  void			*param1, 
									  void			*param2);

}

#endif //WEBM_PREMIERE_IMPORT_H