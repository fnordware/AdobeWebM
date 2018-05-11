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


#include "WebM_Premiere_Export_Params.h"


#include "vpx/vp8cx.h"

#include <assert.h>
#include <math.h>

#include <sstream>
#include <vector>

using std::string;

static void
utf16ncpy(prUTF16Char *dest, const char *src, int max_len)
{
	prUTF16Char *d = dest;
	const char *c = src;
	
	do{
		*d++ = *c;
	}while(*c++ != '\0' && --max_len);
}


prMALError
exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP)
{
	prMALError result = malNoError;
	
	ExportSettings *privateData	= reinterpret_cast<ExportSettings *>(outputSettingsP->privateData);
	
	PrSDKExportParamSuite *paramSuite = privateData->exportParamSuite;
	
	const csSDK_uint32 exID = outputSettingsP->exporterPluginID;
	const csSDK_int32 mgroupIndex = 0;
	
	
	csSDK_uint32 videoBitrate = 0;
	
	
	if(outputSettingsP->inExportVideo)
	{
		exParamValues width, height, frameRate, pixelAspectRatio, fieldType;
	
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		
		outputSettingsP->outVideoWidth = width.value.intValue;
		outputSettingsP->outVideoHeight = height.value.intValue;
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
		
		
		exParamValues methodP, videoQualityP, videoBitrateP;
								
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoMethod, &methodP);
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoQuality, &videoQualityP);
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoBitrate, &videoBitrateP);
		
		
		if(methodP.value.intValue == WEBM_METHOD_CONSTANT_QUALITY || methodP.value.intValue == WEBM_METHOD_CONSTRAINED_QUALITY)
		{
			PrTime ticksPerSecond = 0;
			privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
			
			const float fps = (double)ticksPerSecond / (double)frameRate.value.timeValue;
			
			const int bitsPerFrameUncompressed = (width.value.intValue * height.value.intValue) * 3 * 8;
			const float qual = (float)videoQualityP.value.intValue / 100.0;
			const float quality_gamma = 2.0;  // these numbers arrived at from some experimentation
			const float qualityMaxMult = 0.02;
			const float qualityMinMult = 0.001;
			const float qualityMult = ( powf(qual, quality_gamma) * (qualityMaxMult - qualityMinMult)) + qualityMinMult;
			const int bitsPerFrame = bitsPerFrameUncompressed * qualityMult;
			
			csSDK_uint32 quality_bitrate = (bitsPerFrame * fps) / 1024;
			
			if(methodP.value.intValue == WEBM_METHOD_CONSTRAINED_QUALITY && quality_bitrate > videoBitrateP.value.intValue)
				quality_bitrate = videoBitrateP.value.intValue;
			
			videoBitrate += quality_bitrate;
		}
		else
			videoBitrate += videoBitrateP.value.intValue;
	}
	
	
	if(outputSettingsP->inExportAudio)
	{
		exParamValues sampleRate, channelType;
	
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		
		PrAudioChannelType audioFormat = (PrAudioChannelType)channelType.value.intValue;
		
		if(audioFormat < kPrAudioChannelType_Mono || audioFormat > kPrAudioChannelType_51)
			audioFormat = kPrAudioChannelType_Stereo;
			
		const int audioChannels = (audioFormat == kPrAudioChannelType_51 ? 6 :
									audioFormat == kPrAudioChannelType_Stereo ? 2 :
									audioFormat == kPrAudioChannelType_Mono ? 1 :
									2);
									
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		outputSettingsP->outAudioChannelType = audioFormat;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_Compressed;
		
									
		exParamValues audioCodecP;
		paramSuite->GetParamValue(exID, mgroupIndex, WebMAudioCodec, &audioCodecP);

		const int samples_per_sec = sampleRate.value.floatValue * audioChannels;

		if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
		{
			exParamValues autoBitrateP, opusBitrateP;
			paramSuite->GetParamValue(exID, mgroupIndex, WebMOpusAutoBitrate, &autoBitrateP);
			paramSuite->GetParamValue(exID, mgroupIndex, WebMOpusBitrate, &opusBitrateP);
									
			assert(sampleRate.value.floatValue == 48000);
			
			if(autoBitrateP.value.intValue == kPrTrue)
			{
				videoBitrate += samples_per_sec * 0.001; // number I came up with through experimentation
			}
			else
				videoBitrate += opusBitrateP.value.intValue;
		}
		else
		{
			exParamValues audioMethodP, audioQualityP, audioBitrateP;
			paramSuite->GetParamValue(exID, mgroupIndex, WebMAudioMethod, &audioMethodP);
			paramSuite->GetParamValue(exID, mgroupIndex, WebMAudioQuality, &audioQualityP);
			paramSuite->GetParamValue(exID, mgroupIndex, WebMAudioBitrate, &audioBitrateP);
			
			if(audioMethodP.value.intValue == OGG_QUALITY)
			{
				const float qual = (audioQualityP.value.floatValue + 0.1) / 1.1;
				const float qualityMaxMult = 0.0025; // experimental numbers
				const float qualityMinMult = 0.00005;
				const float qualityMult = (qual * (qualityMaxMult - qualityMinMult)) + qualityMinMult;
			
				videoBitrate += samples_per_sec * qualityMult;
			}
			else
				videoBitrate += audioBitrateP.value.intValue;
		}
	}
	
	// outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = videoBitrate;


	return result;
}


