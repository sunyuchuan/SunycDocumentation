#ifndef ANDROID_RENDER_VIDEO_H
#define ANDROID_RENDER_VIDEO_H

#include <utils/Log.h>
#include <gui/Surface.h>
#include <media/stagefright/foundation/ADebug.h>
#include <ui/GraphicBufferMapper.h>

#include "framequeue.h"
#include "thread.h"
#include "clock.h"

namespace android {

class VideoRenderer : public FFThread
{
public:
    VideoRenderer(sp<ANativeWindow> nativewindow, int32_t videoscalingmode);
    ~VideoRenderer();

    void convertFrameDataFormat(AVFrame *,void *,ANativeWindowBuffer *);
    int writeVideoFrameToSurface(AVFrame *,sp<ANativeWindow>);
    int render(AVFrame *,sp<ANativeWindow>);

    void checkQueueFull();
    void checkQueueEmpty();
    void stop();
    void flush();
    bool prepare();
    void handleRun(void* ptr);
    int process(AVFrameList *);
    int enqueue(AVFrameList *);
    int getQueueSize();
    bool render(void *ptr);
    void *                      mPlayerAddr;
    FFclock*                    mClock;
private:
    sp <ANativeWindow>          mNativeWindow;
    int32_t                     mVideoScalingMode;
    FrameQueue *                mFrameQueue;
};

}
#endif

