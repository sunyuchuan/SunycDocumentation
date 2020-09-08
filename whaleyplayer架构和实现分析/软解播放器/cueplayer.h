
#ifndef ANDROID_CUEPLAYER_H
#define ANDROID_CUEPLAYER_H

#include "baseplayer.h"

// ----------------------------------------------------------------------------
namespace android {

class CuePlayer //: Public BasePlayer
{

public:
    CuePlayer();
    ~CuePlayer();

    status_t initCheck(){return NO_ERROR;}

    status_t setUID(uid_t uid){mUid = uid;return NO_ERROR;}

    status_t setLooping(int loop){mLoop = loop ? true : false;return NO_ERROR;}

    status_t setListener(const wp<MediaPlayerBase> &listener){mListener= listener;return NO_ERROR;}

    status_t setDataSource(const char *url, const KeyedVector<String8, String8> *headers = NULL);

    status_t setDataSource(int fd, int64_t offset, int64_t length);

    status_t setDataSource(const sp<IStreamSource> &source  UNUSED){return INVALID_OPERATION;}

    status_t prepare();
    status_t prepareAsync();
    status_t start();
    status_t stop();
    status_t pause();
    status_t resume();
    status_t seekTo(int msec);
    status_t getCurrentPosition(int *msec);
    status_t getDuration(int *msec);
    status_t reset();
    status_t invoke(const Parcel &request, Parcel *reply);

    status_t setParameter(int key  UNUSED, const Parcel &request  UNUSED){return NO_ERROR;}
    status_t getParameter(int key  UNUSED, Parcel *reply  UNUSED){return NO_ERROR;}

    bool     isPlaying(){return !isPause && (mCurrentState == FF_MEDIA_PLAYER_STARTED || mCurrentState == FF_MEDIA_PLAYER_DECODED);}

    void     notify(int msg, int ext1 = 0, int ext2 = 0){bool send = true/*, locked = false*/;
        if ((mListener != NULL) && send) {
            sp<MediaPlayerBase> listener = mListener.promote();
            if (listener != NULL) {listener->sendEvent(msg, ext1, ext2);}
        }
    }
    status_t suspend();

    status_t dump(int fd  UNUSED, const Vector<String16> &args  UNUSED) const{return INVALID_OPERATION;}

private:

    CuePlayer(const CuePlayer &);
    CuePlayer &operator=(const CuePlayer &);

    status_t                    prepare_audio();
    bool                        shouldCancel(PacketQueue* queue);
    static void                 ffmpeg_notify(void* ptr, int level, const char* fmt, va_list vl);
    static void*                start_player(void* ptr);

    void                        decode_media(void* ptr);
#ifndef OLD_TYPE
	int64_t                     getSongDur(int idx);
#endif
    status_t                    getTrackInfo(Parcel *reply);
    status_t                    getCueInfo(Parcel *reply);
    status_t                    getSongInfo(int index,Parcel *reply);

    status_t                    chooseSong(int index);

    static int                  output(int16_t* buffer, int buffer_size, void* mPlayerAddr);

    status_t                    openFile();

    status_t                    create_audio_output(AVCodecContext* codec_ctxs);

    status_t                    start_audio_output();

    status_t                    stop_audio_output();

    int                         write_audio_output(int16_t* buffer, int buffer_size);

    void                        updateCurrentTime();

    double                      mTime;
    pthread_mutex_t             mLock;
    pthread_t                   mPlayerThread;

    wp<MediaPlayerBase>         mListener;

    AVFormatContext*            mMovieFile;
    APEContext *                mApeCtx;
    int                         mAudioStreamIndex;
    DecoderAudio*               mDecoderAudio;

    void*                       mCookie;
    ff_media_player_states      mCurrentState;
    int64_t                     mDuration;
    int64_t                     mCurrentPosition;
    bool                        mPrepareSync;
    status_t                    mPrepareStatus;
    int                         mStreamType;
    bool                        mLoop;
    float                       mLeftVolume;
    float                       mRightVolume;

    bool                        mNoAudio;
    int                         mAudioFrame;
    uid_t                       mUid;
    Output *                    mOutput;

    uint32_t                    mTotalFrame;

    bool                        isEOF;
    volatile bool               isPause;

    int64_t                     mStartTime;
    int64_t                     mPauseTime;
    int64_t                     mSeekTime;

    bool                        mFFInitialed;
    struct FF_PlayerInfo        mPlayerInfo;
    struct FF_MediaInfo         mMediaInfo;

    cueParse *                  mCueParse;
    CueFileBean                 mCueFile;
    Vector<CueSongBean>         mCueSongs;
    bool                        isCue;
    int                         mCurIdx;
    char                        mRealPath[256];
    char                        mRealPathBackup[256];
    void *                      mPlayerAddr;
#ifdef NEED_AUDIOTRACK
    sp<AudioTrack>              mAudioTrack;
#endif
};

}  // namespace android

#endif  // ANDROID_CUEPLAYER_H
