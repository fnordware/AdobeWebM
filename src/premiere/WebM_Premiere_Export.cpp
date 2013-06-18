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

#include "vpx_config.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "libmkv/EbmlWriter.h"
#include "libmkv/EbmlIDs.h"

}


// this stuff basically just copied from vpxenc.c
typedef off_t EbmlLoc;

struct cue_entry {
  unsigned int time;
  uint64_t     loc;
};

struct EbmlGlobal {

	PrSDKExportFileSuite *fileSuite;
	csSDK_uint32 fileObject;
	
	int debug;

	//FILE    *stream;
	int64_t last_pts_ms;
	vpx_rational_t  framerate;

	/* These pointers are to the start of an element */
	off_t    position_reference;
	off_t    seek_info_pos;
	off_t    segment_info_pos;
	off_t    track_pos;
	off_t    cue_pos;
	off_t    cluster_pos;

	/* This pointer is to a specific element to be serialized */
	off_t    track_id_pos;

	/* These pointers are to the size field of the element */
	EbmlLoc  startSegment;
	EbmlLoc  startCluster;

	uint32_t cluster_timecode;
	int      cluster_open;

	struct cue_entry *cue_list;
	unsigned int      cues;

};


void Ebml_Write(EbmlGlobal *glob, const void *buffer_in, unsigned long len)
{
	glob->fileSuite->Write(glob->fileObject, (void *)buffer_in, len);
}


static EbmlLoc Ebml_Tell(EbmlGlobal *glob)
{
	prInt64 pos = 0;
	
	prSuiteError err = glob->fileSuite->Seek(glob->fileObject, 0, pos, fileSeekMode_Current);
	
	return pos;
}


static int Ebml_Seek(EbmlGlobal *glob, EbmlLoc offset, int whence)
{
	ExFileSuite_SeekMode mode = whence == SEEK_SET ? fileSeekMode_Begin :
								whence == SEEK_CUR ? fileSeekMode_Current :
								whence == SEEK_END ? fileSeekMode_End :
								fileSeekMode_Begin;
	
	prInt64 new_pos = 0;
	
	prSuiteError err = glob->fileSuite->Seek(glob->fileObject, offset, new_pos, mode);

	return (err == suiteError_NoError) ? 0 : -1;
}



#define WRITE_BUFFER(s) \
  for(i = len-1; i>=0; i--)\
  { \
    x = (char)(*(const s *)buffer_in >> (i * CHAR_BIT)); \
    Ebml_Write(glob, &x, 1); \
  }
void Ebml_Serialize(EbmlGlobal *glob, const void *buffer_in, int buffer_size, unsigned long len) {
  char x;
  int i;

  /* buffer_size:
   * 1 - int8_t;
   * 2 - int16_t;
   * 3 - int32_t;
   * 4 - int64_t;
   */
  switch (buffer_size) {
    case 1:
      WRITE_BUFFER(int8_t)
      break;
    case 2:
      WRITE_BUFFER(int16_t)
      break;
    case 4:
      WRITE_BUFFER(int32_t)
      break;
    case 8:
      WRITE_BUFFER(int64_t)
      break;
    default:
      break;
  }
}
#undef WRITE_BUFFER


/* Need a fixed size serializer for the track ID. libmkv provides a 64 bit
 * one, but not a 32 bit one.
 */
static void Ebml_SerializeUnsigned32(EbmlGlobal *glob, unsigned long class_id, uint64_t ui) {
  unsigned char sizeSerialized = 4 | 0x80;
  Ebml_WriteID(glob, class_id);
  Ebml_Serialize(glob, &sizeSerialized, sizeof(sizeSerialized), 1);
  Ebml_Serialize(glob, &ui, sizeof(ui), 4);
}

#define LITERALU64(hi,lo) ((((uint64_t)hi)<<32)|lo)

static void
Ebml_StartSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc,
                     unsigned long class_id) {
  /* todo this is always taking 8 bytes, this may need later optimization */
  /* this is a key that says length unknown */
  uint64_t unknownLen = LITERALU64(0x01FFFFFF, 0xFFFFFFFF);

  Ebml_WriteID(glob, class_id);
  *ebmlLoc = Ebml_Tell(glob);
  Ebml_Serialize(glob, &unknownLen, sizeof(unknownLen), 8);
}

static void
Ebml_EndSubElement(EbmlGlobal *glob, EbmlLoc *ebmlLoc) {
  off_t pos;
  uint64_t size;

  /* Save the current stream pointer */
  pos = Ebml_Tell(glob);

  /* Calculate the size of this element */
  size = pos - *ebmlLoc - 8;
  size |= LITERALU64(0x01000000, 0x00000000);

  /* Seek back to the beginning of the element and write the new size */
  Ebml_Seek(glob, *ebmlLoc, SEEK_SET);
  Ebml_Serialize(glob, &size, sizeof(size), 8);

  /* Reset the stream pointer */
  Ebml_Seek(glob, pos, SEEK_SET);
}


