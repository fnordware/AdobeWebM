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


#include "WebM_Premiere_Import.h"


#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include <vorbis/codec.h>

#include "opus_multistream.h"

#include "mkvparser.hpp"

#include <assert.h>
#include <math.h>

#include <sstream>
#include <map>

#ifdef PRMAC_ENV
	#include <mach/mach.h>
#endif

int g_num_cpus = 1;



#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef PrSDKPPixCacheSuite2 PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion2
#else
typedef PrSDKPPixCacheSuite PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion
#endif


class PrMkvReader : public mkvparser::IMkvReader
{
  public:
	PrMkvReader(imFileRef fileRef);
	virtual ~PrMkvReader() {}
	
	virtual int Read(long long pos, long len, unsigned char* buf);
	virtual int Length(long long* total, long long* available);
	
	const imFileRef FileRef() const { return _fileRef; }
	
	enum {
		PrMkvError = -1,
		PrMkvSuccess = 0
	};
	
  private:
	const imFileRef _fileRef;
	
	long long _size;
};


PrMkvReader::PrMkvReader(imFileRef fileRef) :
	_fileRef(fileRef),
	_size(-1)
{
#ifdef PRWIN_ENV
	LARGE_INTEGER len;

	BOOL ok = GetFileSizeEx(_fileRef, &len);
	
	if(ok)
		_size = len.QuadPart;
#else
	SInt64 fork_size = 0;
	
	OSErr result = FSGetForkSize(CAST_REFNUM(_fileRef), &fork_size);
		
	if(result == noErr)
		_size = fork_size;
#endif
}

int PrMkvReader::Read(long long pos, long len, unsigned char* buf)
{
#ifdef PRWIN_ENV
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = pos;

	BOOL result = SetFilePointerEx(_fileRef, lpos, &out, FILE_BEGIN);
	
	DWORD count = len, out2;
	
	result = ReadFile(_fileRef, (LPVOID)buf, count, &out2, NULL);

	return (result && len == out2) ? PrMkvSuccess : PrMkvError;
#else
	ByteCount count = len, out = 0;
	
	OSErr result = FSReadFork(CAST_REFNUM(_fileRef), fsFromStart, pos, count, buf, &out);

	return (result == noErr && len == out) ? PrMkvSuccess : PrMkvError;
#endif
}


int PrMkvReader::Length(long long* total, long long* available)
{
	// total appears to mean the total length of the file, while
	// available means the amount of data that has been downloaded,
	// as in for a stream.  For a disk-based file, these two are the same.
	
	if(_size >= 0)
	{
		*total = *available = _size;
		
		return PrMkvSuccess;
	}
	else
		return PrMkvError;
}


typedef enum {
	CODEC_NONE = 0,
	CODEC_VP8,
	CODEC_VP9
} VideoCodec;

typedef enum {
	CODEC_ANONE = 0,
	CODEC_VORBIS,
	CODEC_OPUS
} AudioCodec;


typedef std::map<long long, PrAudioSample> SampleMap;

typedef struct
{	
	csSDK_int32				importerID;
	csSDK_int32				fileType;
	csSDK_int32				width;
	csSDK_int32				height;
	csSDK_int32				frameRateNum;
	csSDK_int32				frameRateDen;
	float					audioSampleRate;
	int						numChannels;
	
	PrMkvReader				*reader;
	mkvparser::Segment		*segment;
	int						video_track;
	VideoCodec				video_codec;
	int						audio_track;
	AudioCodec				audio_codec;
	
	bool					vpx_setup;
	vpx_codec_ctx_t			vpx_decoder;
	
	bool					vorbis_setup;
	vorbis_info				vi;
	vorbis_comment			vc;
	
	OpusMSDecoder			*opus_dec;
	
	SampleMap				*sample_map;
	PrAudioSample			total_samples;
	
	PlugMemoryFuncsPtr		memFuncs;
	SPBasicSuite			*BasicSuite;
	PrSDKPPixCreatorSuite	*PPixCreatorSuite;
	PrCacheSuite			*PPixCacheSuite;
	PrSDKPPixSuite			*PPixSuite;
	PrSDKPPix2Suite			*PPix2Suite;
	PrSDKTimeSuite			*TimeSuite;
	PrSDKImporterFileManagerSuite *FileSuite;
} ImporterLocalRec8, *ImporterLocalRec8Ptr, **ImporterLocalRec8H;


static prMALError 
SDKInit(
	imStdParms		*stdParms, 
	imImportInfoRec	*importInfo)
{
	importInfo->canSave				= kPrFalse;		// Can 'save as' files to disk, real file only.
	importInfo->canDelete			= kPrFalse;		// File importers only, use if you only if you have child files
	importInfo->canCalcSizes		= kPrFalse;		// These are for importers that look at a whole tree of files so
													// Premiere doesn't know about all of them.
	importInfo->canTrim				= kPrFalse;
	
	importInfo->hasSetup			= kPrFalse;		// Set to kPrTrue if you have a setup dialog
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	
	importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	importInfo->priority			= 0;
	
	importInfo->avoidAudioConform	= kPrTrue;		// If I let Premiere conform the audio, I get silence when
													// I try to play it in the program.  Seems like a bug to me.

#ifdef PRMAC_ENV
	// get number of CPUs using Mach calls
	host_basic_info_data_t hostInfo;
	mach_msg_type_number_t infoCount;
	
	infoCount = HOST_BASIC_INFO_COUNT;
	host_info(mach_host_self(), HOST_BASIC_INFO, 
			  (host_info_t)&hostInfo, &infoCount);
	
	g_num_cpus = hostInfo.avail_cpus;
#else // WIN_ENV
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	g_num_cpus = systemInfo.dwNumberOfProcessors;
#endif

	return malNoError;
}


static prMALError 
SDKGetIndFormat(
	imStdParms		*stdParms, 
	csSDK_size_t	index, 
	imIndFormatRec	*SDKIndFormatRec)
{
	prMALError	result		= malNoError;
	char formatname[255]	= "WebM";
	char shortname[32]		= "WebM";
	char platformXten[256]	= "webm\0\0";

	switch(index)
	{
		//	Add a case for each filetype.
		
		case 0:		
			SDKIndFormatRec->filetype			= 'WebM';

			SDKIndFormatRec->canWriteTimecode	= kPrFalse;
			SDKIndFormatRec->canWriteMetaData	= kPrFalse;

			SDKIndFormatRec->flags = xfCanImport;

			#ifdef PRWIN_ENV
			strcpy_s(SDKIndFormatRec->FormatName, sizeof (SDKIndFormatRec->FormatName), formatname);				// The long name of the importer
			strcpy_s(SDKIndFormatRec->FormatShortName, sizeof (SDKIndFormatRec->FormatShortName), shortname);		// The short (menu name) of the importer
			strcpy_s(SDKIndFormatRec->PlatformExtension, sizeof (SDKIndFormatRec->PlatformExtension), platformXten);	// The 3 letter extension
			#else
			strcpy(SDKIndFormatRec->FormatName, formatname);			// The Long name of the importer
			strcpy(SDKIndFormatRec->FormatShortName, shortname);		// The short (menu name) of the importer
			strcpy(SDKIndFormatRec->PlatformExtension, platformXten);	// The 3 letter extension
			#endif

			break;

		default:
			result = imBadFormatIndex;
	}

	return result;
}


static int PrivateDataCount(const unsigned char *private_data, size_t private_size)
{
	// the first byte
	unsigned char *p = (unsigned char *)private_data;
	
	return *p + 1;
}

static uint64_t
xiph_lace_value(const unsigned char ** np)
{
	uint64_t lace;
	uint64_t value;
	const unsigned char *p = *np;

	lace = *p++;
	value = lace;
	while (lace == 255) {
		lace = *p++;
		value += lace;
	}

	*np = p;

	return value;
}

static const unsigned char *GetPrivateDataPart(const unsigned char *private_data,
												size_t private_size, int part,
												size_t *part_size)
{
	const unsigned char *result = NULL;
	size_t result_size = 0;
	
	const unsigned char *p = private_data;
	
	int count = *p++ + 1;
	assert(count == 3);
	
	
	if(*p >= part)
	{
		uint64_t sizes[3];
		uint64_t total = 0;
		int i = 0;
		
		while(--count)
		{
			sizes[i] = xiph_lace_value(&p);
			total += sizes[i];
			i++;
		}
		sizes[i] = private_size - total - (p - private_data);
		
		for(i=0; i < part; ++i)
			p += sizes[i];
		
		result = p;
		result_size = sizes[part];
	}
	
	*part_size = result_size;
	
	return result;
}


