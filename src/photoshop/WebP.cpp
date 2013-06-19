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

#include "WebP.h"

#include "WebP_version.h"
#include "WebP_UI.h"


#include "webp/demux.h"
#include "webp/mux.h"
#include "webp/decode.h"
#include "webp/encode.h"

#include <stdio.h>
#include <assert.h>

// globals needed by a bunch of Photoshop SDK routines
#ifdef __PIWin__
HINSTANCE hDllInstance = NULL;
#endif

SPBasicSuite * sSPBasic = NULL;
SPPluginRef gPlugInRef = NULL;


static void DoAbout(AboutRecordPtr aboutP)
{
#ifdef __PIMac__
	const char * const plugHndl = "com.fnordware.Photoshop.WebP";
	const void *hwnd = aboutP;	
#else
	const char * const plugHndl = NULL;
	HWND hwnd = (HWND)((PlatformData *)aboutP->platformData)->hwnd;
#endif

	const int version = WebPGetEncoderVersion();
	
	char version_string[32];
	
	sprintf(version_string, "WebP version %d.%d.%d", 
				(version >> 16) & 0xff,
				(version >> 8) & 0xff,
				(version >> 0) & 0xff);
				

	WebP_About(WebP_Build_Complete_Manual, version_string, plugHndl, hwnd);
}

#pragma mark-

static void InitGlobals(Ptr globalPtr)
{	
	// create "globals" as a our struct global pointer so that any
	// macros work:
	GPtr globals = (GPtr)globalPtr;
		
	globals->fileH				= NULL;
	
	gInOptions.alpha			= WEBP_ALPHA_TRANSPARENCY;
	gInOptions.mult				= FALSE;
	
	gOptions.quality			= 50;
	gOptions.lossless			= TRUE;
	gOptions.alpha				= WEBP_ALPHA_TRANSPARENCY;
}


static Handle myNewHandle(GPtr globals, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		return gStuff->handleProcs->newProc(inSize);
	}
	else
	{
		return PINewHandle(inSize);
	}
}

static Ptr myLockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->lockProc)
	{
		return gStuff->handleProcs->lockProc(h, TRUE);
	}
	else
	{
		return PILockHandle(h, TRUE);
	}
}

static void myUnlockHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->unlockProc)
	{
		gStuff->handleProcs->unlockProc(h);
	}
	else
	{
		PIUnlockHandle(h);
	}
}

static int32 myGetHandleSize(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->getSizeProc)
	{
		return gStuff->handleProcs->getSizeProc(h);
	}
	else
	{
		return PIGetHandleSize(h);
	}
}

static void mySetHandleSize(GPtr globals, Handle h, const int32 inSize)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->setSizeProc)
	{
		gStuff->handleProcs->setSizeProc(h, inSize);
	}
	else
	{
		PISetHandleSize(h, inSize);
	}
}

static void myDisposeHandle(GPtr globals, Handle h)
{
	if(gStuff->handleProcs != NULL && gStuff->handleProcs->numHandleProcs >= 6 && gStuff->handleProcs->newProc != NULL)
	{
		gStuff->handleProcs->disposeProc(h);
	}
	else
	{
		PIDisposeHandle(h);
	}
}

static OSErr myAllocateBuffer(GPtr globals, const int32 inSize, BufferID *outBufferID)
{
	*outBufferID = 0;
	
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->allocateProc != NULL)
		gResult = gStuff->bufferProcs->allocateProc(inSize, outBufferID);
	else
		gResult = memFullErr;

	return gResult;
}

static Ptr myLockBuffer(GPtr globals, const BufferID inBufferID, Boolean inMoveHigh)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->lockProc != NULL)
		return gStuff->bufferProcs->lockProc(inBufferID, inMoveHigh);
	else
		return NULL;
}

static void myFreeBuffer(GPtr globals, const BufferID inBufferID)
{
	if(gStuff->bufferProcs != NULL && gStuff->bufferProcs->numBufferProcs >= 4 && gStuff->bufferProcs->freeProc != NULL)
		gStuff->bufferProcs->freeProc(inBufferID);
}


#pragma mark-


