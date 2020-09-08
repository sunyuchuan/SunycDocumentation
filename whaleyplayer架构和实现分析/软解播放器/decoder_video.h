#ifndef FFMPEG_DECODER_VIDEO_H
#define FFMPEG_DECODER_VIDEO_H

#include "decoder.h"
#include "render_video.h"
#include "clock.h"

namespace android {

class DecoderVideo : public IDecoder
{
public:
    DecoderVideo(AVStream* stream, int total_frame);
    ~DecoderVideo();
    int allocPicture();
    int queueVideoFrame(double pts, int64_t pos);
    void                        stop();

    void *                      mPlayerAddr;
    VideoRenderer *             mVideoRender;
    FFclock*                    mClock;

private:
    AVFrame*                    mFrame;
    AVFrame*                    mSwsFrame;
    double                      mVideoClock;
    int                         mTotalFrame;
    AVFrameList*                mVideoPicture;
    struct SwsContext*          mConvertCtx;

    bool                        prepare();
    double 						synchronize(AVFrame *src_frame, double pts);
    bool                        decode(void* ptr);
    bool                        process(AVPacket *packet);
    static int					getBuffer(struct AVCodecContext *c, AVFrame *pic);
    static void					releaseBuffer(struct AVCodecContext *c, AVFrame *pic);
};

}
#endif //FFMPEG_DECODER_AUDIO_H