prMALError 
SDKOpenFile8(
	imStdParms		*stdParms, 
	imFileRef		*SDKfileRef, 
	imFileOpenRec8	*SDKfileOpenRec8)
{
	prMALError			result = malNoError;

	ImporterLocalRec8H	localRecH = NULL;
	ImporterLocalRec8Ptr localRecP = NULL;

	if(SDKfileOpenRec8->privatedata)
	{
		localRecH = (ImporterLocalRec8H)SDKfileOpenRec8->privatedata;

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));

		localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *localRecH );
	}
	else
	{
		localRecH = (ImporterLocalRec8H)stdParms->piSuites->memFuncs->newHandle(sizeof(ImporterLocalRec8));
		SDKfileOpenRec8->privatedata = (PrivateDataPtr)localRecH;

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(localRecH));

		localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *localRecH );
		
		localRecP->reader = NULL;
		localRecP->segment = NULL;
		localRecP->video_track = -1;
		localRecP->video_codec = CODEC_NONE;
		localRecP->audio_track = -1;
		localRecP->audio_codec = CODEC_ANONE;
		localRecP->vpx_setup = false;
		localRecP->vorbis_setup = false;
		localRecP->opus_dec = NULL;
		localRecP->sample_map = NULL;
		localRecP->total_samples = 0;
		
		// Acquire needed suites
		localRecP->memFuncs = stdParms->piSuites->memFuncs;
		localRecP->BasicSuite = stdParms->piSuites->utilFuncs->getSPBasicSuite();
		if(localRecP->BasicSuite)
		{
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion, (const void**)&localRecP->PPixCreatorSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixCacheSuite, PrCacheVersion, (const void**)&localRecP->PPixCacheSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion, (const void**)&localRecP->PPixSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion, (const void**)&localRecP->PPix2Suite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion, (const void**)&localRecP->TimeSuite);
			localRecP->BasicSuite->AcquireSuite(kPrSDKImporterFileManagerSuite, kPrSDKImporterFileManagerSuiteVersion, (const void**)&localRecP->FileSuite);
		}

		localRecP->importerID = SDKfileOpenRec8->inImporterID;
		localRecP->fileType = SDKfileOpenRec8->fileinfo.filetype;
	}


	SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = reinterpret_cast<imFileRef>(imInvalidHandleValue);


	if(localRecP)
	{
		const prUTF16Char *path = SDKfileOpenRec8->fileinfo.filepath;
	
	#ifdef PRWIN_ENV
		HANDLE fileH = CreateFileW(path,
									GENERIC_READ,
									FILE_SHARE_READ,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);
									
		if(fileH != imInvalidHandleValue)
		{
			SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = fileH;
		}
		else
			result = imFileOpenFailed;
	#else
		FSIORefNum refNum = CAST_REFNUM(imInvalidHandleValue);
				
		CFStringRef filePathCFSR = CFStringCreateWithCharacters(NULL, path, prUTF16CharLength(path));
													
		CFURLRef filePathURL = CFURLCreateWithFileSystemPath(NULL, filePathCFSR, kCFURLPOSIXPathStyle, false);
		
		if(filePathURL != NULL)
		{
			FSRef fileRef;
			Boolean success = CFURLGetFSRef(filePathURL, &fileRef);
			
			if(success)
			{
				HFSUniStr255 dataForkName;
				FSGetDataForkName(&dataForkName);
			
				OSErr err = FSOpenFork(	&fileRef,
										dataForkName.length,
										dataForkName.unicode,
										fsRdWrPerm,
										&refNum);
			}
										
			CFRelease(filePathURL);
		}
									
		CFRelease(filePathCFSR);

		if(CAST_FILEREF(refNum) != imInvalidHandleValue)
		{
			SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = CAST_FILEREF(refNum);
		}
		else
			result = imFileOpenFailed;
	#endif

	}

	if(result == malNoError && localRecP->reader == NULL)
	{
		assert(localRecP->segment == NULL);
	
		localRecP->reader = new PrMkvReader(*SDKfileRef);
		
		long long pos = 0;

		mkvparser::EBMLHeader ebmlHeader;

		ebmlHeader.Parse(localRecP->reader, pos);
		
		long long ret = mkvparser::Segment::CreateInstance(localRecP->reader, pos, localRecP->segment);
		
		if(ret >= 0 && localRecP->segment != NULL)
		{
			ret = localRecP->segment->Load();
			
			if(ret >= 0)
			{
				const mkvparser::Tracks* pTracks = localRecP->segment->GetTracks();
				
				for(int t=0; t < pTracks->GetTracksCount(); t++)
				{
					const mkvparser::Track* const pTrack = pTracks->GetTrackByIndex(t);
					
					if(pTrack != NULL)
					{
						const long trackType = pTrack->GetType();
						const long trackNumber = pTrack->GetNumber();
						
						if(trackType == mkvparser::Track::kVideo)
						{
							const mkvparser::VideoTrack* const pVideoTrack = static_cast<const mkvparser::VideoTrack*>(pTrack);
							
							if(pVideoTrack && localRecP->video_track < 0)
							{
								localRecP->video_codec = pVideoTrack->GetCodecId() == std::string("V_VP8") ? CODEC_VP8 :
															pVideoTrack->GetCodecId() == std::string("V_VP9") ? CODEC_VP9 :
															CODEC_NONE;
							
								localRecP->video_track = trackNumber;
							}
						}
						else if(trackType == mkvparser::Track::kAudio)
						{
							const mkvparser::AudioTrack* const pAudioTrack = static_cast<const mkvparser::AudioTrack*>(pTrack);
							
							if(pAudioTrack && localRecP->audio_track < 0)
							{
								localRecP->audio_codec = pAudioTrack->GetCodecId() == std::string("A_VORBIS") ? CODEC_VORBIS :
															pAudioTrack->GetCodecId() == std::string("A_OPUS") ? CODEC_OPUS :
															CODEC_ANONE;
															
								localRecP->audio_track = trackNumber;
							}
						}
 					}
				}
				
				
				if(localRecP->video_track >= 0 && localRecP->vpx_setup == false)
				{
					const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->video_track);
					
					if(pTrack != NULL && pTrack->GetType() == mkvparser::Track::kVideo)
					{
						const mkvparser::VideoTrack* const pVideoTrack = static_cast<const mkvparser::VideoTrack*>(pTrack);
						
						if(pVideoTrack)
						{
							const char* codec_id = pTrack->GetCodecId();
						
							const vpx_codec_iface_t *iface = (codec_id == std::string("V_VP8") ? vpx_codec_vp8_dx() :
																codec_id == std::string("V_VP9") ? vpx_codec_vp9_dx() :
																NULL);
							
							vpx_codec_err_t codec_err = VPX_CODEC_OK;
							
							vpx_codec_ctx_t &decoder = localRecP->vpx_decoder;
							
							if(iface != NULL)
							{
								vpx_codec_dec_cfg_t config;
								config.threads = g_num_cpus;
								config.w = pVideoTrack->GetWidth();
								config.h = pVideoTrack->GetHeight();
								
								vpx_codec_flags_t flags = VPX_CODEC_CAP_FRAME_THREADING |
															//VPX_CODEC_USE_ERROR_CONCEALMENT | // this doesn't seem to work
															VPX_CODEC_USE_FRAME_THREADING;
								
								// TODO: Explore possibilities of decoding options by setting
								// VPX_CODEC_USE_POSTPROC here.  Things like VP8_DEMACROBLOCK and
								// VP8_MFQE (Multiframe Quality Enhancement) could be cool.
								
								codec_err = vpx_codec_dec_init(&decoder, iface, &config, flags);
								
								if(codec_err == VPX_CODEC_OK)
									localRecP->vpx_setup = true;
							}
						}
					}
				}
		
				if(localRecP->audio_track >= 0)
				{
					const mkvparser::Tracks* pTracks = localRecP->segment->GetTracks();
				
					const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->audio_track);
					
					if(pTrack != NULL && pTrack->GetType() == mkvparser::Track::kAudio)
					{
						const mkvparser::AudioTrack* const pAudioTrack = static_cast<const mkvparser::AudioTrack*>(pTrack);
						
						if(pAudioTrack)
						{
							if(pTrack->GetCodecId() == std::string("A_VORBIS") && localRecP->vorbis_setup == false)
							{
								assert(pAudioTrack->GetSeekPreRoll() == 0);
								assert(pAudioTrack->GetCodecDelay() == 0);

								size_t private_size = 0;
								const unsigned char *private_data = pAudioTrack->GetCodecPrivate(private_size);
								
								if(private_data && private_size && PrivateDataCount(private_data, private_size) == 3)
								{
									vorbis_info &vi = localRecP->vi;
									vorbis_comment &vc = localRecP->vc;
									
									vorbis_info_init(&vi);
									vorbis_comment_init(&vc);
									
									int ogg_packet_num = 0;
									
								#define OV_OK 0
								
									int v_err = OV_OK;
									
									for(int h=0; h < 3 && v_err == OV_OK; h++)
									{
										size_t length = 0;
										const unsigned char *data = GetPrivateDataPart(private_data, private_size,
																						h, &length);
										
										if(data != NULL)
										{
											ogg_packet packet;
											
											packet.packet = (unsigned char *)data;
											packet.bytes = length;
											packet.b_o_s = (h == 0);
											packet.e_o_s = false;
											packet.granulepos = 0;
											packet.packetno = ogg_packet_num++;
											
											v_err = vorbis_synthesis_headerin(&vi, &vc, &packet);
											
											if(v_err == OV_OK)
												localRecP->vorbis_setup = true;
										}
									}
								}
							}
							else if(pTrack->GetCodecId() == std::string("A_OPUS") && localRecP->opus_dec == NULL)
							{
								// Opus specs found here:
								// http://wiki.xiph.org/MatroskaOpus
								
								const unsigned long long seekPreRoll = pAudioTrack->GetSeekPreRoll();
								const unsigned long long codecDelay = pAudioTrack->GetCodecDelay();
								
								assert(seekPreRoll == 80000000);

								size_t private_size = 0;
								const unsigned char *private_data = pAudioTrack->GetCodecPrivate(private_size);
								
								if(private_data && private_size >= 19 && !memcmp(private_data, "OpusHead", 8))
								{
									const unsigned char channels = private_data[9];
									
									const unsigned short pre_skip = private_data[10] | private_data[11] << 8;
									
									const unsigned int sample_rate = private_data[12] | private_data[13] << 8 |
																		private_data[14] << 16 | private_data[15] << 24;
									
									const unsigned short output_gain_u = private_data[16] | private_data[17] << 8;
									
									const short output_gain = (output_gain_u ^ 0x8000) - 0x8000;
									
									unsigned char stream_count = 1;
									unsigned char coupled_count = (channels > 1 ? 1 : 0);
									unsigned char mapping[] = {0, 1, 0, 1, 0, 1};
									
									const unsigned char mapping_family = private_data[18];
									
									if(mapping_family == 1)
									{
										assert(private_size == 21 + channels);
										
										stream_count = private_data[19];
										coupled_count = private_data[20];
										
										memcpy(mapping, &private_data[21], channels);
									}
									else
									{
										assert(mapping_family == 0);
										assert(channels <= 2);
									}
									
									assert(pAudioTrack->GetChannels() == channels);
									assert(pAudioTrack->GetSamplingRate() == sample_rate);
									assert(pAudioTrack->GetSamplingRate() == 48000);
									assert(sample_rate == 48000);
									assert(output_gain == 0);
									assert(codecDelay == (unsigned long long)pre_skip * 1000000000UL / sample_rate); // maybe should give myself some leeway here
									
									
									int err = -1;
									
									localRecP->opus_dec = opus_multistream_decoder_create(sample_rate, channels,
																							stream_count, coupled_count, mapping,
																							&err);
									
									assert(localRecP->opus_dec != NULL && err == OPUS_OK);
								}
							}
						}
					}
					
					
					// Making the SampleMap
					//
					// WebM does not provide a way to get the exact sample number for a packet of audio
					// because all time is done with timestamps, which are generally 1000 for every second.
					// This is not really that ideal for video because of rounding errors, but it's
					// REALLY imprecise for audio, which is something like 48000 samples per second.
					// If we want to do sample-accurate seeking, we have to scan all the audio in the file
					// sequentially and make a table to tell us which audio sample you'll actually get when
					// you seek to a particular cluster.
					assert(localRecP->sample_map == NULL);
					
					size_t buf_size = 1024 * 1024;
					uint8_t *read_buf = (uint8_t *)malloc(buf_size);
					
					if(read_buf != NULL)
					{
						localRecP->sample_map = new SampleMap;
						
					#ifdef CHECK_VORBIS_SAMPLE_ALGORITHM		
						vorbis_dsp_state vd;
						vorbis_block vb;
						
						if(localRecP->vorbis_setup)
						{
							int v_err = vorbis_synthesis_init(&vd, &localRecP->vi);
							
							if(v_err == OV_OK)
								v_err = vorbis_block_init(&vd, &vb);
						}
					#endif
						
						ogg_packet packet;
	
						packet.packet = NULL;
						packet.bytes = 0;
						packet.b_o_s = false;
						packet.e_o_s = false;
						packet.granulepos = -1;
						packet.packetno = 2;
						
						long last_blockSize = 0;
						
						
						long long sampleCount = 0;
						
						const mkvparser::Cluster *pCluster = localRecP->segment->GetFirst();
						
						while((pCluster != NULL) && !pCluster->EOS() && result == malNoError)
						{
							int cluster_packet_num = 0;
						
							const long long cluster_tstamp = pCluster->GetTime();
						
					
							const mkvparser::BlockEntry* pBlockEntry = NULL;
							
							pCluster->GetFirst(pBlockEntry);
							
							while((pBlockEntry != NULL) && !pBlockEntry->EOS() && result == malNoError)
							{
								const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
								
								if(pBlock->GetTrackNumber() == localRecP->audio_track)
								{
									for(int f=0; f < pBlock->GetFrameCount(); f++)
									{
										const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(f);
										
										if(buf_size < blockFrame.len)
										{
											assert(FALSE); // this shouldn't really happen
											
											read_buf = (uint8_t *)realloc(read_buf, blockFrame.len);
											
											buf_size = blockFrame.len;
										}
										
										long read_err = blockFrame.Read(localRecP->reader, read_buf);
										
										if(read_err == PrMkvReader::PrMkvSuccess)
										{
											if(localRecP->vorbis_setup)
											{
												// For Vorbis, the first packet you decode will not yield any samples.  You
												// need a 1-packet run-up.  So what we'll do is record the number of samples
												// at the second packet.  When we seek in the file, we always decode from cluster
												// boundaries.
												if(cluster_packet_num == 1)
												{
													// store the current sampleCount
													SampleMap &sample_map = *localRecP->sample_map;
													
													sample_map[cluster_tstamp] = sampleCount;
												}
												
												packet.packet = read_buf;
												packet.bytes = blockFrame.len;
												packet.packetno++;
												
												const long this_blockSize = vorbis_packet_blocksize(&localRecP->vi, &packet);
												
												const long num_samples = (last_blockSize / 4) + (this_blockSize / 4);
												
												last_blockSize = this_blockSize;
												
												
												// Turn this on if you want to make sure the calculation above is working
												// to count samples without actually decoding audio.  Note that the first
												// Vorbis packet still returns 0 samples, but the calculation should
												// work otherwise.
											#ifdef CHECK_VORBIS_SAMPLE_ALGORITHM	
												int actual_samples = 0;
												
												int synth_err = vorbis_synthesis(&vb, &packet);
												
												if(synth_err == OV_OK)
												{
													int block_err = vorbis_synthesis_blockin(&vd, &vb);
													
													if(block_err == OV_OK)
													{
														float **pcm = NULL;
														int samples = 0;
														
														int synth_result = 1;
														
														while(synth_result != 0 && (samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0)
														{
															actual_samples += samples;
															
															synth_result = vorbis_synthesis_read(&vd, samples);
														}
													}
												}
												
												if( !(cluster_tstamp == 0 && cluster_packet_num == 0) )
													assert(num_samples == actual_samples);
											#endif // !NDEBUG
											
												if( !(cluster_tstamp == 0 && cluster_packet_num == 0) )
													sampleCount += num_samples;
											}
											else if(localRecP->opus_dec != NULL)
											{
												// For Opus, we'll record the number of samples on the actual cluster boundary.
												// Opus uses SeekPreRoll to tell us we have to decode ahead of the audio we
												// actually want.  During the pre-roll period it is still returning samples to
												// us, they just aren't valid.
												if(cluster_packet_num == 0)
												{
													// store the current sampleCount
													SampleMap &sample_map = *localRecP->sample_map;
													
													sample_map[cluster_tstamp] = sampleCount;
												}
											
												sampleCount += opus_packet_get_nb_samples(read_buf, blockFrame.len, 48000);
											}
										}
										else
											result = imFileReadFailed;
									
										cluster_packet_num++;
									}
									
									
									const long long discardPatting = pBlock->GetDiscardPadding();
									
									if(discardPatting > 0)
									{
										sampleCount -= (discardPatting * 48000UL / 1000000000UL);
									}
								}
								
								long status = pCluster->GetNext(pBlockEntry, pBlockEntry);
								
								assert(status == 0);
							}
							
							pCluster = localRecP->segment->GetNext(pCluster);
						}
						
						localRecP->total_samples = sampleCount;
					
							
					#ifdef CHECK_VORBIS_SAMPLE_ALGORITHM	
						if(localRecP->vorbis_setup)
						{
							vorbis_block_clear(&vb);
							vorbis_dsp_clear(&vd);
						}
					#endif
						
						free(read_buf);
					}
				}
				
				if(localRecP->video_track == -1 && localRecP->audio_track == -1)
				{
					result = imFileHasNoImportableStreams;
				}
			}
		}
		else
			result = imBadHeader;
	}
	
	// close file and delete private data if we got a bad file
	if(result != malNoError)
	{
		if(SDKfileOpenRec8->privatedata)
		{
			stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(SDKfileOpenRec8->privatedata));
			SDKfileOpenRec8->privatedata = NULL;
		}
	}
	else
	{
		stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(SDKfileOpenRec8->privatedata));
	}

	return result;
}



