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



#include "WebM_Premiere_Export.h"

#include "WebM_Premiere_Export_Params.h"


#ifdef PRMAC_ENV
	#include <mach/mach.h>
#else
	#include <assert.h>
	#include <time.h>
	#include <math.h>

	#define LONG_LONG_MAX LLONG_MAX
#endif


#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

#include "opus_multistream.h"

#include "mkvmuxer/mkvmuxer.h"


class PrMkvWriter : public mkvmuxer::IMkvWriter
{
  public:
	PrMkvWriter(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject);
	virtual ~PrMkvWriter();
	
	virtual int32_t Write(const void* buf, uint32_t len);
	virtual int64_t Position() const;
	virtual int32_t Position(int64_t position); // seek
	virtual bool Seekable() const { return true; }
	virtual void ElementStartNotify(uint64_t element_id, int64_t position);
	
  private:
	const PrSDKExportFileSuite *_fileSuite;
	const csSDK_uint32 _fileObject;
};

PrMkvWriter::PrMkvWriter(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject) :
	_fileSuite(fileSuite),
	_fileObject(fileObject)
{
	prSuiteError err = _fileSuite->Open(_fileObject);
	
	if(err != malNoError)
		throw err;
}

PrMkvWriter::~PrMkvWriter()
{
	prSuiteError err = _fileSuite->Close(_fileObject);
	
	assert(err == malNoError);
}

int32_t
PrMkvWriter::Write(const void* buf, uint32_t len)
{
	prSuiteError err = _fileSuite->Write(_fileObject, (void *)buf, len);
	
	return err;
}

int64_t
PrMkvWriter::Position() const
{
	prInt64 pos = 0;

// son of a gun, fileSeekMode_End and fileSeekMode_Current are flipped inside Premiere!
#define PR_SEEK_CURRENT fileSeekMode_End

	prSuiteError err = _fileSuite->Seek(_fileObject, 0, pos, PR_SEEK_CURRENT);
	
	if(err != malNoError)
		throw err;
	
	return pos;
}

int32_t
PrMkvWriter::Position(int64_t position)
{
	prInt64 pos = 0;

	prSuiteError err = _fileSuite->Seek(_fileObject, position, pos, fileSeekMode_Begin);
	
	return err;
}

void
PrMkvWriter::ElementStartNotify(uint64_t element_id, int64_t position)
{
	// ummm, should I do something?
}


#pragma mark-


static const csSDK_int32 WebM_ID = 'WebM';
static const csSDK_int32 WebM_Export_Class = 'WebM';

extern int g_num_cpus;


// http://matroska.org/technical/specs/notes.html#TimecodeScale
// Time (in nanoseconds) = TimeCode * TimeCodeScale
// When we call finctions like GetTime, we're given Time in Nanoseconds.
static const long long S2NS = 1000000000LL;


static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}

static void
ncpyUTF16(char *dest, const prUTF16Char *src, int max_len)
{
	char *d = dest;
	const prUTF16Char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}

static int
mylog2(int val)
{
	int ret = 0;
	
	while( pow(2.0, ret) < val )
	{
		ret++;
	}
	
	return ret;
}

static prMALError
exSDKStartup(
	exportStdParms		*stdParmsP, 
	exExporterInfoRec	*infoRecP)
{
	int fourCC = 0;
	VersionInfo version = {0, 0, 0};

	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParmsP->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);

		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_Version, (void *)&version);
	
		stdParmsP->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		// not a good idea to try to run a MediaCore exporter in AE
		if(fourCC == kAppAfterEffects)
			return exportReturn_IterateExporterDone;
	}
	

	infoRecP->fileType			= WebM_ID;
	
	utf16ncpy(infoRecP->fileTypeName, "WebM", 255);
	utf16ncpy(infoRecP->fileTypeDefaultExtension, "webm", 255);
	
	infoRecP->classID = WebM_Export_Class;
	
	infoRecP->exportReqIndex	= 0;
	infoRecP->wantsNoProgressBar = kPrFalse;
	infoRecP->hideInUI			= kPrFalse;
	infoRecP->doesNotSupportAudioOnly = kPrFalse;
	infoRecP->canExportVideo	= kPrTrue;
	infoRecP->canExportAudio	= kPrTrue;
	infoRecP->singleFrameOnly	= kPrFalse;
	
	infoRecP->interfaceVersion	= EXPORTMOD_VERSION;
	
	infoRecP->isCacheable		= kPrFalse;
	
	if(stdParmsP->interfaceVer >= 6 &&
		((fourCC == kAppPremierePro && version.major >= 9) ||
		 (fourCC == kAppMediaEncoder && version.major >= 9)))
	{
	#if EXPORTMOD_VERSION >= 6
		infoRecP->canConformToMatchParams = kPrTrue;
	#else
		// in earlier SDKs, we'll cheat and set this ourselves
		csSDK_uint32 *info = &infoRecP->isCacheable;
		info[1] = kPrTrue; // one spot past isCacheable
	#endif
	}

	return malNoError;
}


static prMALError
exSDKBeginInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result				= malNoError;
	SPErr					spError				= kSPNoError;
	ExportSettings			*mySettings;
	PrSDKMemoryManagerSuite	*memorySuite;
	csSDK_int32				exportSettingsSize	= sizeof(ExportSettings);
	SPBasicSuite			*spBasic			= stdParmsP->getSPBasicSuite();
	
	if(spBasic != NULL)
	{
		spError = spBasic->AcquireSuite(
			kPrSDKMemoryManagerSuite,
			kPrSDKMemoryManagerSuiteVersion,
			const_cast<const void**>(reinterpret_cast<void**>(&memorySuite)));
			
		mySettings = reinterpret_cast<ExportSettings *>(memorySuite->NewPtrClear(exportSettingsSize));

		if(mySettings)
		{
			mySettings->spBasic		= spBasic;
			mySettings->memorySuite	= memorySuite;
			
			spError = spBasic->AcquireSuite(
				kPrSDKExportParamSuite,
				kPrSDKExportParamSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportParamSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportFileSuite,
				kPrSDKExportFileSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportFileSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportInfoSuite,
				kPrSDKExportInfoSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportInfoSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKExportProgressSuite,
				kPrSDKExportProgressSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportProgressSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixCreatorSuite,
				kPrSDKPPixCreatorSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixCreatorSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPixSuite,
				kPrSDKPPixSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKPPix2Suite,
				kPrSDKPPix2SuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppix2Suite))));
			spError = spBasic->AcquireSuite(
				kPrSDKSequenceRenderSuite,
				kPrSDKSequenceRenderSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceRenderSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKSequenceAudioSuite,
				kPrSDKSequenceAudioSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceAudioSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKTimeSuite,
				kPrSDKTimeSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->timeSuite))));
			spError = spBasic->AcquireSuite(
				kPrSDKWindowSuite,
				kPrSDKWindowSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->windowSuite))));
		}


		instanceRecP->privateData = reinterpret_cast<void*>(mySettings);
	}
	else
	{
		result = exportReturn_ErrMemory;
	}
	
	return result;
}


static prMALError
exSDKEndInstance(
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result		= malNoError;
	ExportSettings			*lRec		= reinterpret_cast<ExportSettings *>(instanceRecP->privateData);
	SPBasicSuite			*spBasic	= stdParmsP->getSPBasicSuite();
	PrSDKMemoryManagerSuite	*memorySuite;
	if(spBasic != NULL && lRec != NULL)
	{
		if (lRec->exportParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion);
		}
		if (lRec->exportFileSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion);
		}
		if (lRec->exportInfoSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion);
		}
		if (lRec->exportProgressSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportProgressSuite, kPrSDKExportProgressSuiteVersion);
		}
		if (lRec->ppixCreatorSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion);
		}
		if (lRec->ppixSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		}
		if (lRec->ppix2Suite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		}
		if (lRec->sequenceRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion);
		}
		if (lRec->sequenceAudioSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceAudioSuite, kPrSDKSequenceAudioSuiteVersion);
		}
		if (lRec->timeSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		}
		if (lRec->windowSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}
		if (lRec->memorySuite)
		{
			memorySuite = lRec->memorySuite;
			memorySuite->PrDisposePtr(reinterpret_cast<PrMemoryPtr>(lRec));
			result = spBasic->ReleaseSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion);
		}
	}

	return result;
}



