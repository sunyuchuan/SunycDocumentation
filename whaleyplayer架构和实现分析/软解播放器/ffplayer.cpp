
//#define LOG_NDEBUG 0
#define LOG_TAG "FFPlayer"
#include <utils/Log.h>

#include "ffplayer.h"

#include <media/Metadata.h>

namespace android {

extern "C" {

#include "ff_func.h"

} // end of extern C

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------



FFPlayer::FFPlayer():
#ifdef NEED_AUDIOTRACK
    mAudioTrack(NULL),
#endif
    mNoVideo(false),
    mNoAudio(false),
    mNoSubtitle(false),
    mCurrentState(FF_MEDIA_PLAYER_IDLE),
    mStartTime(-1),
    mDuration(-1),
    mCurrentPosition(-1),
    mSeekTime(-1),
    mTotalFrame(0),
    mAudioFrame(0),
    mApeCtx(NULL),
    isEOF(false),
    isPause(false),
    mStreamType(MUSIC),
    mPrepareSync(false),
    mPrepareStatus(NO_ERROR),
#if 0 //Do not support cue file in FFPlayer
    mCueParse(NULL),
    isCue(false),
    mCurIdx(-1),
#endif
    ///mListener(NULL),
    ///mUid(NULL),
    ///mCookie(NULL),
    ///mLoop(false),
    mVideoStreamIndex(-1),
    mAudioStreamIndex(-1),
    mSubtitleStreamIndex(-1),
    mDecoderAudio(NULL),
    mDecoderVideo(NULL),
    mVideoRender(NULL),
    mMovieFile(NULL),
    mOutput(NULL)
{
    FMP_DBG("[HMP]FFPlayer");
    memset(&mPlayerInfo,0,sizeof(mPlayerInfo));
    memset(&mMediaInfo,0,sizeof(mMediaInfo));
#if 0 //Do not support cue file in FFPlayer
    memset(&mCueFile,0,sizeof(CueFileBean));
#endif

    mVideoWidth = mVideoHeight = 0;
    mPlayerAddr = (void *)this;
    pthread_mutex_init(&mLock, NULL);
    mVideoScalingMode = -1;
    mNativeWindow = NULL;
    mClock = new FFclock();
    mFirstPackets = true;

    ff_initial();
}

FFPlayer::~FFPlayer() {
    FMP_DBG("[HMP]~FFPlayer");
    reset();
    mDecoderAudio = NULL;
    mDecoderVideo = NULL;
    mMovieFile = NULL;
    mOutput = NULL;
    mPlayerAddr = NULL;
    ff_deinit();
    pthread_mutex_destroy(&mLock);
}


status_t FFPlayer::setDataSource(
        const char *url,
        const KeyedVector<String8, String8> *headers) {
    FMP_DBG("[HMP]setDataSource(%s)",url);
    gettimeofday(&startTime, NULL);

    char realPath[1024] = {0};

    if (strcmp(url+strlen(url)-4, ".cue") == 0) {
        FMP_DBG("[HMP]setDataSource: This is CUE file (%s)!!!!",url);
#if 0 //Do not support cue file in FFPlayer
        isCue = true;
        mCueParse = new cueParse();
        if(mCueParse){
            char cuepath[strlen(url)];
            strcpy(cuepath,url);
            mCueParse->parseCueFile(cuepath, &mCueFile, &mCueSongs);
            delete mCueParse;
        }
        //strncpy(realPath, url, strlen(url)-4);
        //strcpy(realPath + strlen(url)-4, ".ape");
        strcpy(realPath, mCueFile.mediaPath);
        if(access(realPath, F_OK) != 0){
            FMP_ERR("[HMP]%s is not exist!",realPath);
            return BAD_VALUE;
        }
#else
        return INVALID_OPERATION;
#endif
    }else if(strcmp(url+strlen(url)-4, ".ape") == 0){
        FMP_DBG("[HMP]setDataSource: This is APE file (%s)!!!!",url);
        strcpy(realPath, url);
    }else{
        strcpy(realPath, url);
    }

    // Open media file
    ALOGI("===open:%s===",realPath);
    char * suffix = strrchr(realPath, '.');
    if((NULL != suffix) && (!strcmp(suffix, ".iso") || !strcmp(suffix, ".ISO")))
    {
        char realURL[1024] = {0};
        int len = 0;
        len = snprintf(realURL, strlen("bluray://")+1, "%s", "bluray://");
        len += snprintf(realURL + len, strlen(realPath)+1, "%s", realPath);
        strcpy(realPath, realURL);
        ALOGI("the iso file path add bluray-head %s",realPath);
    }

    if(ff_open_input_file(&mMovieFile, realPath) != 0) {
        FMP_ERR("[HMP]ff_open_input_file fail! %s", realPath);
        return INVALID_OPERATION;
    }
    // Retrieve stream information
    if(ff_find_stream_info(mMovieFile) < 0) {
        FMP_ERR("[HMP]ff_find_stream_info fail!");
        return INVALID_OPERATION;
    }

    mCurrentState = FF_MEDIA_PLAYER_INITIALIZED;

    return NO_ERROR;
}

status_t FFPlayer::setDataSource(int fd, int64_t offset, int64_t length) {
    FMP_DBG("[HMP]setDataSource(%d, %lld, %lld)", fd, offset, length);
    gettimeofday(&startTime, NULL);

    char s[256], name[256];
    memset(s,0,sizeof(s));
    memset(name,0,sizeof(s));
    snprintf(s, 255, "/proc/%d/fd/%d", getpid(), fd);
    readlink(s, name, 255);
    close(fd);
    return setDataSource(strdup(name));
}

status_t FFPlayer::prepare() {
    FMP_DBG("[HMP]prepare");
    status_t ret;
    if(mCurrentState != FF_MEDIA_PLAYER_INITIALIZED){
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_PREPARING;
    mOutput = new Output();

    if(mMovieFile->filename)
        FMP_DBG("[HMP]filename:%s",mMovieFile->filename);

    //ff_log_set_callback(ffmpeg_notify);
#ifdef FFMPEG_30
    if ((ret = prepare_video()) != NO_ERROR) {
        FMP_ERR("[HMP]prepareVideo fail!");
        mNoVideo = true;
    }
#else
    mNoVideo = true;
#endif
    if ((ret = prepare_audio()) != NO_ERROR) {
        FMP_ERR("[HMP]prepareAudio fail!");
        mNoAudio = true;
    }

    if ((ret = prepare_subtitle()) != NO_ERROR) {
        FMP_ERR("[HMP]prepareSubtitle fail!");
        mNoSubtitle = true;
    }

    if(mNoAudio && mNoVideo){
        mCurrentState = FF_MEDIA_PLAYER_STATE_ERROR;
        return ret;
    }

    if(mNoAudio && !mNoVideo){
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_PURE_VIDEO);
    }
    if(!mNoAudio && mNoVideo){
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_PURE_AUDIO);
    }

