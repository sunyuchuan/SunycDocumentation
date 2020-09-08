#define LOG_TAG "FFAudioDec"
#include <utils/Log.h>
#include "decoder_audio.h"
#include <math.h>
#define AV_SEEK_COMPLETE_THRESHOLD 2

DecoderAudio::DecoderAudio(AVStream* stream, int total_frame):
    IDecoder(stream),
    mTotalFrame(total_frame),
    mNoVideo(true),
    mNoSubtitle(true),
    mClock(NULL)
{
    ALOGI("[HMP]Create DecoderAudio");
    mCurTime = 0;
    mCurFrame = 0;
    mSamples = NULL;
    mSamplesSize = 0;
    mAudioClock = 0.0;
    mSeekFlag = false;
#ifdef FFMPEG_30
    mFrame = NULL;
    mSwrFrame = NULL;
    mSwrCtx = NULL;
    mResample = false;
#endif
}

DecoderAudio::~DecoderAudio()
{
#ifndef FFMPEG_30
    if(mSamples)
        av_free(mSamples);
    mSamples = NULL;
#else
    if(mFrame)
        av_frame_free(&mFrame);
    if(mSwrFrame)
        av_frame_free(&mSwrFrame);
    if(mSwrCtx)
        swr_free(&mSwrCtx);
    mFrame = NULL;
    mSwrFrame = NULL;
    mSwrCtx = NULL;
    mResample = false;
#endif
}

bool DecoderAudio::prepare()
{
    ALOGI("[HMP]prepare DecoderAudio");

#ifndef FFMPEG_30
    mSamplesSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    mSamples = (int16_t *) av_malloc(mSamplesSize);

    if(mSamples == NULL) {
    	return false;
    }
#else
    mFrame = av_frame_alloc();
    if(mFrame == NULL) {
        return false;
    }
    mSwrFrame = av_frame_alloc();
    if(mSwrFrame == NULL) {
        return false;
    }
	mSwrCtx = swr_alloc();
    if(mSwrCtx == NULL) {
        return false;
    }
    initSwr(mStream->codec);
#endif
    return true;
}