static size_t my_fread(GPtr globals, void * buf, size_t len)
{
#ifdef __PIMac__
	ByteCount count = len;
	
	OSErr result = FSReadFork(gStuff->dataFork, fsAtMark, 0, count, buf, &count);
	
	return count;
#else
	DWORD count = len, bytes_read = 0;
	
	BOOL result = ReadFile((HANDLE)gStuff->dataFork, (LPVOID)buf, count, &bytes_read, NULL);

	return bytes_read;
#endif
}

static bool my_fwrite(GPtr globals, const void * buf, size_t len)
{
#ifdef __PIMac__
	ByteCount count = len;

	OSErr result = FSWriteFork(gStuff->dataFork, fsAtMark, 0, count, (const void *)buf, &count);
	
	return (result == noErr && count == len);
#else
	DWORD count = len, out = 0;
	
	BOOL result = WriteFile((HANDLE)gStuff->dataFork, (LPVOID)buf, count, &out, NULL);
	
	return (result && out == count);
#endif
}

static int my_fseek(GPtr globals, long offset, int whence)
{
#ifdef __PIMac__
	UInt16 positionMode = ( whence == SEEK_SET ? fsFromStart :
							whence == SEEK_CUR ? fsFromMark :
							whence == SEEK_END ? fsFromLEOF :
							fsFromMark );
	
	OSErr result = FSSetForkPosition(gStuff->dataFork, positionMode, offset);

	return result;
#else
	LARGE_INTEGER lpos;

	lpos.QuadPart = offset;

	DWORD method = ( whence == SEEK_SET ? FILE_BEGIN :
						whence == SEEK_CUR ? FILE_CURRENT :
						whence == SEEK_END ? FILE_END :
						FILE_CURRENT );

#if _MSC_VER < 1300
	DWORD pos = SetFilePointer((HANDLE)gStuff->dataFork, lpos.u.LowPart, &lpos.u.HighPart, method);

	BOOL result = (pos != 0xFFFFFFFF || NO_ERROR == GetLastError());
#else
	BOOL result = SetFilePointerEx((HANDLE)gStuff->dataFork, lpos, NULL, method);
#endif

	return (result ? 0 : 1);
#endif
}

static long my_ftell(GPtr globals)
{
#ifdef __PIMac__
	SInt64 lpos;

	OSErr result = FSGetForkPosition(gStuff->dataFork, &lpos);
	
	return lpos;
#else
	LARGE_INTEGER lpos, zero;

	zero.QuadPart = 0;

	BOOL result = SetFilePointerEx((HANDLE)gStuff->dataFork, zero, &lpos, FILE_CURRENT);

	return lpos.QuadPart;
#endif
}

static long my_GetFileSize(GPtr globals)
{
#ifdef __PIMac__
	SInt64 fork_size = 0;
	
	OSErr result = FSGetForkSize(gStuff->dataFork, &fork_size);
		
	return fork_size;
#else
	return GetFileSize((HANDLE)gStuff->dataFork, NULL);
#endif
}


#pragma mark-


static void DoFilterFile(GPtr globals)
{
	// copied from ParseRiff()
#define RIFF_HEADER_SIZE 12
#define TAG_SIZE 4

	uint8_t buf[RIFF_HEADER_SIZE];

	my_fseek(globals, 0, SEEK_SET);
	
	if(RIFF_HEADER_SIZE == my_fread(globals, buf, RIFF_HEADER_SIZE))
	{
		if(!memcmp(buf, "RIFF", TAG_SIZE) && !memcmp(buf + 8, "WEBP", TAG_SIZE))
		{
			// we're fine then
		}
		else
			gResult = formatCannotRead;
	}
	else
		gResult = formatCannotRead;
}


// Additional parameter functions
//   These transfer settings to and from gStuff->revertInfo

static void TwiddleOptions(WebP_inData *options)
{
#ifndef __PIMacPPC__
	// none
#endif
}

static void TwiddleOptions(WebP_outData *options)
{
#ifndef __PIMacPPC__
	// none
#endif
}

template <typename T>
static bool ReadParams(GPtr globals, T *options)
{
	bool found_revert = FALSE;
	
	if( gStuff->revertInfo != NULL )
	{
		if( myGetHandleSize(globals, gStuff->revertInfo) == sizeof(T) )
		{
			T *flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
			
			// flatten and copy
			TwiddleOptions(flat_options);
			
			memcpy((char*)options, (char*)flat_options, sizeof(T) );
			
			TwiddleOptions(flat_options);
			
			myUnlockHandle(globals, gStuff->revertInfo);
			
			found_revert = TRUE;
		}
	}
	
	return found_revert;
}