    mDuration =  mMovieFile->duration / 1000;
    FMP_DBG("[DURATION]prepare: get duration is %d",mDuration);

    mCurrentState = FF_MEDIA_PLAYER_PREPARED;
    return NO_ERROR;
}

status_t FFPlayer::prepareAsync() {
    FMP_DBG("[HMP]prepareAsync");
    mPrepareSync = true;
    status_t ret = prepare();
    notify(MEDIA_PREPARED);
    return ret;
}

status_t FFPlayer::start() {

    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED || isPause){
        return resume();
    }

    FMP_DBG("[HMP]start");

    if (mCurrentState != FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_STARTED;
    pthread_create(&mPlayerThread, NULL, start_player, mPlayerAddr);

    return NO_ERROR;
}

status_t FFPlayer::stop() {
    FMP_DBG("[HMP]stop");
    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED || isPause){
        resume();
    }
    mCurrentState = FF_MEDIA_PLAYER_STOPPED;
    return NO_ERROR;  // what's the difference?
}

status_t FFPlayer::pause() {
    FMP_DBG("[HMP]pause");
    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED){
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_PAUSED;
    if(!mNoAudio){
        mDecoderAudio->mPause = true;
    }

    if(!mNoVideo){
        mDecoderVideo->mPause = true;
        mVideoRender->mPause = true;
    }

    if(!mNoSubtitle){
        mDecoderSubtitle->mPause = true;
    }

    isPause = true;

    mPauseTime = getCurSysTime();
    return NO_ERROR;
}

status_t FFPlayer::resume() {
    FMP_DBG("[HMP]resume");
    int64_t deltaTime = getCurSysTime() - mPauseTime;
    if(deltaTime > 0)
        mStartTime +=  deltaTime;
    if(!mNoAudio){
        mDecoderAudio->mPause = false;
        mDecoderAudio->notify();
    }

    if(!mNoVideo){
        mDecoderVideo->mPause = false;
        mDecoderVideo->notify();
        mVideoRender->mPause = false;
        mVideoRender->notify();
    }

    if(!mNoSubtitle){
        mDecoderSubtitle->mPause = false;
        mDecoderSubtitle->notify();
    }

    isPause = false;
    mCurrentState = FF_MEDIA_PLAYER_STARTED;
    return NO_ERROR;
}

status_t FFPlayer::seekTo(int msec) {
    FMP_DBG("[HMP]seekTo %.2f secs", msec / 1E3);
#if 0
    if((msec > mDuration) || (mMovieFile == NULL) || (mAudioStreamIndex == -1)){
        FMP_ERR("seek to fail: seektime(%d) bigger than duration(%d) or mMovieFile is null", msec, mDuration);
        return BAD_VALUE;
    }
    int64_t seekTime = ff_rescale_q(msec,AV_TIME_BASE_Q,mMovieFile->streams[mAudioStreamIndex]->time_base);
    int flags = AVSEEK_FLAG_ANY;
    if(ff_seek_frame(mMovieFile, mAudioStreamIndex, msec, flags) < 0){
        FMP_ERR("seek to fail: ff_seek_frame fail(%d)",seekTime);
        return INVALID_OPERATION;
    }
#else
    mSeekTime = msec;
    if(mDuration) mSeekTime %= mDuration;
#endif
    return NO_ERROR;
}

