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



#ifdef PRMAC_ENV
	#include <mach/mach.h>
#else
	#include <assert.h>
	#include <time.h>
	#include <sys/timeb.h>
#endif

#include <sstream>


extern "C" {

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>

}

#include "mkvmuxer.hpp"

#include <assert.h>


typedef mkvmuxer::int32 int32;
typedef mkvmuxer::int64 int64;

class PrMkvWriter : public mkvmuxer::IMkvWriter
{
  public:
	PrMkvWriter(PrSDKExportFileSuite *fileSuite, csSDK_uint32 fileObject);
	virtual ~PrMkvWriter();
	
	virtual int32 Write(const void* buf, uint32 len);
	virtual int64 Position() const;
	virtual int32 Position(int64 position); // seek
	virtual bool Seekable() const { return true; }
	virtual void ElementStartNotify(uint64 element_id, int64 position);
	
  private:
	//PrMkvWriter() : _fileSuite(NULL), _fileObject(NULL) { throw -1; }
	const PrSDKExportFileSuite *_fileSuite;
	const csSDK_uint32 _fileObject;
	
	//LIBWEBM_DISALLOW_COPY_AND_ASSIGN(PrMkvWriter);
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
}

int32
PrMkvWriter::Write(const void* buf, uint32 len)
{
	prSuiteError err = _fileSuite->Write(_fileObject, (void *)buf, len);
	
	return err;
}


int64
PrMkvWriter::Position() const
{
	prInt64 pos = 0;

// son of a gun, fileSeekMode_End and fileSeekMode_Current are flipped inside Premiere!
#define PR_SEEK_CURRENT fileSeekMode_End

	prSuiteError err = _fileSuite->Seek(_fileObject, 0, pos, PR_SEEK_CURRENT);
	
	return pos;
}

int32
PrMkvWriter::Position(int64 position)
{
	prInt64 pos = 0;

	prSuiteError err = _fileSuite->Seek(_fileObject, position, pos, fileSeekMode_Begin);
	
	return err;
}

void
PrMkvWriter::ElementStartNotify(uint64 element_id, int64 position)
{
	//Position(position);
}




/*
#include "math.h"
#define MAX_PSNR 100
static double vp8_mse2psnr(double Samples, double Peak, double Mse) {
  double psnr;

  if ((double)Mse > 0.0)
    psnr = 10.0 * log10(Peak * Peak * Samples / Mse);
  else
    psnr = MAX_PSNR;      // Limit to prevent / 0

  if (psnr > MAX_PSNR)
    psnr = MAX_PSNR;

  return psnr;
}
*/

#pragma mark-


static const csSDK_int32 WebM_ID = 'WebM';
static const csSDK_int32 WebM_Export_Class = 'WebM';



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


static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}


static prMALError
exSDKStartup(
	exportStdParms		*stdParmsP, 
	exExporterInfoRec	*infoRecP)
{
	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParmsP->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		int fourCC = 0;
	
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);
	
		stdParmsP->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
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



#define ADBEVideoAlpha		"ADBEVideoAlpha"


static prMALError
exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP)
{
	prMALError result = malNoError;
	
	ExportSettings *privateData	= reinterpret_cast<ExportSettings*>(outputSettingsP->privateData);
	
	csSDK_uint32				exID			= outputSettingsP->exporterPluginID;
	exParamValues				width,
								height,
								frameRate,
								pixelAspectRatio,
								fieldType,
								alpha;
	PrSDKExportParamSuite		*paramSuite		= privateData->exportParamSuite;
	csSDK_int32					mgroupIndex		= 0;
	float						fps				= 0.0f;
	PrTime						ticksPerSecond	= 0;
	csSDK_uint32				videoBitrate	= 0;
	
	if(outputSettingsP->inExportVideo)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		outputSettingsP->outVideoWidth = width.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		outputSettingsP->outVideoHeight = height.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
		
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
		
	}
	
	// return outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = 1001;


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

