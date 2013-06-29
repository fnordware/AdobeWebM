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

extern "C" {

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

}


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
	
	ExportSettings *privateData	= reinterpret_cast<ExportSettings*>(outputSettingsP->privateData);
	
	csSDK_uint32				exID			= outputSettingsP->exporterPluginID;
	exParamValues				width,
								height,
								frameRate,
								pixelAspectRatio,
								fieldType,
								//alpha,
								methodP,
								videoQualityP,
								videoBitrateP,
								sampleRate,
								channelType,
								audioQualityP;
	PrSDKExportParamSuite		*paramSuite		= privateData->exportParamSuite;
	csSDK_int32					mgroupIndex		= 0;
	float						fps				= 0.0f;
	PrTime						ticksPerSecond	= 0;
	csSDK_uint32				videoBitrate	= 0;
	
	privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	fps = (float)ticksPerSecond / (float)frameRate.value.timeValue;
	
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
		
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoMethod, &methodP);
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoQuality, &videoQualityP);
		paramSuite->GetParamValue(exID, mgroupIndex, WebMVideoBitrate, &videoBitrateP);
		paramSuite->GetParamValue(exID, mgroupIndex, WebMAudioQuality, &audioQualityP);
		
		if(methodP.value.intValue == WEBM_METHOD_QUALITY)
		{
			int bitsPerFrameUncompressed = (width.value.intValue * height.value.intValue) * 3 * 8;
			int qual = videoQualityP.value.intValue;
			float qualityMaxMult = 0.5;
			float qualityMinMult = 0.01;
			float qualityMult = (((float)qual / 100.0) * (qualityMaxMult - qualityMinMult)) + qualityMinMult;
			int bitsPerFrame = bitsPerFrameUncompressed * qualityMult;
		
			videoBitrate += (bitsPerFrame * fps) / 1024;
		}
		else
			videoBitrate += videoBitrateP.value.intValue;
		
		//paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	}
	
	if(outputSettingsP->inExportAudio)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		outputSettingsP->outAudioChannelType = (PrAudioChannelType)channelType.value.intValue;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_Compressed;
		
		const PrAudioChannelType audioFormat = (PrAudioChannelType)channelType.value.intValue;
		const int audioChannels = (audioFormat == kPrAudioChannelType_51 ? 6 :
									audioFormat == kPrAudioChannelType_Mono ? 1 :
									2);

		float qualityMult = (audioQualityP.value.floatValue + 0.1) / 1.1;
		float ogg_mult = (qualityMult * 0.4) + 0.1;
		
		videoBitrate += (sampleRate.value.floatValue * audioChannels * 8 * 4 * ogg_mult) / 1024; // IDK
	}
	
	// return outBitratePerSecond in kbps
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
	
	// Match source
	exParamValues matchSourceValues;
	matchSourceValues.structVersion = 1;
	matchSourceValues.value.intValue = kPrTrue;
	matchSourceValues.disabled = kPrFalse;
	matchSourceValues.hidden = kPrFalse;
	
	exNewParamInfo matchSourceParam;
	matchSourceParam.structVersion = 1;
	strncpy(matchSourceParam.identifier, ADBEVideoMatchSource, 255);
	matchSourceParam.paramType = exParamType_bool;
	matchSourceParam.flags = exParamFlag_none;
	matchSourceParam.paramValues = matchSourceValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEBasicVideoGroup, &matchSourceParam);
	
	
	// width
	exParamValues widthValues;
	widthValues.structVersion = 1;
	widthValues.rangeMin.intValue = 16;
	widthValues.rangeMax.intValue = 8192;
	widthValues.value.intValue = widthP.mInt32;
	widthValues.disabled = kPrTrue;
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
	heightValues.disabled = kPrTrue;
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
	parValues.disabled = kPrTrue;
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
	fieldOrderValues.disabled = kPrTrue;
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
	fpsValues.disabled = kPrTrue;
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
	
	
	// Method
	exParamValues methodValues;
	methodValues.structVersion = 1;
	methodValues.rangeMin.intValue = WEBM_METHOD_QUALITY;
	methodValues.rangeMax.intValue = WEBM_METHOD_VBR;
	methodValues.value.intValue = WEBM_METHOD_QUALITY;
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
	videoBitrateValues.rangeMax.intValue = 9999;
	videoBitrateValues.value.intValue = 500;
	videoBitrateValues.disabled = kPrFalse;
	videoBitrateValues.hidden = kPrTrue;
	
	exNewParamInfo videoBitrateParam;
	videoBitrateParam.structVersion = 1;
	strncpy(videoBitrateParam.identifier, WebMVideoBitrate, 255);
	videoBitrateParam.paramType = exParamType_int;
	videoBitrateParam.flags = exParamFlag_slider;
	videoBitrateParam.paramValues = videoBitrateValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &videoBitrateParam);
	
	
	// Encoding
	exParamValues vidEncodingValues;
	vidEncodingValues.structVersion = 1;
	vidEncodingValues.rangeMin.intValue = WEBM_ENCODING_REALTIME;
	vidEncodingValues.rangeMax.intValue = WEBM_ENCODING_BEST;
	vidEncodingValues.value.intValue = WEBM_ENCODING_GOOD;
	vidEncodingValues.disabled = kPrFalse;
	vidEncodingValues.hidden = kPrFalse;
	
	exNewParamInfo vidEncodingParam;
	vidEncodingParam.structVersion = 1;
	strncpy(vidEncodingParam.identifier, WebMVideoEncoding, 255);
	vidEncodingParam.paramType = exParamType_int;
	vidEncodingParam.flags = exParamFlag_none;
	vidEncodingParam.paramValues = vidEncodingValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEVideoCodecGroup, &vidEncodingParam);
	
	
	// Version
	exParamValues versionValues;
	versionValues.structVersion = 1;
	versionValues.rangeMin.intValue = 0;
	versionValues.rangeMax.intValue = INT_MAX;
	versionValues.value.intValue = WEBM_PLUGIN_VERSION_MAJOR << 16 |
									WEBM_PLUGIN_VERSION_MINOR << 8 |
									WEBM_PLUGIN_VERSION_BUILD;
	versionValues.disabled = kPrTrue;
	versionValues.hidden = kPrTrue;
	
	exNewParamInfo versionParam;
	versionParam.structVersion = 1;
	strncpy(versionParam.identifier, WebMPluginVersion, 255);
	versionParam.paramType = exParamType_int;
	versionParam.flags = exParamFlag_none;
	versionParam.paramValues = versionValues;
	
	exportParamSuite->AddParam(exID, gIdx, WebMPluginVersion, &versionParam);
	
		
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
	exParamValues audioQualityValues;
	audioQualityValues.structVersion = 1;
	audioQualityValues.rangeMin.floatValue = -0.1f;
	audioQualityValues.rangeMax.floatValue = 1.f;
	audioQualityValues.value.floatValue = 0.5f;
	audioQualityValues.disabled = kPrFalse;
	audioQualityValues.hidden = kPrFalse;
	
	exNewParamInfo audioQualityParam;
	audioQualityParam.structVersion = 1;
	strncpy(audioQualityParam.identifier, WebMAudioQuality, 255);
	audioQualityParam.paramType = exParamType_float;
	audioQualityParam.flags = exParamFlag_slider;
	audioQualityParam.paramValues = audioQualityValues;
	
	exportParamSuite->AddParam(exID, gIdx, ADBEAudioCodecGroup, &audioQualityParam);
	
	

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
	
									
	// Match source
	utf16ncpy(paramString, "Match source", 255);
	exportParamSuite->SetParamName(exID, gIdx, ADBEVideoMatchSource, paramString);
	
	
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
	
	
	// Method
	utf16ncpy(paramString, "Method", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoMethod, paramString);
	
	
	int vidMethods[] = {	WEBM_METHOD_QUALITY,
							WEBM_METHOD_BITRATE,
							WEBM_METHOD_VBR };
	
	const char *vidMethodStrings[]	= {	"Constant Quality",
										"Constant Bitrate",
										"Variable Bitrate (2-pass)" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoMethod);
	
	exOneParamValueRec tempEncodingMethod;
	for(int i=0; i < 3; i++)
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
	bitrateValues.rangeMax.intValue = 9999;
	
	exportParamSuite->ChangeParam(exID, gIdx, WebMVideoBitrate, &bitrateValues);
	
	
	// Encoding
	utf16ncpy(paramString, "Encoding", 255);
	exportParamSuite->SetParamName(exID, gIdx, WebMVideoEncoding, paramString);
	
	
	int vidQualities[] = {	WEBM_ENCODING_REALTIME,
							WEBM_ENCODING_GOOD,
							WEBM_ENCODING_BEST };
	
	const char *vidQualityStrings[]	= {	"Realtime",
										"Good",
										"Best" };

	exportParamSuite->ClearConstrainedValues(exID, gIdx, WebMVideoEncoding);
	
	exOneParamValueRec tempEncodingQuality;
	for(int i=0; i < 3; i++)
	{
		tempEncodingQuality.intValue = vidQualities[i];
		utf16ncpy(paramString, vidQualityStrings[i], 255);
		exportParamSuite->AddConstrainedValuePair(exID, gIdx, WebMVideoEncoding, &tempEncodingQuality, paramString);
	}
	
	
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
	exParamValues width, height, frameRate, alpha;
	
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRate);
	//paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAlpha, &alpha);
	
	exParamValues sampleRateP, channelTypeP;
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioRatePerSecond, &sampleRateP);
	paramSuite->GetParamValue(exID, gIdx, ADBEAudioNumChannels, &channelTypeP);

	exParamValues codecP, methodP, videoQualityP, videoBitrateP, vidEncodingP, audioQualityP;
	paramSuite->GetParamValue(exID, gIdx, WebMVideoCodec, &codecP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoMethod, &methodP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &videoBitrateP);
	paramSuite->GetParamValue(exID, gIdx, WebMVideoEncoding, &vidEncodingP);
	paramSuite->GetParamValue(exID, gIdx, WebMAudioQuality, &audioQualityP);
	

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
	stream2 << ", " << (channelTypeP.value.intValue == kPrAudioChannelType_51 ? "Dolby 5.1" :
						channelTypeP.value.intValue == kPrAudioChannelType_Mono ? "Mono" :
						"Stereo");
	
	summary2 = stream2.str();
	
	
	WebM_Video_Method method = (WebM_Video_Method)methodP.value.intValue;
	
	std::stringstream stream3;
	
	if(method == WEBM_METHOD_QUALITY)
	{
		stream3 << "Quality " << videoQualityP.value.intValue;
	}
	else
	{
		stream3 << videoBitrateP.value.intValue << " kb/s";
		
		if(method == WEBM_METHOD_VBR)
			stream3 << " VBR";
	}
	
	stream3 << (codecP.value.intValue == WEBM_CODEC_VP9 ? ", VP9" : ", VP8");	

	if(vidEncodingP.value.intValue == WEBM_ENCODING_REALTIME)
		stream3 << ", Realtime";
	else if(vidEncodingP.value.intValue == WEBM_ENCODING_BEST)
		stream3 << ", Best";
	
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
	
	if(param == ADBEVideoMatchSource)
	{
		exParamValues matchSourceP, widthP, heightP, pixelAspectRatioP, fieldTypeP, frameRateP;
		
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoMatchSource, &matchSourceP);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoWidth, &widthP);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoHeight, &heightP);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
		paramSuite->GetParamValue(exID, gIdx, ADBEVideoFPS, &frameRateP);
		
		bool disabled = (matchSourceP.value.intValue != 0);
		
		widthP.disabled = heightP.disabled = pixelAspectRatioP.disabled = fieldTypeP.disabled = frameRateP.disabled = disabled;
		
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoWidth, &widthP);
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoHeight, &heightP);
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoAspect, &pixelAspectRatioP);
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoFieldType, &fieldTypeP);
		paramSuite->ChangeParam(exID, gIdx, ADBEVideoFPS, &frameRateP);
	}
	else if(param == WebMVideoMethod)
	{
		exParamValues methodValue, videoQualityValue, videoBitrateValue;
		
		paramSuite->GetParamValue(exID, gIdx, WebMVideoMethod, &methodValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoQuality, &videoQualityValue);
		paramSuite->GetParamValue(exID, gIdx, WebMVideoBitrate, &videoBitrateValue);
		
		videoQualityValue.hidden = !(methodValue.value.intValue == WEBM_METHOD_QUALITY);
		videoBitrateValue.hidden = (methodValue.value.intValue == WEBM_METHOD_QUALITY);
		
		paramSuite->ChangeParam(exID, gIdx, WebMVideoQuality, &videoQualityValue);
		paramSuite->ChangeParam(exID, gIdx, WebMVideoBitrate, &videoBitrateValue);
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
ConfigureEncoderPre(vpx_codec_enc_cfg_t &config, const char *txt)
{
	std::vector<string> args;
	
	if(quotedTokenize(txt, args, " =\t\r\n") && args.size() > 0)
	{
		args.push_back(""); // so there's always an i+1
		
		int i = 0;
		
		while(i < args.size())
		{
			const string &arg = args[i];
			
			if(arg == "-t" || arg == "--threads")
			{	SetValue(config.g_threads, args[i + 1]); i++;	}
			
			else if(arg == "--lag-in-frames")
			{	SetValue(config.g_lag_in_frames, args[i + 1]); i++;	}
			
			else if(arg == "--drop-frame")
			{	SetValue(config.rc_dropframe_thresh, args[i + 1]); i++;	}
			
			else if(arg == "--resize-allowed")
			{	SetValue(config.rc_resize_allowed, args[i + 1]); i++;	}
			
			else if(arg == "--resize-up")
			{	SetValue(config.rc_resize_up_thresh, args[i + 1]); i++;	}
			
			else if(arg == "--resize-down")
			{	SetValue(config.rc_resize_down_thresh, args[i + 1]); i++;	}
			
			else if(arg == "--target-bitrate")
			{	SetValue(config.rc_target_bitrate, args[i + 1]); i++;	}
			
			else if(arg == "--min-q")
			{	SetValue(config.rc_min_quantizer, args[i + 1]); i++;	}
			
			else if(arg == "--max-q")
			{	SetValue(config.rc_max_quantizer, args[i + 1]); i++;	}
			
			else if(arg == "--undershoot-pct")
			{	SetValue(config.rc_undershoot_pct, args[i + 1]); i++;	}
			
			else if(arg == "--overshoot-pct")
			{	SetValue(config.rc_overshoot_pct, args[i + 1]); i++;	}

			else if(arg == "--buf-sz")
			{	SetValue(config.rc_buf_sz, args[i + 1]); i++;	}

			else if(arg == "--buf-initial-sz")
			{	SetValue(config.rc_buf_initial_sz, args[i + 1]); i++;	}

			else if(arg == "--buf-optimal-sz")
			{	SetValue(config.rc_buf_optimal_sz, args[i + 1]); i++;	}

			else if(arg == "--bias-pct")
			{	SetValue(config.rc_2pass_vbr_bias_pct, args[i + 1]); i++;	}

			else if(arg == "--minsection-pct")
			{	SetValue(config.rc_2pass_vbr_minsection_pct, args[i + 1]); i++;	}

			else if(arg == "--maxsection-pct")
			{	SetValue(config.rc_2pass_vbr_maxsection_pct, args[i + 1]); i++;	}

			else if(arg == "--kf-min-dist")
			{	SetValue(config.kf_min_dist, args[i + 1]); i++;	}

			else if(arg == "--kf-max-dist")
			{	SetValue(config.kf_max_dist, args[i + 1]); i++;	}

			else if(arg == "--disable-kf")
			{	config.kf_mode = VPX_KF_DISABLED;	}

			else if(arg == "--periodicity")
			{	SetValue(config.ts_periodicity, args[i + 1]); i++;	}

			
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
		unsigned int v = 0;		\
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
		args.push_back(""); // so there's always an i+1
		
		int i = 0;
		
		while(i < args.size())
		{
			const string &arg = args[i];
		
			if(arg == "--noise-sensitivity")
			{	ConfigureValue(encoder, VP8E_SET_NOISE_SENSITIVITY, args[i + 1]); i++;	}

			else if(arg == "--sharpness")
			{	ConfigureValue(encoder, VP8E_SET_SHARPNESS, args[i + 1]); i++;	}

			else if(arg == "--cpu-used")
			{	ConfigureValue(encoder, VP8E_SET_CPUUSED, args[i + 1]); i++;	}

			else if(arg == "--token-parts")
			{	ConfigureValue(encoder, VP8E_SET_TOKEN_PARTITIONS, args[i + 1]); i++;	}

			else if(arg == "--tile-columns")
			{	ConfigureValue(encoder, VP9E_SET_TILE_COLUMNS, args[i + 1]); i++;	}

			else if(arg == "--auto-alt-ref")
			{	ConfigureValue(encoder, VP8E_SET_ENABLEAUTOALTREF, args[i + 1]); i++;	}

			else if(arg == "--arnr-maxframes")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_MAXFRAMES, args[i + 1]); i++;	}

			else if(arg == "--arnr-strength")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_STRENGTH, args[i + 1]); i++;	}

			else if(arg == "--arnr-type")
			{	ConfigureValue(encoder, VP8E_SET_ARNR_TYPE, args[i + 1]); i++;	}

			else if(arg == "--tune")
			{
				unsigned int val = args[i + 1] == "psnr" ? VP8_TUNE_PSNR :
									args[i + 1] == "ssim" ? VP8_TUNE_SSIM :
									0;
			
				ConfigureValue(encoder, VP8E_SET_TUNING, val);
				i++;
			}

			else if(arg == "--cq-level")
			{	ConfigureValue(encoder, VP8E_SET_CQ_LEVEL, args[i + 1]); i++;	}
			
			else if(arg == "--max-intra-rate")
			{	ConfigureValue(encoder, VP8E_SET_MAX_INTRA_BITRATE_PCT, args[i + 1]); i++;	}

			else if(arg == "--lossless")
			{	ConfigureValue(encoder, VP9E_SET_LOSSLESS, args[i + 1]); i++;	}
			
			
			i++;	
		}
		
		return true;
	}
	else
		return false;
}