status_t FFPlayer::getCurrentPosition(int *msec) {
    //FMP_DBG("[HMP]getCurrentPosition");
#if 0
    //int64_t positionUs = ff_gettime();
    //*msec = (positionUs + 500) / 1000;

    AVStream* stream = mMovieFile->streams[mAudioStreamIndex];
    if(stream->pts.den == 0){
        if(mTotalFrame){
            mCurrentPosition = (mDuration * mDecoderAudio->mCurFrame) / mTotalFrame;
        }else{
            mCurrentPosition = mDecoderAudio->mCurTime;
        }
    }else{
        mCurrentPosition = (stream->pts.val * stream->pts.num * 1000) / stream->pts.den;
    }
#else
    if(mCurrentPosition < 0)  mCurrentPosition = 0;
    if(mCurrentPosition > mDuration) mCurrentPosition = mDuration;
#endif
    *msec = mCurrentPosition;

    return NO_ERROR;
}

status_t FFPlayer::getDuration(int *msec) {
    //FMP_DBG("[HMP]getDuration");

    if (mCurrentState < FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    *msec = mDuration;
    return NO_ERROR;
}

status_t FFPlayer::reset() {
    FMP_DBG("[HMP]reset");
    if(mCurrentState == FF_MEDIA_PLAYER_RESETED
        || FF_MEDIA_PLAYER_IDLE == mCurrentState){
        return INVALID_OPERATION;
    }

    stop();
#ifdef NEED_AUDIOTRACK
    pthread_mutex_lock(&mLock);
    if(mAudioTrack != NULL) {
        mAudioTrack->stop();
        mAudioTrack->flush();
        mAudioTrack.clear();
        mAudioTrack = NULL;
    }
    pthread_mutex_unlock(&mLock);
#else
    //close OS drivers
    if(mOutput){
        mOutput->AudioDriver_unregister();
        mOutput->VideoDriver_unregister();
        delete(mOutput);
        mOutput = NULL;
    }
#endif
    suspend();

    mCurrentState = FF_MEDIA_PLAYER_RESETED;

    return NO_ERROR;
}


status_t FFPlayer::invoke(const Parcel &request, Parcel *reply) {
    FMP_DBG("[HMP]invoke");
    if (NULL == reply)
    {
        return android::BAD_VALUE;
    }
    int32_t methodId;
    status_t ret = request.readInt32(&methodId);
    if (ret != android::OK)
    {
        return ret;
    }

    switch(methodId){
        case INVOKE_ID_GET_TRACK_INFO:
        {
            return getTrackInfo(reply);
        }
        case INVOKE_ID_SET_VIDEO_SCALING_MODE:
        {
            return setVideoScalingMode(request.readInt32());
        }
        case INVOKE_ID_SELECT_TRACK:
        {
            int type;
            int trackIndex = request.readInt32();
            ALOGI("INVOKE_ID_SELECT_TRACK trackIndex %d",trackIndex);
            return OK;
        }
        case INVOKE_ID_GET_SELECTED_TRACK:
        {
            int32_t selected;
            int32_t type = request.readInt32();
            ALOGI("INVOKE_ID_GET_SELECTED_TRACK type = %x", type);
            switch (type) {
            case MEDIA_TRACK_TYPE_VIDEO:
                selected = mVideoStreamIndex;
                break;
            case MEDIA_TRACK_TYPE_AUDIO:
                selected = mAudioStreamIndex;
                break;
            case MEDIA_TRACK_TYPE_TIMEDTEXT:
                selected = mSubtitleStreamIndex;
                break;
            default:
                selected = -1;
                break;
            }
            return reply->writeInt32(selected);
        }
#if 0 //Do not support cue file in FFPlayer
        case INVOKE_ID_GET_CUE_INFO:
        {
            return getCueInfo(reply);
        }
        case INVOKE_ID_SELECT_CUE_SONG:
        {
            int idx = request.readInt32(&methodId);
            getSongInfo(idx, reply);
            return chooseSong(idx);
        }
#endif
        default:
            break;
    }
    return INVALID_OPERATION;
}

status_t FFPlayer::prepare_audio()
{
    FMP_DBG("[HMP]prepareAudio");
    for (int i = 0; i < mMovieFile->nb_streams; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
            mAudioStreamIndex = i;
            break;
        }
    }

    if (mAudioStreamIndex == -1) {
        FMP_ERR("[HMP]mAudioStreamIndex invalid!");
        return INVALID_OPERATION;
    }

    AVStream* stream = mMovieFile->streams[mAudioStreamIndex];
    // Get a pointer to the codec context for the video stream
    AVCodecContext* codec_ctx = stream->codec;
    AVCodec* codec = ff_find_decoder(codec_ctx->codec_id);
    if (codec == NULL) {
        FMP_ERR("[HMP]codec is null!");
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_AUDIO_UNSUPPORT);
        return INVALID_OPERATION;
    }

    // Open codec
    if (ff_codec_open(codec_ctx, codec) < 0) {
        FMP_ERR("[HMP]ff_codec_open fail!");
        return INVALID_OPERATION;
    }

    // prepare os output
    if(NO_ERROR != create_audio_output(codec_ctx))
        return INVALID_OPERATION;

    if(NO_ERROR != start_audio_output())
        return INVALID_OPERATION;

    FMP_DBG("[HMP]prepareAudio  get audio stream id=%d",mAudioStreamIndex);

    prtAudioContext(mMovieFile);

    prtStrInfo(stream);

    if(strcmp(codec->name,"ape") == 0){
        mApeCtx = (APEContext *)mMovieFile->priv_data;
        mTotalFrame = mApeCtx->totalframes;
        ///prtApeInfo(mApeCtx);
    }

    return NO_ERROR;
}


