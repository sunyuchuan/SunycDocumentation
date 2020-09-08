
#ifndef ANDROID_FFPLAYER_H
#define ANDROID_FFPLAYER_H

#include "baseplayer.h"
#include <gui/Surface.h>
#include <media/stagefright/foundation/ADebug.h>
#include <ui/GraphicBufferMapper.h>
#include "render_video.h"
#include "clock.h"

// ----------------------------------------------------------------------------
namespace android {

class FFPlayer : public BasePlayer
{
public:
    FFPlayer();
    virtual ~FFPlayer();

    virtual status_t setDataSource(const char *url, const KeyedVector<String8, String8> *headers = NULL);
    virtual status_t setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t setDataSource(const sp<IStreamSource> &source UNUSED){return INVALID_OPERATION;}

    virtual status_t prepare();
    virtual status_t prepareAsync();
    virtual status_t start();
    virtual status_t stop();
    virtual status_t pause();
    virtual status_t resume();
    virtual bool     isPlaying() {return !isPause && (mCurrentState == FF_MEDIA_PLAYER_STARTED || mCurrentState == FF_MEDIA_PLAYER_DECODED);}

    virtual status_t seekTo(int msec);
    virtual status_t getCurrentPosition(int *msec);
    virtual status_t getDuration(int *msec);
    virtual status_t getVideoWidth(int *w){*w = mVideoWidth;return NO_ERROR;}
    virtual status_t getVideoHeight(int *h){*h = mVideoHeight;return NO_ERROR;}
    virtual status_t reset();

    virtual status_t suspend();

    virtual status_t invoke(const Parcel &request, Parcel *reply);
    status_t setVideoSurfaceTexture(const sp<IGraphicBufferProducer> &bufferProducer);
    status_t setVideoScalingMode(int32_t mode);

    ///virtual status_t setParameter(int key, const Parcel &request);

    ///virtual status_t getParameter(int key, Parcel *reply);


    ///virtual status_t dump(int fd, const Vector<String16> &args) const;

    ///virtual status_t initCheck();

    ///virtual status_t setLooping(int loop);

    ///virtual status_t setUID(uid_t uid);

    ///virtual status_t setListener(const wp<MediaPlayerBase> &listener);

    ///virtual void     notify(int msg, int ext1 = 0, int ext2 = 0);

private:

    FFPlayer(const FFPlayer &);
    FFPlayer &operator=(const FFPlayer &);

    status_t                    prepare_audio();
    status_t                    prepare_video();
    status_t                    prepare_subtitle();
    bool                        shouldCancel(PacketQueue* queue);
    static void                 ffmpeg_notify(void* ptr, int level, const char* fmt, va_list vl);
    static void*                start_player(void* ptr);

    void                        decode_media(void* ptr);

    status_t                    getTrackInfo(Parcel *reply);

    static int                  output(int16_t* buffer, int buffer_size, void* mPlayerAddr);
    status_t                    create_audio_output(AVCodecContext* codec_ctxs);

    status_t                    start_audio_output();

    status_t                    stop_audio_output();

    int                         write_audio_output(int16_t* buffer, int buffer_size);

    void                        updateCurrentTime();
    void                        toggleBuffering(int buffering);

#if 0 //Do not support cue file in FFPlayer
    status_t                    getCueInfo(Parcel *reply);
    status_t                    getSongInfo(int index,Parcel *reply);
    status_t                    chooseSong(int index);
#endif

    double                      mTime;
    FFclock*                    mClock;
    pthread_mutex_t             mLock;
    pthread_t                   mPlayerThread;
    PacketQueue*                mVideoQueue;
    VideoRenderer *             mVideoRender;

    ///wp<MediaPlayerBase>         mListener;
    ///uid_t                       mUid;
    ///bool                        mLoop;
    ///void*                       mCookie;
    ///float                       mLeftVolume;
    ///float                       mRightVolume;

    AVFormatContext*            mMovieFile;
    APEContext *                mApeCtx;
    int                         mAudioStreamIndex;
    int                         mVideoStreamIndex;
    int                         mSubtitleStreamIndex;
    DecoderAudio*               mDecoderAudio;
    DecoderVideo*             	mDecoderVideo;
    DecoderSubtitle*            mDecoderSubtitle;
    int32_t                     mVideoScalingMode;
    sp<ANativeWindow>           mNativeWindow;

    ff_media_player_states      mCurrentState;
    int                         mDuration;
    int                         mCurrentPosition;
    bool                        mPrepareSync;
    status_t                    mPrepareStatus;
    int                         mStreamType;
    int                         mVideoWidth;
    int                         mVideoHeight;

    bool                        mNoVideo;
    bool                        mNoAudio;
    bool                        mNoSubtitle;
    int                         mAudioFrame;
    Output *                    mOutput;

    uint32_t                    mTotalFrame;

    bool                        isEOF;
    volatile bool               isPause;
    bool                        mFirstPackets;

    int64_t                     mStartTime;
    int64_t                     mPauseTime;

    struct FF_PlayerInfo        mPlayerInfo;
    struct FF_MediaInfo         mMediaInfo;

    struct timeval              startTime;

#if 0 //Do not support cue file in FFPlayer
    cueParse *                  mCueParse;
    CueFileBean                 mCueFile;
    Vector<CueSongBean>         mCueSongs;
    bool                        isCue;
    int                         mCurIdx;
#endif
    int64_t                     mSeekTime;
    void *                      mPlayerAddr;
#ifdef NEED_AUDIOTRACK
    sp<AudioTrack>              mAudioTrack;
#endif
};

}  // namespace android

#endif  // ANDROID_FFPLAYER_H