prMALError
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
	utf16ncpy(groupString, "Video Settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, ADBEBasicVideoGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
	
	// width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 16384;
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
	heightValues.rangeMax.intValue = 16384;
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
	if(fieldTypeP.mInt32 == prFieldsUnknown)
		fieldTypeP.mInt32 = prFieldsNone;
	
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
	codecValues.value.intValue = WEBM_CODEC_VP9;
	codecValues.disabled = kPrFalse;
	codecValues.hidden = kPrFalse;
	
	exNewParamInfo codecParam;
	codecParam.structVersion = 1;
	strncpy(codecParam.identifier, WebMVideoCodec, 255);
	codecParam.paramType = exParamType_int;
	codecParam.flags = exParamFlag_none;
	codecParam.paramValues = codecValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &codecParam);
	
	
	// Method
	exParamValues methodValues;
	methodValues.structVersion = 1;
	methodValues.rangeMin.intValue = WEBM_METHOD_CONSTANT_QUALITY;
	methodValues.rangeMax.intValue = WEBM_METHOD_CONSTRAINED_QUALITY;
	methodValues.value.intValue = WEBM_METHOD_CONSTANT_QUALITY;
	methodValues.disabled = kPrFalse;
	methodValues.hidden = kPrFalse;
	
	exNewParamInfo methodParam;
	methodParam.structVersion = 1;
	strncpy(methodParam.identifier, WebMVideoMethod, 255);
	methodParam.paramType = exParamType_int;
	methodParam.flags = exParamFlag_none;
	methodParam.paramValues = methodValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &methodParam);
	
	
	// Quality
	exParamValues videoQualityValues;
	videoQualityValues.structVersion = 1;
	videoQualityValues.rangeMin.intValue = 0;
	videoQualityValues.rangeMax.intValue = 100;
	videoQualityValues.value.intValue = 50;
	videoQualityValues.disabled = kPrFalse;
	videoQualityValues.hidden = kPrFalse;
	
	exNewParamInfo videoQualityParam;
	videoQualityParam.structVersion = 1;
	strncpy(videoQualityParam.identifier, WebMVideoQuality, 255);
	videoQualityParam.paramType = exParamType_int;
	videoQualityParam.flags = exParamFlag_slider;
	videoQualityParam.paramValues = videoQualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &videoQualityParam);
	
	
	// Bitrate
	exParamValues videoBitrateValues;
	videoBitrateValues.structVersion = 1;
	videoBitrateValues.rangeMin.intValue = 1;
	videoBitrateValues.rangeMax.intValue = 10000;
	videoBitrateValues.value.intValue = 1000;
	videoBitrateValues.disabled = kPrFalse;
	videoBitrateValues.hidden = kPrTrue;
	
	exNewParamInfo videoBitrateParam;
	videoBitrateParam.structVersion = 1;
	strncpy(videoBitrateParam.identifier, WebMVideoBitrate, 255);
	videoBitrateParam.paramType = exParamType_int;
	videoBitrateParam.flags = exParamFlag_slider;
	videoBitrateParam.paramValues = videoBitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &videoBitrateParam);
	
	
	// 2-pass
	exParamValues twoPassValues;
	twoPassValues.structVersion = 1;
	twoPassValues.value.intValue = kPrTrue;
	twoPassValues.disabled = kPrFalse;
	twoPassValues.hidden = kPrFalse;
	
	exNewParamInfo twoPassParam;
	twoPassParam.structVersion = 1;
	strncpy(twoPassParam.identifier, WebMVideoTwoPass, 255);
	twoPassParam.paramType = exParamType_bool;
	twoPassParam.flags = exParamFlag_none;
	twoPassParam.paramValues = twoPassValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &twoPassParam);
	
	
	// Keyframe max distance
	exParamValues videoKeyframeMaxDisanceValues;
	videoKeyframeMaxDisanceValues.structVersion = 1;
	videoKeyframeMaxDisanceValues.rangeMin.intValue = 0;
	videoKeyframeMaxDisanceValues.rangeMax.intValue = 999;
	videoKeyframeMaxDisanceValues.value.intValue = 128;
	videoKeyframeMaxDisanceValues.disabled = kPrFalse;
	videoKeyframeMaxDisanceValues.hidden = kPrFalse;
	
	exNewParamInfo videoKeyframeMaxDistanceParam;
	videoKeyframeMaxDistanceParam.structVersion = 1;
	strncpy(videoKeyframeMaxDistanceParam.identifier, WebMVideoKeyframeMaxDistance, 255);
	videoKeyframeMaxDistanceParam.paramType = exParamType_int;
	videoKeyframeMaxDistanceParam.flags = exParamFlag_none;
	videoKeyframeMaxDistanceParam.paramValues = videoKeyframeMaxDisanceValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &videoKeyframeMaxDistanceParam);


	// Sampling
	exParamValues samplingValues;
	samplingValues.structVersion = 1;
	samplingValues.rangeMin.intValue = WEBM_420;
	samplingValues.rangeMax.intValue = WEBM_444;
	samplingValues.value.intValue = WEBM_420;
	samplingValues.disabled = kPrFalse;
	samplingValues.hidden = kPrFalse;
	
	exNewParamInfo samplingParam;
	samplingParam.structVersion = 1;
	strncpy(samplingParam.identifier, WebMVideoSampling, 255);
	samplingParam.paramType = exParamType_int;
	samplingParam.flags = exParamFlag_none;
	samplingParam.paramValues = samplingValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &samplingParam);
	
	
	// Bit Depth
	exParamValues bitDepthValues;
	bitDepthValues.structVersion = 1;
	bitDepthValues.rangeMin.intValue = 8;
	bitDepthValues.rangeMax.intValue = 12;
	bitDepthValues.value.intValue = 8;
	bitDepthValues.disabled = kPrFalse;
	bitDepthValues.hidden = kPrFalse;
	
	exNewParamInfo bitDepthParam;
	bitDepthParam.structVersion = 1;
	strncpy(bitDepthParam.identifier, WebMVideoBitDepth, 255);
	bitDepthParam.paramType = exParamType_int;
	bitDepthParam.flags = exParamFlag_none;
	bitDepthParam.paramValues = bitDepthValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &bitDepthParam);
	
	
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
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &alphaParam);


	// Version
	exParamValues versionValues;
	versionValues.structVersion = 1;
	versionValues.rangeMin.intValue = 0;
	versionValues.rangeMax.intValue = INT_MAX;
	versionValues.value.intValue = WEBM_PLUGIN_VERSION_MAJOR << 16 |
									WEBM_PLUGIN_VERSION_MINOR << 8 |
									WEBM_PLUGIN_VERSION_BUILD;
	versionValues.disabled = kPrFalse;
	versionValues.hidden = kPrTrue;
	
	exNewParamInfo versionParam;
	versionParam.structVersion = 1;
	strncpy(versionParam.identifier, WebMPluginVersion, 255);
	versionParam.paramType = exParamType_int;
	versionParam.flags = exParamFlag_none;
	versionParam.paramValues = versionValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &versionParam);
	
		
	// Custom Settings Group
	utf16ncpy(groupString, "Custom settings", 255);
	exportParamSuite->AddParamGroup(exID, gIdx,
									ADBEVideoTabGroup, WebMCustomGroup, groupString,
									kPrFalse, kPrFalse, kPrFalse);
									
	// Custom field
	exParamValues customArgValues;
	memset(customArgValues.paramString, 0, sizeof(customArgValues.paramString));
	customArgValues.disabled = kPrFalse;
	customArgValues.hidden = kPrFalse;
	
	exNewParamInfo customArgParam;
	customArgParam.structVersion = 1;
	strncpy(customArgParam.identifier, WebMCustomArgs, 255);
	customArgParam.paramType = exParamType_string;
	customArgParam.flags = exParamFlag_multiLine;
	customArgParam.paramValues = customArgValues;
	
	exportParamSuite->AddParam(exID, gIdx, WebMCustomGroup, &customArgParam);



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
	sampleRateValues.value.floatValue = 48000.f; //sampleRateP.mFloat64;
	sampleRateValues.disabled = kPrTrue;
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
									
	// Audio Codec
	exParamValues audioCodecValues;
	audioCodecValues.structVersion = 1;
	audioCodecValues.rangeMin.intValue = WEBM_CODEC_VORBIS;
	audioCodecValues.rangeMax.intValue = WEBM_CODEC_OPUS;
	audioCodecValues.value.intValue = WEBM_CODEC_OPUS;
	audioCodecValues.disabled = kPrFalse;
	audioCodecValues.hidden = kPrFalse;
	
	exNewParamInfo audioCodecParam;
	audioCodecParam.structVersion = 1;
	strncpy(audioCodecParam.identifier, WebMAudioCodec, 255);
	audioCodecParam.paramType = exParamType_int;
	audioCodecParam.flags = exParamFlag_none;
	audioCodecParam.paramValues = audioCodecValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioCodecParam);
	
	
	// Method
	exParamValues audioMethodValues;
	audioMethodValues.structVersion = 1;
	audioMethodValues.rangeMin.intValue = OGG_QUALITY;
	audioMethodValues.rangeMax.intValue = OGG_BITRATE;
	audioMethodValues.value.intValue = OGG_QUALITY;
	audioMethodValues.disabled = kPrFalse;
	audioMethodValues.hidden = kPrTrue;
	
	exNewParamInfo audioMethodParam;
	audioMethodParam.structVersion = 1;
	strncpy(audioMethodParam.identifier, WebMAudioMethod, 255);
	audioMethodParam.paramType = exParamType_int;
	audioMethodParam.flags = exParamFlag_none;
	audioMethodParam.paramValues = audioMethodValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioMethodParam);
	
	
	// Quality
	exParamValues audioQualityValues;
	audioQualityValues.structVersion = 1;
	audioQualityValues.rangeMin.floatValue = -0.1f;
	audioQualityValues.rangeMax.floatValue = 1.f;
	audioQualityValues.value.floatValue = 0.5f;
	audioQualityValues.disabled = kPrFalse;
	audioQualityValues.hidden = kPrTrue;
	
	exNewParamInfo audioQualityParam;
	audioQualityParam.structVersion = 1;
	strncpy(audioQualityParam.identifier, WebMAudioQuality, 255);
	audioQualityParam.paramType = exParamType_float;
	audioQualityParam.flags = exParamFlag_slider;
	audioQualityParam.paramValues = audioQualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioQualityParam);
	

	// Bitrate
	exParamValues audioBitrateValues;
	audioBitrateValues.structVersion = 1;
	audioBitrateValues.rangeMin.intValue = 40;
	audioBitrateValues.rangeMax.intValue = 1000;
	audioBitrateValues.value.intValue = 128;
	audioBitrateValues.disabled = kPrFalse;
	audioBitrateValues.hidden = kPrTrue;
	
	exNewParamInfo audioBitrateParam;
	audioBitrateParam.structVersion = 1;
	strncpy(audioBitrateParam.identifier, WebMAudioBitrate, 255);
	audioBitrateParam.paramType = exParamType_int;
	audioBitrateParam.flags = exParamFlag_slider;
	audioBitrateParam.paramValues = audioBitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioBitrateParam);


	// Opus autoBitrate
	exParamValues autoBitrateValues;
	autoBitrateValues.structVersion = 1;
	autoBitrateValues.value.intValue = kPrTrue;
	autoBitrateValues.disabled = kPrFalse;
	autoBitrateValues.hidden = kPrFalse;
	
	exNewParamInfo autoBitrateParam;
	autoBitrateParam.structVersion = 1;
	strncpy(autoBitrateParam.identifier, WebMOpusAutoBitrate, 255);
	autoBitrateParam.paramType = exParamType_bool;
	autoBitrateParam.flags = exParamFlag_none;
	autoBitrateParam.paramValues = autoBitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &autoBitrateParam);


	// Bitrate
	exParamValues opusBitrateValues;
	opusBitrateValues.structVersion = 1;
	opusBitrateValues.rangeMin.intValue = 1;
	opusBitrateValues.rangeMax.intValue = 512;
	opusBitrateValues.value.intValue = 128;
	opusBitrateValues.disabled = kPrTrue;
	opusBitrateValues.hidden = kPrFalse;
	
	exNewParamInfo opusBitrateParam;
	opusBitrateParam.structVersion = 1;
	strncpy(opusBitrateParam.identifier, WebMOpusBitrate, 255);
	opusBitrateParam.paramType = exParamType_int;
	opusBitrateParam.flags = exParamFlag_slider;
	opusBitrateParam.paramValues = opusBitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &opusBitrateParam);


	exportParamSuite->SetParamsVersion(exID, 1);
	
	
	return result;
}