template <typename T>
static void WriteParams(GPtr globals, T *options)
{
	T *flat_options = NULL;
	
	if(gStuff->hostNewHdl != NULL) // we have the handle function
	{
		if(gStuff->revertInfo == NULL)
		{
			gStuff->revertInfo = myNewHandle(globals, sizeof(T) );
		}
		else
		{
			if(myGetHandleSize(globals, gStuff->revertInfo) != sizeof(T)  )
				mySetHandleSize(globals, gStuff->revertInfo, sizeof(T) );
		}
		
		flat_options = (T *)myLockHandle(globals, gStuff->revertInfo);
		
		// flatten and copy
		TwiddleOptions(flat_options);
		
		memcpy((char*)flat_options, (char*)options, sizeof(T) );	
		
		TwiddleOptions(flat_options);
			
		
		myUnlockHandle(globals, gStuff->revertInfo);
	}
}


static void DoReadPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoReadStart(GPtr globals)
{
	bool reverting = ReadParams(globals, &gInOptions);
	
	if(gResult == noErr)
	{
		assert(globals->fileH == NULL);
		
		long file_size = my_GetFileSize(globals);
		
		globals->fileH = myNewHandle(globals, file_size);
		
		if(globals->fileH)
		{
			Ptr buf = myLockHandle(globals, globals->fileH);
			
			my_fseek(globals, 0, SEEK_SET);
			
			if(file_size == my_fread(globals, buf, file_size))
			{
				WebPData webp_data = { (const uint8_t *)buf, file_size };

				WebPDemuxer *demux = WebPDemux(&webp_data);

				if(demux)
				{
					uint32_t width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
					uint32_t height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);
					uint32_t flags = WebPDemuxGetI(demux, WEBP_FF_FORMAT_FLAGS);
					
					bool has_alpha = (flags & ALPHA_FLAG);
					
					// check the bitstream to see if we REALLY have an alpha
					// (lossless images are always compressed with an alpha)
					WebPIterator iter;
					
					if(has_alpha && WebPDemuxGetFrame(demux, 0, &iter) )
					{
						WebPBitstreamFeatures features;
					
						VP8StatusCode status = WebPGetFeatures(iter.fragment.bytes, iter.fragment.size, &features);
						
						has_alpha = features.has_alpha;
						
						WebPDemuxReleaseIterator(&iter);
					}

					assert(WebPDemuxGetI(demux, WEBP_FF_FRAME_COUNT) >= 1);
					

					if(!reverting)
					{
						WebP_InUI_Data params;
						
					#ifdef __PIMac__
						const char * const plugHndl = "com.fnordware.Photoshop.WebP";
						const void *hwnd = globals;	
					#else
						const char *const plugHndl = NULL;
						HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
					#endif

						// WebP_InUI is responsible for not popping a dialog if the user
						// didn't request it.  It still has to set the read settings from preferences though.
						bool result = WebP_InUI(&params, has_alpha, plugHndl, hwnd);
						
						if(result)
						{
							gInOptions.alpha = params.alpha;
							gInOptions.mult = params.mult;
							
							WriteParams(globals, &gInOptions);
						}
						else
							gResult = userCanceledErr;
					}
					
					
					if(gResult == noErr)
					{
						gStuff->imageMode = plugInModeRGBColor;
						gStuff->depth = 8;

						gStuff->imageSize.h = gStuff->imageSize32.h = width;
						gStuff->imageSize.v = gStuff->imageSize32.v = height;
						
						gStuff->planes = (has_alpha ? 4 : 3);
						
						if(gInOptions.alpha == WEBP_ALPHA_TRANSPARENCY && gStuff->planes == 4)
						{
							gStuff->transparencyPlane = gStuff->planes - 1;
							gStuff->transparencyMatting = 0;
						}
						
						
						WebPChunkIterator chunk_iter;
						
						if(gStuff->canUseICCProfiles && (flags & ICCP_FLAG) && WebPDemuxGetChunk(demux, "ICCP", 1, &chunk_iter) )
						{
							gStuff->iCCprofileSize = chunk_iter.chunk.size;
							gStuff->iCCprofileData = myNewHandle(globals, gStuff->iCCprofileSize);
							
							if(gStuff->iCCprofileData)
							{
								Ptr iccP = myLockHandle(globals, gStuff->iCCprofileData);
								
								memcpy(iccP, chunk_iter.chunk.bytes, gStuff->iCCprofileSize);
								
								myUnlockHandle(globals, gStuff->iCCprofileData);
							}
							
							WebPDemuxReleaseChunkIterator(&chunk_iter);
						}
						
						if(gStuff->propertyProcs && PISetProp)
						{
							if( (flags & EXIF_FLAG) && WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter) )
							{
								Handle exif_handle = myNewHandle(globals, chunk_iter.chunk.size);
								
								if(exif_handle)
								{
									Ptr exifP = myLockHandle(globals, exif_handle);
									
									memcpy(exifP, chunk_iter.chunk.bytes, chunk_iter.chunk.size);
									
									myUnlockHandle(globals, exif_handle);
									
									PISetProp(kPhotoshopSignature, propEXIFData, 0, NULL, exif_handle);
								}
								
								WebPDemuxReleaseChunkIterator(&chunk_iter);
							}

							if( (flags & XMP_FLAG) && WebPDemuxGetChunk(demux, "XMP ", 1, &chunk_iter) )
							{
								Handle xmp_handle = myNewHandle(globals, chunk_iter.chunk.size);
								
								if(xmp_handle)
								{
									Ptr xmpP = myLockHandle(globals, xmp_handle);
									
									memcpy(xmpP, chunk_iter.chunk.bytes, chunk_iter.chunk.size);
									
									myUnlockHandle(globals, xmp_handle);
									
									PISetProp(kPhotoshopSignature, propXMP, 0, NULL, xmp_handle);
								}
								
								WebPDemuxReleaseChunkIterator(&chunk_iter);
							}
						}
					}
					
					WebPDemuxDelete(demux);
				}
				else
					gResult = formatCannotRead;
			}
			else
				gResult = readErr;
				
			myUnlockHandle(globals, globals->fileH);
		}
		else
			gResult = memFullErr;
	}
	
	
	if(gResult != noErr && globals->fileH != NULL)
	{
		myDisposeHandle(globals, globals->fileH);
		
		globals->fileH = NULL;
	}
}