static void
write_webm_seek_element(EbmlGlobal *ebml, unsigned long id, off_t pos) {
  uint64_t offset = pos - ebml->position_reference;
  EbmlLoc start;
  Ebml_StartSubElement(ebml, &start, Seek);
  Ebml_SerializeBinary(ebml, SeekID, id);
  Ebml_SerializeUnsigned64(ebml, SeekPosition, offset);
  Ebml_EndSubElement(ebml, &start);
}


static void
write_webm_seek_info(EbmlGlobal *ebml) {

  off_t pos;

  /* Save the current stream pointer */
  pos = Ebml_Tell(ebml);

  if (ebml->seek_info_pos)
    Ebml_Seek(ebml, ebml->seek_info_pos, SEEK_SET);
  else
    ebml->seek_info_pos = pos;

  {
    EbmlLoc start;

    Ebml_StartSubElement(ebml, &start, SeekHead);
    write_webm_seek_element(ebml, Tracks, ebml->track_pos);
    write_webm_seek_element(ebml, Cues,   ebml->cue_pos);
    write_webm_seek_element(ebml, Info,   ebml->segment_info_pos);
    Ebml_EndSubElement(ebml, &start);
  }
  {
    /* segment info */
    EbmlLoc startInfo;
    uint64_t frame_time;
    char version_string[64];

    /* Assemble version string */
    if (ebml->debug)
      strcpy(version_string, "vpxenc");
    else {
      strcpy(version_string, "vpxenc ");
      strncat(version_string,
              vpx_codec_version_str(),
              sizeof(version_string) - 1 - strlen(version_string));
    }

    frame_time = (uint64_t)1000 * ebml->framerate.den
                 / ebml->framerate.num;
    ebml->segment_info_pos = Ebml_Tell(ebml);
    Ebml_StartSubElement(ebml, &startInfo, Info);
    Ebml_SerializeUnsigned(ebml, TimecodeScale, 1000000);
    Ebml_SerializeFloat(ebml, Segment_Duration,
                        (double)(ebml->last_pts_ms + frame_time));
    Ebml_SerializeString(ebml, 0x4D80, version_string);
    Ebml_SerializeString(ebml, 0x5741, version_string);
    Ebml_EndSubElement(ebml, &startInfo);
  }
}


typedef enum stereo_format {
  STEREO_FORMAT_MONO       = 0,
  STEREO_FORMAT_LEFT_RIGHT = 1,
  STEREO_FORMAT_BOTTOM_TOP = 2,
  STEREO_FORMAT_TOP_BOTTOM = 3,
  STEREO_FORMAT_RIGHT_LEFT = 11
} stereo_format_t;

#define VP8_FOURCC (0x30385056)
#define VP9_FOURCC (0x30395056)

static void
write_webm_file_header(EbmlGlobal                *glob,
                       const vpx_codec_enc_cfg_t *cfg,
                       const struct vpx_rational *fps,
                       stereo_format_t            stereo_fmt,
                       unsigned int               fourcc) {
  {
    EbmlLoc start;
    Ebml_StartSubElement(glob, &start, EBML);
    Ebml_SerializeUnsigned(glob, EBMLVersion, 1);
    Ebml_SerializeUnsigned(glob, EBMLReadVersion, 1);
    Ebml_SerializeUnsigned(glob, EBMLMaxIDLength, 4);
    Ebml_SerializeUnsigned(glob, EBMLMaxSizeLength, 8);
    Ebml_SerializeString(glob, DocType, "webm");
    Ebml_SerializeUnsigned(glob, DocTypeVersion, 2);
    Ebml_SerializeUnsigned(glob, DocTypeReadVersion, 2);
    Ebml_EndSubElement(glob, &start);
  }
  {
    Ebml_StartSubElement(glob, &glob->startSegment, Segment);
    glob->position_reference = Ebml_Tell(glob);
    glob->framerate = *fps;
    write_webm_seek_info(glob);

    {
      EbmlLoc trackStart;
      glob->track_pos = Ebml_Tell(glob);
      Ebml_StartSubElement(glob, &trackStart, Tracks);
      {
        unsigned int trackNumber = 1;
        uint64_t     trackID = 0;

        EbmlLoc start;
        Ebml_StartSubElement(glob, &start, TrackEntry);
        Ebml_SerializeUnsigned(glob, TrackNumber, trackNumber);
        glob->track_id_pos = Ebml_Tell(glob);
        Ebml_SerializeUnsigned32(glob, TrackUID, trackID);
        Ebml_SerializeUnsigned(glob, TrackType, 1);
        Ebml_SerializeString(glob, CodecID,
                             fourcc == VP8_FOURCC ? "V_VP8" : "V_VP9");
        {
          unsigned int pixelWidth = cfg->g_w;
          unsigned int pixelHeight = cfg->g_h;
          float        frameRate   = (float)fps->num / (float)fps->den;

          EbmlLoc videoStart;
          Ebml_StartSubElement(glob, &videoStart, Video);
          Ebml_SerializeUnsigned(glob, PixelWidth, pixelWidth);
          Ebml_SerializeUnsigned(glob, PixelHeight, pixelHeight);
          Ebml_SerializeUnsigned(glob, StereoMode, stereo_fmt);
          Ebml_SerializeFloat(glob, FrameRate, frameRate);
          Ebml_EndSubElement(glob, &videoStart);
        }
        Ebml_EndSubElement(glob, &start); /* Track Entry */
      }
      Ebml_EndSubElement(glob, &trackStart);
    }
    /* segment element is open */
  }
}