/*
static TimeCode
CalculateTimeCode(int frame_num, int frame_rate, bool drop_frame)
{
	// the easiest way to do this is just count!
	int h = 0,
		m = 0,
		s = 0,
		f = 0;
	
	// skip ahead quickly
	int frames_per_ten_mins = (frame_rate * 60 * 10) - (drop_frame ? 9 * (frame_rate == 60 ? 4 : 2) : 0);
	int frames_per_hour = 6 * frames_per_ten_mins;
	
	while(frame_num >= frames_per_hour)
	{
		h++;
		
		frame_num -= frames_per_hour;
	}
	
	while(frame_num >= frames_per_ten_mins)
	{
		m += 10;
		
		frame_num -= frames_per_ten_mins;
	}
	
	// now count out the rest
	int frame = 0;
	
	while(frame++ < frame_num)
	{
		if(f < frame_rate - 1)
		{
			f++;
		}
		else
		{
			f = 0;
			
			if(s < 59)
			{
				s++;
			}
			else
			{
				s = 0;
				
				if(m < 59)
				{
					m++;
					
					if(drop_frame && (m % 10) != 0) // http://en.wikipedia.org/wiki/SMPTE_timecode
					{
						f += (frame_rate == 60 ? 4 : 2);
					}
				}
				else
				{
					m = 0;
					
					h++;
				}
			}
		}
	}
	
	return TimeCode(h, m, s, f, drop_frame);
}
*/

/*
static void
Premultiply(Rgba &in)
{
	if(in.a != 1.f)
	{
		in.r *= in.a;
		in.g *= in.a;
		in.b *= in.a;
	}
}
*/