static prMALError 
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	void				*privateData)
{
	// "Quiet File" really means close the file handle, but we're still
	// using it and might open it again, so hold on to any stored data
	// structures you don't want to re-create.

	// If file has not yet been closed
	if(SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

		if(localRecP->sample_map != NULL)
		{
			delete localRecP->sample_map;
			
			localRecP->sample_map = NULL;
		}

		if(localRecP->vpx_setup)
		{
			vpx_codec_err_t destroy_err = vpx_codec_destroy(&localRecP->vpx_decoder);
			assert(destroy_err == VPX_CODEC_OK);
			
			localRecP->vpx_setup = false;
		}
		
		if(localRecP->vorbis_setup)
		{
			vorbis_info &vi = localRecP->vi;
			vorbis_comment &vc = localRecP->vc;
			
			vorbis_info_clear(&vi);
			vorbis_comment_clear(&vc);
			
			localRecP->vorbis_setup = false;
		}
		
		if(localRecP->opus_dec != NULL)
		{
			opus_multistream_decoder_destroy(localRecP->opus_dec);
			
			localRecP->opus_dec = NULL;
		}

		if(localRecP->segment)
		{
			delete localRecP->segment;
			
			localRecP->segment = NULL;
		}
		
		if(localRecP->reader)
		{
			delete localRecP->reader;
			
			localRecP->reader = NULL;
		}

		stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	#ifdef PRWIN_ENV
		CloseHandle(*SDKfileRef);
	#else
		FSCloseFork( CAST_REFNUM(*SDKfileRef) );
	#endif
	
		*SDKfileRef = imInvalidHandleValue;
	}

	return malNoError; 
}