static void
write_webm_block(EbmlGlobal                *glob,
                 const vpx_codec_enc_cfg_t *cfg,
                 const vpx_codec_cx_pkt_t  *pkt) {
  unsigned long  block_length;
  unsigned char  track_number;
  unsigned short block_timecode = 0;
  unsigned char  flags;
  int64_t        pts_ms;
  int            start_cluster = 0, is_keyframe;

  /* Calculate the PTS of this frame in milliseconds */
  pts_ms = pkt->data.frame.pts * 1000
           * (uint64_t)cfg->g_timebase.num / (uint64_t)cfg->g_timebase.den;
  if (pts_ms <= glob->last_pts_ms)
    pts_ms = glob->last_pts_ms + 1;
  glob->last_pts_ms = pts_ms;

  /* Calculate the relative time of this block */
  if (pts_ms - glob->cluster_timecode > SHRT_MAX)
    start_cluster = 1;
  else
    block_timecode = (unsigned short)pts_ms - glob->cluster_timecode;

  is_keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY);
  if (start_cluster || is_keyframe) {
    if (glob->cluster_open)
      Ebml_EndSubElement(glob, &glob->startCluster);

    /* Open the new cluster */
    block_timecode = 0;
    glob->cluster_open = 1;
    glob->cluster_timecode = (uint32_t)pts_ms;
    glob->cluster_pos = Ebml_Tell(glob);
    Ebml_StartSubElement(glob, &glob->startCluster, Cluster); /* cluster */
    Ebml_SerializeUnsigned(glob, Timecode, glob->cluster_timecode);

    /* Save a cue point if this is a keyframe. */
    if (is_keyframe) {
       struct cue_entry *cue, *new_cue_list;

      new_cue_list = (struct cue_entry *)realloc((void *)glob->cue_list,
                             (glob->cues + 1) * sizeof(struct cue_entry));
      if (new_cue_list)
        glob->cue_list = new_cue_list;
      else
        throw -1; // because we can't call exit()

      cue = &glob->cue_list[glob->cues];
      cue->time = glob->cluster_timecode;
      cue->loc = glob->cluster_pos;
      glob->cues++;
    }
  }

  /* Write the Simple Block */
  Ebml_WriteID(glob, SimpleBlock);

  block_length = (unsigned long)pkt->data.frame.sz + 4;
  block_length |= 0x10000000;
  Ebml_Serialize(glob, &block_length, sizeof(block_length), 4);

  track_number = 1;
  track_number |= 0x80;
  Ebml_Write(glob, &track_number, 1);

  Ebml_Serialize(glob, &block_timecode, sizeof(block_timecode), 2);

  flags = 0;
  if (is_keyframe)
    flags |= 0x80;
  if (pkt->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
    flags |= 0x08;
  Ebml_Write(glob, &flags, 1);

  Ebml_Write(glob, pkt->data.frame.buf, (unsigned long)pkt->data.frame.sz);
}


static void
write_webm_file_footer(EbmlGlobal *glob, long hash) {

  if (glob->cluster_open)
    Ebml_EndSubElement(glob, &glob->startCluster);

  {
    EbmlLoc start;
    unsigned int i;

    glob->cue_pos = Ebml_Tell(glob);
    Ebml_StartSubElement(glob, &start, Cues);
    for (i = 0; i < glob->cues; i++) {
      struct cue_entry *cue = &glob->cue_list[i];
      EbmlLoc start;

      Ebml_StartSubElement(glob, &start, CuePoint);
      {
        EbmlLoc start;

        Ebml_SerializeUnsigned(glob, CueTime, cue->time);

        Ebml_StartSubElement(glob, &start, CueTrackPositions);
        Ebml_SerializeUnsigned(glob, CueTrack, 1);
        Ebml_SerializeUnsigned64(glob, CueClusterPosition,
                                 cue->loc - glob->position_reference);
        Ebml_EndSubElement(glob, &start);
      }
      Ebml_EndSubElement(glob, &start);
    }
    Ebml_EndSubElement(glob, &start);
  }

  Ebml_EndSubElement(glob, &glob->startSegment);

  /* Patch up the seek info block */
  write_webm_seek_info(glob);

  /* Patch up the track id */
  Ebml_Seek(glob, glob->track_id_pos, SEEK_SET);
  Ebml_SerializeUnsigned32(glob, TrackUID, glob->debug ? 0xDEADBEEF : hash);

  Ebml_Seek(glob, 0, SEEK_END);
}