prMALError
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
	widthValues.rangeMax.intValue = 16384;

	exportParamSuite->ChangeParam(exID, gIdx, ADBEVideoWidth, &widthValues);
	
	
	// height
	utf16ncpy(paramString, "Height", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoHeight, paramString);
	
	exParamValues heightValues;
	exportParamSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightValues);

	heightValues.rangeMin.intValue = 16;
	heightValues.rangeMax.intValue = 16384;
	
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
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	static const char *frameRateStrings[] = {	"10",
												"15",
												"23.976",
												"24",
												"25 (PAL)",
												"29.97 (NTSC)",
												"30",
												"47.952",
												"48",
												"50",
												"59.94",
												"60"};
	
	PrTime ticksPerSecond = 0;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
		
		// is there overflow potential here?
		assert(frameRates[i] == ticksPerSecond * frameRateNumDens[i][1] / frameRateNumDens[i][0]);
	}
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEVideoFPS);
	
	exOneParamValueRec tempFrameRate;
	
	for(csSDK_int32 i=0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		tempFrameRate.timeValue = frameRates[i];
		utf16ncpy(paramString, frameRateStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEVideoFPS, &tempFrameRate, paramString);
	}
	
	
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
	
	
	// Method
	utf16ncpy(paramString, "Method", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoMethod, paramString);
	
	
	int vidMethods[] = {	WEBM_METHOD_CONSTANT_QUALITY,
							WEBM_METHOD_CONSTRAINED_QUALITY,
							WEBM_METHOD_BITRATE,
							WEBM_METHOD_VBR };
	
	const char *vidMethodStrings[]	= {	"Constant Quality",
										"Constrained Quality",
										"Constant Bitrate",
										"Variable Bitrate" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoMethod);
	
	exOneParamValueRec tempEncodingMethod;
	for(int i=0; i < 4; i++)
	{
		tempEncodingMethod.intValue = vidMethods[i];
		utf16ncpy(paramString, vidMethodStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoMethod, &tempEncodingMethod, paramString);
	}
	
	
	// Quality
	utf16ncpy(paramString, "Quality", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoQuality, paramString);
	
	exParamValues videoQualityValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityValues);

	videoQualityValues.rangeMin.intValue = 0;
	videoQualityValues.rangeMax.intValue = 100;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoQuality, &videoQualityValues);
	
	
	// Bitrate
	utf16ncpy(paramString, "Bitrate (kb/s)", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoBitrate, paramString);
	
	exParamValues bitrateValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &bitrateValues);

	bitrateValues.rangeMin.intValue = 1;
	bitrateValues.rangeMax.intValue = 10000;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoBitrate, &bitrateValues);
	
	
	// 2-pass
	utf16ncpy(paramString, "2-Pass Encoding", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoTwoPass, paramString);
	
	
	// Max Keyframe Distance
	utf16ncpy(paramString, "Max Keyframe Distance", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoKeyframeMaxDistance, paramString);
	
	exParamValues maxKeyframeDistanceValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMVideoKeyframeMaxDistance, &maxKeyframeDistanceValues);

	maxKeyframeDistanceValues.rangeMin.intValue = 0;
	maxKeyframeDistanceValues.rangeMax.intValue = 999;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoKeyframeMaxDistance, &maxKeyframeDistanceValues);


	// hide old Encoding quality parameter