typedef struct {
	unsigned8	r;
	unsigned8	g;
	unsigned8	b;
	unsigned8	a;
} RGBApixel8;

static void Premultiply(RGBApixel8 *buf, int64 len)
{
	while(len--)
	{
		if(buf->a != 255)
		{	
			float mult = (float)buf->a / 255.f;
			
			buf->r = ((float)buf->r * mult) + 0.5f;
			buf->g = ((float)buf->g * mult) + 0.5f;
			buf->b = ((float)buf->b * mult) + 0.5f;
		}
		
		buf++;
	}
}

static void DoReadContinue(GPtr globals)
{
	if(globals->fileH)
	{
		size_t data_size = myGetHandleSize(globals, globals->fileH);
		
		Ptr data = myLockHandle(globals, globals->fileH);
		

		WebPData webp_data = { (const uint8_t *)data, data_size };

		WebPDemuxer *demux = WebPDemux(&webp_data);

		if(demux)
		{
			WebPIterator iter;
			
			if( WebPDemuxGetFrame(demux, 0, &iter) )
			{
				WebPDecoderConfig config;
				WebPInitDecoderConfig(&config);
				
				config.options.use_threads = TRUE;
				
				VP8StatusCode status = WebPGetFeatures(iter.fragment.bytes, iter.fragment.size, &config.input);
				
				if(status == VP8_STATUS_OK)
				{
					WebPDecBuffer* const output_buffer = &config.output;
					
					output_buffer->colorspace = (gStuff->planes == 4 ? MODE_RGBA : MODE_RGB);
					output_buffer->width = gStuff->imageSize.h;
					output_buffer->height = gStuff->imageSize.v;
					output_buffer->is_external_memory = TRUE;
					
					int32 rowbytes = sizeof(unsigned char) * gStuff->planes * gStuff->imageSize.h;
					int32 buffer_size = rowbytes * gStuff->imageSize.v;
					
					BufferID bufferID = 0;
					
					gResult = myAllocateBuffer(globals, buffer_size, &bufferID);
					
					if(gResult == noErr)
					{
						gStuff->data = myLockBuffer(globals, bufferID, TRUE);
						
						WebPRGBABuffer *buf_info = &output_buffer->u.RGBA;
						
						buf_info->rgba = (uint8_t *)gStuff->data;
						buf_info->stride = rowbytes;
						buf_info->size = buffer_size;
						
						
						status = WebPDecode((const uint8_t *)iter.fragment.bytes, iter.fragment.size, &config);
						
						if(status == VP8_STATUS_OK)
						{
							if(gStuff->planes == 4 && gInOptions.alpha == WEBP_ALPHA_CHANNEL && gInOptions.mult == TRUE)
							{
								Premultiply((RGBApixel8 *)gStuff->data, gStuff->imageSize.h * gStuff->imageSize.v);
							}
						
							gStuff->planeBytes = 1;
							gStuff->colBytes = gStuff->planeBytes * gStuff->planes;
							gStuff->rowBytes = rowbytes;
							
							gStuff->loPlane = 0;
							gStuff->hiPlane = gStuff->planes - 1;
									
							gStuff->theRect.left = gStuff->theRect32.left = 0;
							gStuff->theRect.right = gStuff->theRect32.right = gStuff->imageSize.h;
							
							gStuff->theRect.top = gStuff->theRect32.top = 0;
							gStuff->theRect.bottom = gStuff->theRect32.bottom = gStuff->imageSize.v;
							
							gResult = AdvanceState();
						}
						else
							gResult = formatCannotRead;
						
						
						myFreeBuffer(globals, bufferID);
					}
				}
				
				WebPDemuxReleaseIterator(&iter);
			}
			else
				gResult = formatCannotRead;
			
			
			WebPDemuxDelete(demux);
		}
		else
			gResult = formatCannotRead;
		
		
		myUnlockHandle(globals, globals->fileH);
	}
	else
		gResult = formatBadParameters;
	
	
	// very important!
	gStuff->data = NULL;
}