static void get_framerate(PrTime ticksPerSecond, PrTime ticks_per_frame, exRatioValue *fps)
{
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 50, 59,
							60};
													
	PrTime frameRateNumDens[][2] = {{10, 1}, {15, 1}, {24000, 1001},
									{24, 1}, {25, 1}, {30000, 1001},
									{30, 1}, {50, 1}, {60000, 1001},
									{60, 1}};
	
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
		fps->numerator = 1000 * ticksPerSecond / ticks_per_frame;
		fps->denominator = 1000;
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
	PrSDKExportInfoSuite		*exportInfoSuite		= mySettings->exportInfoSuite;
	PrSDKSequenceRenderSuite	*renderSuite			= mySettings->sequenceRenderSuite;
	PrSDKSequenceAudioSuite		*audioSuite				= mySettings->sequenceAudioSuite;
	PrSDKMemoryManagerSuite		*memorySuite			= mySettings->memorySuite;
	PrSDKPPixCreatorSuite		*pixCreatorSuite		= mySettings->ppixCreatorSuite;
	PrSDKPPixSuite				*pixSuite				= mySettings->ppixSuite;
	PrSDKPPix2Suite				*pix2Suite				= mySettings->ppix2Suite;

	if(!exportInfoP->exportVideo)
		return malNoError;


	PrTime ticksPerSecond = 0;
	mySettings->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	
	csSDK_uint32 exID = exportInfoP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	exParamValues widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP, alphaP, sampleRateP, channelTypeP;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);
	
	
	SequenceRender_ParamsRec renderParms;
	PrPixelFormat pixelFormats[] = { PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709,
									PrPixelFormat_BGRA_4444_8u }; // must support BGRA, even if I don't want to
	
	renderParms.inRequestedPixelFormatArray = pixelFormats;
	renderParms.inRequestedPixelFormatArrayCount = 2;
	renderParms.inWidth = widthP.value.intValue;
	renderParms.inHeight = heightP.value.intValue;
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatioP.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatioP.value.ratioValue.denominator;
	renderParms.inRenderQuality = kPrRenderQuality_High;
	renderParms.inFieldType = fieldTypeP.value.intValue;
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = kPrRenderQuality_High;
	renderParms.inCompositeOnBlack = (alphaP.value.intValue ? kPrFalse: kPrTrue);
	
	csSDK_uint32 videoRenderID;
	renderSuite->MakeVideoRenderer(exID, &videoRenderID, frameRateP.value.timeValue);
	
	//csSDK_uint32 audioRenderID;
	//audioSuite->MakeAudioRenderer(exID, exportInfoP->startTime, (PrAudioChannelType)channelTypeP.value.intValue,
	//								kPrAudioSampleType_32BitFloat, sampleRateP.value.floatValue, &audioRenderID);
	
	try{
	
	if(result == malNoError)
	{
		vpx_codec_iface_t *iface = vpx_codec_vp8_cx();
		
		vpx_codec_enc_cfg_t config;
		vpx_codec_enc_config_default(iface, &config, 0);
		
		config.g_w = renderParms.inWidth;
		config.g_h = renderParms.inHeight;
		
		config.g_pass = VPX_RC_ONE_PASS;
		config.g_threads = 8;
		
		exRatioValue fps;
		get_framerate(ticksPerSecond, frameRateP.value.timeValue, &fps);
		
		config.g_timebase.num = 1;
		config.g_timebase.den = 1000000; // for some reason I get errors if I set this to anything else; must investigate
		
		
		vpx_codec_ctx_t encoder;
		
		vpx_codec_err_t codec_err = vpx_codec_enc_init(&encoder, iface, &config, 0);
		
/*	
	#define OV_OK 0
	
		vorbis_info vi;
		vorbis_comment vc;
		vorbis_dsp_state vd;
		vorbis_block vb;
		
		vorbis_info_init(&vi);
		
		int v_err = vorbis_encode_setup_managed(&vi,
												channelTypeP.value.intValue,
												sampleRateP.value.floatValue,
												123, 456, 789);
		assert(v_err == OV_OK);
		
		vorbis_comment_init(&vc);
		vorbis_analysis_init(&vd, &vi);
		vorbis_block_init(&vd, &vb);
		
		
		ogg_packet header;
		ogg_packet header_comm;
		ogg_packet header_code;
		
		vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
		
		size_t private_size = 0;
		void *private_data = MakePrivateData(header, header_comm, header_code, private_size);
*/		
		
		if(codec_err == VPX_CODEC_OK)
		{
			PrMkvWriter writer(mySettings->exportFileSuite, exportInfoP->fileObject);

			mkvmuxer::Segment muxer_segment;
			
			muxer_segment.Init(&writer);
			muxer_segment.set_mode(mkvmuxer::Segment::kFile);
			
			
			mkvmuxer::SegmentInfo* const info = muxer_segment.GetSegmentInfo();
			
			info->set_timecode_scale(config.g_timebase.den);
			info->set_writing_app("fnord WebM");
			
			
			
			uint64 vid_track = muxer_segment.AddVideoTrack(renderParms.inWidth, renderParms.inHeight, 1);
			
			mkvmuxer::VideoTrack* const video = static_cast<mkvmuxer::VideoTrack *>(muxer_segment.GetTrackByNumber(vid_track));
			
			video->set_frame_rate((double)fps.numerator / (double)fps.denominator);
			video->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
			
			muxer_segment.CuesTrack(vid_track);
			
			
/*			
			uint64 audio_track = muxer_segment.AddAudioTrack(sampleRateP.value.floatValue, channelTypeP.value.intValue, 2);
			
			mkvmuxer::AudioTrack* const audio = static_cast<mkvmuxer::AudioTrack *>(muxer_segment.GetTrackByNumber(audio_track));
			
			if(private_data)
			{
				bool copied = audio->SetCodecPrivate((const uint8 *)private_data, private_size);
				
				assert(copied);
				
				free(private_data);
			}
*/			
		
			SequenceRender_GetFrameReturnRec renderResult;
			
			PrTime videoTime = exportInfoP->startTime;
			
			PrTime audioBegin = exportInfoP->startTime;
			int frames_without_audio = 0;
			
			while(videoTime < exportInfoP->endTime && result == suiteError_NoError)
			{
				result = renderSuite->RenderVideoFrame(videoRenderID,
														videoTime,
														&renderParms,
														kRenderCacheType_None,
														&renderResult);
				
				if(result == suiteError_NoError)
				{
					PrPixelFormat pixFormat;
					prRect bounds;
					csSDK_uint32 parN, parD;
					
					pixSuite->GetPixelFormat(renderResult.outFrame, &pixFormat);
					pixSuite->GetBounds(renderResult.outFrame, &bounds);
					pixSuite->GetPixelAspectRatio(renderResult.outFrame, &parN, &parD);
					
					const int width = bounds.right - bounds.left;
					const int height = bounds.bottom - bounds.top;
					
					vpx_image_t img_data;
					vpx_image_t *img = vpx_img_alloc(&img_data, VPX_IMG_FMT_I420, width, height, 32);;
					
					if(img)
					{
						if(pixFormat == PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709)
						{
							char *Y_PixelAddress, *U_PixelAddress, *V_PixelAddress;
							csSDK_uint32 Y_RowBytes, U_RowBytes, V_RowBytes;
							
							pix2Suite->GetYUV420PlanarBuffers(renderResult.outFrame, PrPPixBufferAccess_ReadOnly,
																&Y_PixelAddress, &Y_RowBytes,
																&U_PixelAddress, &U_RowBytes,
																&V_PixelAddress, &V_RowBytes);
							
							for(int y = 0; y < img->d_h; y++)
							{
								unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
								
								unsigned char *prY = (unsigned char *)Y_PixelAddress + (Y_RowBytes * y);
								
								memcpy(imgY, prY, img->d_w * sizeof(unsigned char));
							}
							
							for(int y = 0; y < img->d_h / 2; y++)
							{
								unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y);
								unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y);
								
								unsigned char *prU = (unsigned char *)U_PixelAddress + (U_RowBytes * y);
								unsigned char *prV = (unsigned char *)V_PixelAddress + (V_RowBytes * y);
								
								memcpy(imgU, prU, (img->d_w / 2) * sizeof(unsigned char));
								memcpy(imgV, prV, (img->d_w / 2) * sizeof(unsigned char));
							}
						}
						else if(pixFormat == PrPixelFormat_BGRA_4444_8u)
						{
							// libvpx can only take PX_IMG_FMT_YV12, VPX_IMG_FMT_I420, VPX_IMG_FMT_VPXI420, VPX_IMG_FMT_VPXYV12
							// see validate_img() in vp8_cx_iface.c
							
							// so here's our dumb RGB to YUV conversion
						
							char *frameBufferP = NULL;
							csSDK_int32 rowbytes = 0;
							
							pixSuite->GetPixels(renderResult.outFrame, PrPPixBufferAccess_ReadOnly, &frameBufferP);
							pixSuite->GetRowBytes(renderResult.outFrame, &rowbytes);
							
							
							for(int y = 0; y < img->d_h; y++)
							{
								unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
								unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / 2));
								unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / 2));
								
								unsigned char *prBGRA = (unsigned char *)frameBufferP + (rowbytes * y);
								
								unsigned char *prB = prBGRA + 0;
								unsigned char *prG = prBGRA + 1;
								unsigned char *prR = prBGRA + 2;
								unsigned char *prA = prBGRA + 3;
								
								for(int x=0; x < img->d_w; x++)
								{
									*imgY++ = (((257 * (int)*prR) + (504 * (int)*prG) + ( 98 * (int)*prB)) / 1000) + 16;
									
									if( (y % 2 == 0) && (x % 2 == 0) )
									{
										*imgV++ = (((439 * (int)*prR) + (368 * (int)*prG) - ( 71 * (int)*prB)) / 1000) + 128;
										*imgU++ = ((-(148 * (int)*prR) - (291 * (int)*prG) + (439 * (int)*prB)) / 1000) + 128;
									}
									
									prR += 4;
									prG += 4;
									prB += 4;
									prA += 4;
								}
							}
						}
						
						vpx_codec_pts_t timeStamp = (videoTime - exportInfoP->startTime) * config.g_timebase.den / ticksPerSecond;
						vpx_codec_pts_t nextTimeStamp = ((videoTime + frameRateP.value.timeValue) - exportInfoP->startTime) * config.g_timebase.den / ticksPerSecond;
						
						vpx_codec_err_t encode_err = vpx_codec_encode(&encoder, img, timeStamp, nextTimeStamp - timeStamp, 0, 0);
						
						if(encode_err == VPX_CODEC_OK)
						{
							const vpx_codec_cx_pkt_t *pkt = NULL;
							vpx_codec_iter_t iter = NULL;
							 
							while( (pkt = vpx_codec_get_cx_data(&encoder, &iter)) )
							{
								if(pkt->kind == VPX_CODEC_CX_FRAME_PKT)
								{
									uint64 timestamp_ns = timeStamp * 1000000000UL / config.g_timebase.den;
									
									bool added = muxer_segment.AddFrame((const uint8 *)pkt->data.frame.buf, pkt->data.frame.sz,
																		vid_track, timestamp_ns,
																		(pkt->data.frame.flags & VPX_FRAME_IS_KEY));
																		
									if(!added)
										result = exportReturn_InternalError;
								}
							}
						}
						else
							result = exportReturn_InternalError;
						
						vpx_img_free(img);
					}
					else
						result = exportReturn_ErrMemory;
					
					pixSuite->Dispose(renderResult.outFrame);
				}
				
				/*
				if(result == malNoError && (frames_without_audio >= 10 || ((videoTime + frameRateP.value.timeValue) >= exportInfoP->endTime)))
				{
					int samples = sampleRateP.value.floatValue * fps.denominator / fps.numerator;
					
					while(frames_without_audio-- && result == malNoError)
					{
						vpx_codec_pts_t timeStamp = (audioBegin - exportInfoP->startTime) * config.g_timebase.den / ticksPerSecond;
						uint64 timestamp_ns = timeStamp * 1000000000UL / config.g_timebase.den;
						
						float *audioBuffer[6];
						
						for(int c=0; c < channelTypeP.value.intValue; c++)
						{
							audioBuffer[c] = (float *)malloc(samples * sizeof(float));
						}
						
						result = audioSuite->GetAudio(audioRenderID, 1, audioBuffer, false);
						
						if(result == malNoError)
						{
							float **buffer = vorbis_analysis_buffer(&vd, samples);
							
							for(int c=0; c < channelTypeP.value.intValue; c++)
							{
								memcpy(buffer[c], audioBuffer[c], samples * sizeof(float));
							}
							
							size_t compressed_buf_size = 0;
							unsigned char *compressed_buf = (unsigned char *)malloc(1);
							size_t compressed_pos = 0;
							
							vorbis_analysis_wrote(&vd, samples);
							
							while(vorbis_analysis_blockout(&vd, &vb) == 1)
							{
								vorbis_analysis(&vb, NULL);
								vorbis_bitrate_addblock(&vb);

								ogg_packet op;
								
								while(vorbis_bitrate_flushpacket(&vd, &op) )
								{
									compressed_buf_size += op.bytes;
									compressed_buf = (unsigned char *)realloc(compressed_buf, compressed_buf_size);
									
									memcpy(&compressed_buf[compressed_pos], op.packet, op.bytes);
									compressed_pos += op.bytes;
								}
							}
							
							bool added = muxer_segment.AddFrame(compressed_buf, compressed_buf_size,
																audio_track, timestamp_ns, 0);
																
							if(!added)
								result = exportReturn_InternalError;
								
							free(compressed_buf);
						}
						
						for(int c=0; c < channelTypeP.value.intValue; c++)
						{
							free(audioBuffer[c]);
						}
						
						audioBegin += frameRateP.value.timeValue;
					}
				}
				else
					frames_without_audio++;
				*/
				
				if(result == malNoError)
				{
					float progress = (double)(videoTime - exportInfoP->startTime) / (double)(exportInfoP->endTime - exportInfoP->startTime);
					
					result = mySettings->exportProgressSuite->UpdateProgressPercent(exID, progress);
					
					if(result == suiteError_ExporterSuspended)
					{
						result = mySettings->exportProgressSuite->WaitForResume(exID);
					}
				}
				
				
				videoTime += frameRateP.value.timeValue;
			}
			
			vpx_codec_err_t destroy_err = vpx_codec_destroy(&encoder);
			assert(destroy_err == VPX_CODEC_OK);
			
			bool final = muxer_segment.Finalize();
			
			if(!final)
				result = exportReturn_InternalError;
		}
		else
			result = exportReturn_InternalError;
		