status_t FFPlayer::prepare_video()
{
    FMP_DBG("[HMP]prepareVideo");
    if(mNativeWindow == NULL)
    {
        FMP_ERR("[HMP]mNativeWindow is NULL!");
        return INVALID_OPERATION;
    }
    // Find the first video stream
    for (int i = 0; i < mMovieFile->nb_streams; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
            mVideoStreamIndex = i;
            break;
        }
    }

    if (mVideoStreamIndex == -1) {
        FMP_ERR("[HMP]mVideoStreamIndex invalid!");
        return INVALID_OPERATION;
    }

    AVStream* stream = mMovieFile->streams[mVideoStreamIndex];
    // Get a pointer to the codec context for the video stream
    AVCodecContext* codec_ctx = stream->codec;
    AVCodec* codec = ff_find_decoder(codec_ctx->codec_id);
    if (codec == NULL) {
        FMP_ERR("[HMP]codec is null!");
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_VIDEO_UNSUPPORT);
        return INVALID_OPERATION;
    }

    // Open codec
    if (ff_codec_open(codec_ctx, codec) < 0) {
        FMP_ERR("[HMP]ff_codec_open fail!");
        return INVALID_OPERATION;
    }

    mVideoWidth = codec_ctx->width;
    mVideoHeight = codec_ctx->height;
    notify(MEDIA_SET_VIDEO_SIZE,mVideoWidth,mVideoHeight);
    //codec->capabilities |= CODEC_CAP_DELAY;
    //codec_ctx->active_thread_type |= 1;

    prtVideoContext(mMovieFile);

    prtStrInfo(stream);

    return NO_ERROR;
}