static void DoReadFinish(GPtr globals)
{
	if(globals->fileH)
	{
		myDisposeHandle(globals, globals->fileH);
		
		globals->fileH = NULL;
	}
}

#pragma mark-

static void DoOptionsPrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoOptionsStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	
	if( ReadScriptParamsOnWrite(globals) )
	{
		bool have_transparency = false;
		const char *alpha_name = NULL;
		
		if(gStuff->hostSig == '8BIM')
			have_transparency = (gStuff->documentInfo && gStuff->documentInfo->mergedTransparency);
		else
			have_transparency = (gStuff->planes == 2 || gStuff->planes == 4);

			
		if(gStuff->documentInfo && gStuff->documentInfo->alphaChannels)
			alpha_name = gStuff->documentInfo->alphaChannels->name;
	
	
		WebP_OutUI_Data params;
		
		params.lossless		= gOptions.lossless;
		params.quality		= gOptions.quality;
		params.alpha		= (DialogAlpha)gOptions.alpha;
	
	
	#ifdef __PIMac__
		const char * const plugHndl = "com.fnordware.Photoshop.WebP";
		const void *hwnd = globals;	
	#else
		const char *const plugHndl = NULL;
		HWND hwnd = (HWND)((PlatformData *)gStuff->platformData)->hwnd;
	#endif

		bool result = WebP_OutUI(&params, have_transparency, alpha_name, plugHndl, hwnd);
		
		
		if(result)
		{
			gOptions.lossless	= params.lossless;
			gOptions.quality	= params.quality;
			gOptions.alpha		= params.alpha;
			
			WriteParams(globals, &gOptions);
			WriteScriptParamsOnWrite(globals);
		}
		else
			gResult = userCanceledErr;

	}
}


static void DoOptionsContinue(GPtr globals)
{

}


static void DoOptionsFinish(GPtr globals)
{

}

#pragma mark-

