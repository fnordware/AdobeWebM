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



#ifndef WEBM_PREMIERE_EXPORT_H
#define WEBM_PREMIERE_EXPORT_H

#include	"PrSDKStructs.h"
#include	"PrSDKExport.h"
#include	"PrSDKExportFileSuite.h"
#include	"PrSDKExportInfoSuite.h"
#include	"PrSDKExportParamSuite.h"
#include	"PrSDKSequenceRenderSuite.h"
#include	"PrSDKSequenceAudioSuite.h"
#include	"PrSDKExportProgressSuite.h"
#include	"PrSDKPPix2Suite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKMemoryManagerSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKAppInfoSuite.h"


typedef struct ExportSettings
{
	SPBasicSuite				*spBasic;
	PrSDKExportParamSuite		*exportParamSuite;
	PrSDKExportInfoSuite		*exportInfoSuite;
	PrSDKExportFileSuite		*exportFileSuite;
	PrSDKExportProgressSuite	*exportProgressSuite;
	PrSDKPPixCreatorSuite		*ppixCreatorSuite;
	PrSDKPPixSuite				*ppixSuite;
	PrSDKPPix2Suite				*ppix2Suite;
	PrSDKTimeSuite				*timeSuite;
	PrSDKMemoryManagerSuite		*memorySuite;
	PrSDKSequenceRenderSuite	*sequenceRenderSuite;
	PrSDKSequenceAudioSuite		*sequenceAudioSuite;
	PrSDKWindowSuite			*windowSuite;
} ExportSettings;


#define WEBM_PLUGIN_VERSION_MAJOR	1
#define WEBM_PLUGIN_VERSION_MINOR	0
#define WEBM_PLUGIN_VERSION_BUILD	0


extern "C" {

DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParms, 
	void			*param1, 
	void			*param2);
	
}


#endif // WEBM_PREMIERE_EXPORT_H