static prMALError 
SDKCloseFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef,
	void				*privateData) 
{
	ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);
	
	// If file has not yet been closed
	if(SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		SDKQuietFile(stdParms, SDKfileRef, privateData);
	}

	// Remove the privateData handle.
	// CLEANUP - Destroy the handle we created to avoid memory leaks
	if(ldataH && *ldataH && (*ldataH)->BasicSuite)
	{
		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCreatorSuite, kPrSDKPPixCreatorSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixCacheSuite, PrCacheVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		localRecP->BasicSuite->ReleaseSuite(kPrSDKImporterFileManagerSuite, kPrSDKImporterFileManagerSuiteVersion);

		stdParms->piSuites->memFuncs->disposeHandle(reinterpret_cast<PrMemoryHandle>(ldataH));
	}

	return malNoError;
}


static prMALError 
SDKGetIndPixelFormat(
	imStdParms			*stdParms,
	csSDK_size_t		idx,
	imIndPixelFormatRec	*SDKIndPixelFormatRec) 
{
	prMALError	result	= malNoError;
	//ImporterLocalRec8H	ldataH	= reinterpret_cast<ImporterLocalRec8H>(SDKIndPixelFormatRec->privatedata);

	switch(idx)
	{
		case 0:
			SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
			break;
	
		default:
			result = imBadFormatIndex;
			break;
	}

	return result;	
}


// TODO: Support imDataRateAnalysis and we'll get a pretty graph in the Properties panel!
// Sounds like a good task for someone who wants to contribute to this open source project.


static prMALError 
SDKAnalysis(
	imStdParms		*stdParms,
	imFileRef		SDKfileRef,
	imAnalysisRec	*SDKAnalysisRec)
{
	// Is this all I'm supposed to do here?
	// The string shows up in the properties dialog.
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKAnalysisRec->privatedata);
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

	std::stringstream stream;

	if(localRecP->video_track >= 0)
	{
		if(localRecP->video_codec == CODEC_VP8)
			stream << "VP8";
		else if(localRecP->video_codec == CODEC_VP9)
			stream << "VP9";
		else
			stream << "unknown";
		
		stream << " video, ";
	}
	
	if(localRecP->audio_track >= 0)
	{
		if(localRecP->audio_codec == CODEC_VORBIS)
			stream << "Vorbis";
		else if(localRecP->audio_codec == CODEC_OPUS)
			stream << "Opus";
		else
			stream << "unknown";
		
		stream << " audio ";
	}

	
	if(SDKAnalysisRec->buffersize > stream.str().size())
		strcpy(SDKAnalysisRec->buffer, stream.str().c_str());

	return malNoError;
}


static void
webm_guess_framerate(mkvparser::Segment *segment,
						int				video_track,
						unsigned int	*fps_den,
						unsigned int	*fps_num)
{
	// Quite a way to deduce the framerate.  Of course *we* are flagging
	// our WebM files with the appropriate frame rate, but many files
	// do not have it.  They just play sound and then pop frames on screen
	// at the right timestamp.  What a life.
	// But some of us have to work for a living, so we watch the
	// timestamps go by and make a judgement to tell our host.

	unsigned int frame = 0;
	uint64_t     tstamp = 0;

	const mkvparser::Cluster* pCluster = segment->GetFirst();
	const mkvparser::Tracks* pTracks = segment->GetTracks();

	long status = 0;

	while( (pCluster != NULL) && !pCluster->EOS() && status >= 0 && tstamp < 1000000000 && frame < 100)
	{
		const mkvparser::BlockEntry* pBlockEntry = NULL;
		
		status = pCluster->GetFirst(pBlockEntry);
		
		while( (pBlockEntry != NULL) && !pBlockEntry->EOS() && status >= 0 && tstamp < 1000000000 && frame < 100)
		{
			const mkvparser::Block* const pBlock  = pBlockEntry->GetBlock();
			const long long trackNum = pBlock->GetTrackNumber();
			
			if(trackNum == video_track)
			{
				const unsigned long tn = static_cast<unsigned long>(trackNum);
				const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(tn);
				
				if(pTrack)
				{
					assert(pTrack->GetType() == mkvparser::Track::kVideo);
					assert(pBlock->GetFrameCount() == 1);
					
					tstamp = pBlock->GetTime(pCluster);
					
					frame++;
				}
			}
			
			status = pCluster->GetNext(pBlockEntry, pBlockEntry);
		}
		
		pCluster = segment->GetNext(pCluster);
	}


	// known frame rates
	static const int frameRateNumDens[][2] = {	{10, 1}, {15, 1}, {24000, 1001},
												{24, 1}, {25, 1}, {30000, 1001},
												{30, 1}, {48000, 1001}, {48, 1},
												{50, 1}, {60000, 1001}, {60, 1}};

	double fps = (double)(frame - 1) * 1000000000.0 / (double)tstamp;

	int match_index = -1;
	double match_episilon = 999;

	for(int i=0; i < 12; i++)
	{
		double rate = (double)frameRateNumDens[i][0] / (double)frameRateNumDens[i][1];
		double episilon = fabs(fps - rate);

		if(episilon < match_episilon)
		{
			match_index = i;
			match_episilon = episilon;
		}
	}

	if(match_index >=0 && match_episilon < 0.01)
	{
		*fps_num = frameRateNumDens[match_index][0];
		*fps_den = frameRateNumDens[match_index][1];
	}
	else
	{
		*fps_num = (fps * 1000.0) + 0.5;
		*fps_den = 1000;
	}
}