static void DoEstimatePrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static void DoEstimateStart(GPtr globals)
{
	if(gStuff->HostSupports32BitCoordinates && gStuff->imageSize32.h && gStuff->imageSize32.v)
		gStuff->PluginUsing32BitCoordinates = TRUE;
		
	int width = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.h : gStuff->imageSize.h);
	int height = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.v : gStuff->imageSize.v);
	
	int64 dataBytes = (int64)width * (int64)height * (int64)gStuff->planes * (int64)(gStuff->depth >> 3);
					  
		
#ifndef MIN
#define MIN(A,B)			( (A) < (B) ? (A) : (B))
#endif
		
	gStuff->minDataBytes = MIN(dataBytes / 2, INT_MAX);
	gStuff->maxDataBytes = MIN(dataBytes, INT_MAX);
	
	gStuff->data = NULL;
}


static void DoEstimateContinue(GPtr globals)
{

}


static void DoEstimateFinish(GPtr globals)
{

}

#pragma mark-

static void DoWritePrepare(GPtr globals)
{
	gStuff->maxData = 0;
}


static int ProgressReport(int percent, const WebPPicture* const picture)
{
	GPtr globals = (GPtr)picture->user_data;
	
	PIUpdateProgress(percent, 100);
	
	return (noErr == (gResult = TestAbort()));
}