status_t FFPlayer::prepare_subtitle()
{
    FMP_DBG("[HMP]prepareSubtitle");
    for (int i = 0; i < mMovieFile->nb_streams; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_SUBTITLE) {
            mSubtitleStreamIndex = i;
            break;
        }
    }

    if (mSubtitleStreamIndex == -1) {
        FMP_ERR("[HMP]mSubtitleStreamIndex invalid!");
        return INVALID_OPERATION;
    }

    AVStream* stream = mMovieFile->streams[mSubtitleStreamIndex];
    // Get a pointer to the codec context for the video stream
    AVCodecContext* codec_ctx = stream->codec;
    AVCodec* codec = ff_find_decoder(codec_ctx->codec_id);
    if (codec == NULL) {
        FMP_ERR("[HMP]subtitle codec is null!");
        return INVALID_OPERATION;
    }

    // Open codec
    if (ff_codec_open(codec_ctx, codec) < 0) {
        FMP_ERR("[HMP]ff_codec_open fail!");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t FFPlayer::suspend() {
    FMP_DBG("[HMP]suspend");

    mCurrentState = FF_MEDIA_PLAYER_STOPPED;

    if(!mNoAudio){
        if(mDecoderAudio != NULL) {
            mDecoderAudio->stop();
        }
    }

    if(!mNoVideo){
        if(mDecoderVideo != NULL) {
            mDecoderVideo->stop();
        }
    }

    if(!mNoSubtitle)
    {
        if(mDecoderSubtitle != NULL) {
            mDecoderSubtitle->stop();
        }
    }

    if(pthread_join(mPlayerThread, NULL) != 0) {
        FMP_ERR("Couldn't cancel player thread");
    }

    // Close the codec
    if(!mNoAudio){
        if(mDecoderAudio)
            delete mDecoderAudio;
        mDecoderAudio = NULL;
    }

    if(!mNoVideo){
        if(mDecoderVideo)
            delete mDecoderVideo;
        mDecoderVideo = NULL;
        if(mVideoRender)
            delete mVideoRender;
        mVideoRender = NULL;
    }

    if(!mNoSubtitle)
    {
        if(mDecoderSubtitle)
            delete mDecoderSubtitle;
        mDecoderSubtitle = NULL;
    }

    if(mClock != NULL) {
        delete mClock;
    }
    // Close the video file
    if(mMovieFile)
        ff_close_input_file(mMovieFile);
    mMovieFile = NULL;
    mNativeWindow = NULL;

    FMP_DBG("suspended");

    return NO_ERROR;
}


void* FFPlayer::start_player(void* ptr)
{
    FMP_DBG("[HMP]starting main player thread");
    FFPlayer * mThis = (FFPlayer *)ptr;
    mThis->decode_media(ptr);
    return NULL;
}


bool FFPlayer::shouldCancel(PacketQueue* queue)
{
    return (mCurrentState == FF_MEDIA_PLAYER_STATE_ERROR || mCurrentState == FF_MEDIA_PLAYER_STOPPED ||
             ((mCurrentState == FF_MEDIA_PLAYER_DECODED || mCurrentState == FF_MEDIA_PLAYER_STARTED) 
              && queue->size() == 0));
}

static int chooseChannelMask(int channels)
{
    switch(channels)
    {
        case 1:
            return AUDIO_CHANNEL_OUT_MONO;
        case 2:
            return AUDIO_CHANNEL_OUT_STEREO;
        case 4:
            return AUDIO_CHANNEL_OUT_QUAD;
        case 6:
            return AUDIO_CHANNEL_OUT_5POINT1;
        case 8:
            return AUDIO_CHANNEL_OUT_7POINT1;
        default:
            return AUDIO_CHANNEL_OUT_ALL;
    }
}

status_t FFPlayer::create_audio_output(AVCodecContext* codec_ctx)
{
    status_t ret = NO_ERROR;
    int sample_rate = codec_ctx->sample_rate;
    int channels = chooseChannelMask(codec_ctx->channels);

#ifdef NEED_AUDIOTRACK

    if(mAudioTrack == NULL){
        mAudioTrack = new AudioTrack();
    }

    if(mAudioTrack == NULL){
        FMP_ERR("[HMP]create AudioTrack fail");
        ret =  INVALID_OPERATION;
    }else{
        mAudioTrack->set(AUDIO_STREAM_MUSIC,sample_rate, AUDIO_FORMAT_PCM_16_BIT,channels);
        if (mAudioTrack->initCheck() != OK) {
            mAudioTrack.clear();

            return INVALID_OPERATION;
        }
        FMP_DBG("[HMP]create AudioTrack: samplerate(%d)",sample_rate);
    }
#else
    ret = mOutput->AudioDriver_set(MUSIC, sample_rate, PCM_16_BIT, channels);
#endif
    return ret;
}


status_t FFPlayer::start_audio_output()
{
    status_t ret = NO_ERROR;
#ifdef NEED_AUDIOTRACK
    if(mAudioTrack == NULL)
        return ret;;
    ret = mAudioTrack->start();
#else
    ret = mOutput->AudioDriver_start();
#endif

    mStartTime = getCurSysTime();

    return ret;
}

status_t FFPlayer::stop_audio_output()
{
    status_t ret = NO_ERROR;
#ifdef NEED_AUDIOTRACK
    if(mAudioTrack == NULL)
        return ret;
    mAudioTrack->stop();
#else
    ret = mOutput->AudioDriver_stop();
#endif
    return ret;
}

int FFPlayer::write_audio_output(int16_t* buffer, int buffer_size)
{
    updateCurrentTime();
    //FMP_DBG("[HMP]write  %d",buffer_size);
    int ret = -1;
#ifdef NEED_AUDIOTRACK
    pthread_mutex_lock(&mLock);
    if(mAudioTrack == NULL)
    {
        ALOGI("write_audio_output mAudioTrack is NULL\n");
        pthread_mutex_unlock(&mLock);
        return ret;
    }
    ret = mAudioTrack->write(buffer, buffer_size);
    pthread_mutex_unlock(&mLock);
#else
    ret = mOutput->AudioDriver_write(buffer, buffer_size);
#endif
    if(ret <= 0)
        FMP_ERR("Couldn't write samples to audio track");

#ifdef FFPLAYER_NEED_DUMP
    ffplayer_dump_audio(buffer, buffer_size);
#endif

#if 0
    gettimeofday(&tv_e, NULL);
    int del_sec = (tv_e.tv_sec - tv_s.tv_sec) * 1000;
    int del_usec = (tv_e.tv_usec - tv_s.tv_usec)/1000;
    FMP_DBG("[HMP]%d write_audio_output size=%d [%ld.%03ld]",sPlayer->mAudioFrame, buffer_size, del_sec, del_usec);
#endif
    return ret;
}

void FFPlayer::updateCurrentTime(){
    if(mStartTime == -1){
        mCurrentPosition = 0;
    }else{
        int64_t curSystemClock = getCurSysTime();
        mCurrentPosition = curSystemClock - mStartTime;
    }
}

int FFPlayer::output(int16_t* buffer, int buffer_size,void* mPlayerAddr)
{
    //FMP_DBG("[HMP]output %d",buffer_size);
    FFPlayer * mThis = (FFPlayer *)mPlayerAddr;
    int ret = 0;
    if(FPS_DEBUGGING) {
        static int frames = 0;
        static double t1 = -1;
        static double t2 = -1;
        t2 = getCurSysTime() / 1000;
        if (t1 == -1 || t2 > t1 + 1) {
            FMP_DBG("Audio fps:%i", frames);
            //sPlayer->notify(MEDIA_INFO_FRAMERATE_AUDIO, frames, -1);
            t1 = t2;
            frames = 0;
        }
        frames++;
    }
    int16_t* outbuf = (int16_t* )ff_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    memcpy(outbuf, buffer, buffer_size);
    ret = mThis->write_audio_output(outbuf, buffer_size);
    ff_free(outbuf);
    mThis->mAudioFrame++;
    return ret;
}

status_t FFPlayer::setVideoScalingMode(int32_t mode)
{
    mVideoScalingMode = mode;
    if (mNativeWindow != NULL)
    {
        status_t ret = native_window_set_scaling_mode(mNativeWindow.get(), mVideoScalingMode);
        if (ret != OK) {
            ALOGE("Failed to set scaling mode (%d): %s",-ret, strerror(-ret));
            return ret;
        }
    }
    return OK;
}

status_t FFPlayer::setVideoSurfaceTexture(
        const sp<IGraphicBufferProducer> &bufferProducer)
{
    FMP_DBG("[HMP]setVideoSurfaceTexture");
    mNativeWindow = new Surface(bufferProducer);
    return NO_ERROR;
}

void FFPlayer::decode_media(void* ptr)
{
    FMP_DBG("[HMP]decodeMovie");

    AVPacket pPacket;
    int ret = -1;

    if(!mNoAudio){
        FMP_DBG("[HMP]create audio decoder");
        AVStream* stream_audio = mMovieFile->streams[mAudioStreamIndex];
        mDecoderAudio = new DecoderAudio(stream_audio, mTotalFrame);
        mDecoderAudio->onDecode = output;
        mDecoderAudio->mPlayerAddr = mPlayerAddr;
        mDecoderAudio->mClock = mClock;
        mDecoderAudio->mNoVideo = mNoVideo;
        mDecoderAudio->mNoSubtitle = mNoSubtitle;
        mDecoderAudio->startAsync();
    }

    if(!mNoVideo){
        FMP_DBG("[HMP]create video renderer\n");
        mVideoRender = new VideoRenderer(mNativeWindow, mVideoScalingMode);
        mVideoRender->mPlayerAddr = mPlayerAddr;
        mVideoRender->mClock = mClock;
        mVideoRender->startAsync();

        FMP_DBG("[HMP]create video decoder");
        AVStream* stream_video = mMovieFile->streams[mVideoStreamIndex];
        mDecoderVideo = new DecoderVideo(stream_video, mTotalFrame);
        mDecoderVideo->mVideoRender = mVideoRender;
        mDecoderVideo->mPlayerAddr = mPlayerAddr;
        mDecoderVideo->mClock = mClock;
        mDecoderVideo->startAsync();
        FMP_DBG("playing %ix%i", mVideoWidth, mVideoHeight);
    }

    if(!mNoSubtitle)
    {
        FMP_DBG("[HMP]create subtitle decoder");
        AVStream* stream_subtitle = mMovieFile->streams[mSubtitleStreamIndex];
        mDecoderSubtitle = new DecoderSubtitle(stream_subtitle,mVideoWidth,mVideoHeight,getListener());
        mDecoderSubtitle->mPlayerAddr = mPlayerAddr;
        mDecoderSubtitle->mClock = mClock;
        mDecoderSubtitle->startAsync();
    }

    while (mCurrentState != FF_MEDIA_PLAYER_DECODED && mCurrentState != FF_MEDIA_PLAYER_STOPPED &&
            mCurrentState != FF_MEDIA_PLAYER_STATE_ERROR && mCurrentState != FF_MEDIA_PLAYER_RESETED)
    {
        int video_packets = 0;
        int audio_packets = 0;

        if(mDecoderVideo)
            video_packets = mDecoderVideo->packets();

        if(mDecoderAudio)
            audio_packets = mDecoderAudio->packets();

        if(mNoVideo)
            video_packets = FFMPEG_VIDEO_MAX_QUEUE_SIZE + 10;
        if (video_packets > FFMPEG_VIDEO_MAX_QUEUE_SIZE && //the max size of video queue need adjust
            audio_packets > FFMPEG_AUDIO_MAX_QUEUE_SIZE) {
            //FMP_ERR("[HMP] video_packets=%d | audio_packets=%d  wait 200us",video_packets,audio_packets);
            usleep(200);
            continue;
        }

        /* If need seek : do sth here*/
        if(mSeekTime >= 0)
        {
            int64_t seek_pos = 0; // in seconds

            ALOGD("Going to seek frame, seek to time:%lld (ms)", mSeekTime);

            seek_pos = (mSeekTime * AV_TIME_BASE)/1000;
            if (mMovieFile->start_time != AV_NOPTS_VALUE)
                seek_pos += mMovieFile->start_time;

            if (ff_seek_frame(mMovieFile, -1, seek_pos, AVSEEK_FLAG_BACKWARD) < 0){
                FMP_ERR("%s,  ff_seek_frame() seek to %.3f failed!", __FUNCTION__, (double)seek_pos/AV_TIME_BASE);
                continue;
            }

            /* Update current time */
            if(mCurrentPosition == -1) updateCurrentTime();
            mStartTime = mStartTime + mCurrentPosition - mSeekTime;
            //mStartTime -= (mSeekTime);

            if(!mNoVideo){
                mDecoderVideo->flush();
                usleep(50000);
                mVideoRender->flush();
                usleep(10000);
                mClock->mSeekFlag = true;
            }

            if(!mNoAudio){
                mDecoderAudio->flush();
                usleep(20000);
                mDecoderAudio->mSeekFlag = true;
            }

            if(!mNoSubtitle){
                mDecoderSubtitle->flush();
                mDecoderSubtitle->_notify_p(NULL,0);
                usleep(10000);
            }
            notify(MEDIA_SEEK_COMPLETE);
            mSeekTime = -1;
        }

        /*Start to read frame*/
        int frmRd = ff_read_frame(mMovieFile, &pPacket);
        if(frmRd < 0)
        {
            if(frmRd == AVERROR_EOF || avio_feof(mMovieFile->pb))
            {
                //FMP_ERR("[EOF]decodeMovie:eof_reached is %d",mMovieFile->pb->eof_reached);
                //pPacket.data = (uint8_t *)(intptr_t)"AUDIOEND";
                if(!mNoVideo)
                {
                    if(0 == mDecoderAudio->packets())
                        mClock->notify();
                    if(0 == mVideoRender->getQueueSize() && 0 == mDecoderVideo->packets() && 0 == mDecoderAudio->packets())
                    {
                        if(mDecoderAudio->mSleeping && mDecoderVideo->mSleeping)
                        {
                            ALOGI("ff_read_frame EOF,video queue is empty\n");
                            isEOF=true;
                            goto complete;
                        }
                    }
                } else {
                    if(0 == mDecoderAudio->packets() && mDecoderAudio->mSleeping)
                    {
                        ALOGI("ff_read_frame EOF,audio queue is empty\n");
                        usleep(10000);
                        isEOF=true;
                        goto complete;
                    }
                }
            }
            //FMP_DBG("[HMP]ff_read_frame fail %d",frmRd);
            //mCurrentState = FF_MEDIA_PLAYER_DECODED;
            continue;
        }

        isEOF=false;
        // Is this a packet from the video stream?

        if (pPacket.stream_index == mVideoStreamIndex) {
            if(mDecoderVideo)
            {
                if(mFirstPackets)
                {
                    mFirstPackets = false;
                    notify(MEDIA_INFO, MEDIA_INFO_RENDERING_START);

                    struct timeval renderTime;
                    gettimeofday(&renderTime, NULL);
                    int spend_time_ms = (renderTime.tv_sec - startTime.tv_sec)*1000 + (renderTime.tv_usec - startTime.tv_usec)/1000;
                    ALOGI("------MEDIA_INFO_RENDERING_START:(spend %d ms)", spend_time_ms);
                }
                mDecoderVideo->enqueue(&pPacket);
            }
            else
                ff_free_packet(&pPacket);
        } else if (pPacket.stream_index == mAudioStreamIndex) {
            if(mDecoderAudio)
                mDecoderAudio->enqueue(&pPacket);
            else
                ff_free_packet(&pPacket);
        } else if (pPacket.stream_index == mSubtitleStreamIndex) {
            if(mDecoderSubtitle)
                mDecoderSubtitle->enqueue(&pPacket);
            else
                ff_free_packet(&pPacket);
        } else {
            // Free the packet that was allocated by ff_read_frame
            FMP_DBG("[HMP]Free the packet that was allocated by ff_read_frame(%d)",pPacket.stream_index);
            ff_free_packet(&pPacket);
        }

    }

    if(!mNoVideo){
        //waits on end of video thread
        FMP_DBG("[HMP]waiting on video thread");
        if(ret = mDecoderVideo->wait() != 0) {
            FMP_ERR("Couldn't cancel video thread: %i", ret);
        }

        FMP_DBG("[HMP]waiting on Video Render thread");
        if(ret = mVideoRender->wait() != 0) {
            FMP_ERR("Couldn't cancel render thread: %i", ret);
        }
    }

    if(!mNoAudio){
        //waits on end of audio thread
        FMP_DBG("[HMP]waiting on audio thread");
        if(ret = mDecoderAudio->wait() != 0) {
            FMP_ERR("Couldn't cancel audio thread: %i", ret);
        }
    }

    if(!mNoSubtitle){
        FMP_DBG("[HMP]waiting on subtitle thread");
        if((ret = mDecoderSubtitle->wait()) != 0) {
            FMP_ERR("Couldn't cancel subtitle thread: %i", ret);
        }
    }

complete:
    if(mCurrentState == FF_MEDIA_PLAYER_STATE_ERROR) {
        FMP_ERR("[HMP]playing err");
    }
    mCurrentState = FF_MEDIA_PLAYER_PLAYBACK_COMPLETE;
    if(isEOF)
        notify(MEDIA_PLAYBACK_COMPLETE);
    FMP_DBG("[HMP]end of playing");
}

void FFPlayer::toggleBuffering(int buffering)
{
    ALOGI("[HMP]toggleBuffering %d", buffering);
    if (buffering) {
        ALOGI("buffering start");
        notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
    } else {
        ALOGI("buffering end");
        notify(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
    }
}

status_t FFPlayer::getTrackInfo(Parcel *reply)
{
    const char * const undLang = "und";
    int aud_num = 0;
    int vid_num = 0;
    int sub_num = 0;
    int field_num = 2;
    int total_stream_num = mMovieFile->nb_streams;

    int i = 0;
    for (i = 0; i < total_stream_num; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
            aud_num++;
        }else if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
            vid_num++;
        }else if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_SUBTITLE) {
            sub_num++;
        }
    }

    reply->writeInt32(aud_num+vid_num+sub_num);

    for(i = 0; i < aud_num; i++)
    {
        reply->writeInt32(field_num);
        reply->writeInt32(MEDIA_TRACK_TYPE_AUDIO);
        reply->writeString16(String16(undLang));
    }

    for(i = 0; i < vid_num; i++)
    {
        reply->writeInt32(field_num);
        reply->writeInt32(MEDIA_TRACK_TYPE_VIDEO);
        reply->writeString16(String16(undLang));
    }

    for(i = 0; i < sub_num; i++)
    {
        reply->writeInt32(field_num);
        reply->writeInt32(MEDIA_TRACK_TYPE_TIMEDTEXT);
        reply->writeString16(String16(undLang));
    }

    return OK;
}