#ifdef FFMPEG_30
void DecoderAudio::initSwr(AVCodecContext *codec)
{
/**************Resample to double track data format**************/
#if 0
    targetChannelLayout = codec->channel_layout;
    targetChannels = codec->channels > 2 ? 2 : codec->channels;

    if (!targetChannelLayout || targetChannels != av_get_channel_layout_nb_channels(targetChannelLayout)) {
        targetChannelLayout = av_get_default_channel_layout(targetChannels);
        targetChannelLayout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    targetChannels = av_get_channel_layout_nb_channels(targetChannelLayout);
    SLOGD("codec->channels %d,codec->channel_layout %lld\n",codec->channels,codec->channel_layout);
    SLOGD("targetChannels %d,targetChannelLayout %lld\n",targetChannels,targetChannelLayout);
#endif
/**************Resample to double track data format**************/
    if (codec->sample_fmt != AV_SAMPLE_FMT_S16)
    {
        av_opt_set_int(mSwrCtx, "in_channel_layout",	codec->channel_layout, 0);
        av_opt_set_int(mSwrCtx, "in_sample_rate",		codec->sample_rate, 0);
        av_opt_set_sample_fmt(mSwrCtx, "in_sample_fmt", codec->sample_fmt, 0);
        av_opt_set_int(mSwrCtx, "out_channel_layout",	 codec->channel_layout, 0);
        av_opt_set_int(mSwrCtx, "out_sample_rate",		 codec->sample_rate, 0);
        av_opt_set_sample_fmt(mSwrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        swr_init(mSwrCtx);
        mResample = true;
    }else{
        mResample = false;
    }
}

int DecoderAudio::TransSample(AVFrame *in_frame, AVFrame *out_frame, AVCodecContext *codec)
{
    int ret;

    in_frame->pts = av_frame_get_best_effort_timestamp(in_frame);
    out_frame->pts = in_frame->pts;

    if (mSwrCtx != NULL)
    {
        out_frame->nb_samples = av_rescale_rnd(swr_get_delay(mSwrCtx, codec->sample_rate) + in_frame->nb_samples,
            codec->sample_rate, codec->sample_rate, AV_ROUND_UP);

        //av_freep(&out_frame->data[0]);
        ret = av_samples_alloc(out_frame->data, &out_frame->linesize[0],
            codec->channels, out_frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
        if (ret < 0)
        {
            ALOGE("av_samples_alloc fail\n");
            return -1;
        }

        ret = swr_convert(mSwrCtx, out_frame->data, out_frame->nb_samples,
            (const uint8_t**)in_frame, in_frame->nb_samples);
        if (ret < 0)
        {
            ALOGE("swr_convert fail\n");
            av_freep(&out_frame->data[0]);
            return -1;
        }
    }
    return 0;
}

void DecoderAudio::videoSync()
{
    mClock->setAudioClock(mAudioClock);
    if(mClock->mSeekFlag)
    {
        mClock->notify();
        mClock->subNotify();
        double diff = mClock->getVideoClock() - mClock->getAudioClock();
        if(fabs(diff) < AV_SEEK_COMPLETE_THRESHOLD)
        {
            mClock->mSeekFlag = false;
            ALOGI("Seek complete,set mSeekFlag for false\n");
        }
    }
    mClock->checkVideoClock();
}

void DecoderAudio::subtitleSync()
{
    mClock->setAudioClock(mAudioClock);
    double subStartClock,subEndClock;
    mClock->getSubClock(&subStartClock,&subEndClock);
    double diff0 = mClock->getAudioClock() - subStartClock;
    double diff1 = mClock->getAudioClock() - subEndClock;
    static bool subSyncFlag = true;
    if(diff0 < -AS_SYNC_THRESHOLD)
    {
        subSyncFlag = false;
    }
    if(diff0 >= -AS_SYNC_THRESHOLD && diff1 <= 0 && !subSyncFlag)
    {
        mClock->subNotify();
        subSyncFlag = true;
    }
    if(diff1 > 0)
    {
        mClock->subNotify();
    }
}

void DecoderAudio::calculateAudioClock(int data_size, AVPacket* pkt)
{
    double temp = 0.0;
/*
    AVRational tb = (AVRational){1, mFrame->sample_rate};
    if (mFrame->pts != AV_NOPTS_VALUE)
        mFrame->pts = av_rescale_q(mFrame->pts, mStream->codec->time_base, tb);
    else if (mFrame->pkt_pts != AV_NOPTS_VALUE)
        mFrame->pts = av_rescale_q(mFrame->pkt_pts, av_codec_get_pkt_timebase(mStream->codec), tb);
	else if (next_pts != AV_NOPTS_VALUE)
        mFrame->pts = av_rescale_q(next_pts, next_pts_tb, tb);
    if (mFrame->pts != AV_NOPTS_VALUE) {
        next_pts = mFrame->pts + mFrame->nb_samples;
        next_pts_tb = tb;
    }
*/
    temp = (double)data_size /
        (mStream->codec->channels * mStream->codec->sample_rate * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

    if (pkt->pts == AV_NOPTS_VALUE)
        mAudioClock += temp;
    else
        mAudioClock = pkt->pts*av_q2d(mStream->time_base) + temp;

    if(!mNoVideo)
    {
        videoSync();
    }
    if(!mNoSubtitle)
    {
        subtitleSync();
    }
}
#endif

bool DecoderAudio::process(AVPacket *packet)
{
    int len = 0;
    int totalSize = 0;
    int ret = -1;
    int data_size = 0, size = 0;

    uint8_t *audiodata = (uint8_t *)av_malloc(packet->size);
    memcpy(audiodata, packet->data, packet->size);
    AVPacket  pPacket = *packet;
    pPacket.size = packet->size;
    pPacket.data = audiodata;
    while(pPacket.size > 0){

        if(mQueue->getAbortRequest() || mSeekFlag)
        {
            ALOGI("AbortRequest or mSeekFlag is true, DecoderAudio::process exit\n");
            break;
        }

        if(mPause)
            waitOnNotify();

#ifndef FFMPEG_30
        size = mSamplesSize;
        int len2 = ff_decode_audio(mStream->codec, mSamples, &size, &pPacket);
#else
        int len2 = ff_decode_audio(mStream->codec, mFrame, &size, &pPacket);
#endif
        if(len2 < 0){
            ALOGE("ff_decode_audio fail,len2:%d\n",len2);
            av_free(audiodata);
            audiodata = NULL;
            return true;
        }

#ifdef FFMPEG_30
        if(mResample == true)
        {
            ret = TransSample(mFrame,mSwrFrame,mStream->codec);
            if(ret < 0)
            {
                ALOGE("TransSample fail,ret:%d\n",ret);
                goto fail;
            }
        }
        else
            mSwrFrame = mFrame;

        if(size){
            data_size = av_samples_get_buffer_size(NULL,  mStream->codec->channels,
				                     mSwrFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);
            if (data_size < 0) {
                ALOGE("Failed to calculate data size\n");
                return false;
            }

            calculateAudioClock(data_size, packet);
            size = data_size;
            mSamples = (int16_t*)mSwrFrame->data[0];
        } else {
            ALOGE("the size is zero,goto fail,size:%d\n",size);
            if(mResample == true)
            {
                av_freep(&mSwrFrame->data[0]);
            }
            goto fail;
        }
#endif
        //ALOGI("[HMP]decode audio len=%d size=%d duration=%d",len,size,pPacket.duration);
        //call handler for posting buffer to os audio driver
        ret = onDecode(mSamples, size, mPlayerAddr);
        if(mResample == true)
        {
            av_freep(&mSwrFrame->data[0]);
        }
        if (ret <= 0){
            av_free(audiodata);
            audiodata = NULL;
            return false;
        }
fail:
        pPacket.size -= len2;
        pPacket.data += len2;
        len += len2;
        totalSize += size;
    }
    mCurTime = pPacket.pts;
    av_free(audiodata);
    audiodata = NULL;
    //ALOGI("[HMP]audio process packet ok: framesize=%d packetsize=%d --------- cur pts = %lld",totalSize, len, mCurTime);
    return true;
}

bool DecoderAudio::decode(void* ptr)
{
    AVPacket  pPacket;

    while(mRunning)
    {
        //if(mLastFrame)
              //mRunning = false;

        mSleeping = mQueue->isEmpty();
        if(mQueue->get(&pPacket, true) < 0)
        {
            ALOGI("[HMP]mQueue can't get queue & return!");
            mRunning = false;
            return false;
        }

        if(mSeekFlag)
        {
            mSeekFlag = false;
            ALOGI("drop the first audio packet of seek complete!");
            goto drop;
        }

        if(!process(&pPacket))
        {
            ALOGI("[HMP]process fail & return!");
            ff_free_packet(&pPacket);
            mRunning = false;
            return false;
        }

        mCurFrame++;
        //ALOGI("=====decode: totalframe:%d  curframe is %d",mTotalFrame,mCurFrame);
        if((mTotalFrame != 0) && (mCurFrame == (mTotalFrame-1))){
            ALOGI("=====Current frame is the last frame!");
            mLastFrame = true;
        }
drop:
        // Free the packet that was allocated by av_read_frame
        ff_free_packet(&pPacket);
    }

    // Free audio samples buffer
    //av_free(mSamples);
    return true;
}
