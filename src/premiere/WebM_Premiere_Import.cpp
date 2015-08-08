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
#include <queue>

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
	csSDK_uint8				bit_depth;
	float					audioSampleRate;
	int						numChannels;
	
	PrMkvReader				*reader;
	mkvparser::Segment		*segment;
	int						video_track;
	VideoCodec				video_codec;
	long long				video_start_tstamp;
	vpx_img_fmt_t			img_fmt;
	vpx_color_space_t		color_space;
	int						audio_track;
	AudioCodec				audio_codec;
	long long				audio_start_tstamp;
	
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


// http://matroska.org/technical/specs/notes.html#TimecodeScale
// Time (in nanoseconds) = TimeCode * TimeCodeScale
// When we call finctions like GetTime, we're given Time in Nanoseconds.
static const long long S2NS = 1000000000LL;


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

			SDKIndFormatRec->flags = xfCanImport | xfIsMovie;

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
		localRecP->video_start_tstamp = -1;
		localRecP->audio_track = -1;
		localRecP->audio_codec = CODEC_ANONE;
		localRecP->audio_start_tstamp = -1;
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
										fsRdPerm,
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
				
				
				// Load Cue Points
				// For some reason the segment constuctor doesn't do this on its own
				const mkvparser::Cues* const cues = localRecP->segment->GetCues();
				
				if(cues != NULL)
				{
					assert(cues->GetFirst() == NULL);
					
					while( !cues->DoneParsing() )
						cues->LoadCuePoint();
					
					assert(cues->GetFirst() != NULL);
				}
				
				
				if(localRecP->video_track >= 0 && localRecP->vpx_setup == false)
				{
					const mkvparser::Track* const pTrack = pTracks->GetTrackByNumber(localRecP->video_track);
					
					if(pTrack != NULL && pTrack->GetType() == mkvparser::Track::kVideo)
					{
						const mkvparser::VideoTrack* const pVideoTrack = static_cast<const mkvparser::VideoTrack*>(pTrack);
						
						if(pVideoTrack)
						{
							const char* codec_id = pVideoTrack->GetCodecId();
						
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
								
								// TODO: Explore possibilities of decoding options by setting
								// VPX_CODEC_USE_POSTPROC here.  Things like VP8_DEMACROBLOCK and
								// VP8_MFQE (Multiframe Quality Enhancement) could be cool.
								
								const vpx_codec_flags_t flags = 0; //VPX_CODEC_USE_FRAME_THREADING;
								
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
							if(pAudioTrack->GetCodecId() == std::string("A_VORBIS") && localRecP->vorbis_setup == false)
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
								
								assert(seekPreRoll == 80000000); // 0.08 * S2NS

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
									assert(codecDelay == (unsigned long long)pre_skip * S2NS / sample_rate); // maybe should give myself some leeway here
									
									
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
											#endif
											
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
										assert(localRecP->opus_dec != NULL);
									
										sampleCount -= (discardPatting * 48000LL / S2NS);
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


static inline PrPixelFormat
vpx_to_premiere_pix_format(const vpx_img_fmt &fmt)
{
	return fmt == VPX_IMG_FMT_I422 ? PrPixelFormat_UYVY_422_8u_601 :
			fmt == VPX_IMG_FMT_I440 ? PrPixelFormat_VUYX_4444_8u :
			fmt == VPX_IMG_FMT_I444 ? PrPixelFormat_VUYX_4444_8u :
			fmt == VPX_IMG_FMT_I42016 ? PrPixelFormat_BGRA_4444_16u : // These are set to RGB
			fmt == VPX_IMG_FMT_I42216 ? PrPixelFormat_BGRA_4444_16u : // because Premiere seems
			fmt == VPX_IMG_FMT_I44016 ? PrPixelFormat_BGRA_4444_16u : // to have a bug with
			fmt == VPX_IMG_FMT_I44416 ? PrPixelFormat_BGRA_4444_16u : // VUYA_4444_16u
			PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
}

static prMALError 
SDKGetIndPixelFormat(
	imStdParms			*stdParms,
	csSDK_size_t		idx,
	imIndPixelFormatRec	*SDKIndPixelFormatRec) 
{
	prMALError	result	= malNoError;
	
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKIndPixelFormatRec->privatedata);
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	if(idx == 0)
	{
		SDKIndPixelFormatRec->outPixelFormat = vpx_to_premiere_pix_format(localRecP->img_fmt);
	}
	else if(idx == 1 && localRecP->bit_depth > 8)
	{
		// if main format is 16-bit, offer 8-bit alternative
		assert(localRecP->img_fmt & VPX_IMG_FMT_HIGHBITDEPTH);
	
		const PrPixelFormat pix_format8 = localRecP->img_fmt == VPX_IMG_FMT_I42216 ? PrPixelFormat_UYVY_422_8u_601 :
											localRecP->img_fmt == VPX_IMG_FMT_I44016 ? PrPixelFormat_VUYX_4444_8u :
											localRecP->img_fmt == VPX_IMG_FMT_I44416 ? PrPixelFormat_VUYX_4444_8u :
											PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
											
		SDKIndPixelFormatRec->outPixelFormat = pix_format8;
	}
	else
		result = imBadFormatIndex;
	

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
	assert(SDKAnalysisRec->privatedata);
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKAnalysisRec->privatedata);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

	std::stringstream stream;

	if(localRecP->video_track >= 0)
	{
		if(localRecP->video_codec == CODEC_VP8)
		{
			assert(localRecP->img_fmt == VPX_IMG_FMT_I420);
			assert(localRecP->bit_depth == 8);
		
			stream << "VP8";
		}
		else if(localRecP->video_codec == CODEC_VP9)
		{
			const std::string sampling = localRecP->img_fmt == VPX_IMG_FMT_I422 ? "4:2:2" :
											localRecP->img_fmt == VPX_IMG_FMT_I444 ? "4:4:4" :
											localRecP->img_fmt == VPX_IMG_FMT_I440 ? "4:4:0" :
											localRecP->img_fmt == VPX_IMG_FMT_I42216 ? "4:2:2" :
											localRecP->img_fmt == VPX_IMG_FMT_I44416 ? "4:4:4" :
											localRecP->img_fmt == VPX_IMG_FMT_I44016 ? "4:4:0" :
											"4:2:0";
									
			stream << "VP9 " << sampling << " " << (int)localRecP->bit_depth << "-bit";
			
		}
		else
			stream << "unknown";
		
		stream << " video";
		
		if(localRecP->color_space != VPX_CS_UNKNOWN)
		{
			const std::string color_space = localRecP->color_space == VPX_CS_BT_601 ? "Rec. 601" :
											localRecP->color_space == VPX_CS_BT_709 ? "Rec. 709" :
											localRecP->color_space == VPX_CS_SMPTE_170 ? "SMPTE 170" :
											localRecP->color_space == VPX_CS_SMPTE_240 ? "SMPTE 240" :
											localRecP->color_space == VPX_CS_BT_2020 ? "Rec. 2020" :
											localRecP->color_space == VPX_CS_RESERVED ? "Reserved" :
											localRecP->color_space == VPX_CS_SRGB ? "sRGB" :
											"Some Weird";
											
			stream << " (" << color_space << " color space)";
		}
		
		if(localRecP->audio_track >= 0)
			stream << ", ";
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
	
	
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return malNoError;
}


static void
webm_guess_framerate(mkvparser::Segment *segment,
						int				video_track,
						long long		start_tstamp,
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
	
	const mkvparser::Tracks* const pTracks = segment->GetTracks();
	const mkvparser::Cluster* pCluster = segment->GetFirst();
	
	long status = 0;

	while( (pCluster != NULL) && !pCluster->EOS() && status >= 0 && tstamp < (1 * S2NS) && frame < 100)
	{
		const mkvparser::BlockEntry* pBlockEntry = NULL;
		
		status = pCluster->GetFirst(pBlockEntry);
		
		while( (pBlockEntry != NULL) && !pBlockEntry->EOS() && status >= 0 && tstamp < (1 * S2NS) && frame < 100)
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
					
					tstamp = pBlock->GetTime(pCluster) - start_tstamp;
					
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

	const double fps = (double)(frame - 1) * (double)S2NS / (double)tstamp;

	int match_index = -1;
	double match_episilon = 999;

	for(int i=0; i < 12; i++)
	{
		const double rate = (double)frameRateNumDens[i][0] / (double)frameRateNumDens[i][1];
		const double episilon = fabs(fps - rate);

		if(episilon < match_episilon)
		{
			match_index = i;
			match_episilon = episilon;
		}
	}

	if(match_index >=0 && match_episilon < 0.1)
	{
		*fps_num = frameRateNumDens[match_index][0];
		*fps_den = frameRateNumDens[match_index][1];
	}
	else if(fps > 0.0)
	{
		*fps_num = (fps * 1000.0) + 0.5;
		*fps_den = 1000;
	}
	else
	{
		*fps_num = 24;
		*fps_den = 1;
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
		
		long long duration = pSegmentInfo->GetDuration();
		
		const long long timeCodeScale = pSegmentInfo->GetTimeCodeScale();
		
		assert(timeCodeScale == 1000000LL);
		
		bool duration_unknown = (duration <= (1 * timeCodeScale)); // the file was not properly "finalized"
		
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
						// all this because FFmpeg was shifting video timestamps over
						if(localRecP->video_start_tstamp < 0)
						{
							assert(localRecP->video_start_tstamp == -1);
							
							long long last_tstamp = 0;
							long long second_last_tstamp = 0;
						
							const mkvparser::Cluster* pCluster = localRecP->segment->GetFirst();
							
							while((pCluster != NULL) && !pCluster->EOS() && (localRecP->video_start_tstamp < 0 || duration_unknown))
							{
								const mkvparser::BlockEntry* pBlockEntry = NULL;
								
								pCluster->GetFirst(pBlockEntry);
								
								while((pBlockEntry != NULL) && !pBlockEntry->EOS() && (localRecP->video_start_tstamp < 0 || duration_unknown))
								{
									const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
									
									second_last_tstamp = last_tstamp;
									last_tstamp = pBlock->GetTime(pCluster);
									
									if(pBlock->GetTrackNumber() == localRecP->video_track && localRecP->video_start_tstamp < 0)
									{
										localRecP->video_start_tstamp = pBlock->GetTime(pCluster);
										
										if(localRecP->video_codec == CODEC_VP9)
										{
											vpx_codec_ctx_t &decoder = localRecP->vpx_decoder;
											
											// decode the first frame to get bit depth and sampling information
											const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(0);
											
											const unsigned int length = blockFrame.len;
											uint8_t *data = (uint8_t *)malloc(length);
											
											if(data != NULL)
											{
												const long read_err = blockFrame.Read(localRecP->reader, data);
												
												if(read_err == PrMkvReader::PrMkvSuccess)
												{
													const vpx_codec_err_t decode_err = vpx_codec_decode(&decoder, data, length, NULL, 0);
													
													if(decode_err == VPX_CODEC_OK)
													{
														vpx_codec_decode(&decoder, NULL, 0, NULL, 0); // flush the decoder
														
														vpx_codec_iter_t iter = NULL;
														
														vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);
														
														if(img)
														{
															localRecP->bit_depth = img->bit_depth;
															localRecP->img_fmt = img->fmt;
															localRecP->color_space = img->cs;
														
															vpx_img_free(img);
														}
														else
															assert(false);
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
										else
										{
											localRecP->bit_depth = 8;
											localRecP->img_fmt = VPX_IMG_FMT_I420;
											localRecP->color_space = VPX_CS_UNKNOWN;
										}
									}
									
									pCluster->GetNext(pBlockEntry, pBlockEntry);
								}
								
								pCluster = localRecP->segment->GetNext(pCluster);
							}
							
							if(duration_unknown)
							{
								duration = last_tstamp + (last_tstamp - second_last_tstamp);
								
								if(duration > 0)
								{
									duration_unknown = false;
								}
								else
									duration = 1 * timeCodeScale;
							}
						}
						
						assert(localRecP->video_start_tstamp == 0); // unfortunately, it sometimes isn't
						
					
						const double embedded_rate = pVideoTrack->GetFrameRate();
						
						unsigned int fps_num = 0;
						unsigned int fps_den = 0;
						
						webm_guess_framerate(localRecP->segment, localRecP->video_track, localRecP->video_start_tstamp, &fps_den, &fps_num);

						if(embedded_rate > 0)
						{
							const long long oneSecond = 1 * S2NS;
						
							if(duration < oneSecond)
							{
								fps_den = 1001;
								fps_num = embedded_rate * fps_den;
							}
							else						
								assert( fabs(embedded_rate - ((double)fps_num / (double)fps_den)) < 0.01 );
						}
						
						
						
						// Video information
						SDKFileInfo8->hasVideo				= kPrTrue;
						SDKFileInfo8->vidInfo.subType		= vpx_to_premiere_pix_format(localRecP->img_fmt);
						SDKFileInfo8->vidInfo.imageWidth	= pVideoTrack->GetWidth();
						SDKFileInfo8->vidInfo.imageHeight	= pVideoTrack->GetHeight();
						SDKFileInfo8->vidInfo.depth			= localRecP->bit_depth * 3;	// for RGB, no A
						SDKFileInfo8->vidInfo.fieldType		= prFieldsUnknown; // Matroska talks about DefaultDecodedFieldDuration but...
						SDKFileInfo8->vidInfo.isStill		= kPrFalse;
						SDKFileInfo8->vidInfo.noDuration	= imNoDurationFalse;
						SDKFileInfo8->vidDuration			= duration * fps_num / S2NS;
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
			if(localRecP->audio_start_tstamp < 0)
			{
				assert(localRecP->audio_start_tstamp == -1);
				
				long long last_tstamp = 0;
				long long second_last_tstamp = 0;
				
				const mkvparser::Cluster* pCluster = localRecP->segment->GetFirst();
				
				while((pCluster != NULL) && !pCluster->EOS() && (localRecP->audio_start_tstamp < 0 || duration_unknown))
				{
					const mkvparser::BlockEntry* pBlockEntry = NULL;
					
					pCluster->GetFirst(pBlockEntry);
					
					while((pBlockEntry != NULL) && !pBlockEntry->EOS() && (localRecP->audio_start_tstamp < 0 || duration_unknown))
					{
						const mkvparser::Block *pBlock = pBlockEntry->GetBlock();
						
						second_last_tstamp = last_tstamp;
						last_tstamp = pBlock->GetTime(pCluster);
						
						if(pBlock->GetTrackNumber() == localRecP->audio_track && localRecP->audio_start_tstamp < 0)
						{
							localRecP->audio_start_tstamp = pBlock->GetTime(pCluster);
						}
						
						pCluster->GetNext(pBlockEntry, pBlockEntry);
					}
					
					pCluster = localRecP->segment->GetNext(pCluster);
				}
				
				if(duration_unknown)
				{
					duration = last_tstamp + (last_tstamp - second_last_tstamp);
					
					if(duration > 0)
					{
						duration_unknown = false;
					}
					else
						duration = 1 * timeCodeScale;
				}
			}
			
			assert(localRecP->audio_start_tstamp == 0); // unfortunately, it sometimes isn't
			
		
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
						
						const PrAudioSample calc_duration	= (uint64_t)SDKFileInfo8->audInfo.sampleRate * duration / S2NS;
																																				
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
		
		assert(!duration_unknown);
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


// Convert regular 16-bit to Adobe 16-bit, max val 0x8000
#define PF_MAX_CHAN16			32768

static inline unsigned short
Demote(const unsigned short &val)
{
	return (val > PF_MAX_CHAN16 ? ( (val - 1) >> 1 ) + 1 : val >> 1);
}

static inline unsigned short
Clamp16(const int &val)
{
	return (val < 0 ? 0 : val > PF_MAX_CHAN16 ? PF_MAX_CHAN16 : val);
}


template <typename IMG_PIX, typename VUYA_PIX>
static inline VUYA_PIX
ConvertDepth(const IMG_PIX &val, const int &depth);

template<>
static inline unsigned short
ConvertDepth<unsigned short, unsigned short>(const unsigned short &val, const int &depth)
{
	return Demote((val << (16 - depth)) | (val >> ((depth * 2) - 16)));
}

template<>
static inline unsigned char
ConvertDepth<unsigned short, unsigned char>(const unsigned short &val, const int &depth)
{
	return (val >> (depth - 8)); 
}

template<>
static inline unsigned char
ConvertDepth<unsigned char, unsigned char>(const unsigned char &val, const int &depth)
{
	assert(depth == 8);
	return val; 
}


template <typename IMG_PIX, typename VUYA_PIX>
static void
CopyImgToVUYA(const vpx_image_t * const img, char *frameBufferP, csSDK_int32 rowbytes)
{
	const unsigned int sub_x = img->x_chroma_shift + 1;
	const unsigned int sub_y = img->y_chroma_shift + 1;
	
	assert(sub_y == 1);
	
	for(int y = 0; y < img->d_h; y++)
	{
		const IMG_PIX *imgY = (IMG_PIX *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
		const IMG_PIX *imgU = (IMG_PIX *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y)));
		const IMG_PIX *imgV = (IMG_PIX *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y)));
		
		VUYA_PIX *prVUYX = (VUYA_PIX *)(frameBufferP + (rowbytes * (img->d_h - 1 - y)));
		
		VUYA_PIX *prV = prVUYX + 0;
		VUYA_PIX *prU = prVUYX + 1;
		VUYA_PIX *prY = prVUYX + 2;
		VUYA_PIX *prA = prVUYX + 3;
		
		for(int x=0; x < img->d_w; x++)
		{
			*prY = ConvertDepth<IMG_PIX, VUYA_PIX>(*imgY++, img->bit_depth);
			
			if(x != 0 && (x % sub_x == 0))
			{
				imgU++;
				imgV++;
			}
			
			*prU = ConvertDepth<IMG_PIX, VUYA_PIX>(*imgU, img->bit_depth);
			*prV = ConvertDepth<IMG_PIX, VUYA_PIX>(*imgV, img->bit_depth);
			*prA = ConvertDepth<unsigned short, VUYA_PIX>(255, 8);
			
			prY += 4;
			prU += 4;
			prV += 4;
			prA += 4;
		}
	}
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
													
		if(img->bit_depth == 8)
		{
			for(int y = 0; y < img->d_h; y++)
			{
				const unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
				
				unsigned char *prY = (unsigned char *)Y_PixelAddress + (Y_RowBytes * y);
				
				memcpy(prY, imgY, img->d_w * sizeof(unsigned char));
			}
		}
		else
		{
			for(int y = 0; y < img->d_h; y++)
			{
				const unsigned short *imgY = (unsigned short *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
				
				unsigned char *prY = (unsigned char *)Y_PixelAddress + (Y_RowBytes * y);
				
				for(int x=0; x < img->d_w; x++)
				{
					*prY++ = ConvertDepth<unsigned short, unsigned char>(*imgY++, img->bit_depth);
				}
			}
		}
		
		const int chroma_width = (img->d_w / 2) + (img->d_w % 2);
		const int chroma_height = (img->d_h / 2) + (img->d_h % 2);
		
		if(img->bit_depth == 8)
		{
			for(int y = 0; y < chroma_height; y++)
			{
				const unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y);
				const unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y);
				
				unsigned char *prU = (unsigned char *)U_PixelAddress + (U_RowBytes * y);
				unsigned char *prV = (unsigned char *)V_PixelAddress + (V_RowBytes * y);
				
				memcpy(prU, imgU, chroma_width * sizeof(unsigned char));
				memcpy(prV, imgV, chroma_width * sizeof(unsigned char));
			}
		}
		else
		{
			for(int y = 0; y < chroma_height; y++)
			{
				const unsigned short *imgU = (unsigned short *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * y));
				const unsigned short *imgV = (unsigned short *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * y));
				
				unsigned char *prU = (unsigned char *)U_PixelAddress + (U_RowBytes * y);
				unsigned char *prV = (unsigned char *)V_PixelAddress + (V_RowBytes * y);
				
				for(int x=0; x < chroma_width; x++)
				{
					*prU++ = ConvertDepth<unsigned short, unsigned char>(*imgU++, img->bit_depth);
					*prV++ = ConvertDepth<unsigned short, unsigned char>(*imgV++, img->bit_depth);
				}
			}
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
			
			if(img->bit_depth == 8)
			{
				for(int y = 0; y < img->d_h; y++)
				{
					const unsigned char *imgY = img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y);
					const unsigned char *imgU = img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y));
					const unsigned char *imgV = img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y));
					
					unsigned char *prUYVY = (unsigned char *)frameBufferP + (rowbytes * y);
					
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
			else
			{
				for(int y = 0; y < img->d_h; y++)
				{
					const unsigned short *imgY = (unsigned short *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
					const unsigned short *imgU = (unsigned short *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y)));
					const unsigned short *imgV = (unsigned short *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y)));
					
					unsigned char *prUYVY = (unsigned char *)frameBufferP + (rowbytes * y);
					
					for(int x=0; x < img->d_w; x++)
					{
						if(x % 2 == 0)
							*prUYVY++ = ConvertDepth<unsigned short, unsigned char>(*imgU++, img->bit_depth);
						else
							*prUYVY++ = ConvertDepth<unsigned short, unsigned char>(*imgV++, img->bit_depth);
						
						*prUYVY++ = ConvertDepth<unsigned short, unsigned char>(*imgY++, img->bit_depth);
					}
				}
			}
		}
		else if(pix_format == PrPixelFormat_VUYX_4444_8u)
		{
			assert(sub_x == 1 && sub_y == 1);
			
			if(img->bit_depth > 8)
			{
				CopyImgToVUYA<unsigned short, unsigned char>(img, frameBufferP, rowbytes);
			}
			else
			{
				assert(img->bit_depth == 8);
				
				CopyImgToVUYA<unsigned char, unsigned char>(img, frameBufferP, rowbytes);
			}
		}
		else if(pix_format == PrPixelFormat_VUYA_4444_16u)
		{
			assert(img->bit_depth > 8);
			
			CopyImgToVUYA<unsigned short, unsigned short>(img, frameBufferP, rowbytes);
		}
		else if(pix_format == PrPixelFormat_BGRA_4444_16u)
		{
			// This is only necessary because of a bug with VUYA_4444_16u
			assert(img->bit_depth > 8);
		
			for(int y = 0; y < img->d_h; y++)
			{
				const unsigned short *imgY = (unsigned short *)(img->planes[VPX_PLANE_Y] + (img->stride[VPX_PLANE_Y] * y));
				const unsigned short *imgU = (unsigned short *)(img->planes[VPX_PLANE_U] + (img->stride[VPX_PLANE_U] * (y / sub_y)));
				const unsigned short *imgV = (unsigned short *)(img->planes[VPX_PLANE_V] + (img->stride[VPX_PLANE_V] * (y / sub_y)));
				
				unsigned short *prBGRA = (unsigned short *)(frameBufferP + (rowbytes * (img->d_h - 1 - y)));
				
				unsigned short *prB = prBGRA + 0;
				unsigned short *prG = prBGRA + 1;
				unsigned short *prR = prBGRA + 2;
				unsigned short *prA = prBGRA + 3;
				
				for(int x=0; x < img->d_w; x++)
				{
					const int prY = ConvertDepth<unsigned short, unsigned short>(*imgY++, img->bit_depth);
					
					if(x != 0 && (x % sub_x == 0))
					{
						imgU++;
						imgV++;
					}
					
					const int prU = ConvertDepth<unsigned short, unsigned short>(*imgU, img->bit_depth);
					const int prV = ConvertDepth<unsigned short, unsigned short>(*imgV, img->bit_depth);
					
					const int subY = ConvertDepth<unsigned short, unsigned short>(16, 8);
					const int subUV = ConvertDepth<unsigned short, unsigned short>(128, 8);
					
					*prB = Clamp16( ((1164 * (prY - subY)) + (2018 * (prU - subUV)) + 500) / 1000 );
					*prG = Clamp16( ((1164 * (prY - subY)) - (813 * (prV - subUV)) - (391 * (prU - subUV)) + 500) / 1000 );
					*prR = Clamp16( ((1164 * (prY - subY)) + (1596 * (prV - subUV)) + 500) / 1000 );
					*prA = PF_MAX_CHAN16;
					
					prB += 4;
					prG += 4;
					prR += 4;
					prA += 4;
				}
			}
		}
		else
			assert(false);
	}
}

