
#include "WebM_Premiere_Import.h"


extern "C" {

//#define VPX_DONT_DEFINE_STDINT_TYPES
//#define VPX_CODEC_DISABLE_COMPAT 1
//#include "vpx_config.h"
#include "vpx/vpx_decoder.h"
//#include "vpx_ports/vpx_timer.h"
//#if CONFIG_VP8_DECODER || CONFIG_VP9_DECODER
#include "vpx/vp8dx.h"
//#endif
//#if CONFIG_MD5
//#include "md5_utils.h"
//#endif
//#include "tools_common.h"
#include "nestegg/include/nestegg/nestegg.h"
//#define INT_TYPES_DEFINED
//#include "third_party/libyuv/include/libyuv/scale.h"

#include <vorbis/codec.h>

}

#include <assert.h>


#if IMPORTMOD_VERSION <= IMPORTMOD_VERSION_9
typedef PrSDKPPixCacheSuite2 PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion2
#else
typedef PrSDKPPixCacheSuite PrCacheSuite;
#define PrCacheVersion	kPrSDKPPixCacheSuiteVersion
#endif



#define NESTEGG_ERR_NONE 0
#define NESTEGG_SUCCESS  1
#define NESTEGG_ERR      (-1)

static int
nestegg_read(void *buffer, size_t length, void *refcon)
{
	imFileRef fileRef = reinterpret_cast<imFileRef>( refcon );
	
#ifdef PRWIN_ENV
	DWORD count = length, out = 0;
	
	BOOL result = ReadFile(fileRef, (LPVOID)buffer, count, &out, NULL);

	return (result && length == out) ? NESTEGG_SUCCESS : NESTEGG_ERR;
#else
	ByteCount count = length, out = 0;
	
	OSErr result = FSReadFork(CAST_REFNUM(fileRef), fsAtMark, 0, count, buffer, &out);

	return (result == noErr && length == out) ? NESTEGG_SUCCESS : (result == eofErr ? 0 : NESTEGG_ERR);
#endif
}


static int
nestegg_seek(int64_t offset, int whence, void *refcon)
{
	imFileRef fileRef = reinterpret_cast<imFileRef>( refcon );
	
#ifdef PRWIN_ENV
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = offset;

	BOOL result = SetFilePointerEx(fileRef, lpos, &out, whence);

	return (result ? NESTEGG_ERR_NONE : NESTEGG_ERR);
#else
	UInt16 positionMode =	whence == NESTEGG_SEEK_SET ? fsFromStart :
							whence == NESTEGG_SEEK_CUR ? (offset == 0 ? fsAtMark : fsFromMark) :
							whence == NESTEGG_SEEK_END ? fsFromLEOF :
							fsFromStart;
	
	OSErr result = FSSetForkPosition(CAST_REFNUM(fileRef), positionMode, offset);
	
	return (result == noErr ? NESTEGG_ERR_NONE : NESTEGG_ERR);
#endif
}


static int64_t
nestegg_tell(void *refcon)
{
	imFileRef fileRef = reinterpret_cast<imFileRef>( refcon );
	
#ifdef PRWIN_ENV
	LARGE_INTEGER lpos, out;

	lpos.QuadPart = 0;

	BOOL result = SetFilePointerEx(fileRef, lpos, &out, FILE_CURRENT);

	return out.QuadPart;
#else
	SInt64 lpos;
	
	OSErr result = FSGetForkPosition(CAST_REFNUM(fileRef), &lpos);
	
	return lpos;
#endif
}


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
	
	nestegg_io				io;
	nestegg					*nestegg_ctx;
	int						video_track;
	int						audio_track;
	int						video_codec_id;
	int						audio_codec_id;
	unsigned int			time_mult;
	
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
	PrSDKAppInfoSuite *appInfoSuite = NULL;
	stdParms->piSuites->utilFuncs->getSPBasicSuite()->AcquireSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion, (const void**)&appInfoSuite);
	
	if(appInfoSuite)
	{
		int fourCC = 0;
	
		appInfoSuite->GetAppInfo(PrSDKAppInfoSuite::kAppInfo_AppFourCC, (void *)&fourCC);
	
		stdParms->piSuites->utilFuncs->getSPBasicSuite()->ReleaseSuite(kPrSDKAppInfoSuite, kPrSDKAppInfoSuiteVersion);
		
		if(fourCC == kAppAfterEffects)
			return imOtherErr;
	}
	
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	importInfo->canSave				= kPrFalse;		// Can 'save as' files to disk, real file only
	
	importInfo->canDelete			= kPrFalse;		// File importers only, use if you only if you have child files
	importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	importInfo->hasSetup			= kPrFalse;		// Set to kPrTrue if you have a setup dialog
	importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	importInfo->priority			= 0;
	importInfo->canTrim				= kPrFalse;
	importInfo->canCalcSizes		= kPrFalse;
	if(stdParms->imInterfaceVer >= IMPORTMOD_VERSION_6)
	{
		importInfo->avoidAudioConform = kPrTrue;
	}							

	return malNoError;
}

