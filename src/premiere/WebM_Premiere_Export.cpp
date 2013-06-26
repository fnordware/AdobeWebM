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

using mkvmuxer::uint8;
using mkvmuxer::int32;
using mkvmuxer::uint32;
using mkvmuxer::int64;
using mkvmuxer::uint64;

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
	// ummm, should I do something?
}

#pragma mark-


static const csSDK_int32 WebM_ID = 'WebM';
static const csSDK_int32 WebM_Export_Class = 'WebM';

extern int g_num_cpus;


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
								//alpha,
								bitrate,
								sampleRate,
								channelType;
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
		
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoBitrate, &bitrate);
		videoBitrate += bitrate.value.intValue;
		
		//paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	}
	
	if(outputSettingsP->inExportAudio)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		outputSettingsP->outAudioChannelType = (PrAudioChannelType)channelType.value.intValue;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_Compressed;
		
		videoBitrate += 33;
	}
	
	// return outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = videoBitrate;


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
		
		// TODO: Understand what's going on here
		// seems like I have to set the denominator to 1000000
		// But why?
		if(fps->denominator == 1)
		{
			fps->numerator *= 1000000;
			fps->denominator *= 1000000;
		}
		else if(fps->denominator == 1001)
		{
			fps->numerator *= 1000;
			fps->denominator *= 1000;
		}
	}
	else
	{
		fps->numerator = 1000000 * ticksPerSecond / ticks_per_frame;
		fps->denominator = 1000000;
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
	
	exParamValues widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP, alphaP;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
	//paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	alphaP.value.intValue = 0;
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);
	
	exParamValues codecP, bitrateP, vidQualityP, qualityP;
	paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &bitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &vidQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &qualityP);
	
	
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
												(PrAudioChannelType)channelTypeP.value.intValue,
												kPrAudioSampleType_32BitFloat,
												sampleRateP.value.floatValue, 
												&audioRenderID);
	}

	
	try{
	
	if(result == malNoError)
	{
		exRatioValue fps;
		get_framerate(ticksPerSecond, frameRateP.value.timeValue, &fps);
		
		
		vpx_codec_err_t codec_err = VPX_CODEC_OK;
		
		vpx_codec_ctx_t encoder;
		
		if(exportInfoP->exportVideo)
		{
			vpx_codec_iface_t *iface = codecP.value.intValue == WEBM_CODEC_VP9 ? vpx_codec_vp9_cx() :
										vpx_codec_vp8_cx();
			
			vpx_codec_enc_cfg_t config;
			vpx_codec_enc_config_default(iface, &config, 0);
			
			config.g_w = renderParms.inWidth;
			config.g_h = renderParms.inHeight;
			
			config.g_pass = VPX_RC_ONE_PASS;
			config.rc_target_bitrate = bitrateP.value.intValue;
			
			config.g_threads = g_num_cpus;
			
			config.g_timebase.num = fps.denominator;
			config.g_timebase.den = fps.numerator;
		
		
			codec_err = vpx_codec_enc_init(&encoder, iface, &config, 0);
		}
		
	
	#define OV_OK 0
	
		int v_err = OV_OK;
	
		vorbis_info vi;
		vorbis_comment vc;
		vorbis_dsp_state vd;
		vorbis_block vb;
		
		size_t private_size = 0;
		void *private_data = NULL;
		
		if(exportInfoP->exportAudio)
		{
			vorbis_info_init(&vi);
			
			v_err = vorbis_encode_init_vbr(&vi,
											channelTypeP.value.intValue,
											sampleRateP.value.floatValue,
											qualityP.value.floatValue);
			
			if(v_err == OV_OK)
			{
				v_err = vorbis_encode_setup_init(&vi);
				
				assert(v_err == OV_OK);
				
				vorbis_comment_init(&vc);
				vorbis_analysis_init(&vd, &vi);
				vorbis_block_init(&vd, &vb);
				
				
				ogg_packet header;
				ogg_packet header_comm;
				ogg_packet header_code;
				
				vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
				
				private_data = MakePrivateData(header, header_comm, header_code, private_size);
			}
		}
		
		
		if(codec_err == VPX_CODEC_OK && v_err == OV_OK)
		{
			PrMkvWriter writer(mySettings->exportFileSuite, exportInfoP->fileObject);

			mkvmuxer::Segment muxer_segment;
			
			muxer_segment.Init(&writer);
			muxer_segment.set_mode(mkvmuxer::Segment::kFile);
			
			
			mkvmuxer::SegmentInfo* const info = muxer_segment.GetSegmentInfo();
			
			info->set_timecode_scale(fps.denominator);
			info->set_writing_app("fnord WebM for Premiere");
			
			
			uint64 vid_track = 0;
			
			if(exportInfoP->exportVideo)
			{
				vid_track = muxer_segment.AddVideoTrack(renderParms.inWidth, renderParms.inHeight, 1);
				
				mkvmuxer::VideoTrack* const video = static_cast<mkvmuxer::VideoTrack *>(muxer_segment.GetTrackByNumber(vid_track));
				
				video->set_frame_rate((double)fps.numerator / (double)fps.denominator);
				video->set_codec_id(codecP.value.intValue == WEBM_CODEC_VP9 ? "V_VP9" :
										mkvmuxer::Tracks::kVp8CodecId);
				
				muxer_segment.CuesTrack(vid_track);
			}
			
			
			uint64 audio_track = 0;
			
			if(exportInfoP->exportAudio)
			{
				audio_track = muxer_segment.AddAudioTrack(sampleRateP.value.floatValue, channelTypeP.value.intValue, 2);
				
				mkvmuxer::AudioTrack* const audio = static_cast<mkvmuxer::AudioTrack *>(muxer_segment.GetTrackByNumber(audio_track));
				
				audio->set_codec_id(mkvmuxer::Tracks::kVorbisCodecId);
				
				if(private_data)
				{
					bool copied = audio->SetCodecPrivate((const uint8 *)private_data, private_size);
					
					assert(copied);
					
					free(private_data);
				}
			}
			
		
			PrTime videoTime = exportInfoP->startTime;
			
			while(videoTime < exportInfoP->endTime && result == suiteError_NoError)
			{
				vpx_codec_pts_t timeStamp = (videoTime - exportInfoP->startTime) * fps.denominator / ticksPerSecond;
				vpx_codec_pts_t nextTimeStamp = ((videoTime + frameRateP.value.timeValue) - exportInfoP->startTime) * fps.denominator / ticksPerSecond;
				unsigned long duration = nextTimeStamp - timeStamp;
				
				uint64 timestamp_ns = timeStamp * 1000000000UL / fps.denominator;
				
				
				if(exportInfoP->exportVideo)
				{
					SequenceRender_GetFrameReturnRec renderResult;
					
					result = renderSuite->RenderVideoFrame(videoRenderID,
															videoTime,
															&renderParms,
															kRenderCacheType_None,
															&renderResult);
					assert(result == suiteError_NoError);
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
									// using the conversion found here: http://www.fourcc.org/fccyvrgb.php
									
									unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
									unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / 2));
									unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / 2));
									
									// the rows in this kind of Premiere buffer are flipped, FYI (or is it flopped?)
									unsigned char *prBGRA = (unsigned char *)frameBufferP + (rowbytes * (img->d_h - 1 - y));
									
									unsigned char *prB = prBGRA + 0;
									unsigned char *prG = prBGRA + 1;
									unsigned char *prR = prBGRA + 2;
									unsigned char *prA = prBGRA + 3;
									
									for(int x=0; x < img->d_w; x++)
									{
										// like the clever integer (fixed point) math?
										*imgY++ = ((257 * (int)*prR) + (504 * (int)*prG) + ( 98 * (int)*prB) + 16500) / 1000;
										
										if( (y % 2 == 0) && (x % 2 == 0) )
										{
											*imgV++ = ((439 * (int)*prR) - (368 * (int)*prG) - ( 71 * (int)*prB) + 128500) / 1000;
											*imgU++ = (-(148 * (int)*prR) - (291 * (int)*prG) + (439 * (int)*prB) + 128500) / 1000;
										}
										
										prR += 4;
										prG += 4;
										prB += 4;
										prA += 4;
									}
								}
							}
							
							unsigned long deadline = vidQualityP.value.intValue == WEBM_QUALITY_REALTIME ? VPX_DL_REALTIME :
														vidQualityP.value.intValue == WEBM_QUALITY_BEST ? VPX_DL_BEST_QUALITY :
														VPX_DL_GOOD_QUALITY;
							
							vpx_codec_err_t encode_err = vpx_codec_encode(&encoder, img, timeStamp, duration, 0, deadline);
							
							if(encode_err == VPX_CODEC_OK)
							{
								const vpx_codec_cx_pkt_t *pkt = NULL;
								vpx_codec_iter_t iter = NULL;
								 
								while( (pkt = vpx_codec_get_cx_data(&encoder, &iter)) )
								{
									if(pkt->kind == VPX_CODEC_CX_FRAME_PKT)
									{
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
				}
				
				
				if(exportInfoP->exportAudio)
				{
					const int samples = sampleRateP.value.floatValue * fps.denominator / fps.numerator;
					
					csSDK_int32 maxBlip = 0;
					mySettings->sequenceAudioSuite->GetMaxBlip(audioRenderID, frameRateP.value.timeValue, &maxBlip);
					
					assert(maxBlip >= samples);
					
					
					float **buffer = vorbis_analysis_buffer(&vd, samples);
					
					result = audioSuite->GetAudio(audioRenderID, samples, buffer, false);
					
					if(result == malNoError)
					{
						vorbis_analysis_wrote(&vd, samples);
				
						while(vorbis_analysis_blockout(&vd, &vb) == 1)
						{
							vorbis_analysis(&vb, NULL);
							vorbis_bitrate_addblock(&vb);

							ogg_packet op;
							
							while( vorbis_bitrate_flushpacket(&vd, &op) )
							{
								bool added = muxer_segment.AddFrame(op.packet, op.bytes,
																	audio_track, timestamp_ns, 0);
																		
								if(!added)
									result = exportReturn_InternalError;
							}
						}
					}
						
					// save the rest of the audio if this is the last frame
					if(result == malNoError &&
						(videoTime >= (exportInfoP->endTime - frameRateP.value.timeValue)))
					{
						vorbis_analysis_wrote(&vd, NULL); // means there will be no more data
				
						while(vorbis_analysis_blockout(&vd, &vb) == 1)
						{
							vorbis_analysis(&vb, NULL);
							vorbis_bitrate_addblock(&vb);

							ogg_packet op;
							
							while( vorbis_bitrate_flushpacket(&vd, &op) )
							{
								bool added = muxer_segment.AddFrame(op.packet, op.bytes,
																	audio_track, timestamp_ns, 0);
																		
								if(!added)
									result = exportReturn_InternalError;
							}
						}
					}
				}
				
				
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
			
			
			bool final = muxer_segment.Finalize();
			
			if(!final)
				result = exportReturn_InternalError;
		}
		else
			result = exportReturn_InternalError;
		


		if(exportInfoP->exportVideo)
		{
			vpx_codec_err_t destroy_err = vpx_codec_destroy(&encoder);
			assert(destroy_err == VPX_CODEC_OK);
		}
			
		if(exportInfoP->exportAudio)
		{
			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);
			vorbis_comment_clear(&vc);
			vorbis_info_clear(&vi);
		}
	}
	
	}catch(...) { result = exportReturn_InternalError; }
	
	
	if(exportInfoP->exportVideo)
		renderSuite->ReleaseVideoRenderer(exID, videoRenderID);

	if(exportInfoP->exportAudio)
		audioSuite->ReleaseAudioRenderer(exID, audioRenderID);

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
	
	//exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &alphaParam);

	
	// Video Codec Settings Group
	utf16ncpy(groupString, "Codec settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEVideoCodecGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
									
	// Codec
	exParamValues codecValues;
	codecValues.structVersion = 1;
	codecValues.rangeMin.intValue = WEBM_CODEC_VP8;
	codecValues.rangeMax.intValue = WEBM_CODEC_VP9;
	codecValues.value.intValue = WEBM_CODEC_VP8;
	codecValues.disabled = kPrFalse;
	codecValues.hidden = kPrFalse;
	
	exNewParamInfo codecParam;
	codecParam.structVersion = 1;
	strncpy(codecParam.identifier, WebMVideoCodec, 255);
	codecParam.paramType = exParamType_int;
	codecParam.flags = exParamFlag_none;
	codecParam.paramValues = codecValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &codecParam);
	
	
	// Bitrate
	exParamValues bitrateValues;
	bitrateValues.structVersion = 1;
	bitrateValues.rangeMin.intValue = 1;
	bitrateValues.rangeMax.intValue = 9999;
	bitrateValues.value.intValue = 500;
	bitrateValues.disabled = kPrFalse;
	bitrateValues.hidden = kPrFalse;
	
	exNewParamInfo bitrateParam;
	bitrateParam.structVersion = 1;
	strncpy(bitrateParam.identifier, WebMVideoBitrate, 255);
	bitrateParam.paramType = exParamType_int;
	bitrateParam.flags = exParamFlag_slider;
	bitrateParam.paramValues = bitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &bitrateParam);
	
	
	// Quality
	exParamValues vidQualityValues;
	vidQualityValues.structVersion = 1;
	vidQualityValues.rangeMin.intValue = WEBM_QUALITY_REALTIME;
	vidQualityValues.rangeMax.intValue = WEBM_QUALITY_BEST;
	vidQualityValues.value.intValue = WEBM_QUALITY_GOOD;
	vidQualityValues.disabled = kPrFalse;
	vidQualityValues.hidden = kPrFalse;
	
	exNewParamInfo vidQualityParam;
	vidQualityParam.structVersion = 1;
	strncpy(vidQualityParam.identifier, WebMVideoQuality, 255);
	vidQualityParam.paramType = exParamType_int;
	vidQualityParam.flags = exParamFlag_none;
	vidQualityParam.paramValues = vidQualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &vidQualityParam);
	
	
	
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
	
	
	
	// Audio Codec Settings Group
	utf16ncpy(groupString, "Codec settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEAudioTabGroup, ADBEAudioCodecGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
									
	// Quality
	exParamValues qualityValues;
	qualityValues.structVersion = 1;
	qualityValues.rangeMin.floatValue = -0.1f;
	qualityValues.rangeMax.floatValue = 1.f;
	qualityValues.value.floatValue = 0.5f;
	qualityValues.disabled = kPrFalse;
	qualityValues.hidden = kPrFalse;
	
	exNewParamInfo qualityParam;
	qualityParam.structVersion = 1;
	strncpy(qualityParam.identifier, WebMAudioQuality, 255);
	qualityParam.paramType = exParamType_float;
	qualityParam.flags = exParamFlag_slider;
	qualityParam.paramValues = qualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &qualityParam);
	
	

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
	//exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAlpha, paramString);
	
	
	// Video codec settings
	utf16ncpy(paramString, "Codec settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoCodecGroup, paramString);
	
	
	// Codec
	utf16ncpy(paramString, "Codec", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoCodec, paramString);
	
	
	WebM_Video_Codec codecs[] = {	WEBM_CODEC_VP8,
									WEBM_CODEC_VP9 };
	
	const char *codecStrings[]	= {	"VP8",
									"VP9" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoCodec);
	
	exOneParamValueRec tempCodec;
	for(int i=0; i < 2; i++)
	{
		tempCodec.intValue = codecs[i];
		utf16ncpy(paramString, codecStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoCodec, &tempCodec, paramString);
	}
	
	
	// Bitrate
	utf16ncpy(paramString, "Bitrate (kb/s)", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoBitrate, paramString);
	
	exParamValues bitrateValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &bitrateValues);

	bitrateValues.rangeMin.intValue = 1;
	bitrateValues.rangeMax.intValue = 9999;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoBitrate, &bitrateValues);
	
	
	// Quality
	utf16ncpy(paramString, "Quality", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoQuality, paramString);
	
	
	int vidQualities[] = {	WEBM_QUALITY_REALTIME,
							WEBM_QUALITY_GOOD,
							WEBM_QUALITY_BEST };
	
	const char *vidQualityStrings[]	= {	"Realtime",
										"Normal",
										"Best" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoQuality);
	
	exOneParamValueRec tempVidQuality;
	for(int i=0; i < 3; i++)
	{
		tempVidQuality.intValue = vidQualities[i];
		utf16ncpy(paramString, vidQualityStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoQuality, &tempVidQuality, paramString);
	}

	
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
	
	
	// Audio codec settings
	utf16ncpy(paramString, "Vorbis settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioCodecGroup, paramString);


	// Quality
	utf16ncpy(paramString, "Quality", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMAudioQuality, paramString);
	
	exParamValues qualityValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &qualityValues);

	qualityValues.rangeMin.floatValue = -0.1f;
	qualityValues.rangeMax.floatValue = 1.f;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMAudioQuality, &qualityValues);
	
	
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

	csSDK_uint32	exID	= summaryRecP->exporterPluginID;
	csSDK_int32		gIdx	= 0;
	
	// Standard settings
	exParamValues width, height, frameRate, alpha;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRate);
	//paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);

	exParamValues codecP, bitrateP, vidQualityP, qualityP;
	paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &bitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &vidQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &qualityP);
	

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
	privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	csSDK_int32 frame_rate_index = -1;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		if(frameRates[i] == frameRate.value.timeValue)
			frame_rate_index = i;
	}


	std::stringstream stream1;
	
	stream1 << width.value.intValue << "x" << height.value.intValue;
	
	if(frame_rate_index >= 0 && frame_rate_index < 10) 
		stream1 << ", " << frameRateStrings[frame_rate_index] << " fps";
	
	//stream1 << ", " << (alpha.value.intValue ? "Alpha" : "No Alpha");
	
	summary1 = stream1.str();
	
	
	std::stringstream stream2;
	
	stream2 << (int)sampleRateP.value.floatValue << " Hz";
	stream2 << ", " << (channelTypeP.value.intValue == 2 ? "Stereo" : "Mono");
	
	summary2 = stream2.str();
	
	
	std::stringstream stream3;
	
	stream3 << (codecP.value.intValue == WEBM_CODEC_VP9 ? "VP9" : "VP8");
	stream3 << ", " << bitrateP.value.intValue << " kb/s";
	stream3 << ", ";
	
	if(vidQualityP.value.intValue == WEBM_QUALITY_REALTIME)
		stream3 << "Realtime";
	else if(vidQualityP.value.intValue == WEBM_QUALITY_BEST)
		stream3 << "Best Quality";
	else
		stream3 << "Normal Quality";
	
	summary3 = stream3.str();
	
	

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
	
	// This is where you flip controls on/off, etc...
	

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