static prMALError
exSDKFileExtension(
	exportStdParms					*stdParmsP, 
	exQueryExportFileExtensionRec	*exportFileExtensionRecP)
{
	utf16ncpy(exportFileExtensionRecP->outFileExtension, "webm", 255);
		
	return malNoError;
}


static void get_framerate(PrTime ticksPerSecond, PrTime ticks_per_frame, exRatioValue *fps)
{
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	int frameRateIndex = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(ticks_per_frame == frameRates[i])
			frameRateIndex = i;
	}
	
	if(frameRateIndex >= 0)
	{
		fps->numerator = frameRateNumDens[frameRateIndex][0];
		fps->denominator = frameRateNumDens[frameRateIndex][1];
	}
	else
	{
		fps->numerator = 1001 * ticksPerSecond / ticks_per_frame;
		fps->denominator = 1001;
	}
}


// converting from Adobe 16-bit to regular 16-bit
#define PF_HALF_CHAN16			16384

static inline unsigned short
Promote(const unsigned short &val)
{
	return (val > PF_HALF_CHAN16 ? ( (val - 1) << 1 ) + 1 : val << 1);
}


template <typename BGRA_PIX, typename IMG_PIX>
static inline IMG_PIX
DepthConvert(const BGRA_PIX &val, const int &depth);

template<>
static inline unsigned short
DepthConvert<unsigned short, unsigned short>(const unsigned short &val, const int &depth)
{
	return (Promote(val) >> (16 - depth));
}

template<>
static inline unsigned short
DepthConvert<unsigned char, unsigned short>(const unsigned char &val, const int &depth)
{
	return ((unsigned short)val << (depth - 8)) | (val >> (16 - depth));
}

template<>
static inline unsigned char
DepthConvert<unsigned short, unsigned char>(const unsigned short &val, const int &depth)
{
	assert(depth == 8);
	return ( (((long)(val) * 255) + 16384) / 32768);
}

template<>
static inline unsigned char
DepthConvert<unsigned char, unsigned char>(const unsigned char &val, const int &depth)
{
	assert(depth == 8);
	return val;
}


template <typename VUYA_PIX, typename IMG_PIX>
static void
CopyVUYAToImg(vpx_image_t *img, const char *frameBufferP, const csSDK_int32 rowbytes)
{
	const unsigned int sub_x = img->x_chroma_shift + 1;
	const unsigned int sub_y = img->y_chroma_shift + 1;
	
	for(int y = 0; y < img->d_h; y++)
	{
		IMG_PIX *imgY = (IMG_PIX *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
		IMG_PIX *imgU = (IMG_PIX *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y)));
		IMG_PIX *imgV = (IMG_PIX *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y)));
	
		const VUYA_PIX *prVUYA = (VUYA_PIX *)(frameBufferP + (rowbytes * (img->d_h - 1 - y)));
		
		const VUYA_PIX *prV = prVUYA + 0;
		const VUYA_PIX *prU = prVUYA + 1;
		const VUYA_PIX *prY = prVUYA + 2;
		
		for(int x=0; x < img->d_w; x++)
		{
			*imgY++ = DepthConvert<VUYA_PIX, IMG_PIX>(*prY, img->bit_depth);
			
			if( (y % sub_y == 0) && (x % sub_x == 0) )
			{
				*imgU++ = DepthConvert<VUYA_PIX, IMG_PIX>(*prU, img->bit_depth);
				*imgV++ = DepthConvert<VUYA_PIX, IMG_PIX>(*prV, img->bit_depth);
			}
			
			prY += 4;
			prU += 4;
			prV += 4;
		}
	}
}


template <typename BGRA_PIX, typename IMG_PIX, bool isARGB>
static void
CopyBGRAToImg(vpx_image_t *img, const char *frameBufferP, const csSDK_int32 rowbytes)
{
	const unsigned int sub_x = img->x_chroma_shift + 1;
	const unsigned int sub_y = img->y_chroma_shift + 1;

	for(int y = 0; y < img->d_h; y++)
	{
		IMG_PIX *imgY = (IMG_PIX *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
		IMG_PIX *imgU = (IMG_PIX *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y)));
		IMG_PIX *imgV = (IMG_PIX *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y)));
		
		const BGRA_PIX *prBGRA = (BGRA_PIX *)(frameBufferP + (rowbytes * (img->d_h - 1 - y)));
		
		const BGRA_PIX *prB = prBGRA + 0;
		const BGRA_PIX *prG = prBGRA + 1;
		const BGRA_PIX *prR = prBGRA + 2;
		
		if(isARGB)
		{
			// Media Encoder CS5 insists on handing us this format in some cases,
			// even though we didn't list it as an option
			prR = prBGRA + 1;
			prG = prBGRA + 2;
			prB = prBGRA + 3;
		}
		
		// These are the pixels below the current one for MPEG-2 chroma siting
		const BGRA_PIX *prBb = prB - (rowbytes / sizeof(BGRA_PIX));
		const BGRA_PIX *prGb = prG - (rowbytes / sizeof(BGRA_PIX));
		const BGRA_PIX *prRb = prR - (rowbytes / sizeof(BGRA_PIX));
		
		// unless this is the last line and there is no pixel below
		if(y == (img->d_h - 1) || sub_y != 2)
		{
			prBb = prB;
			prGb = prG;
			prRb = prR;
		}
		
		
		// using the conversion found here: http://www.fourcc.org/fccyvrgb.php
		
		// these are part of the RGBtoYUV math (uses Adobe 16-bit)
		const int Yadd = (sizeof(BGRA_PIX) > 1 ? 2056500 : 16500);    // to be divided by 1000
		const int UVadd = (sizeof(BGRA_PIX) > 1 ? 16449500 : 128500); // includes extra 500 for rounding
		
		for(int x=0; x < img->d_w; x++)
		{
			*imgY++ = DepthConvert<BGRA_PIX, IMG_PIX>( ((257 * (int)*prR) + (504 * (int)*prG) + ( 98 * (int)*prB) + Yadd) / 1000, img->bit_depth);
			
			if(sub_y > 1)
			{
				if( (y % sub_y == 0) && (x % sub_x == 0) )
				{
					*imgV++ = DepthConvert<BGRA_PIX, IMG_PIX>( (((439 * (int)*prR) - (368 * (int)*prG) - ( 71 * (int)*prB) + UVadd) +
										((439 * (int)*prRb) - (368 * (int)*prGb) - ( 71 * (int)*prBb) + UVadd)) / 2000, img->bit_depth);
					*imgU++ = DepthConvert<BGRA_PIX, IMG_PIX>( ((-(148 * (int)*prR) - (291 * (int)*prG) + (439 * (int)*prB) + UVadd) +
										(-(148 * (int)*prRb) - (291 * (int)*prGb) + (439 * (int)*prBb) + UVadd)) / 2000, img->bit_depth);
				}
				
				prRb += 4;
				prGb += 4;
				prBb += 4;
			}
			else
			{
				if(x % sub_x == 0)
				{
					*imgV++ = DepthConvert<BGRA_PIX, IMG_PIX>( (((439 * (int)*prR) - (368 * (int)*prG) - ( 71 * (int)*prB) + UVadd)) / 1000, img->bit_depth);
					*imgU++ = DepthConvert<BGRA_PIX, IMG_PIX>( ((-(148 * (int)*prR) - (291 * (int)*prG) + (439 * (int)*prB) + UVadd) ) / 1000, img->bit_depth);
				}
			}
			
			prR += 4;
			prG += 4;
			prB += 4;
		}
	}
}


