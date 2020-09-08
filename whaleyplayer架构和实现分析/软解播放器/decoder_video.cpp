
#define LOG_TAG "FFVideoDec"
#include <utils/Log.h>
#include "decoder_video.h"

namespace android {

static uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;

DecoderVideo::DecoderVideo(AVStream* stream, int total_frame):
    IDecoder(stream),
    mTotalFrame(total_frame),
    mClock(NULL)
{
    mVideoClock = 0.0;
    mSwsFrame = NULL;
    mVideoPicture = NULL;
    mConvertCtx = NULL;
#if 0
    mStream->codec->get_buffer = getBuffer;
    mStream->codec->release_buffer = releaseBuffer;
#endif
}

DecoderVideo::~DecoderVideo()
{
    if(mFrame)
        av_frame_free(&mFrame);
    mFrame = NULL;
    if(mConvertCtx)
        sws_freeContext(mConvertCtx);
    mConvertCtx = NULL;
}

void DecoderVideo::stop()
{
    if(mClock != NULL) {
        mClock->stop();
    }

    if(mVideoRender != NULL)
    {
        mVideoRender->stop();
    }

    mQueue->abort();
    ALOGI("waiting on end of DecoderVideo thread");
    int ret = -1;
    if((ret = wait()) != 0) {
        ALOGI("Couldn't cancel DecoderVideo: %i", ret);
        return;
    }
}

bool DecoderVideo::prepare()
{
    mFrame = av_frame_alloc();
    if (mFrame == NULL) {
        return false;
    }

    mConvertCtx = ff_getContext(mStream->codec);
    if (mConvertCtx == NULL) {
        return false;
    }
    return true;
}

double DecoderVideo::synchronize(AVFrame *src_frame, double pts) {

    double frame_delay;

    if (pts != 0) {
        /* if we have pts, set video clock to it */
        mVideoClock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        pts = mVideoClock;
    }
    /* update the video clock */
    frame_delay = av_q2d(mStream->codec->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    mVideoClock += frame_delay;
    return pts;
}

int DecoderVideo::allocPicture()
{
    int ret = 0;
    AVCodecContext* codec_ctx = mStream->codec;

    mSwsFrame = av_frame_alloc();
    if (mSwsFrame == NULL) {
        ALOGE("[HMP]av_frame_alloc fail!\n");
        return -1;
    }

    ret = avpicture_alloc((AVPicture *)mSwsFrame,AV_PIX_FMT_YUV420P,codec_ctx->width, codec_ctx->height);
    if (ret < 0) {
        ALOGE("[HMP]avpicture_alloc fail!\n");
        return -1;
    }

    mVideoPicture = (AVFrameList *)malloc(sizeof(AVFrameList));
    if (mVideoPicture == NULL) {
        ALOGE("[HMP]av_malloc fail!\n");
        return -1;
    }

    return 0;
}

int DecoderVideo::queueVideoFrame(double pts, int64_t pos)
{
    int ret = 0;
    ret = allocPicture();
    if(ret < 0)
    {
        ALOGE("allocPicture fail\n");
        return -1;
    }

    // Convert the image from its native format to RGB
    ff_scale(mConvertCtx, mFrame, mSwsFrame, 0, mStream->codec->height);

    mSwsFrame->width = mFrame->width;
    mSwsFrame->height = mFrame->height;
    mVideoPicture->frame = mSwsFrame;
    mVideoPicture->pts = pts;
    mVideoPicture->pos = pos;
    mVideoPicture->clk = mVideoClock;

    mVideoRender->checkQueueFull();
    ret = mVideoRender->enqueue(mVideoPicture);
    return ret;
}

bool DecoderVideo::process(AVPacket *packet)
{
    int	completed = 0;
    double pts = 0.0;
    int64_t pos = 0;

    if(mPause)
        waitOnNotify();

    int len = ff_decode_video(mStream->codec, mFrame, &completed, packet);
#if 0
    if (packet->dts == AV_NOPTS_VALUE && mFrame->opaque
            && *(uint64_t*) mFrame->opaque != AV_NOPTS_VALUE) {
        pts = *(uint64_t *) mFrame->opaque;
    } else if (packet->dts != AV_NOPTS_VALUE) {
        pts = packet->dts;
    } else {
        pts = 0;
    }
#else
    int decoder_reorder_pts = -1;
    if (decoder_reorder_pts == -1) {
        mFrame->pts = av_frame_get_best_effort_timestamp(mFrame);
    } else if (mFrame->pkt_pts != AV_NOPTS_VALUE) {
        mFrame->pts = mFrame->pkt_pts;
    } else if (mFrame->pkt_dts != AV_NOPTS_VALUE) {
        mFrame->pts = mFrame->pkt_dts;
    } else {
         mFrame->pts = 0;
    }

    pos = packet->pos;
    pts = mFrame->pts*av_q2d(mStream->time_base);
#endif

    if (completed) {
        pts = synchronize(mFrame, pts);

        queueVideoFrame(pts, pos);
        //SLOGD("queueVideoFrame number %d",mVideoRender->getQueueSize());
        return true;
    }
    return false;
}

bool DecoderVideo::decode(void* ptr)
{
    AVPacket        pPacket;

    ALOGV("decoding video");

    while(mRunning)
    {
        mSleeping = mQueue->isEmpty();
        if(mQueue->get(&pPacket, true) < 0)
        {
            ALOGI("[HMP]mQueue can't get queue & return!");
            mRunning = false;
            return false;
        }
        if(!process(&pPacket))
        {
            //mRunning = false;
            //return false;
        }
        // Free the packet that was allocated by av_read_frame
        ff_free_packet(&pPacket);
    }
    ALOGV("decoding video ended");
    return true;
}

/* These are called whenever we allocate a frame
 * buffer. We use this to store the global_pts in
 * a frame at the time it is allocated.
 */
#if 0
int DecoderVideo::getBuffer(struct AVCodecContext *c, AVFrame *pic) {
    int ret = avcodec_default_get_buffer(c, pic);
    uint64_t *pts = (uint64_t *)av_malloc(sizeof(uint64_t));
    *pts = global_video_pkt_pts;
    pic->opaque = pts;
    return ret;
}
void DecoderVideo::releaseBuffer(struct AVCodecContext *c, AVFrame *pic) {
    if (pic)
        av_freep(&pic->opaque);
    avcodec_default_release_buffer(c, pic);
}
#endif

}