static prMALError 
SDKGetIndFormat(
	imStdParms		*stdParms, 
	csSDK_size_t	index, 
	imIndFormatRec	*SDKIndFormatRec)
{
	prMALError	result		= malNoError;
	char formatname[255]	= "WebM Format";
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


prMALError 
SDKOpenFile8(
	imStdParms		*stdParms, 
	imFileRef		*SDKfileRef, 
	imFileOpenRec8	*SDKfileOpenRec8)
{
	prMALError			result = malNoError;

	ImporterLocalRec8H	localRecH = NULL;
	ImporterLocalRec8Ptr localRecP = NULL;

	// Private data stores:
	// 1. Pointers to suites
	// 2. Width, height, and timing information
	// 3. File path

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
		
		localRecP->io.read = nestegg_read;
		localRecP->io.seek = nestegg_seek;
		localRecP->io.tell = nestegg_tell;
		localRecP->video_track = -1;
		localRecP->audio_track = -1;
		localRecP->time_mult = 1;
		localRecP->nestegg_ctx = NULL;
		
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


	localRecP->io.userdata = SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = reinterpret_cast<imFileRef>(imInvalidHandleValue);


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
			localRecP->io.userdata = localRecP->egg.fileRef = SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = fileH;
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
			localRecP->io.userdata = SDKfileOpenRec8->fileinfo.fileref = *SDKfileRef = CAST_FILEREF(refNum);
		}
		else
			result = imFileOpenFailed;
	#endif

	}


	if(result == malNoError && localRecP->nestegg_ctx == NULL)
	{
		localRecP->io.seek(0, SEEK_SET, localRecP->io.userdata);
		
		int init_err = nestegg_init(&localRecP->nestegg_ctx, localRecP->io, NULL);
	
		if(init_err == NESTEGG_ERR_NONE)
		{
			nestegg *ctx = localRecP->nestegg_ctx;
		
			unsigned int tracks;
			nestegg_track_count(ctx, &tracks);
			
			for(int track=0; track < tracks; track++)
			{
				int track_type = nestegg_track_type(ctx, track);
				
				int codec_id = nestegg_track_codec_id(ctx, track);
					
				if(track_type == NESTEGG_TRACK_VIDEO)
				{
					if(codec_id == NESTEGG_CODEC_VP8 || codec_id == NESTEGG_CODEC_VP9)
					{
						localRecP->video_track = track;
						localRecP->video_codec_id = codec_id;
					}
				}
				else if(track_type == NESTEGG_TRACK_AUDIO)
				{
					assert(codec_id == NESTEGG_CODEC_VORBIS);
				
					localRecP->audio_track = track;
					localRecP->audio_codec_id = codec_id;
				}
			}
			
			if(localRecP->video_track == -1 && localRecP->audio_track == -1)
			{
				result = imFileHasNoImportableStreams;
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


//-------------------------------------------------------------------
//	"Quiet" the file (it's being closed, but you maintain your Private data).
//  Premiere does this when you put the app in the background so it's not
//  sitting there with a bunch of open (locked) files.
//	
//	NOTE:	If you don't set any privateData, you will not get an imCloseFile call
//			so close it up here.

static prMALError 
SDKQuietFile(
	imStdParms			*stdParms, 
	imFileRef			*SDKfileRef, 
	void				*privateData)
{
	// If file has not yet been closed
	if (SDKfileRef && *SDKfileRef != imInvalidHandleValue)
	{
		ImporterLocalRec8H ldataH	= reinterpret_cast<ImporterLocalRec8H>(privateData);

		stdParms->piSuites->memFuncs->lockHandle(reinterpret_cast<char**>(ldataH));

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );

		if(localRecP->nestegg_ctx)
		{
			nestegg_destroy(localRecP->nestegg_ctx);
			
			localRecP->nestegg_ctx = NULL;
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


//-------------------------------------------------------------------
//	Close the file.  You MUST have allocated Private data in imGetPrefs or you will not
//	receive this call.

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

		ImporterLocalRec8Ptr localRecP = reinterpret_cast<ImporterLocalRec8Ptr>( *ldataH );;

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


// Go ahead and overwrite any existing file. Premiere will have already checked and warned the user if file will be overwritten.
// Of course, if there are child files, you should check and return imSaveErr if appropriate.
//
// I'm not actually sure if this will ever get called.  It was in the Premiere sample so
// I just left it.  All the calls are not format specific.
static prMALError 
SDKSaveFile8(
	imStdParms			*stdParms, 
	imSaveFileRec8		*SDKSaveFileRec8) 
{
	prMALError	result = malNoError;
	#ifdef PRMAC_ENV
	CFStringRef			sourceFilePathCFSR,
						destFilePathCFSR,
						destFolderCFSR,
						destFileNameCFSR;
	CFRange				destFileNameRange,
						destFolderRange;
	CFURLRef			sourceFilePathURL,
						destFolderURL;
	FSRef				sourceFileRef,
						destFolderRef;
												
	// Convert prUTF16Char filePaths to FSRefs for paths
	sourceFilePathCFSR = CFStringCreateWithCharacters(	kCFAllocatorDefault,
														SDKSaveFileRec8->sourcePath,
														prUTF16CharLength(SDKSaveFileRec8->sourcePath));
	destFilePathCFSR = CFStringCreateWithCharacters(	kCFAllocatorDefault,
														SDKSaveFileRec8->destPath,
														prUTF16CharLength(SDKSaveFileRec8->destPath));
														
	// Separate the folder path from the file name
	destFileNameRange = CFStringFind(	destFilePathCFSR,
										CFSTR("/"),
										kCFCompareBackwards);
	destFolderRange.location = 0;
	destFolderRange.length = destFileNameRange.location;
	destFileNameRange.location += destFileNameRange.length;
	destFileNameRange.length = CFStringGetLength(destFilePathCFSR) - destFileNameRange.location;
	destFolderCFSR = CFStringCreateWithSubstring(	kCFAllocatorDefault,
													destFilePathCFSR,
													destFolderRange);
	destFileNameCFSR = CFStringCreateWithSubstring(	kCFAllocatorDefault,
													destFilePathCFSR,
													destFileNameRange);
		
	// Make FSRefs
	sourceFilePathURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,
														sourceFilePathCFSR,
														kCFURLPOSIXPathStyle,
														false);
	destFolderURL = CFURLCreateWithFileSystemPath(	kCFAllocatorDefault,
													destFolderCFSR,
													kCFURLPOSIXPathStyle,
													true);
	CFURLGetFSRef(sourceFilePathURL, &sourceFileRef);
	CFURLGetFSRef(destFolderURL, &destFolderRef);						
	#endif
	
	if (SDKSaveFileRec8->move)
	{
		#ifdef PRWIN_ENV
		if( MoveFileW(SDKSaveFileRec8->sourcePath, SDKSaveFileRec8->destPath) == 0)
		{
			result = imSaveErr;
		}
		#else
		if( FSCopyObjectSync(	&sourceFileRef,
								&destFolderRef,
								destFileNameCFSR,
								NULL,
								kFSFileOperationOverwrite))
		{
			result = imSaveErr;
		}
		#endif
	}
	else
	{
		#ifdef PRWIN_ENV
		if( CopyFileW (SDKSaveFileRec8->sourcePath, SDKSaveFileRec8->destPath, kPrTrue) == 0)
		{
			result = imSaveErr;
		}
		#else
		if ( FSMoveObjectSync(	&sourceFileRef,
								&destFolderRef,
								destFileNameCFSR,
								NULL,
								kFSFileOperationOverwrite))
		{
			result = imSaveErr;
		}
		#endif
	}
	return result;
}


// This was also in the SDK sample, so figured I'd just leave it here.
static prMALError 
SDKDeleteFile8(
	imStdParms			*stdParms, 
	imDeleteFileRec8	*SDKDeleteFileRec8)
{
	prMALError	result = malNoError;

	#ifdef PRWIN_ENV
	if( DeleteFileW(SDKDeleteFileRec8->deleteFilePath) )
	{
		result = imDeleteErr;
	}
	#else
	CFStringRef	filePathCFSR;
	CFURLRef	filePathURL;
	FSRef		fileRef;

	filePathCFSR = CFStringCreateWithCharacters(kCFAllocatorDefault,
												SDKDeleteFileRec8->deleteFilePath,
												prUTF16CharLength(SDKDeleteFileRec8->deleteFilePath));
	filePathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
												filePathCFSR,
												kCFURLPOSIXPathStyle,
												false);
	CFURLGetFSRef(filePathURL, &fileRef);					
	if( FSDeleteObject(&fileRef) )
	{
		result = imDeleteErr;
	}
	#endif
	
	return result;
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
		// just support one pixel format, 8-bit BGRA
		case 0:
			SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709;
			break;
	
		default:
			result = imBadFormatIndex;
			break;
	}

	return result;	
}


// File analysis - Supplies supplemental compression information to File Properties dialog
// I'm just using this opportunity for inform the user that they can double-click.
static prMALError 
SDKAnalysis(
	imStdParms		*stdParms,
	imFileRef		SDKfileRef,
	imAnalysisRec	*SDKAnalysisRec)
{
	// if you wanted to get the private data
	//ImporterLocalRec8H ldataH = reinterpret_cast<ImporterLocalRec8H>(SDKAnalysisRec->privatedata);

	const char *properties_messsage = "WebM info goes here";

	if(SDKAnalysisRec->buffersize > strlen(properties_messsage))
		strcpy(SDKAnalysisRec->buffer, properties_messsage);

	return malNoError;
}



static int
webm_guess_framerate(nestegg		*ctx,
					int				video_track,
                     unsigned int	*fps_den,
                     unsigned int	*fps_num)
{
  unsigned int i;
  uint64_t     tstamp = 0;

  // Guess the framerate. Read up to 1 second, or 50 video packets,
	//whichever comes first.
  for (i = 0; tstamp < 1000000000 && i < 50;) {
    nestegg_packet *pkt;
    unsigned int track;

    if (nestegg_read_packet(ctx, &pkt) <= 0)
      break;

    nestegg_packet_track(pkt, &track);
    if (track == video_track) {
      nestegg_packet_tstamp(pkt, &tstamp);
      i++;
    }

    nestegg_free_packet(pkt);
  }

  //if (nestegg_track_seek(ctx, video_track, 0))
  //  goto fail;

  *fps_num = (i - 1) * 1000000;
  *fps_den = (unsigned int)(tstamp / 1000);
  return 0;
fail:
  //nestegg_destroy(input->nestegg_ctx);
  //input->nestegg_ctx = NULL;
  //rewind(input->infile);
  return 1;
}


//-------------------------------------------------------------------
// Populate the imFileInfoRec8 structure describing this file instance
// to Premiere.  Check file validity, allocate any private instance data 
// to share between different calls.
//
// Actually, I'm currently verifying the file back during the open phase.
// Is that a problem?  Doing it that way because some FFmpeg structures
// are associated with the file reading operations but we have to know
// if it's actually a PNG first.

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

	if(localRecP)
	{
		assert(localRecP->io.userdata == fileAccessInfo8->fileref);
		assert(localRecP->nestegg_ctx != NULL);
	
		if(localRecP->nestegg_ctx != NULL)
		{
			nestegg *ctx = localRecP->nestegg_ctx;
		
			uint64_t scale;
			nestegg_tstamp_scale(localRecP->nestegg_ctx, &scale);
			
			uint64_t duration;
			int dur_err = nestegg_duration(localRecP->nestegg_ctx, &duration);
			
			unsigned int fps_num = 0;
			unsigned int fps_den = 0;
			webm_guess_framerate(localRecP->nestegg_ctx, localRecP->video_track, &fps_den, &fps_num);
			
			int frames = (duration * fps_num / fps_den) / 1000000000UL;
			
			assert(scale == fps_den);
			
			// getting some very large fps numbers, maybe we can lower them
			if(fps_num % 1000 == 0 && fps_den % 1000 == 0)
			{
				localRecP->time_mult = 1000;
			}
			else
				localRecP->time_mult = 1;
			
			
			if(localRecP->video_track >= 0)
			{
				nestegg_video_params params;
				int params_err = nestegg_track_video_params(ctx, localRecP->video_track, &params);
				
				if(params_err == NESTEGG_ERR_NONE && dur_err == NESTEGG_ERR_NONE)
				{
					// Video information
					SDKFileInfo8->hasVideo				= kPrTrue;
					SDKFileInfo8->vidInfo.subType		= PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709;
					SDKFileInfo8->vidInfo.imageWidth	= params.width;
					SDKFileInfo8->vidInfo.imageHeight	= params.height;
					SDKFileInfo8->vidInfo.depth			= 24;	// The bit depth of the video
					SDKFileInfo8->vidInfo.fieldType		= prFieldsNone; // or prFieldsUnknown
					SDKFileInfo8->vidInfo.isStill		= kPrFalse;
					SDKFileInfo8->vidInfo.noDuration	= imNoDurationFalse;
					SDKFileInfo8->vidDuration			= frames * (fps_den / localRecP->time_mult);
					SDKFileInfo8->vidScale				= fps_num / localRecP->time_mult;
					SDKFileInfo8->vidSampleSize			= fps_den / localRecP->time_mult;

					SDKFileInfo8->vidInfo.alphaType	= alphaNone;

					SDKFileInfo8->vidInfo.pixelAspectNum = 1;
					SDKFileInfo8->vidInfo.pixelAspectDen = 1;

					// store some values we want to get without going to the file
					localRecP->width = SDKFileInfo8->vidInfo.imageWidth;
					localRecP->height = SDKFileInfo8->vidInfo.imageHeight;

					localRecP->frameRateNum = SDKFileInfo8->vidScale;
					localRecP->frameRateDen = SDKFileInfo8->vidSampleSize;
				}
				else
					result = imBadFile;
			}
			else
				SDKFileInfo8->hasVideo = kPrFalse;
			
			
			if(localRecP->audio_track >= 0)
			{
				nestegg_audio_params params;
				int params_err = nestegg_track_audio_params(ctx, localRecP->audio_track, &params);
				
				if(params_err == NESTEGG_ERR_NONE && dur_err == NESTEGG_ERR_NONE)
				{
					// Audio information
					SDKFileInfo8->hasAudio				= kPrTrue;
					SDKFileInfo8->audInfo.numChannels	= params.channels;
					SDKFileInfo8->audInfo.sampleRate	= params.rate;
					SDKFileInfo8->audInfo.sampleType	= params.depth == 8 ? kPrAudioSampleType_8BitInt :
															params.depth == 16 ? kPrAudioSampleType_16BitInt :
															params.depth == 24 ? kPrAudioSampleType_24BitInt :
															params.depth == 32 ? kPrAudioSampleType_32BitFloat :
															params.depth == 64 ? kPrAudioSampleType_64BitFloat :
															kPrAudioSampleType_8BitTwosInt;
															
					SDKFileInfo8->audDuration			= (uint64_t)params.rate * duration / 1000000000UL;
					
					
					localRecP->audioSampleRate			= SDKFileInfo8->audInfo.sampleRate;
					localRecP->numChannels				= SDKFileInfo8->audInfo.numChannels;
				}
				else
					result = imBadFile;
			}
			else
				SDKFileInfo8->hasAudio = kPrFalse;
		}
		else
			result = imBadFile;
	}
		
	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
}


static prMALError 
SDKCalcSize8(
	imStdParms			*stdParms, 
	imCalcSizeRec		*calcSizeRec,
	imFileAccessRec8	*fileAccessRec8)
{
	// tell Premiere the file size
	
	return imUnsupported;
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


	bool can_shrink = false; // doesn't look like we can decode a smaller frame

	if(preferredFrameSizeRec->inIndex == 0)
	{
		preferredFrameSizeRec->outWidth = localRecP->width;
		preferredFrameSizeRec->outHeight = localRecP->height;
	}
	else
	{
		// we store width and height in private data so we can produce it here
		const int divisor = pow(2, preferredFrameSizeRec->inIndex);
		
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


// Here we go - copy a frame to Premiere
static prMALError 
SDKGetSourceVideo(
	imStdParms			*stdParms, 
	imFileRef			fileRef, 
	imSourceVideoRec	*sourceVideoRec)
{
	prMALError		result		= malNoError;
	csSDK_int32		theFrame	= 0;
	//RowbyteType		rowBytes	= 0;
	imFrameFormat	*frameFormat;
	//char			*frameBuffer;

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
		

		assert(localRecP->io.userdata == fileRef);
		assert(localRecP->nestegg_ctx != NULL);

		if(localRecP->nestegg_ctx != NULL)
		{
			uint64_t scale;
			nestegg_tstamp_scale(localRecP->nestegg_ctx, &scale);
			
			const uint64_t fps_num = localRecP->frameRateNum * localRecP->time_mult;
			const uint64_t fps_den = localRecP->frameRateDen * localRecP->time_mult;
			
			assert(scale == fps_den);
			
			uint64_t tstamp = ((uint64_t)theFrame * fps_den * 1000000000UL / fps_num);
			uint64_t tstamp2 = (uint64_t)sourceVideoRec->inFrameTime * 1000UL / ((uint64_t)ticksPerSecond / 1000000UL); // alternate way of calculating it
			
			assert(tstamp == tstamp2);
			
			const uint64_t half_frame_time = (1000000000UL * fps_den / fps_num) / 2; // half-a-frame
			
			
			int seek_err = nestegg_track_seek(localRecP->nestegg_ctx, localRecP->video_track, tstamp);
			
			if(seek_err == NESTEGG_ERR_NONE)
			{
				bool first_frame = true;
			
				int codec_id = nestegg_track_codec_id(localRecP->nestegg_ctx, localRecP->video_track);
				assert(localRecP->video_codec_id == codec_id);
			
				const vpx_codec_iface_t *iface = (codec_id == NESTEGG_CODEC_VP8 ? vpx_codec_vp8_dx() : vpx_codec_vp9_dx());
				
				vpx_codec_ctx_t decoder;
				
				vpx_codec_err_t codec_err = vpx_codec_dec_init(&decoder, iface, NULL, 0);
				
				if(codec_err == VPX_CODEC_OK)
				{
					nestegg_packet *pkt = NULL;
					
					uint64_t packet_tstamp = 0;
					
					int read_result = NESTEGG_SUCCESS;
					int data_err = NESTEGG_ERR_NONE;
					vpx_codec_err_t decode_err = VPX_CODEC_OK;
					
					unsigned char *data = NULL;
					size_t length;
					
					bool reached_iframe = false;
					
					do{
						if(pkt)
						{
							nestegg_free_packet(pkt);
							pkt = NULL;
						}

						read_result = nestegg_read_packet(localRecP->nestegg_ctx, &pkt);
						
						if(read_result == NESTEGG_SUCCESS)
						{
							nestegg_packet_tstamp(pkt, &packet_tstamp);
							
							unsigned int track;
							nestegg_packet_track(pkt, &track);
							
							if(track == localRecP->video_track)
							{
								unsigned int chunks;
								nestegg_packet_count(pkt, &chunks);
							
								for(int i=0; i < chunks && !reached_iframe && data_err == NESTEGG_ERR_NONE && decode_err == VPX_CODEC_OK; i++)
								{
									data_err = nestegg_packet_data(pkt, i, &data, &length);
									
									if(data_err == NESTEGG_ERR_NONE)
									{
										vpx_codec_stream_info_t stream_info;
										stream_info.sz = sizeof(stream_info);
									
										vpx_codec_err_t peek_err = vpx_codec_peek_stream_info(iface, data, length, &stream_info);
										
										if(!first_frame && stream_info.is_kf)
											reached_iframe = true;
										
										if(!reached_iframe)
										{
											decode_err = vpx_codec_decode(&decoder, data, length, NULL, 0);
											
											if(decode_err == VPX_CODEC_OK)
											{
												csSDK_int32 decodedFrame = ((packet_tstamp + half_frame_time) / fps_den) * fps_num / 1000000000UL;
												
												csSDK_int32 hopingforFrame = ((tstamp + half_frame_time) / fps_den) * fps_num / 1000000000UL;
												assert(hopingforFrame == theFrame);
												
												vpx_codec_iter_t iter = NULL;
												
												vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);
												
												if(img)
												{
													PPixHand ppix;
													
													localRecP->PPixCreatorSuite->CreatePPix(&ppix, PrPPixBufferAccess_ReadWrite, frameFormat->inPixelFormat, &theRect);

													assert(frameFormat->inPixelFormat == PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709);
													
													char *Y_PixelAddress, *U_PixelAddress, *V_PixelAddress;
													csSDK_uint32 Y_RowBytes, U_RowBytes, V_RowBytes;
													
													localRecP->PPix2Suite->GetYUV420PlanarBuffers(ppix, PrPPixBufferAccess_ReadWrite,
																									&Y_PixelAddress, &Y_RowBytes,
																									&U_PixelAddress, &U_RowBytes,
																									&V_PixelAddress, &V_RowBytes);
																								
													assert(frameFormat->inFrameHeight == img->d_h);
													assert(frameFormat->inFrameWidth == img->d_w);

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
													
													localRecP->PPixCacheSuite->AddFrameToCache(	localRecP->importerID,
																								0,
																								ppix,
																								decodedFrame,
																								NULL,
																								NULL);
													
													if(decodedFrame == theFrame)
													{
														*sourceVideoRec->outFrame = ppix;
													}
													else
													{
														localRecP->PPixSuite->Dispose(ppix);
													}
												}
											}
										}
									}
								}
							}
						}
						
						first_frame = false;
						
					}while(!reached_iframe && read_result == NESTEGG_SUCCESS && data_err == NESTEGG_ERR_NONE && decode_err == VPX_CODEC_OK);
					
					
					if(pkt)
						nestegg_free_packet(pkt);
					
					
					if( !(read_result == NESTEGG_SUCCESS && data_err == NESTEGG_ERR_NONE && decode_err == VPX_CODEC_OK) )
						result = imFileReadFailed;
				}
				else
					result = imBadCodec;
					
				vpx_codec_err_t destroy_err = vpx_codec_destroy(&decoder);
				assert(destroy_err == VPX_CODEC_OK);
			}
			else
				result = imFileReadFailed;
		}
		else
			result = imOtherErr;
	}


	stdParms->piSuites->memFuncs->unlockHandle(reinterpret_cast<char**>(ldataH));

	return result;
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


	assert(localRecP->io.userdata == SDKfileRef);
	assert(localRecP->nestegg_ctx != NULL);

	if(localRecP->nestegg_ctx != NULL)
	{
		uint64_t tstamp = audioRec7->position * 1000000000UL / localRecP->audioSampleRate;
		
		int ogg_packet_num = 0;
		
		unsigned int headers = 0;
		
		int count_err = nestegg_track_codec_data_count(localRecP->nestegg_ctx, localRecP->audio_track, &headers);
		
		if(count_err == NESTEGG_ERR_NONE && headers == 3)
		{
			vorbis_info vi;
			vorbis_comment vc;
			vorbis_dsp_state vd;
			vorbis_block vb;
			
			vorbis_info_init(&vi);
			vorbis_comment_init(&vc);
			
			
			int v_err = NESTEGG_ERR_NONE;
			
			for(int h=0; h < headers && v_err == NESTEGG_ERR_NONE; h++)
			{
				unsigned char *data = NULL;
				size_t length = 0;
				
				v_err = nestegg_track_codec_data(localRecP->nestegg_ctx, localRecP->audio_track,
														h, &data, &length);
														
				if(v_err == NESTEGG_ERR_NONE)
				{
					ogg_packet packet;
					
					packet.packet = data;
					packet.bytes = length;
					packet.b_o_s = (h == 0);
					packet.e_o_s = false;
					packet.granulepos = 0;
					packet.packetno = ogg_packet_num++;
					
					v_err = vorbis_synthesis_headerin(&vi, &vc, &packet);
				}
			}
					
			if(v_err == NESTEGG_ERR_NONE)
			{
				v_err = vorbis_synthesis_init(&vd, &vi);
				
				if(v_err == NESTEGG_ERR_NONE)
					v_err = vorbis_block_init(&vd, &vb);
				
				v_err = nestegg_track_seek(localRecP->nestegg_ctx,
											localRecP->video_track >= 0 ? localRecP->video_track : localRecP->audio_track,
											tstamp);
											
				if(v_err == NESTEGG_ERR_NONE)
				{
					nestegg_packet *pkt = NULL;
					
					csSDK_uint32 samples_copied = 0;
					csSDK_uint32 samples_left = audioRec7->size;
					
					uint64_t packet_tstamp = 0;
					
					int read_result = NESTEGG_SUCCESS;
					int data_err = NESTEGG_ERR_NONE;
					
					do{
						if(pkt)
						{
							nestegg_free_packet(pkt);
							pkt = NULL;
						}

						read_result = nestegg_read_packet(localRecP->nestegg_ctx, &pkt);
						
						if(read_result == NESTEGG_SUCCESS)
						{
							nestegg_packet_tstamp(pkt, &packet_tstamp);
							
							unsigned int track;
							nestegg_packet_track(pkt, &track);
							
							if(track == localRecP->audio_track)
							{
								PrAudioSample packet_start = localRecP->audioSampleRate * packet_tstamp / 1000000000UL;
								
								PrAudioSample packet_offset = audioRec7->position - packet_start; // in other words the audio frames in the beginning that we'll skip over
								
								if(packet_offset < 0)
									packet_offset = 0;
							
								unsigned int chunks;
								nestegg_packet_count(pkt, &chunks);
							
								for(int i=0; i < chunks && data_err == NESTEGG_ERR_NONE; i++)
								{
									unsigned char *data = NULL;
									size_t length;
									
									data_err = nestegg_packet_data(pkt, i, &data, &length);
									
									if(data_err == NESTEGG_ERR_NONE)
									{
										ogg_packet packet;
					
										packet.packet = data;
										packet.bytes = length;
										packet.b_o_s = false;
										packet.e_o_s = false;
										packet.granulepos = -1;
										packet.packetno = ogg_packet_num++;

										int synth_err = vorbis_synthesis(&vb, &packet);
										
										if(synth_err == NESTEGG_ERR_NONE)
										{
											int block_err = vorbis_synthesis_blockin(&vd, &vb);
											
											if(block_err == NESTEGG_ERR_NONE)
											{
												float **pcm = NULL;
												int samples;
												
												int synth_result = 1;
												
												while(synth_result != 0 && (samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0)
												{
													int samples_to_copy = samples_left;
													
													if(packet_offset >= samples)
													{
														samples_to_copy = 0;
													}
													else if(samples_to_copy > (samples - packet_offset))
													{
														samples_to_copy = (samples - packet_offset);
													}
												
													// how nice, audio samples are float, just like Premiere wants 'em
													for(int c=0; c < localRecP->numChannels && samples_to_copy > 0; c++)
													{
														memcpy(audioRec7->buffer[c] + samples_copied, pcm[c] + packet_offset, samples_to_copy * sizeof(float));
													}
													
													samples_copied += samples_to_copy;
													samples_left -= samples_to_copy;
													
													if(samples_to_copy > 0)
														packet_offset = 0;
													else
														packet_offset -= samples;
													
													
													synth_result = vorbis_synthesis_read(&vd, samples);
												}
											}
										}
										else
											read_result = 0;
									}
								}
							}
						}
						
					}while(samples_left > 0 && read_result == NESTEGG_SUCCESS && data_err == NESTEGG_ERR_NONE);

					assert(samples_left == 0 && samples_copied == audioRec7->size);

					if(pkt)
						nestegg_free_packet(pkt);
					
					
					if( !(read_result == NESTEGG_SUCCESS && data_err == NESTEGG_ERR_NONE) )
						result = imFileReadFailed;
						
					assert(result == malNoError);
				}
				else
					result = imFileReadFailed;
			}
			else
				result = imFileReadFailed;
			
			
			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);
			vorbis_info_clear(&vi);
			vorbis_comment_clear(&vc);
		}
		else
			result = imFileReadFailed;
	}
	else
		result = imOtherErr;
		
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

		case imSaveFile8:
			result =	SDKSaveFile8(	stdParms, 
										reinterpret_cast<imSaveFileRec8*>(param1));
			break;
			
		case imDeleteFile8:
			result =	SDKDeleteFile8(	stdParms, 
										reinterpret_cast<imDeleteFileRec8*>(param1));
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

		case imCalcSize8:
			result =	SDKCalcSize8(	stdParms,
										reinterpret_cast<imCalcSizeRec*>(param1),
										reinterpret_cast<imFileAccessRec8*>(param2));
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

	return result;
}