static void
CopyPixToImg(vpx_image_t *img, const PPixHand &outFrame, PrSDKPPixSuite *pixSuite, PrSDKPPix2Suite *pix2Suite)
{
	prRect boundsRect;
	pixSuite->GetBounds(outFrame, &boundsRect);
	
	assert(boundsRect.right == img->d_w && boundsRect.bottom == img->d_h);

	PrPixelFormat pixFormat;
	pixSuite->GetPixelFormat(outFrame, &pixFormat);

	const unsigned int sub_x = img->x_chroma_shift + 1;
	const unsigned int sub_y = img->y_chroma_shift + 1;

	if(pixFormat == PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601)
	{
		assert(sub_x == 2 && sub_y == 2);
		assert(img->bit_depth == 8);
		
		char *Y_PixelAddress, *U_PixelAddress, *V_PixelAddress;
		csSDK_uint32 Y_RowBytes, U_RowBytes, V_RowBytes;
		
		pix2Suite->GetYUV420PlanarBuffers(outFrame, PrPPixBufferAccess_ReadOnly,
											&Y_PixelAddress, &Y_RowBytes,
											&U_PixelAddress, &U_RowBytes,
											&V_PixelAddress, &V_RowBytes);
		
		for(int y = 0; y < img->d_h; y++)
		{
			unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
			
			const unsigned char *prY = (unsigned char *)Y_PixelAddress + (Y_RowBytes * y);
			
			memcpy(imgY, prY, img->d_w * sizeof(unsigned char));
		}
		
		const int chroma_width = (img->d_w / 2) + (img->d_w % 2);
		const int chroma_height = (img->d_h / 2) + (img->d_h % 2);
		
		for(int y = 0; y < chroma_height; y++)
		{
			unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y);
			unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y);
			
			const unsigned char *prU = (unsigned char *)U_PixelAddress + (U_RowBytes * y);
			const unsigned char *prV = (unsigned char *)V_PixelAddress + (V_RowBytes * y);
			
			memcpy(imgU, prU, chroma_width * sizeof(unsigned char));
			memcpy(imgV, prV, chroma_width * sizeof(unsigned char));
		}
	}
	else
	{
		char *frameBufferP = NULL;
		csSDK_int32 rowbytes = 0;
		
		pixSuite->GetPixels(outFrame, PrPPixBufferAccess_ReadOnly, &frameBufferP);
		pixSuite->GetRowBytes(outFrame, &rowbytes);
		
		
		if(pixFormat == PrPixelFormat_UYVY_422_8u_601)
		{
			assert(sub_x == 2 && sub_y == 1);
			assert(img->bit_depth == 8);
			
			for(int y = 0; y < img->d_h; y++)
			{
				unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
				unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y);
				unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y);
			
				const unsigned char *prUYVY = (unsigned char *)frameBufferP + (rowbytes * y);
				
				for(int x=0; x < img->d_w; x++)
				{
					if(x % 2 == 0)
						*imgU++ = *prUYVY++;
					else
						*imgV++ = *prUYVY++;
					
					*imgY++ = *prUYVY++;;
				}
			}
		}
		else if(pixFormat == PrPixelFormat_VUYX_4444_8u)
		{
			assert(sub_x == 1 && sub_y == 1);
			assert(img->bit_depth == 8);
			
			CopyVUYAToImg<unsigned char, unsigned char>(img, frameBufferP, rowbytes);
		}
		else if(pixFormat == PrPixelFormat_VUYA_4444_16u)
		{
			assert(img->bit_depth > 8);
			
			CopyVUYAToImg<unsigned short, unsigned short>(img, frameBufferP, rowbytes);
		}
		else if(pixFormat == PrPixelFormat_BGRA_4444_16u)
		{
			if(img->bit_depth > 8)
				CopyBGRAToImg<unsigned short, unsigned short, false>(img, frameBufferP, rowbytes);
			else
				CopyBGRAToImg<unsigned short, unsigned char, false>(img, frameBufferP, rowbytes);
		}
		else if(pixFormat == PrPixelFormat_BGRA_4444_8u)
		{
			if(img->bit_depth > 8)
				CopyBGRAToImg<unsigned char, unsigned short, false>(img, frameBufferP, rowbytes);
			else
				CopyBGRAToImg<unsigned char, unsigned char, false>(img, frameBufferP, rowbytes);
		}
		else if(pixFormat == PrPixelFormat_ARGB_4444_8u)
		{
			if(img->bit_depth > 8)
				CopyBGRAToImg<unsigned char, unsigned short, true>(img, frameBufferP, rowbytes);
			else
				CopyBGRAToImg<unsigned char, unsigned char, true>(img, frameBufferP, rowbytes);
		}
		else
			assert(false);
	}
}


static int
xiph_len(int l)
{
    return 1 + l / 255 + l;
}

static void
xiph_lace(unsigned char **np, uint64_t val)
{
	unsigned char *p = *np;

	while(val >= 255)
	{
		*p++ = 255;
		val -= 255;
	}
	
	*p++ = val;
	
	*np = p;
}

static void *
MakePrivateData(ogg_packet &header, ogg_packet &header_comm, ogg_packet &header_code, size_t &size)
{
	size = 1 + xiph_len(header.bytes) + xiph_len(header_comm.bytes) + header_code.bytes;
	
	void *buf = malloc(size);
	
	if(buf)
	{
		unsigned char *p = (unsigned char *)buf;
		
		*p++ = 2;
		
		xiph_lace(&p, header.bytes);
		xiph_lace(&p, header_comm.bytes);
		
		memcpy(p, header.packet, header.bytes);
		p += header.bytes;
		memcpy(p, header_comm.packet, header_comm.bytes);
		p += header_comm.bytes;
		memcpy(p, header_code.packet, header_code.bytes);
	}
	
	return buf;
}