prMALError 
SDKGetInfo8(
	imStdParms			*stdParms, 
	imFileAccessRec8	*fileAccessInfo8, 
	imFileInfoRec8		*SDKFileInfo8)
{
	prMALError					result				= malNoError;


	SDKFileInfo8->vidInfo.supportsAsyncIO			= kPrFalse;
	SDKFileInfo8->vidInfo.supportsGetSourceVideo	= kPrTrue;
	SDKFileInfo8->vidInfo.hasPulldown				= kPrFalse;
	SDKFileInfo8->hasDataRate						= kPrFalse;


	// private data
	assert(SDKFileInfo8->privatedata);
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKFileInfo8->privatedata);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	SDKFileInfo8->hasVideo = kPrFalse;
	SDKFileInfo8->hasAudio = kPrFalse;
	
	
	if(localRecP && localRecP->segment)
	{
		const mkvparser::SegmentInfo* const pSegmentInfo = localRecP->segment->GetInfo();
		
		const long long duration = pSegmentInfo->GetDuration();
		
		const mkvparser::Tracks* pTracks = localRecP->segment->GetTracks();
		
		if(localRecP->video_track >= 0)
		{
			const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->video_track);
			
			if(pTrack != NULL)
			{
				const long trackType = pTrack->GetType();
			
				if(trackType == mkvparser::Track::kVideo)
				{
					const mkvparser::VideoTrack* const pVideoTrack = static_cast<const mkvparser::VideoTrack*>(pTrack);
					
					if(pVideoTrack)
					{
						const double embedded_rate = pVideoTrack->GetFrameRate(); // never seems to contain anything
						
						unsigned int fps_num = 0;
						unsigned int fps_den = 0;
						
						webm_guess_framerate(localRecP->segment, localRecP->video_track, &fps_den, &fps_num);

						if(embedded_rate > 0)
						{
							if(duration < 1000000000UL)
							{
								fps_den = 1001;
								fps_num = embedded_rate * fps_den;
							}
							else						
								assert( fabs(embedded_rate - ((double)fps_num / (double)fps_den)) < 0.01 );
						}
						
						
						// Video information
						SDKFileInfo8->hasVideo				= kPrTrue;
						SDKFileInfo8->vidInfo.subType		= PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
						SDKFileInfo8->vidInfo.imageWidth	= pVideoTrack->GetWidth();
						SDKFileInfo8->vidInfo.imageHeight	= pVideoTrack->GetHeight();
						SDKFileInfo8->vidInfo.depth			= 24;	// for RGB, no A
						SDKFileInfo8->vidInfo.fieldType		= prFieldsUnknown; // Matroska talk about DefaultDecodedFieldDuration but...
						SDKFileInfo8->vidInfo.isStill		= kPrFalse;
						SDKFileInfo8->vidInfo.noDuration	= imNoDurationFalse;
						SDKFileInfo8->vidDuration			= duration * fps_num / 1000000000UL;
						SDKFileInfo8->vidScale				= fps_num;
						SDKFileInfo8->vidSampleSize			= fps_den;

						SDKFileInfo8->vidInfo.alphaType		= alphaNone;

						// Matroska defined a chunk called DisplayUnit, but libwebm doesn't support it
						// http://www.matroska.org/technical/specs/index.html#DisplayUnit
						// We'll just let Premiere guess what the pixel aspect ratio is
						//SDKFileInfo8->vidInfo.pixelAspectNum = 1;
						//SDKFileInfo8->vidInfo.pixelAspectDen = 1;

						// store some values we want to get without going to the file
						localRecP->width = SDKFileInfo8->vidInfo.imageWidth;
						localRecP->height = SDKFileInfo8->vidInfo.imageHeight;

						localRecP->frameRateNum = SDKFileInfo8->vidScale;
						localRecP->frameRateDen = SDKFileInfo8->vidSampleSize;
					}
				}
			}
		}
		
		if(localRecP->audio_track >= 0)
		{
			const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->audio_track);
			
			if(pTrack != NULL)
			{
				const long trackType = pTrack->GetType();
				
				if(trackType == mkvparser::Track::kAudio)
				{
					const mkvparser::AudioTrack* const pAudioTrack = static_cast<const mkvparser::AudioTrack*>(pTrack);
					
					if(pAudioTrack)
					{
						const long long bitDepth = pAudioTrack->GetBitDepth();
						
						
						// Audio information
						SDKFileInfo8->hasAudio				= kPrTrue;
						SDKFileInfo8->audInfo.numChannels	= pAudioTrack->GetChannels();
						SDKFileInfo8->audInfo.sampleRate	= pAudioTrack->GetSamplingRate();
						SDKFileInfo8->audInfo.sampleType	= bitDepth == 8 ? kPrAudioSampleType_8BitInt :
																bitDepth == 16 ? kPrAudioSampleType_16BitInt :
																bitDepth == 24 ? kPrAudioSampleType_24BitInt :
																bitDepth == 32 ? kPrAudioSampleType_32BitFloat :
																bitDepth == 64 ? kPrAudioSampleType_64BitFloat :
																kPrAudioSampleType_Compressed;
						
						const PrAudioSample calc_duration	= (uint64_t)SDKFileInfo8->audInfo.sampleRate * duration / 1000000000UL;
																																				
						SDKFileInfo8->audDuration			= (localRecP->total_samples > 0 ? localRecP->total_samples : calc_duration);
						
						
						if(SDKFileInfo8->audInfo.numChannels > 2 && SDKFileInfo8->audInfo.numChannels != 6)
						{
							// Premiere can only handle 1, 2, or 6 channels
							SDKFileInfo8->hasAudio = kPrFalse;
							SDKFileInfo8->audInfo.numChannels = 0;
						}
						
						localRecP->audioSampleRate			= SDKFileInfo8->audInfo.sampleRate;
						localRecP->numChannels				= SDKFileInfo8->audInfo.numChannels;
					}
				}
			}
		}
	}
		
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


static prMALError 
SDKPreferredFrameSize(
	imStdParms					*stdparms, 
	imPreferredFrameSizeRec		*preferredFrameSizeRec)
{
	prMALError			result	= imIterateFrameSizes;
	ImporterLocalRec8H	ldataH	= reinterpret_cast<ImporterLocalRec8H>(preferredFrameSizeRec->inPrivateData);

	stdparms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	// TODO: Make sure it really isn't possible to decode a smaller frame
	bool can_shrink = false;

	if(preferredFrameSizeRec->inIndex == 0)
	{
		preferredFrameSizeRec->outWidth = localRecP->width;
		preferredFrameSizeRec->outHeight = localRecP->height;
	}
	else
	{
		// we store width and height in private data so we can produce it here
		const int divisor = pow(2.0, preferredFrameSizeRec->inIndex);
		
		if(can_shrink &&
			preferredFrameSizeRec->inIndex < 4 &&
			localRecP->width % divisor == 0 &&
			localRecP->height % divisor == 0 )
		{
			preferredFrameSizeRec->outWidth = localRecP->width / divisor;
			preferredFrameSizeRec->outHeight = localRecP->height / divisor;
		}
		else
			result = malNoError;
	}


	stdparms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


static void
CopyImgToPix(const vpx_image_t * const img, PPixHand &ppix, PrSDKPPixSuite *PPixSuite, PrSDKPPix2Suite *PPix2Suite)
{
	PrPixelFormat pix_format;
	PPixSuite->GetPixelFormat(ppix, &pix_format);

	const unsigned int sub_x = img->x_chroma_shift + 1;
	const unsigned int sub_y = img->y_chroma_shift + 1;
	
	if(pix_format == PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601)
	{
		assert(sub_x == 2 && sub_y == 2);

		char *Y_PixelAddress, *U_PixelAddress, *V_PixelAddress;
		csSDK_uint32 Y_RowBytes, U_RowBytes, V_RowBytes;
		
		PPix2Suite->GetYUV420PlanarBuffers(ppix, PrPPixBufferAccess_ReadWrite,
														&Y_PixelAddress, &Y_RowBytes,
														&U_PixelAddress, &U_RowBytes,
														&V_PixelAddress, &V_RowBytes);
													
		for(int y = 0; y < img->d_h; y++)
		{
			unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
			
			unsigned char *prY = (unsigned char *)Y_PixelAddress + (Y_RowBytes * y);
			
			memcpy(prY, imgY, img->d_w * sizeof(unsigned char));
		}
		
		for(int y = 0; y < img->d_h / 2; y++)
		{
			unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y);
			unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y);
			
			unsigned char *prU = (unsigned char *)U_PixelAddress + (U_RowBytes * y);
			unsigned char *prV = (unsigned char *)V_PixelAddress + (V_RowBytes * y);
			
			memcpy(prU, imgU, (img->d_w / 2) * sizeof(unsigned char));
			memcpy(prV, imgV, (img->d_w / 2) * sizeof(unsigned char));
		}
	}
	else
	{
		char *frameBufferP = NULL;
		csSDK_int32 rowbytes = 0;
		
		PPixSuite->GetPixels(ppix, PrPPixBufferAccess_ReadWrite, &frameBufferP);
		PPixSuite->GetRowBytes(ppix, &rowbytes);
		
								
		if(pix_format == PrPixelFormat_UYVY_422_8u_601)
		{
			assert(sub_x == 2 && sub_y == 1);
			
			for(int y = 0; y < img->d_h; y++)
			{
				unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
				unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y));
				unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y));
				
				unsigned char *prUYVY = (unsigned char *)frameBufferP + (rowbytes * (img->d_h - 1 - y));
				
				for(int x=0; x < img->d_w; x++)
				{
					if(x % 2 == 0)
						*prUYVY++ = *imgU++;
					else
						*prUYVY++ = *imgV++;
					
					*prUYVY++ = *imgY++;
				}
			}
		}
		else if(pix_format == PrPixelFormat_VUYX_4444_8u)
		{
			assert(sub_x == 1 && sub_y == 1);
			
			for(int y = 0; y < img->d_h; y++)
			{
				unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
				unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y));
				unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y));
				
				unsigned char *prVUYX = (unsigned char *)frameBufferP + (rowbytes * (img->d_h - 1 - y));
				
				unsigned char *prV = prVUYX + 0;
				unsigned char *prU = prVUYX + 1;
				unsigned char *prY = prVUYX + 2;
				
				for(int x=0; x < img->d_w; x++)
				{
					*prY = *imgY++;
					*prU = *imgU++;
					*prV = *imgV++;
					
					prY += 4;
					prU += 4;
					prV += 4;
				}
			}
		}
		else
			assert(false);
	}
}