#define WebMVideoEncoding "WebMVideoEncoding"
	exParamValues encodingValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMVideoEncoding, &encodingValues);
	encodingValues.hidden = kPrTrue;
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoEncoding, &encodingValues);
	
	
	// Sampling
	utf16ncpy(paramString, "Sampling", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoSampling, paramString);
	
	
	int vidSampling[] = {	WEBM_420,
							WEBM_422,
							WEBM_444 };
	
	const char *vidSamplingStrings[]	= {	"4:2:0",
											"4:2:2",
											"4:4:4" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoSampling);
	
	exOneParamValueRec tempSamplingMethod;
	for(int i=0; i < 3; i++)
	{
		tempSamplingMethod.intValue = vidSampling[i];
		utf16ncpy(paramString, vidSamplingStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoSampling, &tempSamplingMethod, paramString);
	}
	

	// Bit depth
	utf16ncpy(paramString, "Bit depth", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoBitDepth, paramString);
	
	
	int vidBitDepth[] = {	VPX_BITS_8,
							VPX_BITS_10,
							VPX_BITS_12 };
	
	const char *vidBitDepthStrings[]	= {	"8-bit",
											"10-bit",
											"12-bit" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoBitDepth);
	
	exOneParamValueRec tempBitDepthMethod;
	for(int i=0; i < 3; i++)
	{
		tempBitDepthMethod.intValue = vidBitDepth[i];
		utf16ncpy(paramString, vidBitDepthStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoBitDepth, &tempBitDepthMethod, paramString);
	}
	
	
	// Alpha channel
	utf16ncpy(paramString, "Include Alpha Channel", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoAlpha, paramString);
	

	// Custom settings
	utf16ncpy(paramString, "Custom settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMCustomGroup, paramString);
	
	utf16ncpy(paramString, "Custom args", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMCustomArgs, paramString);
	
	
	
	
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
	
	csSDK_int32 channelTypes[] = { kPrAudioChannelType_Mono,
									kPrAudioChannelType_Stereo,
									kPrAudioChannelType_51 };
	
	const char *channelTypeStrings[] = { "Mono", "Stereo", "Dolby 5.1" };
	
	
	exportParamSuite->ClearConstrainedValues(exID, gIdx, ADBEAudioNumChannels);
	
	exOneParamValueRec tempChannelType;
	
	for(csSDK_int32 i=0; i < sizeof(channelTypes) / sizeof(csSDK_int32); i++)
	{
		tempChannelType.intValue = channelTypes[i];
		utf16ncpy(paramString, channelTypeStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, ADBEAudioNumChannels, &tempChannelType, paramString);
	}
	
	
	// Audio codec settings
	utf16ncpy(paramString, "Codec settings", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEAudioCodecGroup, paramString);


	// Audio Codec
	utf16ncpy(paramString, "Codec", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMAudioCodec, paramString);
	
	
	WebM_Audio_Codec audio_codecs[] = {	WEBM_CODEC_VORBIS,
										WEBM_CODEC_OPUS };
	
	const char *audioCodecStrings[]	= {	"Vorbis",
										"Opus" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMAudioCodec);
	
	exOneParamValueRec tempAudioCodec;
	for(int i=0; i < 2; i++)
	{
		tempAudioCodec.intValue = audio_codecs[i];
		utf16ncpy(paramString, audioCodecStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMAudioCodec, &tempAudioCodec, paramString);
	}
	
	
	// Method
	utf16ncpy(paramString, "Method", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMAudioMethod, paramString);
	
	
	int audioMethods[] = {	OGG_QUALITY,
							OGG_BITRATE };
	
	const char *audioMethodStrings[]	= {	"Quality",
											"Bitrate" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMAudioMethod);
	
	exOneParamValueRec tempAudioEncodingMethod;
	for(int i=0; i < 2; i++)
	{
		tempAudioEncodingMethod.intValue = audioMethods[i];
		utf16ncpy(paramString, audioMethodStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMAudioMethod, &tempAudioEncodingMethod, paramString);
	}
	
	
	// Quality
	utf16ncpy(paramString, "Quality", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMAudioQuality, paramString);
	
	exParamValues qualityValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &qualityValues);

	qualityValues.rangeMin.floatValue = -0.1f;
	qualityValues.rangeMax.floatValue = 1.f;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMAudioQuality, &qualityValues);
	

	// Bitrate
	utf16ncpy(paramString, "Bitrate (kb/s)", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMAudioBitrate, paramString);
	
	exParamValues audioBitrateValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMAudioBitrate, &audioBitrateValues);

	audioBitrateValues.rangeMin.intValue = 40;
	audioBitrateValues.rangeMax.intValue = 1000;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMAudioBitrate, &audioBitrateValues);


	// Auto bitrate
	utf16ncpy(paramString, "Auto bitrate", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMOpusAutoBitrate, paramString);


	// Opus Bitrate
	utf16ncpy(paramString, "Bitrate (kb/s)", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMOpusBitrate, paramString);
	
	exParamValues opusBitrateValues;
	exportParamSuite->GetParamValue(exID, gIdx, WebMOpusBitrate, &opusBitrateValues);

	opusBitrateValues.rangeMin.intValue = 1;
	opusBitrateValues.rangeMax.intValue = 512;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMOpusBitrate, &opusBitrateValues);


	return result;
}