void FFPlayer::ffmpeg_notify(void* ptr, int level, const char* fmt, va_list vl) {
    FMP_DBG("[HMP]ffmpegNotify level=%d  fmt=%s",level, fmt);
    switch(level) {
            /* Something went really wrong and we will crash now. */
        case AV_LOG_PANIC:
            FMP_DBG( "AV_LOG_PANIC: %s", fmt);
            //sPlayer->mCurrentState = FF_MEDIA_PLAYER_STATE_ERROR;
            break;

            /**
                      * Something went wrong and recovery is not possible.
                      * For example, no header was found for a format which depends
                      * on headers or an illegal combination of parameters is used.
                      */
        case AV_LOG_FATAL:
            FMP_DBG( "AV_LOG_FATAL: %s", fmt);
            //sPlayer->mCurrentState = FF_MEDIA_PLAYER_STATE_ERROR;
            break;

            /**
                      * Something went wrong and cannot losslessly be recovered.
                      * However, not all future data is affected.
                      */
        case AV_LOG_ERROR:
            FMP_DBG("AV_LOG_ERROR: %s", fmt);
            //sPlayer->mCurrentState = FF_MEDIA_PLAYER_STATE_ERROR;
            break;

            /**
                      * Something somehow does not look correct. This may or may not
                      * lead to problems. An example would be the use of '-vstrict -2'.
                      */
        case AV_LOG_WARNING:
            FMP_DBG( "AV_LOG_WARNING: %s", fmt);
            break;

        case AV_LOG_INFO:
            FMP_DBG("AV_LOG_INFO %s", fmt);
            break;

        case AV_LOG_DEBUG:
            FMP_DBG("%s", fmt);
            break;
    }
}