/*
		vorbis_block_clear(&vb);
		vorbis_dsp_clear(&vd);
		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);*/
	}
	
	}catch(...) { result = exportReturn_InternalError; }
	
	
	renderSuite->ReleaseVideoRenderer(exID, videoRenderID);

	//audioSuite->ReleaseAudioRenderer(exID, audioRenderID);

	return result;
}


static prMALError
exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec)
{
	prMALError				result				= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(generateDefaultParamRec->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = generateDefaultParamRec->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	
	// get current settings
	PrParam widthP, heightP, parN, parD, fieldTypeP, frameRateP, channelsTypeP, sampleRateP;
	
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoWidth, &widthP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoHeight, &heightP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectNumerator, &parN);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectDenominator, &parD);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFieldType, &fieldTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFrameRate, &frameRateP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioChannelsType, &channelsTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_AudioSampleRate, &sampleRateP);
	
	if(widthP.mInt32 == 0)
	{
		widthP.mInt32 = 1920;
	}
	
	if(heightP.mInt32 == 0)
	{
		heightP.mInt32 = 1080;
	}



	prUTF16Char groupString[256];
	
	// Video Tab
	exportParamSuite->AddMultiGroup(exID, &gIdx);
	
	utf16ncpy(groupString, "Video Tab", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBETopParamGroup, ADBEVideoTabGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	

	// Image Settings group
	utf16ncpy(groupString, "Image Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEBasicVideoGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;
	widthValues.value.intValue = widthP.mInt32;
	widthValues.disabled = kPrFalse;
	widthValues.hidden = kPrFalse;
	
	exNewParamInfo widthParam;
	widthParam.structVersion = 1;
	strncpy(widthParam.identifier, ADBEVideoWidth, 255);
	widthParam.paramType = exParamType_int;
	widthParam.flags = exParamFlag_none;
	widthParam.paramValues = widthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &widthParam);


	// height
	exParamValues heightValues;
	heightValues.structVersion = 1;
	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	heightValues.value.intValue = heightP.mInt32;
	heightValues.disabled = kPrFalse;
	heightValues.hidden = kPrFalse;
	
	exNewParamInfo heightParam;
	heightParam.structVersion = 1;
	strncpy(heightParam.identifier, ADBEVideoHeight, 255);
	heightParam.paramType = exParamType_int;
	heightParam.flags = exParamFlag_none;
	heightParam.paramValues = heightValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &heightParam);


	// pixel aspect ratio
	exParamValues parValues;
	parValues.structVersion = 1;
	parValues.rangeMin.ratioValue.numerator = 10;
	parValues.rangeMin.ratioValue.denominator = 11;
	parValues.rangeMax.ratioValue.numerator = 2;
	parValues.rangeMax.ratioValue.denominator = 1;
	parValues.value.ratioValue.numerator = parN.mInt32;
	parValues.value.ratioValue.denominator = parD.mInt32;
	parValues.disabled = kPrFalse;
	parValues.hidden = kPrFalse;
	
	exNewParamInfo parParam;
	parParam.structVersion = 1;
	strncpy(parParam.identifier, ADBEVideoAspect, 255);
	parParam.paramType = exParamType_ratio;
	parParam.flags = exParamFlag_none;
	parParam.paramValues = parValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &parParam);
	
	
	// field order
	exParamValues fieldOrderValues;
	fieldOrderValues.structVersion = 1;
	fieldOrderValues.value.intValue = fieldTypeP.mInt32;
	fieldOrderValues.disabled = kPrFalse;
	fieldOrderValues.hidden = kPrFalse;
	
	exNewParamInfo fieldOrderParam;
	fieldOrderParam.structVersion = 1;
	strncpy(fieldOrderParam.identifier, ADBEVideoFieldType, 255);
	fieldOrderParam.paramType = exParamType_int;
	fieldOrderParam.flags = exParamFlag_none;
	fieldOrderParam.paramValues = fieldOrderValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fieldOrderParam);


	// frame rate
	exParamValues fpsValues;
	fpsValues.structVersion = 1;
	fpsValues.rangeMin.timeValue = 1;
	timeSuite->GetTicksPerSecond(&fpsValues.rangeMax.timeValue);
	fpsValues.value.timeValue = frameRateP.mInt64;
	fpsValues.disabled = kPrFalse;
	fpsValues.hidden = kPrFalse;
	
	exNewParamInfo fpsParam;
	fpsParam.structVersion = 1;
	strncpy(fpsParam.identifier, ADBEVideoFPS, 255);
	fpsParam.paramType = exParamType_ticksFrameRate;
	fpsParam.flags = exParamFlag_none;
	fpsParam.paramValues = fpsValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &fpsParam);


	// Alpha channel
	exParamValues alphaValues;
	alphaValues.structVersion = 1;
	alphaValues.value.intValue = kPrFalse;
	alphaValues.disabled = kPrFalse;
	alphaValues.hidden = kPrFalse;
	
	exNewParamInfo alphaParam;
	alphaParam.structVersion = 1;
	strncpy(alphaParam.identifier, ADBEVideoAlpha, 255);
	alphaParam.paramType = exParamType_bool;
	alphaParam.flags = exParamFlag_none;
	alphaParam.paramValues = alphaValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &alphaParam);


	// Audio Tab
	utf16ncpy(groupString, "Audio Tab", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBETopParamGroup, ADBEAudioTabGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);


	// Audio Settings group
	utf16ncpy(groupString, "Audio Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEAudioTabGroup, ADBEBasicAudioGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// Sample rate
	exParamValues sampleRateValues;
	sampleRateValues.value.floatValue = sampleRateP.mFloat64;
	sampleRateValues.disabled = kPrFalse;
	sampleRateValues.hidden = kPrFalse;
	
	exNewParamInfo sampleRateParam;
	sampleRateParam.structVersion = 1;
	strncpy(sampleRateParam.identifier, ADBEAudioRatePerSecond, 255);
	sampleRateParam.paramType = exParamType_float;
	sampleRateParam.flags = exParamFlag_none;
	sampleRateParam.paramValues = sampleRateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicAudioGroup, &sampleRateParam);
	
	
	// Channel type
	exParamValues channelTypeValues;
	channelTypeValues.value.intValue = channelsTypeP.mInt32;
	channelTypeValues.disabled = kPrFalse;
	channelTypeValues.hidden = kPrFalse;
	
	exNewParamInfo channelTypeParam;
	channelTypeParam.structVersion = 1;
	strncpy(channelTypeParam.identifier, ADBEAudioNumChannels, 255);
	channelTypeParam.paramType = exParamType_int;
	channelTypeParam.flags = exParamFlag_none;
	channelTypeParam.paramValues = channelTypeValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicAudioGroup, &channelTypeParam);
	

	exportParamSuite->SetParamsVersion(exID, 1);
	
	
	return result;
}


