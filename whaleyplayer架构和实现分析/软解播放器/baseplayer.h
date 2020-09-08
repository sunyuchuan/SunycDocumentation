
#ifndef ANDROID_BASEPLAYER_H
#define ANDROID_BASEPLAYER_H

#include <media/MediaPlayerInterface.h>

#include "decoder_audio.h"
#include "decoder_video.h"
#include "decoder_subtitle.h"
#include "output.h"
#include "cueparse.h"

#define NEED_AUDIOTRACK
#ifdef NEED_AUDIOTRACK
#include <media/AudioTrack.h>
#endif

#define FMP_DBG  ALOGI
#define FMP_ERR  ALOGE

#define UNUSED  __unused

#define FPS_DEBUGGING false

#define FFMPEG_AUDIO_MAX_QUEUE_SIZE 10
#define FFMPEG_VIDEO_MAX_QUEUE_SIZE 5

namespace android {

enum e_ff_player_status{
    FF_PLAYER_IDLE,
    FF_PLAYER_INIT,
    FF_PLAYER_READY,
    FF_PLAYER_PLAYING,
    FF_PLAYER_STOPPED,
    FF_PLAYER_PAUSED,
    FF_PLAYER_TRICKPLAY,
};

typedef struct FF_MediaInfo{
    char * file_name;
    char * file_path;
    int    duration;
    bool   only_audio;

    int    samplerate;
    int    channel;
    int    track_num;
    int    bitrate;
    int    stream_num;
    int    video_width;
    int    video_height;

    int    file_size;
}FF_MediaInfo;

typedef struct FF_PlayerInfo{
    e_ff_player_status player_status;
    bool               initialted;
}FF_PlayerInfo;

enum ff_media_info_type {
    // 0xx
    FF_MEDIA_INFO_UNKNOWN = 1,
    // 7xx
    // The video is too complex for the decoder: it can't decode frames fast
    // enough. Possibly only the audio plays fine at this stage.
    FF_MEDIA_INFO_VIDEO_TRACK_LAGGING = 700,
    // 8xx
    // Bad interleaving means that a media has been improperly interleaved or not
    // interleaved at all, e.g has all the video samples first then all the audio
    // ones. Video is playing but a lot of disk seek may be happening.
    FF_MEDIA_INFO_BAD_INTERLEAVING = 800,
    // The media is not seekable (e.g live stream).
    FF_MEDIA_INFO_NOT_SEEKABLE = 801,
    // New media metadata is available.
    FF_MEDIA_INFO_METADATA_UPDATE = 802,

    FF_MEDIA_INFO_FRAMERATE_VIDEO = 900,
    FF_MEDIA_INFO_FRAMERATE_AUDIO,
};



enum ff_media_player_states {
    FF_MEDIA_PLAYER_STATE_ERROR        = 0,
    FF_MEDIA_PLAYER_IDLE               = 1 << 0,
    FF_MEDIA_PLAYER_INITIALIZED        = 1 << 1,
    FF_MEDIA_PLAYER_PREPARING          = 1 << 2,
    FF_MEDIA_PLAYER_PREPARED           = 1 << 3,
    FF_MEDIA_PLAYER_DECODED            = 1 << 4,
    FF_MEDIA_PLAYER_STARTED            = 1 << 5,
    FF_MEDIA_PLAYER_PAUSED             = 1 << 6,
    FF_MEDIA_PLAYER_STOPPED            = 1 << 7,
    FF_MEDIA_PLAYER_PLAYBACK_COMPLETE  = 1 << 8,
    FF_MEDIA_PLAYER_RESETED            = 1 << 9
};

// ----------------------------------------------------------------------------

class BasePlayer : public RefBase
{
public:
                     BasePlayer() : /*mUid(NULL), */mListener(NULL),mLoop(false)/*,mCookie(NULL)*/ {
                         mLeftVolume = mRightVolume = 1.0;}
    virtual          ~BasePlayer() {}

    virtual status_t initCheck() {return NO_ERROR;}

    virtual status_t setUID(uid_t uid UNUSED){/*mUid = uid;*/return INVALID_OPERATION;}

    virtual status_t setLooping(int loop){mLoop = loop ? true : false;return NO_ERROR;}

    virtual status_t setListener(const wp<MediaPlayerBase> &listener){mListener = listener;return NO_ERROR;}

    virtual status_t setDataSource(const char *url, const KeyedVector<String8, String8> *headers = NULL)=0;

    virtual status_t setDataSource(int fd, int64_t offset, int64_t length)=0;

    virtual status_t setDataSource(const sp<IStreamSource> &source)=0;

    virtual status_t prepare()=0;
    virtual status_t prepareAsync()=0;
    virtual status_t start()=0;
    virtual status_t stop()=0;
    virtual status_t pause()=0;
    virtual status_t resume()=0;
    virtual bool     isPlaying()=0;
    virtual status_t seekTo(int msec)=0;
    virtual status_t getCurrentPosition(int *msec)=0;
    virtual status_t getDuration(int *msec)=0;
    virtual status_t getVideoWidth(int *w)=0;
    virtual status_t getVideoHeight(int *h)=0;
    virtual status_t reset()=0;
	virtual status_t invoke(const Parcel &request, Parcel *reply)=0;
    virtual status_t setParameter(int key UNUSED, const Parcel &request UNUSED){return NO_ERROR;}
    virtual status_t getParameter(int key UNUSED, Parcel *reply UNUSED){return NO_ERROR;}
    virtual void     notify(int msg, int ext1 = 0, int ext2 = 0){bool send = true/*, locked = false*/;
        if ((mListener != NULL) && send) {
            sp<MediaPlayerBase> listener = mListener.promote();
            if (listener != NULL) {listener->sendEvent(msg, ext1, ext2);}
        }
    }
    wp<MediaPlayerBase> getListener(){return mListener;}
    virtual status_t suspend()=0;
    virtual status_t dump(int fd UNUSED, const Vector<String16> &args UNUSED) const{return INVALID_OPERATION;}
private:
    wp<MediaPlayerBase>         mListener;

    ///uid_t						mUid;
    bool                        mLoop;

    ///void*                       mCookie;

    float						mLeftVolume;
    float						mRightVolume;
};

}  // namespace android

#endif  // ANDROID_BASEPLAYER_H
