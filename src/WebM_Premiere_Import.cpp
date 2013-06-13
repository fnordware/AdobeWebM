
// Chyron_File_Import.cpp
//
// Premiere file importer for reading .chy files, which are really PNG or
// QuickTime RLE (Animation codec) files.
//
// Also writes file's path to $(HOME)/ChyronAXISClip.txt when clip is double-clicked (SDKGetPrefs8)
//
// Written by Brendan Bolles <brendan@fnordware.com> for Bill Ferster <bferster@stagetools.com>
// Part of the Chyron AXIS project
//

#include "WebM_Premiere_Import.h"


extern "C" {

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx_config.h"
#include "vpx/vpx_decoder.h"
#include "vpx_ports/vpx_timer.h"
#if CONFIG_VP8_DECODER || CONFIG_VP9_DECODER
#include "vpx/vp8dx.h"
#endif
#if CONFIG_MD5
#include "md5_utils.h"
#endif
#include "tools_common.h"
#include "nestegg/include/nestegg/nestegg.h"
#define INT_TYPES_DEFINED
#include "third_party/libyuv/include/libyuv/scale.h"

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
	
	nestegg_io				io;
	nestegg					*nestegg_ctx;
	unsigned int			video_track;
	int						codec_id;
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
	importInfo->setupOnDblClk		= kPrFalse;		// If user dbl-clicks file you imported, pop your setup dialog
	importInfo->canSave				= kPrFalse;		// Can 'save as' files to disk, real file only
	
	importInfo->canDelete			= kPrFalse;		// File importers only, use if you only if you have child files
	importInfo->dontCache			= kPrFalse;		// Don't let Premiere cache these files
	importInfo->hasSetup			= kPrFalse;		// Set to kPrTrue if you have a setup dialog
	importInfo->keepLoaded			= kPrFalse;		// If you MUST stay loaded use, otherwise don't: play nice
	importInfo->priority			= 0;
	importInfo->canTrim				= kPrFalse;
	importInfo->canCalcSizes		= kPrFalse;
	if (stdParms->imInterfaceVer >= IMPORTMOD_VERSION_6)
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
		localRecP->video_track = 0;
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
		assert(localRecP->io.tell(localRecP->io.userdata) == 0);
		
		int init_err = nestegg_init(&localRecP->nestegg_ctx, localRecP->io, NULL);
	
		if(init_err == NESTEGG_ERR_NONE)
		{
			nestegg *ctx = localRecP->nestegg_ctx;
		
			unsigned int tracks;
			nestegg_track_count(ctx, &tracks);
			
			int codec_id = -1;
			
			for(int track=0; track < tracks && codec_id == -1; track++)
			{
				int track_type = nestegg_track_type(ctx, track);
				
				if(track_type == NESTEGG_TRACK_VIDEO)
				{
					codec_id = nestegg_track_codec_id(ctx, track);
					
					if(codec_id == NESTEGG_CODEC_VP8 || codec_id == NESTEGG_CODEC_VP9)
					{
						localRecP->video_track = track;
						localRecP->codec_id = codec_id;
					}
				}
			}
			
			if(codec_id == -1)
			{
				result = imFileOpenFailed;
			}
		}
		else
			result = imFileOpenFailed;
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
	ImporterLocalRec8H	ldataH	= reinterpret_cast<ImporterLocalRec8H>(SDKIndPixelFormatRec->privatedata);

	switch(idx)
	{
		// just support one pixel format, 8-bit BGRA
		case 0:
			SDKIndPixelFormatRec->outPixelFormat = PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
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
		
			nestegg_video_params params;
			int params_err = nestegg_track_video_params(ctx, localRecP->video_track, &params);
			
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
			
			
			if(params_err == NESTEGG_ERR_NONE && dur_err == NESTEGG_ERR_NONE)
			{
				// Video information
				SDKFileInfo8->hasVideo				= kPrTrue;
				SDKFileInfo8->vidInfo.subType		= PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601;
				SDKFileInfo8->vidInfo.imageWidth	= params.width;
				SDKFileInfo8->vidInfo.imageHeight	= params.height;
				SDKFileInfo8->vidInfo.depth			= 24;	// The bit depth of the video
				SDKFileInfo8->vidInfo.fieldType		= prFieldsNone; // or prFieldsUnknown

				//SDKFileInfo8->vidInfo.isStill = kPrTrue;
				//SDKFileInfo8->vidInfo.noDuration = imNoDurationStillDefault;
				//SDKFileInfo8->vidDuration				= 1;
				//SDKFileInfo8->vidScale				= 1;
				//SDKFileInfo8->vidSampleSize			= 1;
				SDKFileInfo8->vidInfo.isStill		= kPrFalse;
				SDKFileInfo8->vidInfo.noDuration	= imNoDurationFalse;
				//SDKFileInfo8->vidDuration			= ((duration / fps_den) * (fps_num / 1000)) / localRecP->time_mult;
				//SDKFileInfo8->vidDuration			= 48 * fps_den / localRecP->time_mult;
				SDKFileInfo8->vidDuration			= frames * (fps_den / localRecP->time_mult);
				SDKFileInfo8->vidScale				= fps_num / localRecP->time_mult;
				SDKFileInfo8->vidSampleSize			= fps_den / localRecP->time_mult;

				SDKFileInfo8->vidInfo.alphaType	= alphaStraight;

				SDKFileInfo8->vidInfo.pixelAspectNum = 1;
				SDKFileInfo8->vidInfo.pixelAspectDen = 1;

				// not doing audio
				SDKFileInfo8->hasAudio = kPrFalse;

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
		// we store frame rate in private data so we don't have to go to the FFmpeg struct yet
		// although really we could
		//theFrame = ((csSDK_uint64)sourceVideoRec->inFrameTime * (csSDK_uint64)localRecP->frameRateNum) / ((csSDK_uint64)ticksPerSecond * (csSDK_uint64)localRecP->frameRateDen);
		//theFrame = ((sourceVideoRec->inFrameTime / (PrTime)localRecP->frameRateDen) * (PrTime)localRecP->frameRateNum) / ticksPerSecond;

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
		//localRecP->PPixCreatorSuite->CreatePPix(sourceVideoRec->outFrame, PrPPixBufferAccess_ReadWrite, frameFormat->inPixelFormat, &theRect);
		//localRecP->PPixSuite->GetPixels(*sourceVideoRec->outFrame, PrPPixBufferAccess_ReadWrite, &frameBuffer);
		//localRecP->PPixSuite->GetRowBytes(*sourceVideoRec->outFrame, &rowBytes);
		

		assert(localRecP->io.userdata == fileRef);
		assert(localRecP->nestegg_ctx != NULL);

		if(localRecP->nestegg_ctx != NULL)
		{
			uint64_t scale;
			nestegg_tstamp_scale(localRecP->nestegg_ctx, &scale);
			
			uint64_t fps_num = localRecP->frameRateNum * localRecP->time_mult;
			uint64_t fps_den = localRecP->frameRateDen * localRecP->time_mult;
			
			assert(scale == fps_den);
			
			uint64_t tstamp = ((uint64_t)theFrame * fps_den * 1000000000UL / fps_num);
			uint64_t tstamp2 = (uint64_t)sourceVideoRec->inFrameTime * 1000UL / ((uint64_t)ticksPerSecond / 1000000UL);
			
			assert(tstamp == tstamp2);
			
			uint64_t half_frame_time = (1000000000UL * fps_den / fps_num) / 2; // half-a-frame
			
			
			int seek_err = nestegg_track_seek(localRecP->nestegg_ctx, localRecP->video_track, tstamp);
			
			if(seek_err == NESTEGG_ERR_NONE)
			{
				bool first_frame = true;
			
				assert(localRecP->codec_id == nestegg_track_codec_id(localRecP->nestegg_ctx, localRecP->video_track));
			
				const vpx_codec_iface_t *iface = (localRecP->codec_id == NESTEGG_CODEC_VP8 ? vpx_codec_vp8_dx() : vpx_codec_vp9_dx());
				
				vpx_codec_ctx_t decoder;
				
				vpx_codec_dec_cfg_t	cfg;
				cfg.threads = 8;
				cfg.w = frameFormat->inFrameWidth;
				cfg.h = frameFormat->inFrameHeight;
				
				int dec_flags = VPX_CODEC_USE_FRAME_THREADING | VPX_CODEC_CAP_FRAME_THREADING;
				
				vpx_codec_err_t codec_err = vpx_codec_dec_init(&decoder,
																iface,
																&cfg,
																dec_flags);
																
				if(codec_err == VPX_CODEC_OK)
				{
					nestegg_packet *pkt = NULL;
					
					uint64_t found_tstamp = 0;
					
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
							nestegg_packet_tstamp(pkt, &found_tstamp);
							
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
												csSDK_int32 decodedFrame = ((found_tstamp + half_frame_time) / fps_den) * fps_num / 1000000000UL;
												
												csSDK_int32 hopingforFrame = ((tstamp + half_frame_time) / fps_den) * fps_num / 1000000000UL;
												assert(hopingforFrame == theFrame);
												
												vpx_codec_iter_t iter = NULL;
												
												vpx_image_t *img = vpx_codec_get_frame(&decoder, &iter);
												
												if(img)
												{
													PPixHand ppix;
													
													localRecP->PPixCreatorSuite->CreatePPix(&ppix, PrPPixBufferAccess_ReadWrite, frameFormat->inPixelFormat, &theRect);

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
					
					
					vpx_codec_err_t destroy_err = vpx_codec_destroy(&decoder);
					assert(destroy_err == VPX_CODEC_OK);
					
					if(pkt)
						nestegg_free_packet(pkt);
					
					
					if( !(read_result == NESTEGG_SUCCESS && data_err == NESTEGG_ERR_NONE && decode_err == VPX_CODEC_OK) )
						result = imFileReadFailed;
				}
				else
					result = imBadCodec;
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

		case imCreateAsyncImporter:
			result =	imUnsupported;
			break;
	}

	return result;
}