#if 0 //Do not support cue file in FFPlayer
status_t FFPlayer::getCueInfo(Parcel *reply)
{
    reply->writeString16(String16(mCueFile.fileName));
    reply->writeString16(String16(mCueFile.title));
    reply->writeInt32(mCueFile.songNum);
    for(int i=0; i<mCueSongs.size(); i++){
        CueSongBean song = mCueSongs[i];
        reply->writeInt32(song.idx);
        reply->writeString16(String16(song.title));
        reply->writeString16(String16(song.performer));
        reply->writeInt32(song.duration);
        reply->writeInt32(song.start_time);
    }
    return OK;
}

status_t FFPlayer::getSongInfo(int index,Parcel *reply)
{
    if(!isPlaying() || !isCue)
        return INVALID_OPERATION;

    if(index < 0 || index >= mCueSongs.size())
        return BAD_VALUE;

    CueSongBean song = mCueSongs[index];
    reply->writeInt32(song.idx);
    reply->writeString16(String16(song.title));
    reply->writeString16(String16(song.performer));
    reply->writeInt32(song.duration);
    reply->writeInt32(song.start_time);

    return OK;
}

status_t FFPlayer::chooseSong(int index)
{
    if(!isPlaying() || !isCue)
        return INVALID_OPERATION;

    if(index < 0 || index >= mCueSongs.size())
        return BAD_VALUE;

    mCurIdx = index;

    mDuration = mCueSongs[index].duration;
    int songPos = mCueSongs[index].start_time;
    status_t ret = seekTo(songPos);

    mStartTime = getCurSysTime();

    return ret;
}
#endif