static prMALError 
SDKGetSourceVideo(
	imStdParms			*stdParms, 
	imFileRef			fileRef, 
	imSourceVideoRec	*sourceVideoRec)
{
	prMALError		result		= malNoError;
	csSDK_int32		theFrame	= 0;
	imFrameFormat	*frameFormat;

	// privateData
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(sourceVideoRec->inPrivateData);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	PrTime ticksPerSecond = 0;
	localRecP->TimeSuite->GetTicksPerSecond(&ticksPerSecond);
	

	if(localRecP->frameRateDen == 0) // i.e. still frame
	{
		theFrame = 0;
	}
	else
	{
		PrTime ticksPerFrame = (ticksPerSecond * (PrTime)localRecP->frameRateDen) / (PrTime)localRecP->frameRateNum;
		theFrame = sourceVideoRec->inFrameTime / ticksPerFrame;
	}

	// Check to see if frame is already in cache
	result = localRecP->PPixCacheSuite->GetFrameFromCache(	localRecP->importerID,
															0,
															theFrame,
															1,
															sourceVideoRec->inFrameFormats,
															sourceVideoRec->outFrame,
															NULL,
															NULL);

	// If frame is not in the cache, read the frame and put it in the cache; otherwise, we're done
	if(result != suiteError_NoError)
	{
		// ok, we'll read the file - clear error
		result = malNoError;
		
		// get the Premiere buffer
		frameFormat = &sourceVideoRec->inFrameFormats[0];
		prRect theRect;
		if(frameFormat->inFrameWidth == 0 && frameFormat->inFrameHeight == 0)
		{
			frameFormat->inFrameWidth = localRecP->width;
			frameFormat->inFrameHeight = localRecP->height;
		}
		
		// Windows and MacOS have different definitions of Rects, so use the cross-platform prSetRect
		prSetRect(&theRect, 0, 0, frameFormat->inFrameWidth, frameFormat->inFrameHeight);
		

		assert(localRecP->reader != NULL && localRecP->reader->FileRef() == fileRef);
		assert(localRecP->segment != NULL);
		
		if(localRecP->segment)
		{
			const uint64_t fps_num = localRecP->frameRateNum;
			const uint64_t fps_den = localRecP->frameRateDen;
			
			
			// convert PrTime to timeCode and then to absolute time
			// http://matroska.org/technical/specs/notes.html#TimecodeScale
			// Time (in nanoseconds) = TimeCode * TimeCodeScale.
			// This is the source of some very imprecise timings.  If the time scale is 1 million (as it is by default),
			// That means 1 second is time code of 1000.  For 24 frames per second, each frame would
			// be 41.6666 of time code, but that gets rounded off to 42.  If the TimeCode scale were lower,
			// we could be more precise, the problem presumably being that our biggest time would be
			// reduced.
			const long long timeCodeScale = localRecP->segment->GetInfo()->GetTimeCodeScale();
			const long long timeCode = ((sourceVideoRec->inFrameTime * (1000000000UL / timeCodeScale)) + (ticksPerSecond / 2)) / ticksPerSecond;
			const long long tstamp = timeCode * timeCodeScale;
			
			
			if(localRecP->video_track >= 0 && localRecP->vpx_setup)
			{
				vpx_codec_ctx_t &decoder = localRecP->vpx_decoder;
			
				const mkvparser::Tracks* pTracks = localRecP->segment->GetTracks();
			
				const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->video_track);
				
				if(pTrack != NULL && pTrack->GetType() == mkvparser::Track::kVideo)
				{
					const mkvparser::VideoTrack* const pVideoTrack = static_cast<const mkvparser::VideoTrack*>(pTrack);
					
					if(pVideoTrack)
					{
						assert(pVideoTrack->GetSeekPreRoll() == 0);
						assert(pVideoTrack->GetCodecDelay() == 0);
						
						const mkvparser::BlockEntry* pSeekBlockEntry = NULL;
						
						pVideoTrack->Seek(tstamp, pSeekBlockEntry);
						
						// TODO: Any way I can seek with cues instead of a binary search through clusters?
						//const mkvparser::Cues* cues = localRecP->segment->GetCues();

						if(pSeekBlockEntry != NULL)
						{
							const mkvparser::Cluster *pCluster = pSeekBlockEntry->GetCluster();

							// The seek took us to this Cluster for a reason.
							// It stars with a keyframe, although not necessarily the last keyframe before
							// the requested frame. I have to decode each frame starting with the keyframe,
							// and then I continue afterwards until I decode the entire cluster.
							// TODO: Maybe we should seek for keyframes within the cluster.

							assert(tstamp >= pSeekBlockEntry->GetBlock()->GetTime(pCluster));
							assert(tstamp >= pCluster->GetTime());

							bool got_frame = false;
							
							while((pCluster != NULL) && !pCluster->EOS() && !got_frame && result == malNoError)
							{
								assert(pCluster->GetTime() >= 0);
								assert(pCluster->GetTime() == pCluster->GetFirstTime());
								assert(got_frame || tstamp >= pCluster->GetTime());

								const mkvparser::BlockEntry* pBlockEntry = NULL;
								
								pCluster->GetFirst(pBlockEntry);

								while((pBlockEntry != NULL) && !pBlockEntry->EOS() && result == malNoError)
								{
									const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
								
									if(pBlock->GetTrackNumber() == localRecP->video_track)
									{
										assert(pBlock->GetFrameCount() == 1);
										assert(pBlock->GetDiscardPadding() == 0);
										
										long long packet_tstamp = pBlock->GetTime(pCluster);
										
										long long packet_tcode = pBlock->GetTimeCode(pCluster);
										long long tcode_scale = localRecP->segment->GetInfo()->GetTimeCodeScale();
										
										const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(0);
										
										unsigned int length = blockFrame.len;
										uint8_t *data = (uint8_t *)malloc(length);
										
										if(data != NULL)
										{
											//int read_err = localRecP->reader->Read(blockFrame.pos, blockFrame.len, data);
											long read_err = blockFrame.Read(localRecP->reader, data);
											
											if(read_err == PrMkvReader::PrMkvSuccess)
											{
												vpx_codec_err_t decode_err = vpx_codec_decode(&decoder, data, length, NULL, 0);
												
												assert(decode_err == VPX_CODEC_OK);

												if(decode_err == VPX_CODEC_OK)
												{
													csSDK_int32 decodedFrame = ((packet_tstamp * fps_num / fps_den) + 500000000UL) / 1000000000UL;
													
													vpx_codec_iter_t iter = NULL;
													
													vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);
													
													if(img)
													{
														const unsigned int sub_x = img->x_chroma_shift + 1;
														const unsigned int sub_y = img->y_chroma_shift + 1;
														
														assert(frameFormat->inPixelFormat == PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601);
														assert(frameFormat->inFrameHeight == img->d_h && frameFormat->inFrameWidth == img->d_w);
														assert(img->fmt == VPX_IMG_FMT_I420 || img->fmt == VPX_IMG_FMT_I422 || img->fmt == VPX_IMG_FMT_I444);

														
														// apparently pix_format doesn't have to match frameFormat->inPixelFormat
														const PrPixelFormat pix_format = img->fmt == VPX_IMG_FMT_I422 ? PrPixelFormat_UYVY_422_8u_601 :
																							img->fmt == VPX_IMG_FMT_I444 ? PrPixelFormat_VUYX_4444_8u :
																							PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
																							
														PPixHand ppix;
														
														localRecP->PPixCreatorSuite->CreatePPix(&ppix, PrPPixBufferAccess_ReadWrite, pix_format, &theRect);
														
														
														CopyImgToPix(img, ppix, localRecP->PPixSuite, localRecP->PPix2Suite);
														

														// Should never have another image waiting in the queue
														assert( NULL == (img = vpx_codec_get_frame(&decoder, &iter) ) );
														
														
														// This is a nice Premiere feature.  We often have to decode many frames
														// in a GOP (group of pictures) before we decode the one Premiere asked for.
														// This suite lets us cache those frames for later.  We keep going past the
														// requested frame to end of the cluster to save us the trouble in the future.
														localRecP->PPixCacheSuite->AddFrameToCache(	localRecP->importerID,
																									0,
																									ppix,
																									decodedFrame,
																									NULL,
																									NULL);
														
														if(decodedFrame == theFrame)
														{
															*sourceVideoRec->outFrame = ppix;
															
															got_frame = true;
														}
														else
														{
															// Premiere copied the frame to its cache, so we dispose ours.
															// Very obvious memory leak if we don't.
															localRecP->PPixSuite->Dispose(ppix);
														}
														
														vpx_img_free(img);
													}
													else
														assert(false); // this would mean that I passed the decoder a frame, but didn't get an image
												}
												else
													result = imFileReadFailed;
											}
											else
												result = imFileReadFailed;
											
											free(data);
										}
										else
											result = imMemErr;
									}
									
									long status = pCluster->GetNext(pBlockEntry, pBlockEntry);
									
									assert(status == 0);
								}
								
								//assert(got_frame); // otherwise our cluster seek function has failed us (turns out it will)
								
								pCluster = localRecP->segment->GetNext(pCluster);
							}

							assert(got_frame);
						}
					}
				}
			}
		}
	}


	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


												
