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

#ifndef __WebP_Photoshop_H__
#define __WebP_Photoshop_H__


#include "PIDefines.h"
#include "PIFormat.h"
#include "PIUtilities.h"
#include "PIProperties.h"


enum {
	WEBP_ALPHA_NONE = 0,
	WEBP_ALPHA_TRANSPARENCY,
	WEBP_ALPHA_CHANNEL
};
typedef uint8 WebP_Alpha;

typedef struct {
	WebP_Alpha	alpha;
	Boolean		mult;
	
} WebP_inData;

typedef struct {
	Boolean			lossless;
	uint8			quality;
	WebP_Alpha		alpha;
	Boolean			lossy_alpha;
	Boolean			alpha_cleanup;
	Boolean			save_metadata;
	uint8			reserved[250];
	
} WebP_outData;


typedef struct Globals
{ // This is our structure that we use to pass globals between routines:

	short				*result;			// Must always be first in Globals.
	FormatRecord		*formatParamBlock;	// Must always be second in Globals.

	Handle				fileH;				// stores the entire binary file
	
	WebP_inData			in_options;
	WebP_outData		options;
	
} Globals, *GPtr, **GHdl;				// *GPtr = global pointer; **GHdl = global handle
	
// The routines that are dispatched to from the jump list should all be
// defined as
//		void RoutineName (GPtr globals);
// And this typedef will be used as the type to create a jump list:
typedef void (* FProc)(GPtr globals);


//-------------------------------------------------------------------------------
//	Globals -- definitions and macros
//-------------------------------------------------------------------------------

#define gResult				(*(globals->result))
#define gStuff				(globals->formatParamBlock)

#define gInOptions			(globals->in_options)
#define gOptions			(globals->options)

//-------------------------------------------------------------------------------
//	Prototypes
//-------------------------------------------------------------------------------


// Everything comes in and out of PluginMain. It must be first routine in source:
DLLExport MACPASCAL void PluginMain (const short selector,
					  	             FormatRecord *formatParamBlock,
						             intptr_t *data,
						             short *result);

// Scripting functions
Boolean ReadScriptParamsOnWrite (GPtr globals);	// Read any scripting params.
OSErr WriteScriptParamsOnWrite (GPtr globals);	// Write any scripting params.

//-------------------------------------------------------------------------------

#endif // __WebP_Photoshop_H__