static bool
store_vpx_img(
	const vpx_image_t *img,
	ImporterLocalRec8Ptr localRecP,
	imSourceVideoRec *sourceVideoRec,
	const long long decoded_tstamp)
{
	PrTime ticksPerSecond = 0;
	localRecP->TimeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	csSDK_int32 theFrame = 0;
	
	if(localRecP->frameRateDen == 0) // i.e. still frame
	{
		theFrame = 0;
	}
	else
	{
		PrTime ticksPerFrame = (ticksPerSecond * (PrTime)localRecP->frameRateDen) / (PrTime)localRecP->frameRateNum;
		theFrame = sourceVideoRec->inFrameTime / ticksPerFrame;
	}
	
	imFrameFormat *frameFormat = &sourceVideoRec->inFrameFormats[0];
	
	prRect theRect;
	if(frameFormat->inFrameWidth == 0 && frameFormat->inFrameHeight == 0)
	{
		frameFormat->inFrameWidth = localRecP->width;
		frameFormat->inFrameHeight = localRecP->height;
	}
	
	// Windows and MacOS have different definitions of Rects, so use the cross-platform prSetRect
	prSetRect(&theRect, 0, 0, frameFormat->inFrameWidth, frameFormat->inFrameHeight);
	
	assert(frameFormat->inFrameHeight == img->d_h && frameFormat->inFrameWidth == img->d_w);
	
	vpx_img_fmt img_fmt = img->fmt;
	
	// maybe Premiere doesn't want 16-bit right now
	if(img_fmt & VPX_IMG_FMT_HIGHBITDEPTH)
	{
		if(frameFormat->inPixelFormat != PrPixelFormat_BGRA_4444_16u) // the only 16-bit format we currently support
		{
			img_fmt = (img_fmt == VPX_IMG_FMT_I42216 ? VPX_IMG_FMT_I422 :
						img_fmt == VPX_IMG_FMT_I44016 ? VPX_IMG_FMT_I440 :
						img_fmt == VPX_IMG_FMT_I44416 ? VPX_IMG_FMT_I444 :
						VPX_IMG_FMT_I420);
		}
	}

	
	// apparently pix_format doesn't have to match frameFormat->inPixelFormat
	const PrPixelFormat pix_format = vpx_to_premiere_pix_format(img_fmt);
										
	PPixHand ppix;
	
	localRecP->PPixCreatorSuite->CreatePPix(&ppix, PrPPixBufferAccess_ReadWrite, pix_format, &theRect);
	
	
	CopyImgToPix(img, ppix, localRecP->PPixSuite, localRecP->PPix2Suite);
	

	
	const uint64_t fps_num = localRecP->frameRateNum;
	const uint64_t fps_den = localRecP->frameRateDen;
	
	const csSDK_int32 decodedFrame = (((decoded_tstamp - localRecP->video_start_tstamp) * fps_num / fps_den) + (S2NS / 2)) / S2NS;
	
	const bool requested_frame = (decodedFrame == theFrame);
	
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
	
	if(requested_frame)
	{
		*sourceVideoRec->outFrame = ppix;
	}
	else
	{
		// Premiere copied the frame to its cache, so we dispose ours.
		// Very obvious memory leak if we don't.
		localRecP->PPixSuite->Dispose(ppix);
	}
	
	
	return requested_frame;
}

