/*
 *  libwebm_duration.cpp
 *  WebM_Premiere
 *
 *  Created by Brendan Bolles on 3/31/16.
 *  Copyright 2016 fnord. All rights reserved.
 *
 */

#include "mkvmuxer/mkvmuxer.h"
#include "mkvparser/mkvparser.h"

#include <iostream>

#include <stdio.h>
#include <assert.h>

using namespace std;

class MyWriter : public mkvmuxer::IMkvWriter
{
  public:
	MyWriter(const char *path);
	virtual ~MyWriter();
	
	virtual int32_t Write(const void* buf, uint32_t len);
	virtual int64_t Position() const;
	virtual int32_t Position(int64_t position);
	virtual bool Seekable() const { return true; }
	virtual void ElementStartNotify(uint64_t element_id, int64_t position) {}

  private:
	FILE *_fp;
	
	enum {
		Fail = -1,
		Success = 0
	};
};

MyWriter::MyWriter(const char *path)
{
	_fp = fopen(path, "wb");
	
	if(_fp == NULL)
		throw Fail;
}

MyWriter::~MyWriter()
{
	fclose(_fp);
}

int32_t
MyWriter::Write(const void* buf, uint32_t len)
{
	const size_t count = fwrite(buf, len, 1, _fp);
	
	return (count == 1 ? Success : Fail);
}

int64_t
MyWriter::Position() const
{
	return ftell(_fp);
}

int32_t
MyWriter::Position(int64_t position)
{
	return fseek(_fp, position, SEEK_SET);
}


class MyReader : public mkvparser::IMkvReader
{
  public:
	MyReader(const char *path);
	virtual ~MyReader();
	
	virtual int Read(long long pos, long len, unsigned char* buf);
	virtual int Length(long long* total, long long* available);
	
  private:
	FILE *_fp;
	size_t _file_size;
	
	enum {
		Fail = -1,
		Success = 0
	};
};

MyReader::MyReader(const char *path)
{
	_fp = fopen(path, "rb");
	
	if(_fp == NULL)
		throw Fail;
	
	fseek(_fp, 0, SEEK_END);
	
	_file_size = ftell(_fp) + 1;
	
	fseek(_fp, 0, SEEK_SET);
}

MyReader::~MyReader()
{
	fclose(_fp);
}

int
MyReader::Read(long long pos, long len, unsigned char* buf)
{
	fseek(_fp, pos, SEEK_SET);

	const size_t read = fread(buf, len, 1, _fp);
	
	return (read == 1 ? Success : Fail);
}

int
MyReader::Length(long long* total, long long* available)
{
	*total = *available = _file_size;
	
	return Success;
}

#pragma mark-

static const long long S2NS = 1000000000LL; // seconds to nanoseconds

static const double fps = 24.0;
static const double sampleRate = 48000.0;
static const int audioChannels = 2;

static const int framesToWrite = 48;