#if 0
status_t FFPlayer::setDataSource(const sp<IStreamSource> &source) {
    return INVALID_OPERATION;
}

bool FFPlayer::isPlaying() {
    //FMP_DBG("[HMP]isPlaying");
    return mCurrentState == FF_MEDIA_PLAYER_STARTED || mCurrentState == FF_MEDIA_PLAYER_DECODED;
}

status_t FFPlayer::initCheck() {
    FMP_DBG("[HMP]initCheck");
    return NO_ERROR;//INVALID_OPERATION;
}

status_t FFPlayer::setLooping(int loop) {
    FMP_DBG("[HMP]setLooping");
    mLoop = loop ? true : false;
    return NO_ERROR;
}

status_t FFPlayer::setUID(uid_t uid) {
    FMP_DBG("[HMP]setUID %d",uid);
    mUid = uid;
    return INVALID_OPERATION;
}

status_t FFPlayer::getVideoWidth(int *w)
{
    FMP_DBG("[HMP]getVideoWidth %d",mVideoWidth);

    if (mCurrentState < MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    *w = mVideoWidth;
    return NO_ERROR;
}

status_t FFPlayer::getVideoHeight(int *h)
{
    FMP_DBG("[HMP]getVideoHeight %d",mVideoHeight);

    if (mCurrentState < FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    *h = mVideoHeight;
    return NO_ERROR;
}

status_t FFPlayer::setParameter(int key, const Parcel &request) {
    FMP_DBG("[HMP]setParameter(key=%d)", key);
    return NO_ERROR;
}

status_t FFPlayer::getParameter(int key, Parcel *reply) {
    FMP_DBG("[HMP]getParameter");
    return NO_ERROR;
}

status_t FFPlayer::dump(int fd, const Vector<String16> &args) const {
    return INVALID_OPERATION;
}

//status_t FFPlayer::setListener(const sp<MediaPlayerBase> &listener)
status_t FFPlayer::setListener(const wp<MediaPlayerBase> &listener)
{
    FMP_DBG( "[HMP]setListener");
    mListener = listener;
    return NO_ERROR;
}

void FFPlayer::notify(int msg, int ext1, int ext2)
{
    //ALOGV("message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);
    bool send = true;
    bool locked = false;

    if ((mListener != NULL) && send) {
        sp<MediaPlayerBase> listener = mListener.promote();

        if (listener != NULL) {
            listener->sendEvent(msg, ext1, ext2);
        }
    }
}
#endif
}  // namespace android