static void DoWriteStart(GPtr globals)
{
	ReadParams(globals, &gOptions);
	ReadScriptParamsOnWrite(globals);

	assert(gStuff->imageMode == plugInModeRGBColor);
	assert(gStuff->depth == 8);
	assert(gStuff->planes == 3 || gStuff->planes == 4);
	
	
	bool have_transparency = (gStuff->planes >= 4);
	bool have_alpha_channel = (gStuff->channelPortProcs && gStuff->documentInfo && gStuff->documentInfo->alphaChannels);

	bool use_transparency = (have_transparency && gOptions.alpha == WEBP_ALPHA_TRANSPARENCY);
	bool use_alpha_channel = (have_alpha_channel && gOptions.alpha == WEBP_ALPHA_CHANNEL);
	
	bool use_alpha = (use_transparency || use_alpha_channel);
	
	
	const int width = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.h : gStuff->imageSize.h);
	const int height = (gStuff->PluginUsing32BitCoordinates ? gStuff->imageSize32.v : gStuff->imageSize.v);


	gStuff->loPlane = 0;
	gStuff->hiPlane = (use_transparency ? 3 : 2);
	gStuff->colBytes = sizeof(unsigned char) * (use_alpha ? 4 : 3);
	gStuff->rowBytes = gStuff->colBytes * width;
	gStuff->planeBytes = sizeof(unsigned char);
	
	gStuff->theRect.left = gStuff->theRect32.left = 0;
	gStuff->theRect.right = gStuff->theRect32.right = width;
	gStuff->theRect.top = gStuff->theRect32.top = 0;
	gStuff->theRect.bottom = gStuff->theRect32.bottom = height;


	ReadPixelsProc ReadProc = NULL;
	ReadChannelDesc *alpha_channel = NULL;
	
	// ReadProc being non-null means we're going to get the channel from the channels palette
	if(use_alpha && gOptions.alpha == WEBP_ALPHA_CHANNEL &&
		gStuff->channelPortProcs && gStuff->documentInfo && gStuff->documentInfo->alphaChannels)
	{
		ReadProc = gStuff->channelPortProcs->readPixelsProc;
		
		alpha_channel = gStuff->documentInfo->alphaChannels;
	}
	
	
	int32 buffer_size = gStuff->rowBytes * height;
	
	BufferID bufferID = 0;
	
	gResult = myAllocateBuffer(globals, buffer_size, &bufferID);
	
	if(gResult == noErr)
	{
		gStuff->data = myLockBuffer(globals, bufferID, TRUE);
		
		gResult = AdvanceState();
		
		
		if(gResult == noErr && ReadProc)
		{
			VRect wroteRect;
			VRect writeRect = { 0, 0, height, width };
			PSScaling scaling; scaling.sourceRect = scaling.destinationRect = writeRect;
			PixelMemoryDesc memDesc = { (char *)gStuff->data, gStuff->rowBytes * 8, gStuff->colBytes * 8, 3 * 8, gStuff->depth };					
		
			gResult = ReadProc(alpha_channel->port, &scaling, &writeRect, &memDesc, &wroteRect);
		}
		
		if(gResult == noErr)
		{
			WebPMux *mux = WebPMuxNew();
			
			if(mux)
			{
				WebPPicture picture;
				WebPPictureInit(&picture);
				
				picture.width = width;
				picture.height = height;
				picture.use_argb = TRUE;
				
				int ok = use_alpha ? WebPPictureImportRGBA(&picture, (const uint8_t *)gStuff->data, gStuff->rowBytes) :
										WebPPictureImportRGB(&picture, (const uint8_t *)gStuff->data, gStuff->rowBytes);
				
				if(ok)
				{
					WebPMemoryWriter memory_writer;
					WebPMemoryWriterInit(&memory_writer);
				
					WebPConfig config;
					WebPConfigInit(&config);
					
					config.thread_level = TRUE;
					config.lossless = gOptions.lossless;
					config.quality = gOptions.quality;
					
					picture.progress_hook = ProgressReport;
					picture.user_data = globals;
					picture.writer = WebPMemoryWrite;
					picture.custom_ptr = &memory_writer;
					
					int success = WebPEncode(&config, &picture);
					
					if(success && gResult == noErr)
					{
						WebPData image_data = { memory_writer.mem, memory_writer.size };
					
						WebPMuxError img_err = WebPMuxSetImage(mux, &image_data, FALSE);
						
						if(img_err == WEBP_MUX_OK)
						{
							// add metadata
							if(gStuff->canUseICCProfiles && (gStuff->iCCprofileSize > 0) && (gStuff->iCCprofileData != NULL))
							{
								WebPData chunk_data;
								
								chunk_data.bytes = (const uint8_t *)myLockHandle(globals, gStuff->iCCprofileData);
								chunk_data.size = myGetHandleSize(globals, gStuff->iCCprofileData);
											
								WebPMuxError chunk_err = WebPMuxSetChunk(mux, "ICCP", &chunk_data, TRUE);
								
								myUnlockHandle(globals, gStuff->iCCprofileData);
							}
						
							if(gStuff->propertyProcs && PIGetProp)
							{
								intptr_t simp;
								
								
								Handle exif_handle = NULL;
								
								PIGetProp(kPhotoshopSignature, propEXIFData, 0, &simp, &exif_handle);
								
								if(exif_handle)
								{
									WebPData chunk_data;
									
									chunk_data.bytes = (const uint8_t *)myLockHandle(globals, exif_handle);
									chunk_data.size = myGetHandleSize(globals, exif_handle);
									
									WebPMuxError chunk_err = WebPMuxSetChunk(mux, "EXIF", &chunk_data, TRUE);
									
									myDisposeHandle(globals, exif_handle);
								}


								Handle xmp_handle = NULL;
								
								PIGetProp(kPhotoshopSignature, propXMP, 0, &simp, &exif_handle);
								
								if(xmp_handle)
								{
									WebPData chunk_data;
									
									chunk_data.bytes = (const uint8_t *)myLockHandle(globals, xmp_handle);
									chunk_data.size = myGetHandleSize(globals, xmp_handle);
									
									WebPMuxError chunk_err = WebPMuxSetChunk(mux, "XMP ", &chunk_data, TRUE);
									
									myDisposeHandle(globals, xmp_handle);
								}
							}
							
							// assemble and write the file
							WebPData output_data;
							
							WebPMuxError err = WebPMuxAssemble(mux, &output_data);
							
							if(err == WEBP_MUX_OK)
							{
								bool ok = my_fwrite(globals, output_data.bytes, output_data.size);
								
								WebPDataClear(&output_data);
								
								if(!ok)
									gResult = writErr; // or maybe dskFulErr
							}
							else
								gResult = formatBadParameters;
						}
						else
							gResult = formatBadParameters;
					}
					else if(gResult == noErr)
					{
						gResult = formatBadParameters;
					}
					
					if(memory_writer.mem)
						free(memory_writer.mem);
				}
			
				WebPMuxDelete(mux);
			}
			else
				gResult = formatBadParameters;
		}
		
		myFreeBuffer(globals, bufferID);
	}
	
	// muy importante
	gStuff->data = NULL;
}


static void DoWriteContinue(GPtr globals)
{

}


