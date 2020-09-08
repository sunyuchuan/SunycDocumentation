#define LOG_TAG "FFRenderVideo"
#include "render_video.h"

#define ALIGN(x, mask) ( ((x) + (mask) - 1) & ~((mask) - 1) )

namespace android {

VideoRenderer::VideoRenderer(sp<ANativeWindow> nativewindow, int32_t videoscalingmode):
    mNativeWindow(nativewindow),
    mVideoScalingMode(videoscalingmode),
    mPlayerAddr(NULL),
    mClock(NULL)
{
    ALOGI("[HMP]create VideoRenderer\n");
    mFrameQueue = new FrameQueue();
}

VideoRenderer::~VideoRenderer()
{
    ALOGI("[HMP]destory VideoRenderer\n");
    if(mNativeWindow != NULL) {
        mNativeWindow.clear();
        mNativeWindow = NULL;
    }
    if(mRunning)
    {
        stop();
    }
    delete mFrameQueue;
}

void VideoRenderer::stop()
{
    mFrameQueue->abort();
    ALOGI("waiting on end of render thread");
    int ret = -1;
    if((ret = wait()) != 0) {
        ALOGI("Couldn't cancel Video Renderer: %i", ret);
        return;
    }
}

void VideoRenderer::flush()
{
    mFrameQueue->flush();
}

int VideoRenderer::getQueueSize()
{
    return mFrameQueue->size();
}

void VideoRenderer::checkQueueFull()
{
    mFrameQueue->checkQueueFull();
}

void VideoRenderer::checkQueueEmpty()
{
    mFrameQueue->checkQueueEmpty();
}

int VideoRenderer::enqueue(AVFrameList *frameList)
{
    return mFrameQueue->put(frameList);
}

void VideoRenderer::convertFrameDataFormat(AVFrame *frame,void *dstYUV,ANativeWindowBuffer *buf)
{
    const uint8_t *src_y = (const uint8_t *)frame->data[0];
    const uint8_t *src_u = (const uint8_t *)frame->data[1];
    const uint8_t *src_v = (const uint8_t *)frame->data[2];
    uint8_t *dst_y = (uint8_t *)dstYUV;
    size_t dst_y_size = buf->width * buf->height;
    size_t dst_c_stride = ALIGN(buf->width / 2, 16);
    size_t dst_c_size = dst_c_stride * buf->height / 2;
    uint8_t *dst_v = dst_y + dst_y_size;
    uint8_t *dst_u = dst_v + dst_c_size;

    for (int y = 0; y < buf->height; ++y)
    {
        memcpy(dst_y, src_y, buf->width);
        src_y += frame->linesize[0];
        dst_y += buf->width;
    }

    for (int y = 0; y < (buf->height + 1) / 2; ++y)
    {
        memcpy(dst_u, src_v, (buf->width + 1) / 2);
        memcpy(dst_v, src_u, (buf->width + 1) / 2);
        src_u += frame->linesize[1];
        src_v += frame->linesize[2];
        dst_u += dst_c_stride;
        dst_v += dst_c_stride;
    }
}

int VideoRenderer::writeVideoFrameToSurface(AVFrame* frame, sp<ANativeWindow> nativeWindow)
{
    ANativeWindowBuffer *nativeWindowBuffer = NULL;
    int ret = 0;
    void *dstYUV = NULL;

    CHECK_EQ(0,
        native_window_set_usage(
        nativeWindow.get(),
        GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
        | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP));

    CHECK_EQ(0,
        native_window_set_scaling_mode(
        nativeWindow.get(),
        NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));

    android_native_rect_t crop;
    crop.left = 0;
    crop.top = 0;
    crop.right = frame->width - 1;
    crop.bottom = frame->height - 1;

    CHECK_EQ(0, native_window_set_crop(nativeWindow.get(), &crop));

    CHECK_EQ(0, native_window_set_buffers_dimensions(
        nativeWindow.get(),
        frame->width,
        frame->height));
    CHECK_EQ(0, native_window_set_buffers_format(
        nativeWindow.get(),
        HAL_PIXEL_FORMAT_YV12));

    if ((ret = native_window_dequeue_buffer_and_wait(nativeWindow.get(),&nativeWindowBuffer)) != 0) {
        ALOGE("Surface::dequeueBuffer returned error %d", ret);
        return ret;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(frame->width, frame->height);
    CHECK_EQ(0, mapper.lock(nativeWindowBuffer->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dstYUV));

    ANativeWindowBuffer *buf = nativeWindowBuffer;

    convertFrameDataFormat(frame, dstYUV, buf);

    CHECK_EQ(0, mapper.unlock(nativeWindowBuffer->handle));
    //native_window_set_buffers_timestamp(mThis->mNativeWindow.get(), 100);
    ret = nativeWindow->queueBuffer(nativeWindow.get(), buf, -1);
    if (ret != 0) {
        ALOGE("queueBuffer failed with error %s (%d)", strerror(-ret), -ret);
        return ret;
    }

    //nativeWindow->cancelBuffer(nativeWindow.get(), nativeWindowBuffer, -1);
    //free(dstYUV);
    buf = NULL;
    return ret;
}

int VideoRenderer::process(AVFrameList *Frame)
{
    int ret = 0;
    if(!Frame)
        return 0;
    if(mPause)
        waitOnNotify();
    ret = writeVideoFrameToSurface(Frame->frame, mNativeWindow);
    return ret;
}

bool VideoRenderer::render(void *ptr)
{
    AVFrameList*    Frame = NULL;
    ALOGI("render video start\n");

    while(mRunning)
    {
        if(mFrameQueue->get(&Frame, true) < 0)
        {
            ALOGI("[HMP]mFrameQueue can't get queue & return!\n");
            mRunning = false;
            return false;
        }

        checkQueueEmpty();
        mClock->setVideoClock(Frame->clk);
        mClock->syncVideoClock();

        if(process(Frame) < 0)
        {
            ALOGI("[HMP]process fail & return!\n");
            mRunning = false;
            return false;
        }

        avpicture_free((AVPicture *)Frame->frame);
        av_frame_free(&Frame->frame);
        free(Frame);
    }
    ALOGI("Video Renderer ended\n");
    return true;
}

bool VideoRenderer::prepare()
{
    return true;
}

void VideoRenderer::handleRun(void* ptr)
{
    if(!prepare())
    {
        ALOGE("Couldn't prepare VideoRenderer");
        return;
    }
    render(ptr);
}

}