prMALError
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
	exParamValues width, height, frameRate;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRate);
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);

	exParamValues codecP, methodP, samplingP, bitDepthP, videoQualityP, videoBitrateP, alphaP, twoPassP;
	paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoMethod, &methodP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoSampling, &samplingP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitDepth, &bitDepthP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &videoBitrateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoTwoPass, &twoPassP);
	

	exParamValues audioCodecP, audioMethodP, audioQualityP, audioBitrateP;
	paramSuite->GetParamValue(exID, gIdx, WebMAudioCodec, &audioCodecP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioMethod, &audioMethodP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &audioQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
	
	exParamValues autoBitrateP, opusBitrateP;
	paramSuite->GetParamValue(exID, gIdx, WebMOpusAutoBitrate, &autoBitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
	
	
	// oh boy, figure out frame rate
	PrTime frameRates[] = {	10, 15, 23,
							24, 25, 29,
							30, 48, 48,
							50, 59, 60};
													
	static const PrTime frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
													{24, 1}, {25, 1}, {30000, 1001},
													{30, 1}, {48000, 1001}, {48, 1},
													{50, 1}, {60000, 1001}, {60, 1}};
	
	static const char *frameRateStrings[] = {	"10",
												"15",
												"23.976",
												"24",
												"25 (PAL)",
												"29.97 (NTSC)",
												"30",
												"47.952",
												"48",
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
	
	if(frame_rate_index >= 0 && frame_rate_index < 12) 
		stream1 << ", " << frameRateStrings[frame_rate_index] << " fps";
	
	summary1 = stream1.str();
	
	
	std::stringstream stream2;
	
	stream2 << (int)sampleRateP.value.floatValue << " Hz";
	stream2 << ", " << (channelTypeP.value.intValue == kPrAudioChannelType_51 ? "Dolby 5.1" :
						channelTypeP.value.intValue == kPrAudioChannelType_Mono ? "Mono" :
						"Stereo");

	stream2 << ", ";
	
	if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
	{
		stream2 << "Opus ";
		
		if(autoBitrateP.value.intValue)
		{
			stream2 << "auto bitrate";
		}
		else
		{
			stream2 << opusBitrateP.value.intValue << " kbps";
		}
	}
	else
	{
		stream2 << "Vorbis ";
		
		if(audioMethodP.value.intValue == OGG_BITRATE)
		{
			stream2 << audioBitrateP.value.intValue << " kbps";
		}
		else
		{
			stream2 << "Quality " << audioQualityP.value.floatValue;
		}
	}
	

	
	summary2 = stream2.str();
	
	
	WebM_Video_Method method = (WebM_Video_Method)methodP.value.intValue;
	
	std::stringstream stream3;
	
	if(method == WEBM_METHOD_CONSTANT_QUALITY || method == WEBM_METHOD_CONSTRAINED_QUALITY)
	{
		stream3 << "Quality " << videoQualityP.value.intValue;
		
		if(method == WEBM_METHOD_CONSTRAINED_QUALITY)
			stream3 << ", Limit " << videoBitrateP.value.intValue << " kb/s";
	}
	else
	{
		stream3 << videoBitrateP.value.intValue << " kb/s";
		
		if(method == WEBM_METHOD_VBR)
			stream3 << " VBR";
	}
	
	stream3 << (codecP.value.intValue == WEBM_CODEC_VP9 ? ", VP9" : ", VP8");
	
	if(twoPassP.value.intValue)
		stream3 << " 2-pass";

	if(codecP.value.intValue == WEBM_CODEC_VP9)
	{
		if(samplingP.value.intValue == WEBM_444)
			stream3 << " 4:4:4";
		else if(samplingP.value.intValue == WEBM_422)
			stream3 << " 4:2:2";
		else
			stream3 << " 4:2:0";
		
		if(bitDepthP.value.intValue == VPX_BITS_10)
			stream3 << " 10-bit";
		else if(bitDepthP.value.intValue == VPX_BITS_12)
			stream3 << " 12-bit";
		else
			stream3 << " 8-bit";
	}
	
	if(alphaP.value.intValue)
		stream3 << ", Alpha";
	
	summary3 = stream3.str();
	
	

	utf16ncpy(summaryRecP->Summary1, summary1.c_str(), 255);
	utf16ncpy(summaryRecP->Summary2, summary2.c_str(), 255);
	utf16ncpy(summaryRecP->Summary3, summary3.c_str(), 255);
	
	return malNoError;
}


prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP)
{
	ExportSettings			*privateData	= reinterpret_cast<ExportSettings*>(validateParamChangedRecP->privateData);
	PrSDKExportParamSuite	*paramSuite		= privateData->exportParamSuite;
	
	csSDK_int32 exID = validateParamChangedRecP->exporterPluginID;
	csSDK_int32 gIdx = validateParamChangedRecP->multiGroupIndex;
	
	std::string param = validateParamChangedRecP->changedParamIdentifier;
	
	if(param == WebMVideoCodec)
	{
		exParamValues codecValue, samplingValue, bitDepthValue;
		
		paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoSampling, &samplingValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoBitDepth, &bitDepthValue);
		
		bitDepthValue.disabled = samplingValue.disabled = (codecValue.value.intValue != WEBM_CODEC_VP9);
		
		paramSuite->ChangeParam(exID, gIdx, WebMVideoSampling, &samplingValue);
		paramSuite->ChangeParam(exID, gIdx, WebMVideoBitDepth, &bitDepthValue);
	}
	else if(param == WebMVideoMethod)
	{
		exParamValues methodValue, videoQualityValue, videoBitrateValue;
		
		paramSuite->GetParamValue(exID, gIdx, WebMVideoMethod, &methodValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &videoBitrateValue);
		
		videoQualityValue.hidden = (methodValue.value.intValue == WEBM_METHOD_BITRATE || methodValue.value.intValue == WEBM_METHOD_VBR);
		videoBitrateValue.hidden = (methodValue.value.intValue == WEBM_METHOD_CONSTANT_QUALITY);
		
		paramSuite->ChangeParam(exID, gIdx, WebMVideoQuality, &videoQualityValue);
		paramSuite->ChangeParam(exID, gIdx, WebMVideoBitrate, &videoBitrateValue);
	}
	else if(param == WebMAudioMethod)
	{
		exParamValues audioMethodP, audioQualityP, audioBitrateP;
		paramSuite->GetParamValue(exID, gIdx, WebMAudioMethod, &audioMethodP);
		paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &audioQualityP);
		paramSuite->GetParamValue(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
		
		audioQualityP.hidden = (audioMethodP.value.intValue == OGG_BITRATE);
		audioBitrateP.hidden = !audioQualityP.hidden;
		
		paramSuite->ChangeParam(exID, gIdx, WebMAudioQuality, &audioQualityP);
		paramSuite->ChangeParam(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
	}
	else if(param == WebMAudioCodec)
	{
		exParamValues audioCodecP;
		paramSuite->GetParamValue(exID, gIdx, WebMAudioCodec, &audioCodecP);
		
		exParamValues sampleRateP;
		paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
		
		exParamValues audioMethodP, audioQualityP, audioBitrateP;
		paramSuite->GetParamValue(exID, gIdx, WebMAudioMethod, &audioMethodP);
		paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &audioQualityP);
		paramSuite->GetParamValue(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
		
		exParamValues autoBitrateP, opusBitrateP;
		paramSuite->GetParamValue(exID, gIdx, WebMOpusAutoBitrate, &autoBitrateP);
		paramSuite->GetParamValue(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
		
		
		bool showVorbis = (audioCodecP.value.intValue == WEBM_CODEC_VORBIS);
		
		audioMethodP.hidden = audioQualityP.hidden = audioBitrateP.hidden = !showVorbis;
		autoBitrateP.hidden = opusBitrateP.hidden = showVorbis;
		
		if(audioMethodP.value.intValue == OGG_BITRATE)
			audioQualityP.hidden = kPrTrue;
		else
			audioBitrateP.hidden = kPrTrue;
		
		if(audioCodecP.value.intValue == WEBM_CODEC_OPUS)
		{
			sampleRateP.value.floatValue = 48000.0f;
			sampleRateP.disabled = kPrTrue;
		}
		else
		{
			sampleRateP.disabled = kPrFalse;
		}
			
		
		paramSuite->ChangeParam(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
		
		paramSuite->ChangeParam(exID, gIdx, WebMAudioMethod, &audioMethodP);
		paramSuite->ChangeParam(exID, gIdx, WebMAudioQuality, &audioQualityP);
		paramSuite->ChangeParam(exID, gIdx, WebMAudioBitrate, &audioBitrateP);
		
		paramSuite->ChangeParam(exID, gIdx, WebMOpusAutoBitrate, &autoBitrateP);
		paramSuite->ChangeParam(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
	}
	else if(param == WebMOpusAutoBitrate)
	{
		exParamValues autoBitrateP, opusBitrateP;
		paramSuite->GetParamValue(exID, gIdx, WebMOpusAutoBitrate, &autoBitrateP);
		paramSuite->GetParamValue(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
		
		opusBitrateP.disabled = !!autoBitrateP.value.intValue;
		
		paramSuite->ChangeParam(exID, gIdx, WebMOpusBitrate, &opusBitrateP);
	}

	return malNoError;
}


static bool
quotedTokenize(const string& str,
				  std::vector<string>& tokens,
				  const string& delimiters = " ")
{
	// this function will respect quoted strings when tokenizing
	// the quotes will be included in the returned strings
	
	int i = 0;
	bool in_quotes = false;
	
	// if there are un-quoted delimiters in the beginning, skip them
	while(i < str.size() && str[i] != '\"' && string::npos != delimiters.find(str[i]) )
		i++;
	
	string::size_type lastPos = i;
	
	while(i < str.size())
	{
		if(str[i] == '\"' && (i == 0 || str[i-1] != '\\'))
			in_quotes = !in_quotes;
		else if(!in_quotes)
		{
			if( string::npos != delimiters.find(str[i]) )
			{
				tokens.push_back(str.substr(lastPos, i - lastPos));
				
				lastPos = i + 1;
				
				// if there are more delimiters ahead, push forward
				while(lastPos < str.size() && (str[lastPos] != '\"' || str[lastPos-1] != '\\') && string::npos != delimiters.find(str[lastPos]) )
					lastPos++;
					
				i = lastPos;
				continue;
			}
		}
		
		i++;
	}
	
	if(in_quotes)
		return false;
	
	// we're at the end, was there anything left?
	if(str.size() - lastPos > 0)
		tokens.push_back( str.substr(lastPos) );
	
	return true;
}


template <typename T>
static void SetValue(T &v, string s)
{
	std::stringstream ss;
	
	ss << s;
	
	ss >> v;
}


bool
ConfigureEncoderPre(vpx_codec_enc_cfg_t &config, unsigned long &deadline, const char *txt)
{
	std::vector<string> args;
	
	if(quotedTokenize(txt, args, " =\t\r\n") && args.size() > 0)
	{
		const int num_args = args.size();
	
		args.push_back(""); // so there's always an i+1
		
		int i = 0;
		
		while(i < num_args)
		{
			const string &arg = args[i];
			const string &val = args[i + 1];
			
			if(arg == "--best")
			{	deadline = VPX_DL_BEST_QUALITY;	}
			
			else if(arg == "--good")
			{	deadline = VPX_DL_GOOD_QUALITY;	}
			
			else if(arg == "--rt")
			{	deadline = VPX_DL_REALTIME;	}
			
			else if(arg == "-d" || arg == "--deadline")
			{	SetValue(deadline, val); i++;	}

			else if(arg == "-t" || arg == "--threads")
			{	SetValue(config.g_threads, val); i++;	}
			
			else if(arg == "--lag-in-frames")
			{	SetValue(config.g_lag_in_frames, val); i++;	}
			
			else if(arg == "--drop-frame")
			{	SetValue(config.rc_dropframe_thresh, val); i++;	}
			
			else if(arg == "--resize-allowed")
			{	SetValue(config.rc_resize_allowed, val); i++;	}
			
			else if(arg == "--resize-width")
			{	SetValue(config.rc_scaled_width, val); i++;	}
			
			else if(arg == "--resize-height")
			{	SetValue(config.rc_scaled_height, val); i++;	}
			
			else if(arg == "--resize-up")
			{	SetValue(config.rc_resize_up_thresh, val); i++;	}
			
			else if(arg == "--resize-down")
			{	SetValue(config.rc_resize_down_thresh, val); i++;	}
			
			else if(arg == "--target-bitrate")
			{	SetValue(config.rc_target_bitrate, val); i++;	}
			
			else if(arg == "--min-q")
			{	SetValue(config.rc_min_quantizer, val); i++;	}
			
			else if(arg == "--max-q")
			{	SetValue(config.rc_max_quantizer, val); i++;	}
			
			else if(arg == "--undershoot-pct")
			{	SetValue(config.rc_undershoot_pct, val); i++;	}
			
			else if(arg == "--overshoot-pct")
			{	SetValue(config.rc_overshoot_pct, val); i++;	}

			else if(arg == "--buf-sz")
			{	SetValue(config.rc_buf_sz, val); i++;	}

			else if(arg == "--buf-initial-sz")
			{	SetValue(config.rc_buf_initial_sz, val); i++;	}

			else if(arg == "--buf-optimal-sz")
			{	SetValue(config.rc_buf_optimal_sz, val); i++;	}

			else if(arg == "--bias-pct")
			{	SetValue(config.rc_2pass_vbr_bias_pct, val); i++;	}

			else if(arg == "--minsection-pct")
			{	SetValue(config.rc_2pass_vbr_minsection_pct, val); i++;	}

			else if(arg == "--maxsection-pct")
			{	SetValue(config.rc_2pass_vbr_maxsection_pct, val); i++;	}

			else if(arg == "--kf-min-dist")
			{	SetValue(config.kf_min_dist, val); i++;	}

			else if(arg == "--kf-max-dist")
			{	SetValue(config.kf_max_dist, val); i++;	}

			else if(arg == "--disable-kf")
			{	config.kf_mode = VPX_KF_DISABLED;	}

			
			i++;
		}
	
		return true;
	}
	else
		return false;
}


#define ConfigureValue(encoder, ctrl_id, s) \
	do{							\
		std::stringstream ss;	\
		ss << s;				\
		int v = 0;		\
		ss >> v;				\
		config_err = vpx_codec_control(encoder, ctrl_id, v); \
	}while(0)

bool
ConfigureEncoderPost(vpx_codec_ctx_t *encoder, const char *txt)
{
	std::vector<string> args;
	
	vpx_codec_err_t config_err = VPX_CODEC_OK;
	
	if(quotedTokenize(txt, args, " =\t\r\n") && args.size() > 0)
	{
		const int num_args = args.size();

		args.push_back(""); // so there's always an i+1
		
		int i = 0;
		
		while(i < num_args)
		{
			const string &arg = args[i];
			const string &val = args[i + 1];
		
			if(arg == "--noise-sensitivity")
			{	ConfigureValue(encoder, VP8E_SET_NOISE_SENSITIVITY, val); i++;	}

			else if(arg == "--sharpness")
			{	ConfigureValue(encoder, VP8E_SET_SHARPNESS, val); i++;	}

			else if(arg == "--static-thresh")
			{	ConfigureValue(encoder, VP8E_SET_STATIC_THRESHOLD, val); i++;	}

			else if(arg == "--cpu-used")
			{	ConfigureValue(encoder, VP8E_SET_CPUUSED, val); i++;	}

			else if(arg == "--token-parts")
			{	ConfigureValue(encoder, VP8E_SET_TOKEN_PARTITIONS, val); i++;	}

			else if(arg == "--tile-columns")
			{	ConfigureValue(encoder, VP9E_SET_TILE_COLUMNS, val); i++;	}

			else if(arg == "--tile-rows")
			{	ConfigureValue(encoder, VP9E_SET_TILE_ROWS, val); i++;	}
			
			else if(arg == "--auto-alt-ref")
			{	ConfigureValue(encoder, VP8E_SET_ENABLEAUTOALTREF, val); i++;	}

			else if(arg == "--arnr-maxframes")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_MAXFRAMES, val); i++;	}

			else if(arg == "--arnr-strength")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_STRENGTH, val); i++;	}

			else if(arg == "--arnr-type")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_TYPE, val); i++;	}

			else if(arg == "--tune")
			{
				unsigned int ival = val == "psnr" ? VP8_TUNE_PSNR :
									val == "ssim" ? VP8_TUNE_SSIM :
									VP8_TUNE_PSNR;
			
				ConfigureValue(encoder, VP8E_SET_TUNING, ival);
				i++;
			}

			else if(arg == "--cq-level")
			{	ConfigureValue(encoder, VP8E_SET_CQ_LEVEL, val); i++;	}
			
			else if(arg == "--max-intra-rate")
			{	ConfigureValue(encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, val); i++;	}

			else if(arg == "--gf-cbr-boost")
			{	ConfigureValue(encoder, VP9E_SET_GF_CBR_BOOST_PCT, val); i++;	}

			else if(arg == "--screen-content-mode")
			{	ConfigureValue(encoder, VP8E_SET_SCREEN_CONTENT_MODE, val);	i++;	}
			
			else if(arg == "--lossless")
			{	ConfigureValue(encoder, VP9E_SET_LOSSLESS, 1);	}
			
			else if(arg == "--frame-parallel")
			{	ConfigureValue(encoder, VP9E_SET_FRAME_PARALLEL_DECODING, val);	i++;	}

			else if(arg == "--aq-mode")
			{	ConfigureValue(encoder, VP9E_SET_AQ_MODE, val);	i++;	}

			else if(arg == "--frame_boost")
			{	ConfigureValue(encoder, VP9E_SET_FRAME_PERIODIC_BOOST, val); i++;	}

			else if(arg == "--noise-sensitivity")
			{	ConfigureValue(encoder, VP9E_SET_NOISE_SENSITIVITY, val); i++;	}
			
			else if(arg == "--tune-content")
			{
				unsigned int ival = val == "default" ? VP9E_CONTENT_DEFAULT :
									val == "screen" ? VP9E_CONTENT_SCREEN :
									VP9E_CONTENT_DEFAULT;
			
				ConfigureValue(encoder, VP9E_SET_TUNE_CONTENT, ival);
				i++;
			}

			else if(arg == "--color-space")
			{
				unsigned int ival = val == "unknown" ? VPX_CS_UNKNOWN :
									val == "bt601" ? VPX_CS_BT_601 :
									val == "bt709" ? VPX_CS_BT_709 :
									val == "smpte170" ? VPX_CS_SMPTE_170 :
									val == "smpte240" ? VPX_CS_SMPTE_240 :
									val == "bt2020" ? VPX_CS_BT_2020 :
									val == "reserved" ? VPX_CS_RESERVED :
									val == "sRGB" ? VPX_CS_SRGB :
									VPX_CS_UNKNOWN;
			
				ConfigureValue(encoder, VP9E_SET_COLOR_SPACE, ival);
				i++;
			}
			
			else if(arg == "--min-gf-interval")
			{	ConfigureValue(encoder, VP9E_SET_MIN_GF_INTERVAL, val); i++;	}
			
			else if(arg == "--max-gf-interval")
			{	ConfigureValue(encoder, VP9E_SET_MAX_GF_INTERVAL, val); i++;	}

			//else if(arg == "--target-level")
			//{	ConfigureValue(encoder, VP9E_SET_TARGET_LEVEL, val); i++;	}

			//else if(arg == "--row-mt")
			//{	ConfigureValue(encoder, VP9E_SET_ROW_MT, 1);	}

			else if(arg == "--color-range")
			{
				unsigned int ival = val == "studio" ? VPX_CR_STUDIO_RANGE :
									val == "full" ? VPX_CR_FULL_RANGE :
									VPX_CR_STUDIO_RANGE;
			
				ConfigureValue(encoder, VP9E_SET_COLOR_RANGE, ival);
				i++;
			}

			i++;	
		}
		
		return true;
	}
	else
		return false;
}