static prMALError 
SDKGetSourceVideo(
	imStdParms			*stdParms, 
	imFileRef			fileRef, 
	imSourceVideoRec	*sourceVideoRec)
{
	prMALError		result		= malNoError;

	// privateData
	ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(sourceVideoRec->inPrivateData);
	stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));
	ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );


	PrTime ticksPerSecond = 0;
	localRecP->TimeSuite->GetTicksPerSecond(&ticksPerSecond);

	csSDK_int32		theFrame	= 0;
	
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
		
		assert(localRecP->reader != NULL && localRecP->reader->FileRef() == fileRef);
		assert(localRecP->segment != NULL);
		
		if(localRecP->segment && localRecP->video_track >= 0 && localRecP->vpx_setup)
		{
			// convert PrTime to timeCode and then to absolute time
			// http://matroska.org/technical/specs/notes.html#TimecodeScale
			// Time (in nanoseconds) = TimeCode * TimeCodeScale
			// This is the source of some very imprecise timings.  If the time scale is 1 million (as it is by default),
			// That means 1 second is time code of 1000.  For 24 frames per second, each frame would
			// be 41.6666 of time code, but that gets rounded off to 42.  If the TimeCode scale were lower,
			// we could be more precise, the problem presumably being that our biggest time would be
			// reduced.
			const long long timeCodeScale = localRecP->segment->GetInfo()->GetTimeCodeScale();
			const long long timeCode = ((sourceVideoRec->inFrameTime * (S2NS / timeCodeScale)) + (ticksPerSecond / 2)) / ticksPerSecond;
			const long long tstamp = (timeCode * timeCodeScale) + localRecP->video_start_tstamp;
			
			
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
					
					
					// If the file has Cues, we'll use them to seek.
					// Expect to have one cue for each keyframe, and each keyframe
					// should begin a new cluster.  This will not always be the case, though.
					const mkvparser::Cues* const cues = localRecP->segment->GetCues();
					
					if(cues != NULL)
					{
						assert(cues->GetFirst() != NULL);
					
						const mkvparser::CuePoint* cue = NULL;
						const mkvparser::CuePoint::TrackPosition *pTrackPos = NULL;
						
						const bool seek_success = cues->Find(tstamp, pVideoTrack, cue, pTrackPos);
						
						if(seek_success && cue != NULL && pTrackPos != NULL)
						{
							pSeekBlockEntry = cues->GetBlock(cue, pTrackPos);
						}
					}
					
					
					// A more brute foce seek method that doesn't use cues
					if(pSeekBlockEntry == NULL)
						pVideoTrack->Seek(tstamp, pSeekBlockEntry);
					
					
					if(pSeekBlockEntry != NULL)
					{
						const mkvparser::Cluster *pCluster = pSeekBlockEntry->GetCluster();

						// The seek took us to this Cluster for a reason.
						// It should start with a keyframe, although not necessarily the last keyframe before
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
							
							std::queue<long long> tstamp_queue;

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
									
									const mkvparser::Block::Frame& blockFrame = pBlock->GetFrame(0);
									
									const unsigned int length = blockFrame.len;
									uint8_t *data = (uint8_t *)malloc(length);
									
									if(data != NULL)
									{
										//int read_err = localRecP->reader->Read(blockFrame.pos, blockFrame.len, data);
										const long read_err = blockFrame.Read(localRecP->reader, data);
										
										if(read_err == PrMkvReader::PrMkvSuccess)
										{
											const vpx_codec_err_t decode_err = vpx_codec_decode(&decoder, data, length, NULL, 0);
											
											assert(decode_err == VPX_CODEC_OK);

											if(decode_err == VPX_CODEC_OK)
											{
												tstamp_queue.push(packet_tstamp);
												
												vpx_codec_iter_t iter = NULL;
												
												while(vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter))
												{
													const bool requested_frame = store_vpx_img(img,
																								localRecP,
																								sourceVideoRec,
																								tstamp_queue.front());
													tstamp_queue.pop();
													
													if(requested_frame)
														got_frame = true;
													
													vpx_img_free(img);
												}
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
							
							// clear out any more frames left over from the multithreaded decode
							if(result == malNoError)
							{
								const vpx_codec_err_t decode_err = vpx_codec_decode(&decoder, NULL, 0, NULL, 0);
								
								assert(decode_err == VPX_CODEC_OK);
								
								vpx_codec_iter_t iter = NULL;
								
								while(vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter))
								{
									const bool requested_frame = store_vpx_img(img,
																				localRecP,
																				sourceVideoRec,
																				tstamp_queue.front());
									tstamp_queue.pop();
									
									if(requested_frame)
										got_frame = true;
									
									vpx_img_free(img);
								}
								
								assert(tstamp_queue.size() == 0);
							}
							
							//assert(got_frame); // otherwise our cluster seek function has failed us (turns out it will)
							
							pCluster = localRecP->segment->GetNext(pCluster);
						}

						assert(got_frame);
					}
				}
			}
		}
		else
			result = malUnknownError;
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
					
					const PrAudioSample seekPreRoll_samples = seekPreRoll * localRecP->audioSampleRate / S2NS;
					const PrAudioSample codecDelay_samples = codecDelay * localRecP->audioSampleRate / S2NS;
					
					PrAudioSample seek_sample = audioRec7->position - seekPreRoll_samples + codecDelay_samples;
					
					if(seek_sample < 0)
						seek_sample = 0;
					
						
					const long long calc_tstamp = (audioRec7->position * S2NS / localRecP->audioSampleRate) + localRecP->audio_start_tstamp;
					
					
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
					else
						assert(false);
					
					
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
							
								packet_start = localRecP->audioSampleRate * cluster_tstamp / S2NS;
								
								if(localRecP->sample_map != NULL)
								{
									// For Vorbis, this will tell us the sample number of the fist
									// sample we'll actually get.  Because we just did a seek to here
									// and re-started the decoder, the first packet will yield nothing.
									// SampleMap has taken this into account.
									SampleMap &sample_map = *localRecP->sample_map;
									
									if(sample_map.find(cluster_tstamp) != sample_map.end());
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
																	memcpy(audioRec7->buffer[c] + samples_copied, pcm[swizzle[c]] + packet_offset, samples_to_copy * sizeof(float));
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
									
									PrAudioSample cluster_start = localRecP->audioSampleRate * cluster_tstamp / S2NS;
									
									// Get the actual sample number this cluster starts with from our pre-built table
									if(localRecP->sample_map != NULL)
									{
										SampleMap &sample_map = *localRecP->sample_map;
										
										if(sample_map.find(cluster_tstamp) != sample_map.end());
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
														
														int len_to_copy = len - (int)packet_offset;
														
														if(len_to_copy < 0)
															len_to_copy = 0;
														else if(len_to_copy > samples_left)
															len_to_copy = samples_left;
														
														for(int i = 0; i < len_to_copy; i++)
														{
															for(int c=0; c < localRecP->numChannels; c++)
															{
																audioRec7->buffer[c][samples_copied + i] = interleaved_buffer[((i + packet_offset) * localRecP->numChannels) + swizzle[c]];
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