/* Murmur hash derived from public domain reference implementation at
 *   http:// sites.google.com/site/murmurhash/
 */
static unsigned int murmur(const void *key, int len, unsigned int seed) {
  const unsigned int m = 0x5bd1e995;
  const int r = 24;

  unsigned int h = seed ^ len;

  const unsigned char *data = (const unsigned char *)key;

  while (len >= 4) {
    unsigned int k;

    k  = data[0];
    k |= data[1] << 8;
    k |= data[2] << 16;
    k |= data[3] << 24;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  switch (len) {
    case 3:
      h ^= data[2] << 16;
    case 2:
      h ^= data[1] << 8;
    case 1:
      h ^= data[0];
      h *= m;
  };

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}

#include "math.h"
#define MAX_PSNR 100
static double vp8_mse2psnr(double Samples, double Peak, double Mse) {
  double psnr;

  if ((double)Mse > 0.0)
    psnr = 10.0 * log10(Peak * Peak * Samples / Mse);
  else
    psnr = MAX_PSNR;      /* Limit to prevent / 0 */

  if (psnr > MAX_PSNR)
    psnr = MAX_PSNR;

  return psnr;
}

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
	infoRecP->canExportAudio	= kPrFalse;
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
	paramSuite->GetParamValue(exID, gIdx, ADBEVideoAlpha, &alphaP);
	
	
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
	
	
	result = mySettings->exportFileSuite->Open(exportInfoP->fileObject);
	
	EbmlGlobal ebml;
	memset(&ebml, 0, sizeof(ebml));
	
	try{
	
	if(result == malNoError)
	{
		ebml.fileSuite = mySettings->exportFileSuite;
		ebml.fileObject = exportInfoP->fileObject;
		ebml.debug = false;
		ebml.last_pts_ms = -1;
		
	
		vpx_codec_iface_t *iface = vpx_codec_vp8_cx();
		
		vpx_codec_enc_cfg_t config;
		vpx_codec_enc_config_default(iface, &config, 0);
		
		config.g_w = renderParms.inWidth;
		config.g_h = renderParms.inHeight;
		
		config.g_pass = VPX_RC_ONE_PASS;
		config.g_threads = 8;
		
		exRatioValue fps;
		get_framerate(ticksPerSecond, frameRateP.value.timeValue, &fps);
		
		stereo_format_t stereo_fmt = STEREO_FORMAT_MONO;
		
		vpx_rational vpx_fps;
		vpx_fps.num = fps.numerator * 1000;
		vpx_fps.den = fps.denominator * 1000;
		
		config.g_timebase.num = 1;
		config.g_timebase.den = vpx_fps.den;
		
		
		write_webm_file_header(&ebml, &config, &vpx_fps, stereo_fmt, VP8_FOURCC);
		
		
		long hash = 0;
		
		
		vpx_codec_ctx_t encoder;
		
		vpx_codec_err_t codec_err = vpx_codec_enc_init(&encoder, iface, &config, 0);
		
		if(codec_err == VPX_CODEC_OK)
		{
			SequenceRender_GetFrameReturnRec renderResult;
			
			PrTime videoTime = exportInfoP->startTime;
			
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
								hash = murmur(pkt->data.frame.buf, pkt->data.frame.sz, hash);
							
								write_webm_block(&ebml, &config, pkt);
							}
						}
					}
					else
						result = exportReturn_InternalError;
					
					vpx_img_free(img);
					
					pixSuite->Dispose(renderResult.outFrame);
					
					
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
			
			write_webm_file_footer(&ebml, hash);
		}
		else
			result = exportReturn_InternalError;
			
	}
	
	}catch(...) { result = exportReturn_InternalError; }
	
	mySettings->exportFileSuite->Close(exportInfoP->fileObject);
	
	if(ebml.cue_list)
		free(ebml.cue_list);
	
	renderSuite->ReleaseVideoRenderer(exID, videoRenderID);


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
	PrParam widthP, heightP, parN, parD, fieldTypeP, frameRateP;
	
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoWidth, &widthP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoHeight, &heightP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectNumerator, &parN);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_PixelAspectDenominator, &parD);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFieldType, &fieldTypeP);
	exportInfoSuite->GetExportSourceInfo(exID, kExportInfo_VideoFrameRate, &frameRateP);
	
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
