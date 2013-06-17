//
// WebP
//
// by Brendan Bolles
//

#ifndef __WebP_Photoshop_H__
#define __WebP_Photoshop_H__


#include "PIDefines.h"
#include "PIFormat.h"
#include "PIUtilities.h"

#include "WebP_Terminology.h"


enum {
	WEBP_ALPHA_NONE = 0,
	WEBP_ALPHA_TRANSPARENCY,
	WEBP_ALPHA_CHANNEL
};
typedef uint8 WebP_Alpha;

typedef struct {
	WebP_Alpha	alpha;
	bool		mult;
	
} WebP_inData;

typedef struct {
	bool			lossless;
	uint8			quality;
	WebP_Alpha		alpha;
	uint8			reserved[253];
	
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

// funcs living in other files
void SuperPNG_VerifyFile(GPtr globals);
void SuperPNG_FileInfo(GPtr globals);
void SuperPNG_ReadFile(GPtr globals);

void SuperPNG_WriteFile(GPtr globals);

// my backward compatible buffer and handle routines
/*
Handle myNewHandle(GPtr globals, const int32 inSize);
Ptr myLockHandle(GPtr globals, Handle h);
void myUnlockHandle(GPtr globals, Handle h);
int32 myGetHandleSize(GPtr globals, Handle h);
void mySetHandleSize(GPtr globals, Handle h, const int32 inSize);
void myDisposeHandle(GPtr globals, Handle h);

OSErr myAllocateBuffer(GPtr globals, const int32 inSize, BufferID *outBufferID);
Ptr myLockBuffer(GPtr globals, const BufferID inBufferID, Boolean inMoveHigh);
void myFreeBuffer(GPtr globals, const BufferID inBufferID);
*/

// Scripting functions
Boolean ReadScriptParamsOnWrite (GPtr globals);	// Read any scripting params.
OSErr WriteScriptParamsOnWrite (GPtr globals);	// Write any scripting params.

//-------------------------------------------------------------------------------

#endif // __WebP_Photoshop_H__
