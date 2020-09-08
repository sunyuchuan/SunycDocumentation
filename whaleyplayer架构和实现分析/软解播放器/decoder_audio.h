#ifndef FFMPEG_DECODER_AUDIO_H
#define FFMPEG_DECODER_AUDIO_H

#include "decoder.h"
#include "clock.h"

typedef int (*AudioDecodingHandler) (int16_t*,int,void*);

class DecoderAudio : public IDecoder
{
public:
    DecoderAudio(AVStream* stream,int total_frame);

    ~DecoderAudio();
#ifdef FFMPEG_30
    void initSwr(AVCodecContext *codec);
    int TransSample(AVFrame *in_frame, AVFrame *out_frame, AVCodecContext *codec);
    void calculateAudioClock(int data_size, AVPacket *pkt);
    void videoSync();
    void subtitleSync();
#endif
    AudioDecodingHandler		onDecode;
    void *                      mPlayerAddr;
    FFclock*                    mClock;
    bool                        mNoVideo;
    bool                        mNoSubtitle;
    bool                        mSeekFlag;

private:
    int16_t*                    mSamples;
    AVFrame*                    mFrame;
    AVFrame*                    mSwrFrame;
#ifdef FFMPEG_30
    SwrContext*                 mSwrCtx;
#endif
    int                         mSamplesSize;
    int                         mTotalFrame;
    bool                        mResample;
    double                      mAudioClock;
    uint64_t                    targetChannelLayout;
    int                         targetChannels;

    bool                        prepare();
    bool                        decode(void* ptr);
    bool                        process(AVPacket *packet);
};

#endif //FFMPEG_DECODER_AUDIO_H