template <typename T>
static inline T minimum(T one, T two)
{
	return (one < two ? one : two);
}


static prMALError 
SDKImportAudio7(
	imStdParms			*stdParms, 
	imFileRef			SDKfileRef, 
	imImportAudioRec7	*audioRec7)
{
	prMALError		result		= malNoError;

	// privateData
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(audioRec7->privateData);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	assert(localRecP->reader != NULL && localRecP->reader->FileRef() == SDKfileRef);
	assert(localRecP->segment != NULL);
	
	if(localRecP->segment)
	{
		assert(audioRec7->position >= 0); // Do they really want contiguous samples?

		// for surround channels
		// Premiere uses Left, Right, Left Rear, Right Rear, Center, LFE
		// Ogg (and Opus) uses Left, Center, Right, Left Read, Right Rear, LFE
		// http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
		static const int surround_swizzle[] = {0, 2, 3, 4, 1, 5};
		static const int stereo_swizzle[] = {0, 1, 2, 3, 4, 5}; // no swizzle, actually
		
		const int *swizzle = localRecP->numChannels > 2 ? surround_swizzle : stereo_swizzle;


		if(localRecP->audio_track >= 0)
		{
			const mkvparser::Tracks* pTracks = localRecP->segment->GetTracks();
		
			const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->audio_track);
			
			if(pTrack != NULL && pTrack->GetType() == mkvparser::Track::kAudio)
			{
				const mkvparser::AudioTrack* const pAudioTrack = static_cast<const mkvparser::AudioTrack*>(pTrack);
				
				if(pAudioTrack)
				{
					const unsigned long long seekPreRoll = pAudioTrack->GetSeekPreRoll();
					const unsigned long long codecDelay = pAudioTrack->GetCodecDelay();
					
					const PrAudioSample seekPreRoll_samples = seekPreRoll * localRecP->audioSampleRate / 1000000000UL;
					const PrAudioSample codecDelay_samples = codecDelay * localRecP->audioSampleRate / 1000000000UL;
					
					PrAudioSample seek_sample = audioRec7->position - seekPreRoll_samples + codecDelay_samples;
					
					if(seek_sample < 0)
						seek_sample = 0;
					
						
					const long long calc_tstamp = audioRec7->position * 1000000000UL / localRecP->audioSampleRate;
					
					
					// Use the SampleMap to figure out which cluster to seek to
					assert(localRecP->sample_map != NULL);
					
					long long lookup_tstamp = -1;
					
					if(localRecP->sample_map != NULL)
					{
						const SampleMap &sample_map = *localRecP->sample_map;
						
						SampleMap::const_iterator i = sample_map.begin();
						
						while(i != sample_map.end() && i->second <= seek_sample)
						{
							lookup_tstamp = i->first;
							
							i++;
						}
					}
					
					
					const long long tstamp = (lookup_tstamp > 0 ? lookup_tstamp : calc_tstamp);
					
					
					if(pTrack->GetCodecId() == std::string("A_VORBIS") && localRecP->vorbis_setup)
					{
						vorbis_info &vi = localRecP->vi;
						vorbis_comment &vc = localRecP->vc;
						
						vorbis_dsp_state vd;
						vorbis_block vb;
						
						int v_err = vorbis_synthesis_init(&vd, &vi);
						
						if(v_err == OV_OK)
							v_err = vorbis_block_init(&vd, &vb);
							
						
						const mkvparser::BlockEntry* pSeekBlockEntry = NULL;
						
						pAudioTrack->Seek(tstamp, pSeekBlockEntry);
						
						if(pSeekBlockEntry != NULL && v_err == OV_OK)
						{
							int ogg_packet_num = 3;
						
							csSDK_uint32 samples_copied = 0;
							csSDK_uint32 samples_left = audioRec7->size;
							
							const mkvparser::Cluster *pCluster = pSeekBlockEntry->GetCluster();
							
							PrAudioSample packet_start = 0;
							
							if(pCluster != NULL)
							{
								const long long cluster_tstamp = pCluster->GetTime();
							
								packet_start = localRecP->audioSampleRate * cluster_tstamp / 1000000000UL;
								
								if(localRecP->sample_map != NULL)
								{
									// For Vorbis, this will tell us the sample number of the fist
									// sample we'll actually get.  Because we just did a seek to here
									// and re-started the decoder, the first packet will yield nothing.
									// SampleMap has taken this into account.
									SampleMap &sample_map = *localRecP->sample_map;
									
									assert(sample_map.find(cluster_tstamp) != sample_map.end());
									
									packet_start = sample_map[cluster_tstamp];
								}
							}
							
							while((pCluster != NULL) && !pCluster->EOS() && samples_left > 0 && result == malNoError)
							{
								const mkvparser::BlockEntry* pBlockEntry = NULL;
								
								pCluster->GetFirst(pBlockEntry);
								
								while((pBlockEntry != NULL) && !pBlockEntry->EOS() && samples_left > 0 && result == malNoError)
								{
									const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
									
									if(pBlock->GetTrackNumber() == localRecP->audio_track)
									{
										assert(pBlock->GetDiscardPadding() == 0);

										const long long packet_tstamp = pBlock->GetTime(pCluster);
										
										PrAudioSample packet_offset = 0;
											
										if(audioRec7->position > packet_start)
											packet_offset = audioRec7->position - packet_start; // in other words the audio frames in the beginning that we'll skip over
										
											
										int position_in_packet = 0;
										
											
										for(int f=0; f < pBlock->GetFrameCount(); f++)
										{
											const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(f);
											
											unsigned int length = blockFrame.len;
											uint8_t *data = (uint8_t *)malloc(length);
											
											if(data != NULL)
											{
												//int read_err = localRecP->reader->Read(blockFrame.pos, blockFrame.len, data);
												long read_err = blockFrame.Read(localRecP->reader, data);
												
												if(read_err == PrMkvReader::PrMkvSuccess)
												{
													ogg_packet packet;
								
													packet.packet = data;
													packet.bytes = length;
													packet.b_o_s = false;
													packet.e_o_s = false;
													packet.granulepos = -1;
													packet.packetno = ogg_packet_num++;

													int synth_err = vorbis_synthesis(&vb, &packet);
													
													if(synth_err == OV_OK)
													{
														int block_err = vorbis_synthesis_blockin(&vd, &vb);
														
														if(block_err == OV_OK)
														{
															float **pcm = NULL;
															int samples = 0;
															
															int synth_result = 1;
															
															while(synth_result != 0 && (samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0)
															{
																// I feel like the code here is hard to read.  Sorry about that.
															
																int samples_to_copy = 0;
																
																// if the offset is bigger than the samples, we'll just move along
																if(packet_offset < samples)
																{
																	int samples_after_offset = samples - packet_offset;
																	
																	samples_to_copy = minimum<int>(samples_left, samples_after_offset);
																}
															
																// how nice, audio samples are float, just like Premiere wants 'em
																for(int c=0; c < localRecP->numChannels && samples_to_copy > 0; c++)
																{
																	memcpy(audioRec7->buffer[swizzle[c]] + samples_copied, pcm[c] + packet_offset, samples_to_copy * sizeof(float));
																}
																
																// now samples_to_copy is more like samples_I_just_copied
																samples_copied += samples_to_copy;
																samples_left -= samples_to_copy;
																
																
																// If we copied any samples, that means the offset has been gobbled up
																// If we didn't, reduce the offset by number of samples
																if(samples_to_copy > 0)
																{
																	packet_offset = 0;
																	
																	position_in_packet += samples_to_copy;
																}
																else
																	packet_offset -= samples;
																
																
																synth_result = vorbis_synthesis_read(&vd, samples);
																
																packet_start += samples;
															}
														}
														else
															result = imFileReadFailed;
													}
													else
														result = imFileReadFailed;
												}
												else
													result = imFileReadFailed;
												
												free(data);
											}
											else
												result = imMemErr;
										}
									}
									
									long status = pCluster->GetNext(pBlockEntry, pBlockEntry);
									
									assert(status == 0);
								}
								
								pCluster = localRecP->segment->GetNext(pCluster);
							}

							// there might not be samples left at the end; not much we can do about that
							assert(pCluster == NULL || pCluster->EOS() || (samples_left == 0 && samples_copied == audioRec7->size));
							
							vorbis_block_clear(&vb);
							vorbis_dsp_clear(&vd);
						}
						else
							result = imFileReadFailed;
					}
					else if(pTrack->GetCodecId() == std::string("A_OPUS") && localRecP->opus_dec != NULL)
					{
						int err = opus_multistream_decoder_ctl(localRecP->opus_dec, OPUS_RESET_STATE);
						
						assert(err == OPUS_OK);
					
						const mkvparser::BlockEntry* pSeekBlockEntry = NULL;
						
						pAudioTrack->Seek(tstamp, pSeekBlockEntry);
						
						if(pSeekBlockEntry != NULL)
						{
							const int opus_frame_size = 5760; // maximum Opus frame size at 48kHz
						
							float *interleaved_buffer = (float *)malloc(sizeof(float) * localRecP->numChannels * opus_frame_size);
							
							if(interleaved_buffer != NULL)
							{
								csSDK_uint32 samples_copied = 0;
								csSDK_uint32 samples_left = audioRec7->size;
								
								const mkvparser::Cluster *pCluster = pSeekBlockEntry->GetCluster();
								
								while((pCluster != NULL) && !pCluster->EOS() && samples_left > 0 && result == malNoError)
								{
									const long long cluster_tstamp = pCluster->GetTime();
									
									PrAudioSample cluster_start = localRecP->audioSampleRate * cluster_tstamp / 1000000000UL;
									
									// Get the actual sample number this cluster starts with from our pre-built table
									if(localRecP->sample_map != NULL)
									{
										SampleMap &sample_map = *localRecP->sample_map;
										
										assert(sample_map.find(cluster_tstamp) != sample_map.end());
										
										cluster_start = sample_map[cluster_tstamp];
									}
										
									PrAudioSample packet_start = cluster_start;
									
									
									const mkvparser::BlockEntry* pBlockEntry = NULL;
									
									pCluster->GetFirst(pBlockEntry);
									
									while((pBlockEntry != NULL) && !pBlockEntry->EOS() && samples_left > 0 && result == malNoError)
									{
										const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
										
										if(pBlock->GetTrackNumber() == localRecP->audio_track)
										{
											PrAudioSample packet_offset = 0;
												
											if(audioRec7->position > packet_start)
												packet_offset = audioRec7->position - packet_start; // in other words the audio frames in the beginning that we'll skip over
											
											for(int f=0; f < pBlock->GetFrameCount(); f++)
											{
												const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(f);
												
												unsigned int length = blockFrame.len;
												uint8_t *data = (uint8_t *)malloc(length);
												
												if(data != NULL)
												{
													//int read_err = localRecP->reader->Read(blockFrame.pos, blockFrame.len, data);
													long read_err = blockFrame.Read(localRecP->reader, data);
													
													if(read_err == PrMkvReader::PrMkvSuccess)
													{
														const int len = opus_multistream_decode_float(localRecP->opus_dec, data, length, interleaved_buffer, opus_frame_size, 0);
														
														int len_to_copy = len - packet_offset;
														
														if(len_to_copy > samples_left)
															len_to_copy = samples_left;
														else if(len_to_copy < 0)
															len_to_copy = 0;
														
														for(int i = 0; i < len_to_copy; i++)
														{
															for(int c=0; c < localRecP->numChannels; c++)
															{
																audioRec7->buffer[swizzle[c]][samples_copied + i] = interleaved_buffer[((i + packet_offset) * localRecP->numChannels) + c];
															}
														}
														
														if(packet_offset < len)
														{
															samples_copied += len_to_copy;
															samples_left -= len_to_copy;
															
															packet_offset = 0;
														}
														else
															packet_offset -= len;
														
														packet_start += len;
													}
													else
														result = imFileReadFailed;
													
													free(data);
												}
												else
													result = imMemErr;
											}
										}
										
										long status = pCluster->GetNext(pBlockEntry, pBlockEntry);
										
										assert(status == 0);
									}
									
									pCluster = localRecP->segment->GetNext(pCluster);
								}
								
								// there might not be samples left at the end; not much we can do about that
								assert(pCluster == NULL || pCluster->EOS() || (samples_left == 0 && samples_copied == audioRec7->size));
								
								free(interleaved_buffer);
							}
							else
								result = imMemErr;
						}
						else
							result = imFileReadFailed;
					}
				}
				else
					result = imFileReadFailed;
			}
			else
				result = imFileReadFailed;
		}
	}
	
					
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));
	
	assert(result == malNoError);
	
	return result;
}