static prMALError
exSDKPostProcessParams(
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP)
{
	prMALError		result	= malNoError;

	ExportSettings			*lRec				= reinterpret_cast<ExportSettings *>(postProcessParamsRecP->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	//PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;

	csSDK_int32 exID = postProcessParamsRecP->exporterPluginID;
	csSDK_int32 gIdx = 0;
	
	prUTF16Char paramString[256];
	
	
	// Image Settings group
	utf16ncpy(paramString, "Image Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEBasicVideoGroup, paramString);
	
									
	// width
	utf16ncpy(paramString, "Width", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoWidth, paramString);
	
	exParamValues widthValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthValues);

	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;

	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoWidth, &widthValues);
	
	
	// height
	utf16ncpy(paramString, "Height", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoHeight, paramString);
	
	exParamValues heightValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightValues);

	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 8192;
	
	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoHeight, &heightValues);
	
	
	// pixel aspect ratio
	utf16ncpy(paramString, "Pixel Aspect Ratio", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAspect, paramString);
	
	csSDK_int32	PARs[][2] = {{1, 1}, {10, 11}, {40, 33}, {768, 702}, 
							{1024, 702}, {2, 1}, {4, 3}, {3, 2}};
							
	const char *PARStrings[] = {"Square pixels (1.0)",
								"D1/DV NTSC (0.9091)",
								"D1/DV NTSC Widescreen 16:9 (1.2121)",
								"D1/DV PAL (1.0940)", 
								"D1/DV PAL Widescreen 16:9 (1.4587)",
								"Anamorphic 2:1 (2.0)",
								"HD Anamorphic 1080 (1.3333)",
								"DVCPRO HD (1.5)"};


	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoAspect);
	
	exOneParamValueRec tempPAR;
	
	for(csSDK_int32 i=0; i < sizeof (PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		utf16ncpy(paramString, PARStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoAspect, &tempPAR, paramString);
	}
	
	
	// field type
	utf16ncpy(paramString, "Field Type", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFieldType, paramString);
	
	csSDK_int32	fieldOrders[] = {	prFieldsUpperFirst,
									prFieldsLowerFirst,
									prFieldsNone};
	
	const char *fieldOrderStrings[]	= {	"Upper First",
										"Lower First",
										"None"};

	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFieldType);
	
	exOneParamValueRec tempFieldOrder;
	for(int i=0; i < 3; i++)
	{
		tempFieldOrder.intValue = fieldOrders[i];
		utf16ncpy(paramString, fieldOrderStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFieldType, &tempFieldOrder, paramString);
	}
	
	
	// frame rate
	utf16ncpy(paramString, "Frame Rate", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoFPS, paramString);
	
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 50, 59,
							60};
													
	PrTime frameRateNumDens[][2] = {{10, 1}, {15, 1}, {24000, 1001},
									{24, 1}, {25, 1}, {30000, 1001},
									{30, 1}, {50, 1}, {60000, 1001},
									{60, 1}};
	
	const char *frameRateStrings[] = {	"10",
										"15",
										"23.976",
										"24",
										"25 (PAL)",
										"29.97 (NTSC)",
										"30",
										"50",
										"59.94",
										"60"};
	
	PrTime ticksPerSecond = 0;
	timeSuite->GetTicksPerSecond (&ticksPerSecond);
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
	}
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFPS);
	
	exOneParamValueRec tempFrameRate;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		tempFrameRate.timeValue = frameRates[i];
		utf16ncpy(paramString, frameRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFPS, &tempFrameRate, paramString);
	}
	
	
	// Alpha channel
	utf16ncpy(paramString, "Include Alpha Channel", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAlpha, paramString);
	
	
	
	// Audio Settings group
	utf16ncpy(paramString, "Audio Settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEBasicAudioGroup, paramString);
	
	
	// Sample rate
	utf16ncpy(paramString, "Sample Rate", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioRatePerSecond, paramString);
	
	float sampleRates[] = { 8000.0f, 16000.0f, 32000.0f, 44100.0f, 48000.0f, 96000.0f };
	
	const char *sampleRateStrings[] = { "8000 Hz", "16000 Hz", "32000 Hz", "44100 Hz", "48000 Hz", "96000 Hz" };
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEAudioRatePerSecond);
	
	exOneParamValueRec tempSampleRate;
	
	for(csSDK_int32 i=0; i < sizeof(sampleRates) / sizeof(float); i++)
	{
		tempSampleRate.floatValue = sampleRates[i];
		utf16ncpy(paramString, sampleRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEAudioRatePerSecond, &tempSampleRate, paramString);
	}

	
	// Channels
	utf16ncpy(paramString, "Channels", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioNumChannels, paramString);
	
	csSDK_int32 channelTypes[] = { kPrAudioChannelType_Mono, kPrAudioChannelType_Stereo };
	
	const char *channelTypeStrings[] = { "Mono", "Stereo" };
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEAudioNumChannels);
	
	exOneParamValueRec tempChannelType;
	
	for(csSDK_int32 i=0; i < sizeof(channelTypes) / sizeof(csSDK_int32); i++)
	{
		tempChannelType.intValue = channelTypes[i];
		utf16ncpy(paramString, channelTypeStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEAudioNumChannels, &tempChannelType, paramString);
	}
	
	
	
	return result;
}


