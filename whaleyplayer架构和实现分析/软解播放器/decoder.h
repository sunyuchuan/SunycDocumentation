#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H

extern "C" {
	
#include "ff_func.h"

}

#include "thread.h"
#include "packetqueue.h"

#define DDBG  ALOGI

class IDecoder : public FFThread
{
public:
	IDecoder(AVStream* stream);
	~IDecoder();

    virtual void                stop();
	void						enqueue(AVPacket* packet);
    void						flush();
	int							packets();

    int64_t 					mCurTime;
	int                         mCurFrame;

protected:
    PacketQueue*                mQueue;
    AVStream*             		mStream;

    virtual bool                prepare();
    virtual bool                decode(void* ptr);
    virtual bool                process(AVPacket *packet);
	void						handleRun(void* ptr);
};

#endif //FFMPEG_DECODER_H