int main(int argumentCount, char* argumentVector[])
{
	if(argumentCount != 2)
	{
		cout << "Usage: " << argumentVector[0] << " <outfile>" << endl;
		
		return -1;
	}

	const size_t privateDataSize = 1000;
	void * const privateData = malloc(privateDataSize);
		
	const size_t audioSize = 150;
	void * const audioBuffer = malloc(audioSize);
	
	const size_t videoSize = 1000;
	void * const videoBuffer = malloc(videoSize);
	
			
	const long long expectedDuration = (((double)framesToWrite / fps) * (double)S2NS) + 0.5;
	
									
	try
	{
		// Write out a file
		{
			MyWriter writer(argumentVector[1]);
			
			// Segment
			mkvmuxer::Segment segment;
			
			segment.Init(&writer);
			segment.set_mode(mkvmuxer::Segment::kFile);
			
			mkvmuxer::SegmentInfo* const info = segment.GetSegmentInfo();
			
			info->set_writing_app("libwebm demo program");
			
			const uint64_t timeCodeScale = info->timecode_scale();
			
			assert(timeCodeScale == 1000000); // WebM standard
			
			const long long expectedDurationTimeCode = ((double)expectedDuration / (double)timeCodeScale) + 0.5;
			
			
			// Video track
			const uint64_t vid_track = segment.AddVideoTrack(640, 480, 1);
			
			mkvmuxer::VideoTrack* const video = static_cast<mkvmuxer::VideoTrack *>(segment.GetTrackByNumber(vid_track));
			
			video->set_frame_rate(fps);
			
			video->set_codec_id(mkvmuxer::Tracks::kVp8CodecId);
			
			segment.CuesTrack(vid_track);
			
			
			// Audio track
			const uint64_t audio_track = segment.AddAudioTrack(sampleRate, audioChannels, 2);
			
			mkvmuxer::AudioTrack* const audio = static_cast<mkvmuxer::AudioTrack *>(segment.GetTrackByNumber(audio_track));
			
			audio->set_codec_id(mkvmuxer::Tracks::kVorbisCodecId);
			
			audio->SetCodecPrivate((uint8_t *)privateData, privateDataSize);
			
			
			assert(segment.estimate_file_duration()); // not that we need it
			
			
			// Write video
			
			for(int i = 0; i < framesToWrite; i++)
			{
				const double timeInSeconds = i * (1.0 / fps);
				
				// Time (in nanoseconds) = TimeCode * TimeCodeScale
				const long long timeCode = timeInSeconds * (S2NS / timeCodeScale);
				const uint64_t timeStamp = timeCode * timeCodeScale;
			
				// Audio
				{
					const bool added = segment.AddFrame((const uint8_t *)audioBuffer, audioSize,
														audio_track, timeStamp, true);
					
					if(!added)
						throw -1;
				}
				
				// Video
				{
					const bool isKey = (i % 5 == 0); // every 5th frame
					
					const bool added = segment.AddFrame((const uint8_t *)videoBuffer, videoSize,
														vid_track, timeStamp, isKey);
					
					if(!added)
						throw -1;
				}
				
				// More Audio
				{
					const uint64_t thisTimeStamp = timeStamp + (S2NS / (2 * fps));
					
					const bool added = segment.AddFrame((const uint8_t *)audioBuffer, audioSize,
														audio_track, thisTimeStamp, true);
					
					if(!added)
						throw -1;
				}
			}
			
			
			segment.set_duration(expectedDurationTimeCode);  // YES!
			
			
			const bool final = segment.Finalize();
			
			if(!final)
				cout << "Finalize failed!" << endl;
		}
		
		free(privateData);
		free(audioBuffer);
		free(videoBuffer);
		
		
		// Read the file back in
		{
			MyReader reader(argumentVector[1]);
			
			long long pos = 0;
			
			mkvparser::EBMLHeader ebmlHeader;
			
			ebmlHeader.Parse(&reader, pos);
			
			mkvparser::Segment *segment = NULL;
			
			long long ret = mkvparser::Segment::CreateInstance(&reader, pos, segment);
			
			if(ret >= 0 && segment != NULL)
			{
				ret = segment->Load();
				
				if(ret >= 0)
				{
					const mkvparser::SegmentInfo* const pSegmentInfo = segment->GetInfo();
					
					const long long duration = pSegmentInfo->GetDuration();
					
					assert(pSegmentInfo->GetDuration() == segment->GetDuration());
					
					const uint64_t timeCodeScale = pSegmentInfo->GetTimeCodeScale();
					
					assert(timeCodeScale == 1000000); // WebM standard
					
					if(expectedDuration == duration)
					{
						cout << "Duration was as expected." << endl;
					}
					else
					{
						const int64_t delta = abs((int64_t)expectedDuration - duration);
						
						const double framesDelta = ((double)delta * fps / (double)S2NS);
					
						cout << "Duration expected: " << expectedDuration <<
								"  actual: " << duration <<
								"  delta: " << delta <<
								" (" << framesDelta << " frames)" << endl;
					}
				}
				else
					cout << "Error loading file" << endl;
			}
			else
				cout << "Error reading file" << endl;
				
			delete segment;
		}
	}
	catch (...)
	{
		cout << "exception!" << endl;
	}

	return 0;
}