static prMALError
exSDKGetParamSummary(
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(summaryRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	std::string summary1, summary2, summary3;

	csSDK_uint32				exID			= summaryRecP->exporterPluginID;
	csSDK_int32					mgroupIndex		= 0;
	
	// Standard settings
	exParamValues width, height, frameRate, sequence, alpha;
	
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	

	// oh boy, figure out frame rate
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 50, 59,
							60};
													
	PrTime frameRateNumDens[][2] = {{10, 1}, {15, 1}, {24000, 1001},
									{24, 1}, {25, 1}, {30000, 1001},
									{30, 1}, {50, 1}, {60000, 1001},
									{60, 1}};
	
	const char *frameRateStrings[] = {	"10",
										"15",
										"23.976",
										"24",
										"25",
										"29.97",
										"30",
										"50",
										"59.94",
										"60"};
	
	PrTime ticksPerSecond = 0;
	privateData->timeSuite->GetTicksPerSecond (&ticksPerSecond);
	
	csSDK_int32 frame_rate_index = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(frameRates[i] == frameRate.value.timeValue)
			frame_rate_index = i;
	}


	std::stringstream stream1;
	
	stream1 << width.value.intValue << "x" << height.value.intValue;
	
	if(frame_rate_index >= 0 && sequence.value.intValue)
	{
		stream1 << ", " << frameRateStrings[frame_rate_index] << " fps";
	}
	else if(!sequence.value.intValue)
	{
		stream1 << ", Single Frame";
	}
	
	stream1 << ", " << (alpha.value.intValue ? "Alpha" : "No Alpha");
	
	
	summary1 = stream1.str();
	
	
	

	utf16ncpy(summaryRecP->Summary1, summary1.c_str(), 255);
	utf16ncpy(summaryRecP->Summary2, summary2.c_str(), 255);
	utf16ncpy(summaryRecP->Summary3, summary3.c_str(), 255);
	
	return malNoError;
}


static prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(validateParamChangedRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	csSDK_int32 exID = validateParamChangedRecP->exporterPluginID;
	csSDK_int32 gIdx = validateParamChangedRecP->multiGroupIndex;
	
	std::string param = validateParamChangedRecP->changedParamIdentifier;
	

	return malNoError;
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