static prMALError
exSDKExport(
	exportStdParms	*stdParmsP,
	exDoExportRec	*exportInfoP)
{
	prMALError					result					= malNoError;
	ExportSettings				*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	PrSDKExportParamSuite		*paramSuite				= mySettings->exportParamSuite;
	PrSDKSequenceRenderSuite	*renderSuite			= mySettings->sequenceRenderSuite;
	PrSDKSequenceAudioSuite		*audioSuite				= mySettings->sequenceAudioSuite;
	PrSDKMemoryManagerSuite		*memorySuite			= mySettings->memorySuite;
	PrSDKPPixSuite				*pixSuite				= mySettings->ppixSuite;
	PrSDKPPix2Suite				*pix2Suite				= mySettings->ppix2Suite;


	PrTime ticksPerSecond = 0;
	mySettings->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	
	csSDK_uint32 exID = exportInfoP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	exParamValues widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);
	
	PrAudioChannelType audioFormat = (PrAudioChannelType)channelTypeP.value.intValue;
	
	if(audioFormat < kPrAudioChannelType_Mono || audioFormat > kPrAudioChannelType_51)
		audioFormat = kPrAudioChannelType_Stereo;
	
	const int audioChannels = (audioFormat == kPrAudioChannelType_51 ? 6 :
								audioFormat == kPrAudioChannelType_Stereo ? 2 :
								audioFormat == kPrAudioChannelType_Mono ? 1 :
								2);
	
	exParamValues codecP, methodP, videoQualityP, bitrateP, twoPassP, samplingP, bitDepthP, customArgsP;
	paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoMethod, &methodP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &bitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoTwoPass, &twoPassP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoSampling, &samplingP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitDepth, &bitDepthP);
	paramSuite->GetParamValue(exID, gIdx, WebMCustomArgs, &customArgsP);
	
	const bool use_vp9 = (codecP.value.intValue == WEBM_CODEC_VP9);
	const WebM_Video_Method method = (WebM_Video_Method)methodP.value.intValue;
	const WebM_Chroma_Sampling chroma = (use_vp9 ? (WebM_Chroma_Sampling)samplingP.value.intValue : WEBM_420);
	const int bit_depth = (use_vp9 ? bitDepthP.value.intValue : 8);

	char customArgs[256];
	ncpyUTF16(customArgs, customArgsP.paramString, 255);
	customArgs[255] = '\0';
	

	exParamValues audioCodecP, audioMethodP, audioQualityP, audioBitrateP;
	paramSuite->GetParamValue(exID, gIdx, WebMAudioCodec, &audioCodecP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioMethod, &audioMethodP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &audioQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
	
	exParamValues autoBitrateP, opusBitrateP;
	paramSuite->GetParamValue(exID, gIdx, WebMOpusAutoBitrate, &autoBitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
	
	
	const PrPixelFormat yuv_format8 = (chroma == WEBM_444 ? PrPixelFormat_VUYX_4444_8u :
										chroma == WEBM_422 ? PrPixelFormat_UYVY_422_8u_601 :
										PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601);

	const PrPixelFormat yuv_format16 = PrPixelFormat_BGRA_4444_16u; // can't trust PrPixelFormat_VUYA_4444_16u, only 16-bit YUV format
	
	const PrPixelFormat yuv_format = (bit_depth > 8 ? yuv_format16 : yuv_format8);
	
	SequenceRender_ParamsRec renderParms;
	PrPixelFormat pixelFormats[] = { yuv_format,
									PrPixelFormat_BGRA_4444_16u, // must support BGRA, even if I don't want to
									PrPixelFormat_BGRA_4444_8u };
									
	renderParms.inRequestedPixelFormatArray = pixelFormats;
	renderParms.inRequestedPixelFormatArrayCount = 3;
	renderParms.inWidth = widthP.value.intValue;
	renderParms.inHeight = heightP.value.intValue;
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatioP.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatioP.value.ratioValue.denominator;
	renderParms.inRenderQuality = (exportInfoP->maximumRenderQuality ? kPrRenderQuality_Max : kPrRenderQuality_High);
	renderParms.inFieldType = fieldTypeP.value.intValue;
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = (exportInfoP->maximumRenderQuality ? kPrRenderQuality_Max : kPrRenderQuality_High);
	renderParms.inCompositeOnBlack = kPrTrue;
	
	
	csSDK_uint32 videoRenderID = 0;
	
	if(exportInfoP->exportVideo)
	{
		result = renderSuite->MakeVideoRenderer(exID, &videoRenderID, frameRateP.value.timeValue);
	}
	
	csSDK_uint32 audioRenderID = 0;
	
	if(exportInfoP->exportAudio)
	{
		result = audioSuite->MakeAudioRenderer(exID,
												exportInfoP->startTime,
												audioFormat,
												kPrAudioSampleType_32BitFloat,
												sampleRateP.value.floatValue, 
												&audioRenderID);
	}

	
	PrMemoryPtr vbr_buffer = NULL;
	size_t vbr_buffer_size = 0;


	PrMkvWriter *writer = NULL;

	mkvmuxer::Segment *muxer_segment = NULL;
	
			
	try{
	
	const int passes = ( (exportInfoP->exportVideo && twoPassP.value.intValue) ? 2 : 1);
	
	for(int pass = 0; pass < passes && result == malNoError; pass++)
	{
		const bool vbr_pass = (passes > 1 && pass == 0);
		

		if(passes > 1)
		{
			prUTF16Char utf_str[256];
		
			if(vbr_pass)
				utf16ncpy(utf_str, "Analyzing video", 255);
			else
				utf16ncpy(utf_str, "Encoding WebM movie", 255);
			
			// This doesn't seem to be doing anything
			mySettings->exportProgressSuite->SetProgressString(exID, utf_str);
		}
		
		
	
		exRatioValue fps;
		get_framerate(ticksPerSecond, frameRateP.value.timeValue, &fps);
		
		
		vpx_codec_err_t codec_err = VPX_CODEC_OK;
		
		vpx_codec_ctx_t encoder;
		vpx_codec_iter_t encoder_iter = NULL;
		
		unsigned long deadline = VPX_DL_GOOD_QUALITY;

												
		PrTime videoEncoderTime = exportInfoP->startTime;
		
		if(exportInfoP->exportVideo)
		{
			vpx_codec_iface_t *iface = use_vp9 ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
			
			vpx_codec_enc_cfg_t config;
			vpx_codec_enc_config_default(iface, &config, 0);
			
			config.g_w = renderParms.inWidth;
			config.g_h = renderParms.inHeight;
			
			// (only applies to VP9)
			// Profile 0 is 4:2:0 only
			// Profile 1 can do 4:4:4 and 4:2:2
			// Profile 2 can do 10- and 12-bit, 4:2:0 only
			// Profile 3 can do 10- and 12-bit, 4:4:4 and 4:2:2
			config.g_profile = (chroma > WEBM_420 ?
									(bit_depth > 8 ? 3 : 1) :
									(bit_depth > 8 ? 2 : 0) );
			
			config.g_bit_depth = (bit_depth == 12 ? VPX_BITS_12 :
									bit_depth == 10 ? VPX_BITS_10 :
									VPX_BITS_8);
			
			config.g_input_bit_depth = config.g_bit_depth;
			
			
			if(method == WEBM_METHOD_CONSTANT_QUALITY || method == WEBM_METHOD_CONSTRAINED_QUALITY)
			{
				config.rc_end_usage = (method == WEBM_METHOD_CONSTANT_QUALITY ? VPX_Q : VPX_CQ);
				config.g_pass = VPX_RC_ONE_PASS;
				
				const int min_q = config.rc_min_quantizer + 1;
				const int max_q = config.rc_max_quantizer;
				
				// our 0...100 slider will be used to bring max_q down to min_q
				config.rc_max_quantizer = min_q + ((((float)(100 - videoQualityP.value.intValue) / 100.f) * (max_q - min_q)) + 0.5f);
			}
			else
			{
				if(method == WEBM_METHOD_VBR)
				{
					config.rc_end_usage = VPX_VBR;
				}
				else if(method == WEBM_METHOD_BITRATE)
				{
					config.rc_end_usage = VPX_CBR;
					config.g_pass = VPX_RC_ONE_PASS;
				}
				else
					assert(false);
			}
			
			if(passes == 2)
			{
				if(vbr_pass)
				{
					config.g_pass = VPX_RC_FIRST_PASS;
				}
				else
				{
					config.g_pass = VPX_RC_LAST_PASS;
					
					config.rc_twopass_stats_in.buf = vbr_buffer;
					config.rc_twopass_stats_in.sz = vbr_buffer_size;
				}
			}
			else
				config.g_pass = VPX_RC_ONE_PASS;
				
			
			config.rc_target_bitrate = bitrateP.value.intValue;
			
			
			config.g_threads = g_num_cpus;
			
			config.g_timebase.num = fps.denominator;
			config.g_timebase.den = fps.numerator;
			
			ConfigureEncoderPre(config, deadline, customArgs);
			
			
			const vpx_codec_flags_t flags = (config.g_bit_depth == VPX_BITS_8 ? 0 : VPX_CODEC_USE_HIGHBITDEPTH);
			
			codec_err = vpx_codec_enc_init(&encoder, iface, &config, flags);
			
			
			if(codec_err == VPX_CODEC_OK)
			{
				if(method == WEBM_METHOD_CONSTANT_QUALITY || method == WEBM_METHOD_CONSTRAINED_QUALITY)
				{
					const int min_q = config.rc_min_quantizer;
					const int max_q = config.rc_max_quantizer;
					
					// CQ Level should be between min_q and max_q
					const int cq_level = (min_q + max_q) / 2;
				
					vpx_codec_control(&encoder, VP8E_SET_CQ_LEVEL, cq_level);
				}
				
				if(use_vp9)
				{
					vpx_codec_control(&encoder, VP8E_SET_CPUUSED, 2); // much faster if we do this
					
					vpx_codec_control(&encoder, VP9E_SET_TILE_COLUMNS, mylog2(g_num_cpus)); // this gives us some multithreading
					vpx_codec_control(&encoder, VP9E_SET_FRAME_PARALLEL_DECODING, 1);
				}
			
				ConfigureEncoderPost(&encoder, customArgs);
			}
		}
		
	
	#define OV_OK 0
	
		int v_err = OV_OK;
	
		vorbis_info vi;
		vorbis_comment vc;
		vorbis_dsp_state vd;
		vorbis_block vb;
		ogg_packet op;
		
		bool packet_waiting = false;
		op.granulepos = 0;
		op.packet = NULL;
		op.bytes = 0;
		
		OpusMSEncoder *opus = NULL;
		float *opus_buffer = NULL;
		unsigned char *opus_compressed_buffer = NULL;
		opus_int32 opus_compressed_buffer_size = 0;
		int opus_pre_skip = 0;
										
		int opus_frame_size = 960;
		float *pr_audio_buffer[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
		
		size_t private_size = 0;
		void *private_data = NULL;
		
		csSDK_int32 maxBlip = 100;
		
		if(exportInfoP->exportAudio && !vbr_pass)
		{
			mySettings->sequenceAudioSuite->GetMaxBlip(audioRenderID, frameRateP.value.timeValue, &maxBlip);
			
			if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
			{
				const int sample_rate = 48000;
				
				const int mapping_family = (audioChannels > 2 ? 1 : 0);
				
				const int streams = (audioChannels > 2 ? 4 : 1);
				const int coupled_streams = (audioChannels > 2 ? 2 : 1);
				
				const unsigned char surround_mapping[6] = {0, 4, 1, 2, 3, 5};
				const unsigned char stereo_mapping[6] = {0, 1, 0, 1, 0, 1};
				
				const unsigned char *mapping = (audioChannels > 2 ? surround_mapping : stereo_mapping);
				
				int err = -1;
				
				opus = opus_multistream_encoder_create(sample_rate, audioChannels,
														streams, coupled_streams, mapping,
														OPUS_APPLICATION_AUDIO, &err);
				
				if(opus != NULL && err == OPUS_OK)
				{
					if(!autoBitrateP.value.intValue) // OPUS_AUTO is the default
						opus_multistream_encoder_ctl(opus, OPUS_SET_BITRATE(opusBitrateP.value.intValue * 1000));
						
				
					// build Opus headers
					// http://wiki.xiph.org/OggOpus
					// http://tools.ietf.org/html/draft-terriberry-oggopus-01
					// http://wiki.xiph.org/MatroskaOpus
					
					// ID header
					unsigned char id_head[28];
					memset(id_head, 0, 28);
					size_t id_header_size = 0;
					
					strcpy((char *)id_head, "OpusHead");
					id_head[8] = 1; // version
					id_head[9] = audioChannels;
					
					
					// pre-skip
					opus_int32 skip = 0;
					opus_multistream_encoder_ctl(opus, OPUS_GET_LOOKAHEAD(&skip));
					opus_pre_skip = skip;
					
					const unsigned short skip_us = skip;
					id_head[10] = skip_us & 0xff;
					id_head[11] = skip_us >> 8;
					
					
					// sample rate
					const unsigned int sample_rate_ui = sample_rate;
					id_head[12] = sample_rate_ui & 0xff;
					id_head[13] = (sample_rate_ui & 0xff00) >> 8;
					id_head[14] = (sample_rate_ui & 0xff0000) >> 16;
					id_head[15] = (sample_rate_ui & 0xff000000) >> 24;
					
					
					// output gain (set to 0)
					id_head[16] = id_head[17] = 0;
					
					
					// channel mapping
					id_head[18] = mapping_family;
					
					if(mapping_family == 1)
					{
						assert(audioChannels == 6);
					
						id_head[19] = streams;
						id_head[20] = coupled_streams;
						memcpy(&id_head[21], mapping, 6);
						
						id_header_size = 27;
					}
					else
					{
						id_header_size = 19;
					}
					
					private_size = id_header_size;
					
					private_data = malloc(private_size);
					
					memcpy(private_data, id_head, private_size);
					
					
					// figure out the frame size to use
					opus_frame_size = sample_rate / 400;
					
					const int samples_per_frame = sample_rate * fps.denominator / fps.numerator;
					
					while(opus_frame_size * 2 < samples_per_frame && opus_frame_size * 2 < maxBlip)
					{
						opus_frame_size *= 2;
					}
					
					opus_buffer = (float *)malloc(sizeof(float) * audioChannels * opus_frame_size);
					
					opus_compressed_buffer_size = sizeof(float) * audioChannels * opus_frame_size * 2; // why not?
					
					opus_compressed_buffer = (unsigned char *)malloc(opus_compressed_buffer_size);
				}
				else
					v_err = (err != 0 ? err : -1);
			}
			else
			{
				vorbis_info_init(&vi);
				
				if(audioMethodP.value.intValue == OGG_BITRATE)
				{
					v_err = vorbis_encode_init(&vi,
												audioChannels,
												sampleRateP.value.floatValue,
												-1,
												audioBitrateP.value.intValue * 1000,
												-1);
				}
				else
				{
					v_err = vorbis_encode_init_vbr(&vi,
													audioChannels,
													sampleRateP.value.floatValue,
													audioQualityP.value.floatValue);
				}
				
				if(v_err == OV_OK)
				{
					vorbis_comment_init(&vc);
					vorbis_analysis_init(&vd, &vi);
					vorbis_block_init(&vd, &vb);
					
					
					ogg_packet header;
					ogg_packet header_comm;
					ogg_packet header_code;
					
					vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
					
					private_data = MakePrivateData(header, header_comm, header_code, private_size);
					
					opus_frame_size = maxBlip;
				}
				else
					exportInfoP->exportAudio = kPrFalse;
			}
			
			for(int i=0; i < audioChannels; i++)
			{
				pr_audio_buffer[i] = (float *)malloc(sizeof(float) * opus_frame_size);
			}
		}
		
		
		if(codec_err == VPX_CODEC_OK && v_err == OV_OK)
		{
			// I'd say think about lowering this to get better precision,
			// but I get some messed up stuff when I do that.  Maybe a bug in the muxer?
			// The WebM spec says to keep it at one million:
			// http://www.webmproject.org/docs/container/#muxer-guidelines
			const long long timeCodeScale = 1000000LL;
			
			uint64_t vid_track = 0;
			uint64_t audio_track = 0;
			
			if(!vbr_pass)
			{
				writer = new PrMkvWriter(mySettings->exportFileSuite, exportInfoP->fileObject);
				
				muxer_segment = new mkvmuxer::Segment;
				
				muxer_segment->Init(writer);
				muxer_segment->set_mode(mkvmuxer::Segment::kFile);
				
				
				mkvmuxer::SegmentInfo* const info = muxer_segment->GetSegmentInfo();
				
				
				info->set_writing_app("fnord WebM for Premiere, built " __DATE__);
				
				
				// date_utc is defined as the number of nanoseconds since the beginning of the millenium (1 Jan 2001)
				// http://www.matroska.org/technical/specs/index.html
				struct tm date_utc_base;
				memset(&date_utc_base, 0, sizeof(struct tm));
				
				date_utc_base.tm_year = 2001 - 1900;
				
				time_t base = mktime(&date_utc_base);
				
				info->set_date_utc( (int64_t)difftime(time(NULL), base) * S2NS );
				
				
				info->set_timecode_scale(timeCodeScale);
				
		
				if(exportInfoP->exportVideo)
				{
					vid_track = muxer_segment->AddVideoTrack(renderParms.inWidth, renderParms.inHeight, 1);
					
					mkvmuxer::VideoTrack* const video = static_cast<mkvmuxer::VideoTrack *>(muxer_segment->GetTrackByNumber(vid_track));
					
					video->set_frame_rate((double)fps.numerator / (double)fps.denominator);

					video->set_codec_id(codecP.value.intValue == WEBM_CODEC_VP9 ? mkvmuxer::Tracks::kVp9CodecId :
																					mkvmuxer::Tracks::kVp8CodecId);
											
					if(renderParms.inPixelAspectRatioNumerator != renderParms.inPixelAspectRatioDenominator)
					{
						const uint64_t display_width = ((double)renderParms.inWidth *
														(double)renderParms.inPixelAspectRatioNumerator /
														(double)renderParms.inPixelAspectRatioDenominator)
														+ 0.5;
					
						video->set_display_width(display_width);
						video->set_display_height(renderParms.inHeight);
					}
					
					muxer_segment->CuesTrack(vid_track);
				}
				
				
				if(exportInfoP->exportAudio)
				{
					if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
					{
						assert(sampleRateP.value.floatValue == 48000.f);
						
						sampleRateP.value.floatValue = 48000.f; // we'll just go ahead and enforce that
					}
				
					audio_track = muxer_segment->AddAudioTrack(sampleRateP.value.floatValue, audioChannels, 2);
					
					mkvmuxer::AudioTrack* const audio = static_cast<mkvmuxer::AudioTrack *>(muxer_segment->GetTrackByNumber(audio_track));
					
					audio->set_codec_id(audioCodecP.value.intValue == WEBM_CODEC_OPUS ? mkvmuxer::Tracks::kOpusCodecId :
																						mkvmuxer::Tracks::kVorbisCodecId);
					
					if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
					{
						// http://wiki.xiph.org/MatroskaOpus
						
						audio->set_seek_pre_roll(80000000);
						
						audio->set_codec_delay((PrAudioSample)opus_pre_skip * S2NS / (PrAudioSample)sampleRateP.value.floatValue);
					}

					if(private_data)
					{
						bool copied = audio->SetCodecPrivate((const uint8_t *)private_data, private_size);
						
						assert(copied);
						
						free(private_data);
					}

					if(!exportInfoP->exportVideo)
						muxer_segment->CuesTrack(audio_track);
				}
			}
			
			PrAudioSample currentAudioSample = 0;

			// Here's a question: what do we do when the number of audio samples doesn't match evenly
			// with the number of frames?  This could especially happen when the user changes the frame
			// rate to something other than what Premiere is using to specify the out time.  So you could
			// have just enough time to pop up one more frame, but not enough time to fill the audio for
			// that frame.  What to do?  We could extend the movie duration to the whole frame, but right now
			// we'll just encode the amount of audio originally requested.  One ramification is that you could
			// be done encoding all your audio but still have a final frame to encode.
			const PrAudioSample endAudioSample = (exportInfoP->endTime - exportInfoP->startTime) /
													(ticksPerSecond / (PrAudioSample)sampleRateP.value.floatValue);
													
			assert(ticksPerSecond % (PrAudioSample)sampleRateP.value.floatValue == 0);
			
		
			PrTime videoTime = exportInfoP->startTime;
			
			while(videoTime <= exportInfoP->endTime && result == malNoError)
			{
				const PrTime fileTime = videoTime - exportInfoP->startTime;
				
				// Time (in nanoseconds) = TimeCode * TimeCodeScale.
				const long long timeCode = ((fileTime * (S2NS / timeCodeScale)) + (ticksPerSecond / 2)) / ticksPerSecond;
				
				const uint64_t timeStamp = timeCode * timeCodeScale;
			
				const bool last_frame = (videoTime > (exportInfoP->endTime - frameRateP.value.timeValue));
				
				if(last_frame)
					assert(videoTime == exportInfoP->endTime); // don't think this will be true, actually
				
				// This is not accurate enough, so we'll use doubles
				//const PrTime ticksPerNanosecond = (ticksPerSecond / S2NS);
				//assert(ticksPerSecond % S2NS == 0);
				//
				//const uint64_t frameDuration = (last_frame && videoTime != exportInfoP->endTime ? 
				//								((exportInfoP->endTime - videoTime) + (ticksPerNanosecond / 2)) / ticksPerNanosecond :
				//								(frameRateP.value.timeValue + (ticksPerNanosecond / 2)) / ticksPerNanosecond);
						
				const uint64_t frameDuration = (last_frame && videoTime != exportInfoP->endTime ? 
												(((double)(exportInfoP->endTime - videoTime) / (double)ticksPerSecond) * (double)S2NS) + 0.5 :
												(((double)frameRateP.value.timeValue / (double)ticksPerSecond) * (double)S2NS) + 0.5);
				
				// When writing WebM, we want blocks of audio and video interleaved.
				// But encoders don't always cooperate with our wishes.  We feed them some data,
				// but they may not be ready to produce output right away.  So what we do is keep
				// feeding in the data until the output we want is produced.
				
				if(exportInfoP->exportAudio && !vbr_pass)
				{
					// Premiere uses Left, Right, Left Rear, Right Rear, Center, LFE
					// Opus and Vorbis use Left, Center, Right, Left Read, Right Rear, LFE
					// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
					static const int stereo_swizzle[] = {0, 1, 0, 1, 0, 1};
					static const int surround_swizzle[] = {0, 4, 1, 2, 3, 5};
					
					const int *swizzle = (audioChannels > 2 ? surround_swizzle : stereo_swizzle);
					
					
					if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
					{
						assert(opus != NULL);
						
						long long opus_timeStamp = currentAudioSample * S2NS / (long long)sampleRateP.value.floatValue;
						
						while(((opus_timeStamp <= timeStamp) || last_frame) && currentAudioSample < (endAudioSample + opus_pre_skip) && result == malNoError)
						{
							const int samples = opus_frame_size;
							
							result = audioSuite->GetAudio(audioRenderID, samples, pr_audio_buffer, false);
							
							if(result == malNoError)
							{
								for(int i=0; i < samples; i++)
								{
									for(int c=0; c < audioChannels; c++)
									{
										opus_buffer[(i * audioChannels) + c] = pr_audio_buffer[swizzle[c]][i];
									}
								}
								
								int len = opus_multistream_encode_float(opus, opus_buffer, opus_frame_size,
																			opus_compressed_buffer, opus_compressed_buffer_size);
								
								if(len > 0)
								{
									mkvmuxer::Frame frame;
									
									const bool inited = frame.Init(opus_compressed_buffer, len);
									
									if(!inited)
										throw -1;
									
									frame.set_track_number(audio_track);
									frame.set_timestamp(opus_timeStamp);
									frame.set_is_key(true);
									
									if(last_frame)
										frame.set_duration(frameDuration);
																																				
									if((currentAudioSample + samples) > (endAudioSample + opus_pre_skip))
									{
										assert(last_frame);
									
										const int64_t discardPaddingSamples = (currentAudioSample + samples) - (endAudioSample + opus_pre_skip);
										const int64_t discardPadding = discardPaddingSamples * S2NS / (int64_t)sampleRateP.value.floatValue;
										
										frame.set_discard_padding(discardPadding);
									}
									
									const bool added = muxer_segment->AddGenericFrame(&frame);
																			
									if(!added)
										result = exportReturn_InternalError;
								}
								else if(len < 0)
									result = exportReturn_InternalError;
								
								
								currentAudioSample += samples;
								
								opus_timeStamp = currentAudioSample * S2NS / (long long)sampleRateP.value.floatValue;
							}
						}
					}
					else
					{
						long long op_timeStamp = op.granulepos * S2NS / (long long)sampleRateP.value.floatValue;
					
						while(op_timeStamp <= timeStamp && op.granulepos < endAudioSample && result == malNoError)
						{	
							// We don't know what samples are in the packet until we get it,
							// but by then it's too late to decide if we don't want it for this frame.
							// So we'll hold on to that packet and use it next frame.
							if(packet_waiting && op.packet != NULL && op.bytes > 0)
							{
								mkvmuxer::Frame frame;
								
								const bool inited = frame.Init(op.packet, op.bytes);
								
								if(!inited)
									throw -1;
								
								frame.set_track_number(audio_track);
								frame.set_timestamp(op_timeStamp);
								frame.set_is_key(true);
								
								if(last_frame)
									frame.set_duration(frameDuration);
								
								const bool added = muxer_segment->AddGenericFrame(&frame);
																		
								if(!added)
									result = exportReturn_InternalError;
							}
							
							
							// push out packets
							while(vorbis_analysis_blockout(&vd, &vb) == 1 && result == malNoError)
							{
								vorbis_analysis(&vb, NULL);
								vorbis_bitrate_addblock(&vb);
								
								while(vorbis_bitrate_flushpacket(&vd, &op) && result == malNoError)
								{
									assert(!packet_waiting);
									
									op_timeStamp = op.granulepos * S2NS / (long long)sampleRateP.value.floatValue;
									
									if(op_timeStamp <= timeStamp || last_frame)
									{
										mkvmuxer::Frame frame;
										
										const bool inited = frame.Init(op.packet, op.bytes);
										
										if(!inited)
											throw -1;
										
										frame.set_track_number(audio_track);
										frame.set_timestamp(op_timeStamp);
										frame.set_is_key(true);
										
										const bool added = muxer_segment->AddGenericFrame(&frame);
																				
										if(!added)
											result = exportReturn_InternalError;
									}
									else
										packet_waiting = true;
								}
								
								if(packet_waiting)
									break;
							}
							
							
							if(packet_waiting)
								break;
							
							
							// make new packets
							if(op_timeStamp <= timeStamp && op.granulepos < endAudioSample && result == malNoError)
							{
								int samples = opus_frame_size; // opus_frame_size is also the size of our buffer in samples
								
								assert(samples == maxBlip);
								
								assert(currentAudioSample <= endAudioSample); // so samples won't be negative
								
								if(samples > (endAudioSample - currentAudioSample))
									samples = (endAudioSample - currentAudioSample);
									
								if(samples > 0)
								{
									float **buffer = vorbis_analysis_buffer(&vd, samples);
									
									result = audioSuite->GetAudio(audioRenderID, samples, pr_audio_buffer, false);
									
									for(int c=0; c < audioChannels; c++)
									{
										for(int i=0; i < samples; i++)
										{
											buffer[c][i] = pr_audio_buffer[swizzle[c]][i];
										}
									}
								}
								
								currentAudioSample += samples;
								
								
								if(result == malNoError)
								{
									vorbis_analysis_wrote(&vd, samples);
									
									if(currentAudioSample >= endAudioSample)
										vorbis_analysis_wrote(&vd, NULL); // we have sent everything in
								}
							}
						}
					}
				}
				
				
				if(exportInfoP->exportVideo && (videoTime < exportInfoP->endTime)) // there will some audio after the last video frame
				{
					bool made_frame = false;
					
					while(!made_frame && result == suiteError_NoError)
					{
						const vpx_codec_cx_pkt_t *pkt = NULL;
						
						if( (pkt = vpx_codec_get_cx_data(&encoder, &encoder_iter)) )
						{
							if(pkt->kind == VPX_CODEC_STATS_PKT)
							{
								assert(vbr_pass);
							
								if(vbr_buffer_size == 0)
									vbr_buffer = memorySuite->NewPtr(pkt->data.twopass_stats.sz);
								else
									memorySuite->SetPtrSize(&vbr_buffer, vbr_buffer_size + pkt->data.twopass_stats.sz);
								
								memcpy(&vbr_buffer[vbr_buffer_size], pkt->data.twopass_stats.buf, pkt->data.twopass_stats.sz);
								
								vbr_buffer_size += pkt->data.twopass_stats.sz;
								
								made_frame = true;
							}
							else if(pkt->kind == VPX_CODEC_CX_FRAME_PKT)
							{
								assert( !vbr_pass );
								assert( !(pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE) ); // libwebm not handling these now
								assert( !(pkt->data.frame.flags & VPX_FRAME_IS_FRAGMENT) );
								assert( pkt->data.frame.pts == (videoTime - exportInfoP->startTime) * fps.numerator / (ticksPerSecond * fps.denominator) );
								assert( pkt->data.frame.duration == 1 ); // because of how we did the timescale
							
								mkvmuxer::Frame frame;
								
								const bool inited = frame.Init((uint8_t *)pkt->data.frame.buf, pkt->data.frame.sz);
								
								if(!inited)
									throw -1;
								
								frame.set_track_number(vid_track);
								frame.set_timestamp(timeStamp);
								frame.set_is_key(pkt->data.frame.flags & VPX_FRAME_IS_KEY);
								
								if(last_frame)
									frame.set_duration(frameDuration);
								
								const bool added = muxer_segment->AddGenericFrame(&frame);
																		
								if( !(pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE) )
									made_frame = true;
								
								if(!added)
									result = exportReturn_InternalError;
							}
							
							assert(pkt->kind != VPX_CODEC_FPMB_STATS_PKT); // don't know what to do with this
						}
						
						if(vbr_pass)
						{
							// if that was the last VBR packet, we have to finalize and write a summary packet,
							// so go through the loop once more
							if(videoEncoderTime >= exportInfoP->endTime)
								made_frame = false;
							
							// the final packet was just written, so break
							if(videoEncoderTime == LONG_LONG_MAX)
								break;
						}
						
						
						if(!made_frame && result == suiteError_NoError)
						{
							// this is for the encoder, which does its own math based on config.g_timebase
							// let's do the math
							// time = timestamp * timebase :: time = videoTime / ticksPerSecond : timebase = 1 / fps
							// timestamp = time / timebase
							// timestamp = (videoTime / ticksPerSecond) * (fps.num / fps.den)
							const PrTime encoder_fileTime = videoEncoderTime - exportInfoP->startTime;
							const PrTime encoder_nextFileTime = encoder_fileTime + frameRateP.value.timeValue;
							
							const vpx_codec_pts_t encoder_timeStamp = encoder_fileTime * fps.numerator / (ticksPerSecond * fps.denominator);
							const vpx_codec_pts_t encoder_nextTimeStamp = encoder_nextFileTime * fps.numerator / (ticksPerSecond * fps.denominator);
							const unsigned long encoder_duration = encoder_nextTimeStamp - encoder_timeStamp;
							
							// BUT, if we're setting timebase to 1/fps, then timestamp is just frame number.
							// And since frame number isn't going to overflow at big times the way encoder_timeStamp is,
							// let's just use that.
							const vpx_codec_pts_t encoder_FrameNumber = encoder_fileTime / frameRateP.value.timeValue;
							const unsigned long encoder_FrameDuration = 1;
							
							// these asserts will not be true for big time values (int64_t overflow)
							if(videoEncoderTime < LONG_MAX)
							{
								assert(encoder_FrameNumber == encoder_timeStamp);
								assert(encoder_FrameDuration == encoder_duration);
							}
							
				
							if(videoEncoderTime < exportInfoP->endTime)
							{
								SequenceRender_GetFrameReturnRec renderResult;
								
								result = renderSuite->RenderVideoFrame(videoRenderID,
																		videoEncoderTime,
																		&renderParms,
																		kRenderCacheType_None,
																		&renderResult);
								
								if(result == suiteError_NoError)
								{
									prRect bounds;
									csSDK_uint32 parN, parD;
									
									pixSuite->GetBounds(renderResult.outFrame, &bounds);
									pixSuite->GetPixelAspectRatio(renderResult.outFrame, &parN, &parD);
									
									const int width = bounds.right - bounds.left;
									const int height = bounds.bottom - bounds.top;
									
									assert(width == widthP.value.intValue);
									assert(height == heightP.value.intValue);
									assert(parN == pixelAspectRatioP.value.ratioValue.numerator);  // Premiere sometimes screws this up
									assert(parD == pixelAspectRatioP.value.ratioValue.denominator);
									
									
									// see validate_img() and validate_config() in vp8_cx_iface.c and vp9_cx_iface.c
									const vpx_img_fmt_t imgfmt8 = chroma == WEBM_444 ? VPX_IMG_FMT_I444 :
																	chroma == WEBM_422 ? VPX_IMG_FMT_I422 :
																	VPX_IMG_FMT_I420;
																	
									const vpx_img_fmt_t imgfmt16 = chroma == WEBM_444 ? VPX_IMG_FMT_I44416 :
																	chroma == WEBM_422 ? VPX_IMG_FMT_I42216 :
																	VPX_IMG_FMT_I42016;
																	
									const vpx_img_fmt_t imgfmt = (bit_depth > 8 ? imgfmt16 : imgfmt8);
									
											
									vpx_image_t img_data;
									vpx_image_t *img = vpx_img_alloc(&img_data, imgfmt, width, height, 32);
									
									if(img)
									{
										if(bit_depth > 8)
										{
											img->bit_depth = bit_depth;
											img->bps = img->bps * bit_depth / 16;
										}
									
										CopyPixToImg(img, renderResult.outFrame, pixSuite, pix2Suite);
										
										
										vpx_codec_err_t encode_err = vpx_codec_encode(&encoder, img, encoder_FrameNumber, encoder_FrameDuration, 0, deadline);
										
										if(encode_err == VPX_CODEC_OK)
										{
											videoEncoderTime += frameRateP.value.timeValue;

											encoder_iter = NULL;
										}
										else
											result = exportReturn_InternalError;
										
										
										vpx_img_free(img);
									}
									else
										result = exportReturn_ErrMemory;
									
									
									pixSuite->Dispose(renderResult.outFrame);
								}
							}
							else
							{
								// squeeze the last bit out of the encoder
								vpx_codec_err_t encode_err = vpx_codec_encode(&encoder, NULL, encoder_FrameNumber, encoder_FrameDuration, 0, deadline);
								
								if(encode_err == VPX_CODEC_OK)
								{
									videoEncoderTime = LONG_LONG_MAX;
									
									encoder_iter = NULL;
								}
								else
									result = exportReturn_InternalError;
							}
						}
					}
				}
				
				
				if(result == malNoError)
				{
					float progress = (double)(videoTime - exportInfoP->startTime) / (double)(exportInfoP->endTime - exportInfoP->startTime);
					
					if(passes == 2)
					{
						const float firstpass_frac = (use_vp9 ? 0.1f : 0.3f);
					
						if(pass == 1)
						{
							progress = firstpass_frac + (progress * (1.f - firstpass_frac));
						}
						else
							progress = (progress * firstpass_frac);
					}

					result = mySettings->exportProgressSuite->UpdateProgressPercent(exID, progress);
					
					if(result == suiteError_ExporterSuspended)
					{
						result = mySettings->exportProgressSuite->WaitForResume(exID);
					}
				}
				
				
				videoTime += frameRateP.value.timeValue;
			}
			
			
			// audio sanity check
			if(result == malNoError && exportInfoP->exportAudio && !vbr_pass)
			{
				if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
					assert(currentAudioSample >= (endAudioSample + opus_pre_skip));
				else
					assert(op.granulepos == endAudioSample);
			}
		}
		else
			result = exportReturn_InternalError;
		


		if(exportInfoP->exportVideo)
		{
			if(result == malNoError)
				assert(NULL == vpx_codec_get_cx_data(&encoder, &encoder_iter));
		
			vpx_codec_err_t destroy_err = vpx_codec_destroy(&encoder);
			assert(destroy_err == VPX_CODEC_OK);
		}
			
		if(exportInfoP->exportAudio && !vbr_pass)
		{
			if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
			{
				opus_multistream_encoder_destroy(opus);
				
				if(opus_buffer)
					free(opus_buffer);
				
				if(opus_compressed_buffer)
					free(opus_compressed_buffer);
			}
			else
			{
				if(result == malNoError)
					assert(vorbis_analysis_blockout(&vd, &vb) == 0);
			
				vorbis_block_clear(&vb);
				vorbis_dsp_clear(&vd);
				vorbis_comment_clear(&vc);
				vorbis_info_clear(&vi);
			}
			
			for(int i=0; i < audioChannels; i++)
			{
				if(pr_audio_buffer[i] != NULL)
					free(pr_audio_buffer[i]);
			}
		}
	}
	
	
	if(muxer_segment != NULL)
	{
		bool final = muxer_segment->Finalize();
		
		if(!final)
			result = exportReturn_InternalError;
	}
	
	
	}catch(...) { result = exportReturn_InternalError; }
	
	
	delete muxer_segment;
	
	delete writer;
	
	
	if(vbr_buffer != NULL)
		memorySuite->PrDisposePtr(vbr_buffer);
	
	
	if(exportInfoP->exportVideo)
		renderSuite->ReleaseVideoRenderer(exID, videoRenderID);

	if(exportInfoP->exportAudio)
		audioSuite->ReleaseAudioRenderer(exID, audioRenderID);
	

	return result;
}




DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParmsP, 
	void			*param1, 
	void			*param2)
{
	prMALError result = exportReturn_Unsupported;
	
	switch (selector)
	{
		case exSelStartup:
			result = exSDKStartup(	stdParmsP, 
									reinterpret_cast<exExporterInfoRec*>(param1));
			break;

		case exSelBeginInstance:
			result = exSDKBeginInstance(stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelEndInstance:
			result = exSDKEndInstance(	stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelGenerateDefaultParams:
			result = exSDKGenerateDefaultParams(stdParmsP,
												reinterpret_cast<exGenerateDefaultParamRec*>(param1));
			break;

		case exSelPostProcessParams:
			result = exSDKPostProcessParams(stdParmsP,
											reinterpret_cast<exPostProcessParamsRec*>(param1));
			break;

		case exSelGetParamSummary:
			result = exSDKGetParamSummary(	stdParmsP,
											reinterpret_cast<exParamSummaryRec*>(param1));
			break;

		case exSelQueryOutputSettings:
			result = exSDKQueryOutputSettings(	stdParmsP,
												reinterpret_cast<exQueryOutputSettingsRec*>(param1));
			break;

		case exSelQueryExportFileExtension:
			result = exSDKFileExtension(stdParmsP,
										reinterpret_cast<exQueryExportFileExtensionRec*>(param1));
			break;

		case exSelValidateParamChanged:
			result = exSDKValidateParamChanged(	stdParmsP,
												reinterpret_cast<exParamChangedRec*>(param1));
			break;

		case exSelValidateOutputSettings:
			result = malNoError;
			break;

		case exSelExport:
			result = exSDKExport(	stdParmsP,
									reinterpret_cast<exDoExportRec*>(param1));
			break;
	}
	
	return result;
}