PREMPLUGENTRY DllExport xImportEntry (
	csSDK_int32		selector, 
	imStdParms		*stdParms, 
	void			*param1, 
	void			*param2)
{
	prMALError	result				= imUnsupported;

	try{

	switch (selector)
	{
		case imInit:
			result =	SDKInit(stdParms, 
								reinterpret_cast<imImportInfoRec*>(param1));
			break;

		case imGetInfo8:
			result =	SDKGetInfo8(stdParms, 
									reinterpret_cast<imFileAccessRec8*>(param1), 
									reinterpret_cast<imFileInfoRec8*>(param2));
			break;

		case imOpenFile8:
			result =	SDKOpenFile8(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										reinterpret_cast<imFileOpenRec8*>(param2));
			break;
		
		case imQuietFile:
			result =	SDKQuietFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2); 
			break;

		case imCloseFile:
			result =	SDKCloseFile(	stdParms, 
										reinterpret_cast<imFileRef*>(param1), 
										param2);
			break;

		case imAnalysis:
			result =	SDKAnalysis(	stdParms,
										reinterpret_cast<imFileRef>(param1),
										reinterpret_cast<imAnalysisRec*>(param2));
			break;

		case imGetIndFormat:
			result =	SDKGetIndFormat(stdParms, 
										reinterpret_cast<csSDK_size_t>(param1),
										reinterpret_cast<imIndFormatRec*>(param2));
			break;

		case imGetIndPixelFormat:
			result = SDKGetIndPixelFormat(	stdParms,
											reinterpret_cast<csSDK_size_t>(param1),
											reinterpret_cast<imIndPixelFormatRec*>(param2));
			break;

		// Importers that support the Premiere Pro 2.0 API must return malSupports8 for this selector
		case imGetSupports8:
			result = malSupports8;
			break;

		case imGetPreferredFrameSize:
			result =	SDKPreferredFrameSize(	stdParms,
												reinterpret_cast<imPreferredFrameSizeRec*>(param1));
			break;

		case imGetSourceVideo:
			result =	SDKGetSourceVideo(	stdParms,
											reinterpret_cast<imFileRef>(param1),
											reinterpret_cast<imSourceVideoRec*>(param2));
			break;
			
		case imImportAudio7:
			result =	SDKImportAudio7(	stdParms,
											reinterpret_cast<imFileRef>(param1),
											reinterpret_cast<imImportAudioRec7*>(param2));
			break;

		case imCreateAsyncImporter:
			result =	imUnsupported;
			break;
	}
	
	}catch(...) { result = imOtherErr; }

	return result;
}