static void DoWriteFinish(GPtr globals)
{
	if(gStuff->hostSig != 'FXTC')
		WriteScriptParamsOnWrite(globals);
}


#pragma mark-


DLLExport MACPASCAL void PluginMain(const short selector,
						             FormatRecord *formatParamBlock,
						             intptr_t *data,
						             short *result)
{
	if (selector == formatSelectorAbout)
	{
		sSPBasic = ((AboutRecordPtr)formatParamBlock)->sSPBasic;

	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)((AboutRecordPtr)formatParamBlock)->plugInRef);
	#endif

		DoAbout((AboutRecordPtr)formatParamBlock);
	}
	else
	{
		sSPBasic = formatParamBlock->sSPBasic;  //thanks Tom
		
		gPlugInRef = (SPPluginRef)formatParamBlock->plugInRef;
		
	#ifdef __PIWin__
		if(hDllInstance == NULL)
			hDllInstance = GetDLLInstance((SPPluginRef)formatParamBlock->plugInRef);
	#endif

		
	 	static const FProc routineForSelector [] =
		{
			/* formatSelectorAbout  				DoAbout, */
			
			/* formatSelectorReadPrepare */			DoReadPrepare,
			/* formatSelectorReadStart */			DoReadStart,
			/* formatSelectorReadContinue */		DoReadContinue,
			/* formatSelectorReadFinish */			DoReadFinish,
			
			/* formatSelectorOptionsPrepare */		DoOptionsPrepare,
			/* formatSelectorOptionsStart */		DoOptionsStart,
			/* formatSelectorOptionsContinue */		DoOptionsContinue,
			/* formatSelectorOptionsFinish */		DoOptionsFinish,
			
			/* formatSelectorEstimatePrepare */		DoEstimatePrepare,
			/* formatSelectorEstimateStart */		DoEstimateStart,
			/* formatSelectorEstimateContinue */	DoEstimateContinue,
			/* formatSelectorEstimateFinish */		DoEstimateFinish,
			
			/* formatSelectorWritePrepare */		DoWritePrepare,
			/* formatSelectorWriteStart */			DoWriteStart,
			/* formatSelectorWriteContinue */		DoWriteContinue,
			/* formatSelectorWriteFinish */			DoWriteFinish,
			
			/* formatSelectorFilterFile */			DoFilterFile
		};
		
		Ptr globalPtr = NULL;		// Pointer for global structure
		GPtr globals = NULL; 		// actual globals

		
		if(formatParamBlock->handleProcs)
		{
			bool must_init = false;
			
			if(*data == NULL)
			{
				*data = (intptr_t)formatParamBlock->handleProcs->newProc(sizeof(Globals));
				
				must_init = true;
			}

			if(*data != NULL)
			{
				globalPtr = formatParamBlock->handleProcs->lockProc((Handle)*data, TRUE);
				
				if(must_init)
					InitGlobals(globalPtr);
			}
			else
			{
				*result = memFullErr;
				return;
			}

			globals = (GPtr)globalPtr;

			globals->result = result;
			globals->formatParamBlock = formatParamBlock;
		}
		else
		{
			// old lame way
			globalPtr = AllocateGlobals(result,
										 formatParamBlock,
										 formatParamBlock->handleProcs,
										 sizeof(Globals),
						 				 data,
						 				 InitGlobals);

			if(globalPtr == NULL)
			{ // Something bad happened if we couldn't allocate our pointer.
			  // Fortunately, everything's already been cleaned up,
			  // so all we have to do is report an error.
			  
			  *result = memFullErr;
			  return;
			}
			
			// Get our "globals" variable assigned as a Global Pointer struct with the
			// data we've returned:
			globals = (GPtr)globalPtr;
		}


		// Dispatch selector
		if (selector > formatSelectorAbout && selector <= formatSelectorFilterFile)
			(routineForSelector[selector-1])(globals); // dispatch using jump table
		else
			gResult = formatBadParameters;
		
		
		if((Handle)*data != NULL)
		{
			if(formatParamBlock->handleProcs)
			{
				formatParamBlock->handleProcs->unlockProc((Handle)*data);
			}
			else
			{
				PIUnlockHandle((Handle)*data);
			}
		}
		
	
	} // about selector special		
}
