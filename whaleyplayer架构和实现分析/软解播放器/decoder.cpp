
#define LOG_TAG "FFMpegIDecoder"
#include <utils/Log.h>
#include "decoder.h"

IDecoder::IDecoder(AVStream* stream)
{
    mQueue = new PacketQueue();
    mStream = stream;
}

IDecoder::~IDecoder()
{
    if(mRunning)
    {
        stop();
    }
    delete mQueue;
    avcodec_close(mStream->codec);
}

void IDecoder::enqueue(AVPacket* packet)
{
    mQueue->put(packet);
}

void IDecoder::flush()
{
    mQueue->flush();
}

int IDecoder::packets()
{
    return mQueue->size();
}

void IDecoder::stop()
{
    mQueue->abort();
    DDBG("waiting on end of decoder thread");
    int ret = -1;
    if((ret = wait()) != 0) {
        DDBG("Couldn't cancel IDecoder: %i", ret);
        return;
    }
}

void IDecoder::handleRun(void* ptr)
{
    if(!prepare())
    {
        DDBG("Couldn't prepare decoder");
        return;
    }
    decode(ptr);
}

bool IDecoder::prepare()
{
    return false;
}

bool IDecoder::process(AVPacket *packet)
{
    return false;
}

bool IDecoder::decode(void* ptr)
{
    return false;
}
