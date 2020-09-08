
//#define LOG_NDEBUG 0
#define LOG_TAG "CuePlayer"
#include <utils/Log.h>

#include "cueplayer.h"

#include <media/Metadata.h>

namespace android {

extern "C" {

#include "ff_func.h"

} // end of extern C

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------



CuePlayer::CuePlayer():
#ifdef NEED_AUDIOTRACK
    mAudioTrack(NULL),
#endif
    mListener(NULL),
    mCookie(NULL),
    mNoAudio(false),
    mCurIdx(-1),
    mUid(NULL),
    mCurrentState(FF_MEDIA_PLAYER_IDLE),
    mStartTime(-1),
    mDuration(-1),
    mCurrentPosition(-1),
    mSeekTime(-1),
    mTotalFrame(0),
    mAudioFrame(0),
    mCueParse(NULL),
    mApeCtx(NULL),
    isCue(true),
    isEOF(false),
    isPause(false),
    mLoop(false),
    mStreamType(MUSIC),
    mPrepareStatus(NO_ERROR),
    mPrepareSync(false),
    mFFInitialed(false),
    mAudioStreamIndex(-1),
    mDecoderAudio(NULL),
    mMovieFile(NULL),
    mOutput(NULL)
{
    FMP_DBG("[HMP-CUE]CuePlayer");
    memset(&mPlayerInfo,0,sizeof(mPlayerInfo));
    memset(&mMediaInfo,0,sizeof(mMediaInfo));
    memset(&mCueFile,0,sizeof(CueFileBean));
    memset(mRealPath,0,sizeof(mRealPath));
    memset(mRealPathBackup,0,sizeof(mRealPathBackup));

    mLeftVolume = mRightVolume = 1.0;//not used

    mPlayerAddr = (void *)this;

    pthread_mutex_init(&mLock, NULL);

    av_register_all();
}

CuePlayer::~CuePlayer() {
    FMP_DBG("[HMP-CUE]~CuePlayer");
    reset();
    mDecoderAudio = NULL;
    mMovieFile = NULL;
    mOutput = NULL;
    mPlayerAddr = NULL;
}


status_t CuePlayer::setDataSource(
        const char *url,
        const KeyedVector<String8, String8> *headers) {
    FMP_DBG("[HMP-CUE]setDataSource(%s)",url);

    if (strcmp(url+strlen(url)-4, ".cue") == 0) {
        FMP_DBG("[HMP-CUE]setDataSource: This is CUE file (%s)!!!!",url);
        isCue = true;
        mCueParse = new cueParse();
        if(mCueParse){
            char cuepath[strlen(url)];
            strcpy(cuepath,url);
            strncpy(mRealPathBackup,cuepath,strlen(cuepath)-4);
            strncpy(mRealPathBackup+strlen(cuepath)-4,".ape",4);
            mRealPathBackup[strlen(cuepath)] = '\0';
            mCueParse->parseCueFile(cuepath, &mCueFile, &mCueSongs);
            FMP_DBG("[HMP-CUE]parse cue info complete!");
#if 0
            ALOGI("cue+++cuePath:%s+++",mCueFile.cuePath);
            ALOGI("cue+++mediaPath:%s+++",mCueFile.mediaPath);
            ALOGI("cue+++performer:%s+++",mCueFile.performer);
            ALOGI("cue+++albumName:%s+++",mCueFile.albumName);
            ALOGI("cue+++fileName:%s+++",mCueFile.fileName);
            ALOGI("cue+++title:%s+++",mCueFile.title);
            ALOGI("cue+++songNum:%d+++",mCueFile.songNum);
            for(int i=0;i<mCueSongs.size();i++){
                ALOGI("cue----%d  %s  (%d)",mCueSongs[i].idx,mCueSongs[i].title, mCueSongs[i].duration);
            }
#endif
            delete mCueParse;
        }
        //strncpy(mRealPath, url, strlen(url)-4);
        //strcpy(mRealPath + strlen(url)-4, ".ape");
        strcpy(mRealPath, mCueFile.mediaPath);
    }else if(strcmp(url+strlen(url)-4, ".ape") == 0){
        FMP_DBG("[HMP-CUE]setDataSource: This is APE file (%s)!!!!",url);
        isCue = false;
        strcpy(mRealPath, url);
    }else{
        isCue = false;
        strcpy(mRealPath, url);
    }

    ALOGI("cue===open:%s===",mRealPath);

    mCurrentState = FF_MEDIA_PLAYER_INITIALIZED;

    return NO_ERROR;
}

status_t CuePlayer::setDataSource(int fd, int64_t offset, int64_t length) {
    FMP_DBG("[HMP-CUE]setDataSource(%d, %lld, %lld)", fd, offset, length);

    char s[256], name[256];
    memset(s,0,sizeof(s));
    memset(name,0,sizeof(s));
    snprintf(s, 255, "/proc/%d/fd/%d", getpid(), fd);
    readlink(s, name, 255);
    close(fd);
    return setDataSource(strdup(name));
}

status_t CuePlayer::prepare() {
    FMP_DBG("[HMP-CUE]prepare");
    status_t ret;
    if(mCurrentState != FF_MEDIA_PLAYER_INITIALIZED){
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_PREPARING;

    if(openFile() != NO_ERROR){
        FMP_ERR("[HMP_CUE] FFMPEG open file Failed");
		return INVALID_OPERATION;
    }

    mOutput = new Output();

    if(mMovieFile->filename)
        FMP_DBG("[HMP-CUE]filename:%s",mMovieFile->filename);

    //av_log_set_callback(ffmpeg_notify);

    if ((ret = prepare_audio()) != NO_ERROR) {
        FMP_ERR("[HMP-CUE]prepareAudio fail!");
        mNoAudio = true;
    }

    if(mNoAudio){
        mCurrentState = FF_MEDIA_PLAYER_STATE_ERROR;
        return ret;
    }

    if(!mNoAudio){
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_PURE_AUDIO);
    }

    mDuration =  mMovieFile->duration / 1000;
    FMP_DBG("[DURATION]prepare: get duration is %d",mDuration);

    mCurrentState = FF_MEDIA_PLAYER_PREPARED;
    return NO_ERROR;
}

status_t CuePlayer::prepareAsync() {
    FMP_DBG("[HMP-CUE]prepareAsync");
    mPrepareSync = true;
    status_t ret = prepare();
    notify(MEDIA_PREPARED);
    return ret;
}

status_t CuePlayer::start() {

    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED || isPause){
        return resume();
    }

    FMP_DBG("[HMP-CUE]start");

    if (mCurrentState != FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_STARTED;
    pthread_create(&mPlayerThread, NULL, start_player, mPlayerAddr);

    return NO_ERROR;
}

status_t CuePlayer::stop() {
    FMP_DBG("[HMP-CUE]stop");
    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED || isPause){
        resume();
    }
    mCurrentState = FF_MEDIA_PLAYER_STOPPED;
    return NO_ERROR;  // what's the difference?
}

status_t CuePlayer::pause() {
    FMP_DBG("[HMP-CUE]pause");
    if(mCurrentState == FF_MEDIA_PLAYER_PAUSED){
        return INVALID_OPERATION;
    }
    mCurrentState = FF_MEDIA_PLAYER_PAUSED;
    mDecoderAudio->mPause = true;
    isPause = true;

    mPauseTime = getCurSysTime();
    return NO_ERROR;
}

status_t CuePlayer::resume() {
    FMP_DBG("[HMP-CUE]resume");
    int64_t deltaTime = getCurSysTime() - mPauseTime;
    if(deltaTime > 0)
        mStartTime +=  deltaTime;
    mDecoderAudio->mPause = false;
    mDecoderAudio->notify();
    isPause = false;
    mCurrentState = FF_MEDIA_PLAYER_STARTED;
    return NO_ERROR;
}

status_t CuePlayer::seekTo(int msec) {
    FMP_DBG("[HMP-CUE]seekTo %.2f secs", msec / 1E3);

    mSeekTime = msec;
    if(mDuration) mSeekTime %= mDuration;

    return NO_ERROR;
}

status_t CuePlayer::getCurrentPosition(int *msec) {
    //FMP_DBG("[HMP-CUE]getCurrentPosition");
    if(mCurrentPosition < 0)  mCurrentPosition = 0;
    if(mCurrentPosition > mDuration) mCurrentPosition = mDuration;

    if(isCue && (mCurIdx != -1) && (mCurrentPosition >= mCueSongs[mCurIdx].start_time)){
        mCurrentPosition -= mCueSongs[mCurIdx].start_time;
    }
    *msec = mCurrentPosition;

    if(isCue){
        int64_t dur = 0;
#ifdef OLD_TYPE
        dur = mCueSongs[mCurIdx].duration;
#else
        dur = getSongDur(mCurIdx);
#endif
        if(mCurrentPosition >= (dur)) {
            FMP_DBG("[HMP-CUE] This Song Complete!");
            mCurrentState = FF_MEDIA_PLAYER_PLAYBACK_COMPLETE;
            notify(MEDIA_PLAYBACK_COMPLETE);
            stop();
        }
    }

    return NO_ERROR;
}

status_t CuePlayer::getDuration(int *msec) {
    //FMP_DBG("[HMP-CUE]getDuration");

    if (mCurrentState < FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }

    if(isCue && mCurIdx != -1){
#ifdef OLD_TYPE
        *msec = mCueSongs[mCurIdx].duration;
#else
        *msec = getSongDur(mCurIdx);
#endif
    }else{
        *msec = mDuration;
    }
    return NO_ERROR;
}

status_t CuePlayer::reset() {
    FMP_DBG("[HMP-CUE]reset");
    if(mCurrentState == FF_MEDIA_PLAYER_RESETED
        || FF_MEDIA_PLAYER_IDLE == mCurrentState){
        return INVALID_OPERATION;
    }

    stop();
#ifdef NEED_AUDIOTRACK
    if(mAudioTrack != NULL) {
        mAudioTrack->stop();
        mAudioTrack->flush();
        mAudioTrack.clear();
    }
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

status_t CuePlayer::invoke(const Parcel &request, Parcel *reply) {
    FMP_DBG("[HMP-CUE]invoke");
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
            FMP_DBG("+++Invoke: Get Track Info");
            return getTrackInfo(reply);
        }
        case INVOKE_ID_GET_CUE_INFO:
        {
            FMP_DBG("+++Invoke: Get Cue File Info");
            return getCueInfo(reply);
        }
        case INVOKE_ID_SELECT_CUE_SONG:
        {
            int idx = 0;
            ret = request.readInt32(&idx);
            FMP_DBG("+++Invoke: Display Song No.%d", idx);
            getSongInfo(idx, reply);
            return chooseSong(idx);
        }
        default:
            break;
    }
    return INVALID_OPERATION;
}

status_t CuePlayer::openFile()
{
    //Check file
    if(access(mRealPath, F_OK) != 0){
        FMP_ERR("[HMP-CUE]%s is not exist!",mRealPath);
        if(access(mRealPathBackup, F_OK) != 0){
            FMP_ERR("[HMP-CUE]%s is not exist!",mRealPathBackup);
            notify(MEDIA_ERROR, WHALEY_ERROR_OPENFILE_FAIL);
        }
        strcpy(mRealPath,mRealPathBackup);
    }

    // Open media file
    if(ff_open_input_file(&mMovieFile, mRealPath) != 0) {
        FMP_ERR("[HMP-CUE]ff_open_input_file fail! %s",mRealPath);
        return INVALID_OPERATION;
    }
    // Retrieve stream information
    if(ff_find_stream_info(mMovieFile) < 0) {
        FMP_ERR("[HMP-CUE]ff_find_stream_info fail!");
        return INVALID_OPERATION;
    }

    mFFInitialed = true;
    return NO_ERROR;
}

status_t CuePlayer::prepare_audio()
{
    FMP_DBG("[HMP-CUE]prepareAudio");
    for (int i = 0; i < mMovieFile->nb_streams; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
            mAudioStreamIndex = i;
            break;
        }
    }

    if (mAudioStreamIndex == -1) {
        FMP_ERR("[HMP-CUE]mAudioStreamIndex invalid!");
        return INVALID_OPERATION;
    }

    AVStream* stream = mMovieFile->streams[mAudioStreamIndex];
    // Get a pointer to the codec context for the video stream
    AVCodecContext* codec_ctx = stream->codec;
    AVCodec* codec = ff_find_decoder(codec_ctx->codec_id);
    if (codec == NULL) {
        FMP_ERR("[HMP-CUE]codec is null!");
        notify(MEDIA_INFO, WHALEY_MEDIA_INFO_AUDIO_UNSUPPORT);
        return INVALID_OPERATION;
    }

    // Open codec
    if (ff_codec_open(codec_ctx, codec) < 0) {
        FMP_ERR("[HMP-CUE]ff_codec_open fail!");
        return INVALID_OPERATION;
    }

    // prepare os output
    if(NO_ERROR != create_audio_output(codec_ctx))
        return INVALID_OPERATION;

    if(NO_ERROR != start_audio_output())
        return INVALID_OPERATION;

    FMP_DBG("[HMP-CUE]prepareAudio  get audio stream id=%d",mAudioStreamIndex);

    prtAudioContext(mMovieFile);

    prtStrInfo(stream);

    if(strcmp(codec->name,"ape") == 0){
        mApeCtx = (APEContext *)mMovieFile->priv_data;
        mTotalFrame = mApeCtx->totalframes;
        ///prtApeInfo(mApeCtx);
    }

    return NO_ERROR;
}

status_t CuePlayer::suspend() {
    FMP_DBG("[HMP-CUE]suspend");

    mCurrentState = FF_MEDIA_PLAYER_STOPPED;
    if(mDecoderAudio != NULL) {
        mDecoderAudio->stop();
    }

    if(pthread_join(mPlayerThread, NULL) != 0) {
        FMP_ERR("Couldn't cancel player thread");
    }

    // Close the codec
    if(mDecoderAudio)
        delete mDecoderAudio;
    mDecoderAudio = NULL;

    if(mFFInitialed){
        // Close the video file
        if(mMovieFile)
            ff_close_input_file(mMovieFile);
        mMovieFile = NULL;
    }

    FMP_DBG("suspended");

    return NO_ERROR;
}


void* CuePlayer::start_player(void* ptr)
{
    FMP_DBG("[HMP-CUE]starting main player thread");
    CuePlayer* mThis = (CuePlayer *)ptr;
    mThis->decode_media(ptr);
    return NULL;
}


bool CuePlayer::shouldCancel(PacketQueue* queue)
{
    return (mCurrentState == FF_MEDIA_PLAYER_STATE_ERROR || mCurrentState == FF_MEDIA_PLAYER_STOPPED ||
             ((mCurrentState == FF_MEDIA_PLAYER_DECODED || mCurrentState == FF_MEDIA_PLAYER_STARTED) 
              && queue->size() == 0));
}

status_t CuePlayer::create_audio_output(AVCodecContext* codec_ctx)
{
    status_t ret = NO_ERROR;
    int sample_rate = codec_ctx->sample_rate;
    int channels = (codec_ctx->channels == 2) ? AUDIO_CHANNEL_OUT_STEREO : AUDIO_CHANNEL_OUT_MONO;

#ifdef NEED_AUDIOTRACK

    if(mAudioTrack == NULL){
        mAudioTrack = new AudioTrack();
    }

    if(mAudioTrack == NULL){
        FMP_ERR("[HMP-CUE]create AudioTrack fail");
        ret =  INVALID_OPERATION;
    }else{
        mAudioTrack->set(AUDIO_STREAM_MUSIC,sample_rate, AUDIO_FORMAT_PCM_16_BIT,channels);
        if (mAudioTrack->initCheck() != OK) {
            mAudioTrack.clear();

            return INVALID_OPERATION;
        }
        FMP_DBG("[HMP-CUE]create AudioTrack: samplerate(%d)",sample_rate);
    }
#else
    ret = mOutput->AudioDriver_set(MUSIC, sample_rate, PCM_16_BIT, channels);
#endif
    return ret;
}


status_t CuePlayer::start_audio_output()
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

status_t CuePlayer::stop_audio_output()
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

int CuePlayer::write_audio_output(int16_t* buffer, int buffer_size)
{
    updateCurrentTime();
    //FMP_DBG("[HMP-CUE]write  %d",buffer_size);
    int ret = 0;
#ifdef NEED_AUDIOTRACK
    if(mAudioTrack == NULL)
        return ret;

    ret = mAudioTrack->write(buffer, buffer_size);
#else
    ret = mOutput->AudioDriver_write(buffer, buffer_size);
#endif
    if(ret <= 0)
        FMP_ERR("Couldn't write samples to audio track");

#ifdef CUEPLAYER_NEED_DUMP
    ffplayer_dump_audio(buffer, buffer_size);
#endif

    return ret;
}

void CuePlayer::updateCurrentTime(){
    if(mStartTime == -1){
        mCurrentPosition = 0;
    }else{
        int64_t curSystemClock = getCurSysTime();
        mCurrentPosition = curSystemClock - mStartTime;
    }
}

int CuePlayer::output(int16_t* buffer, int buffer_size,void* mPlayerAddr)
{
    //FMP_DBG("[HMP-CUE]output %d",buffer_size);
    CuePlayer * mThis = (CuePlayer *)mPlayerAddr;
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
    int16_t* outbuf = (int16_t* )av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);
    memcpy(outbuf, buffer, buffer_size);
    ret = mThis->write_audio_output(outbuf, buffer_size);
    av_free(outbuf);
    mThis->mAudioFrame++;
    return ret;
}

void CuePlayer::decode_media(void* ptr)
{
    FMP_DBG("[HMP-CUE]decode_media");

    AVPacket pPacket;
    int ret = -1;

    if(!mNoAudio){
        FMP_DBG("[HMP-CUE]create audio decoder");
        AVStream* stream_audio = mMovieFile->streams[mAudioStreamIndex];
        mDecoderAudio = new DecoderAudio(stream_audio, mTotalFrame);
        mDecoderAudio->onDecode = output;
        mDecoderAudio->mPlayerAddr = mPlayerAddr;
        mDecoderAudio->startAsync();
        mDecoderAudio->mNoVideo = true;
    }

    while (mCurrentState != FF_MEDIA_PLAYER_DECODED && mCurrentState != FF_MEDIA_PLAYER_STOPPED &&
            mCurrentState != FF_MEDIA_PLAYER_STATE_ERROR && mCurrentState != FF_MEDIA_PLAYER_RESETED)
    {
        int audio_packets = 0;

        if(mDecoderAudio)
            audio_packets = mDecoderAudio->packets();

        if (audio_packets > FFMPEG_AUDIO_MAX_QUEUE_SIZE) {
            //FMP_ERR("[HMP-CUE]audio_packets=%d  wait 200us",audio_packets);
            usleep(200);
            continue;
        }

        /* If need seek : do sth here*/
        if(mSeekTime >= 0)
        {
            if(mSeekTime != mCueSongs[mCurIdx].start_time)
                mSeekTime += mCueSongs[mCurIdx].start_time;

            int64_t seek_pos = 0; // in seconds

            ALOGD("Going to seek frame, seek to time:%d (ms)", mSeekTime);

            seek_pos = (mSeekTime * AV_TIME_BASE)/1000;
            if (mMovieFile->start_time != AV_NOPTS_VALUE)
                seek_pos += mMovieFile->start_time;

            if (ff_seek_frame(mMovieFile, -1, seek_pos, AVSEEK_FLAG_BACKWARD) < 0) {
                FMP_ERR("%s,  ff_seek_frame() seek to %.3f failed!", __FUNCTION__, (double)seek_pos/AV_TIME_BASE);
                continue;
            }

            /* Update current time */
            if(mCurrentPosition == -1) updateCurrentTime();
            mStartTime = mStartTime + mCurrentPosition - mSeekTime;
            mDecoderAudio->flush();
            usleep(10000);
            notify(MEDIA_SEEK_COMPLETE);
            mSeekTime = -1;
        }

        isEOF=false;
        /*Start to read frame*/
        int frmRd = ff_read_frame(mMovieFile, &pPacket);
        if(frmRd == AVERROR_EOF || avio_feof(mMovieFile->pb))
        {
            //FMP_ERR("[EOF]decodeMovie:eof_reached is %d",mMovieFile->pb->eof_reached);
            //pPacket.data = (uint8_t *)(intptr_t)"AUDIOEND";
            if(0 == mDecoderAudio->packets())
            {
                ALOGI("ff_read_frame EOF,audio queue is empty\n");
                usleep(10000);
                isEOF=true;
                goto complete;
            }
        }
        if(frmRd < 0) {
            //FMP_DBG("[HMP-CUE]ff_read_frame fail %d",frmRd);
            //mCurrentState = FF_MEDIA_PLAYER_DECODED;
            continue;
        }

        // Is this a packet from the video stream?
        if (pPacket.stream_index == mAudioStreamIndex) {
            if(mDecoderAudio)  mDecoderAudio->enqueue(&pPacket);
        }
        else {
            // Free the packet that was allocated by av_read_frame
            FMP_DBG("[HMP-CUE]Free the packet that was allocated by ff_read_frame(%d)",pPacket.stream_index);
            ff_free_packet(&pPacket);
        }

    }

    if(!mNoAudio){
        //waits on end of audio thread
        FMP_DBG("[HMP-CUE]waiting on audio thread");
        if(mDecoderAudio->wait() != 0) {
            FMP_ERR("Couldn't cancel audio thread: %i", ret);
        }
    }

complete:
    if(mCurrentState == FF_MEDIA_PLAYER_STATE_ERROR) {
        FMP_ERR("[HMP-CUE]playing err");
    }
    mCurrentState = FF_MEDIA_PLAYER_PLAYBACK_COMPLETE;
    if(isEOF)
        notify(MEDIA_PLAYBACK_COMPLETE);
    FMP_DBG("[HMP-CUE]end of playing");
}

status_t CuePlayer::getTrackInfo(Parcel *reply)
{
    const char * const undLang = "und";
    int aud_num = 0;
    int vid_num = 0;
    int field_num = 2;
    int total_stream_num = mMovieFile->nb_streams;
    reply->writeInt32(total_stream_num);

    int i = 0;
    for (i = 0; i < total_stream_num; i++) {
        if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
            aud_num++;
        }else if (mMovieFile->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO) {
            vid_num++;
        }
    }

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
    return OK;
}

status_t CuePlayer::getCueInfo(Parcel *reply)
{
    if(/*!isPlaying() || */!isCue)
        return INVALID_OPERATION;

    reply->writeString16(String16(mCueFile.fileName));
    reply->writeString16(String16(mCueFile.title));
    reply->writeInt32(mCueFile.songNum);
    for(int i=0; i<mCueSongs.size(); i++){
        CueSongBean song = mCueSongs[i];
        reply->writeInt32(song.idx);
        reply->writeString16(String16(song.title));
        reply->writeString16(String16(song.performer));
#ifdef OLD_TYPE
        reply->writeInt32(song.duration);
#else
        reply->writeInt32(getSongDur(i));
#endif
        reply->writeInt32(song.start_time);
    }
    return OK;
}

status_t CuePlayer::getSongInfo(int index,Parcel *reply)
{
    if(/*!isPlaying() || */!isCue)
        return INVALID_OPERATION;

    FMP_DBG("[HMP-CUE]getSongInfo");
    if(index < 0 || index >= mCueSongs.size())
        return BAD_VALUE;

    CueSongBean song = mCueSongs[index];
    reply->writeInt32(song.idx);
    reply->writeString16(String16(song.title));
    reply->writeString16(String16(song.performer));
#ifdef OLD_TYPE
    reply->writeInt32(song.duration);
#else
    reply->writeInt32(getSongDur(index));
#endif
    reply->writeInt32(song.start_time);

    return OK;
}

#ifndef OLD_TYPE
int64_t CuePlayer::getSongDur(int idx)
{
    if(idx < 0 || idx >= mCueSongs.size()){
        return -1;
    }

    CueSongBean song = mCueSongs[idx];
    int64_t dur = song.duration;
    int64_t endTime = 0;
    if(idx == mCueSongs.size()-1){
        endTime = (mDuration==-1) ? song.start_time + 180000 : mDuration;
    }else{
        CueSongBean nextSone = mCueSongs[idx+1];
        endTime = (nextSone.spare_time==0) ? nextSone.start_time : nextSone.spare_time;
    }
    dur = endTime - song.start_time;
    if(dur < 0) dur = 0;
    ///FMP_DBG("CueTest:  %d(%d)-endTime=%lld|startTime=%lld|dur=%lld",idx,mCueSongs.size(),endTime,song.start_time,dur);
    return dur;
}
#endif
status_t CuePlayer::chooseSong(int index)
{
    if(/*!isPlaying() || */!isCue)
        return INVALID_OPERATION;

    FMP_DBG("[HMP-CUE]chooseSong %d(%d)",index,mCueSongs.size());
    if(index < 0 || index >= mCueSongs.size())
        return BAD_VALUE;

    mCurIdx = index;

    int songPos = mCueSongs[index].start_time;
    status_t ret = seekTo(songPos);

    FMP_DBG("[HMP-CUE]chooseSong:  pos=%d | dur=%d", songPos, mDuration);
    return ret;
}

void CuePlayer::ffmpeg_notify(void* ptr, int level, const char* fmt, va_list vl) {
    FMP_DBG("[HMP-CUE]ffmpegNotify level=%d  fmt=%s",level, fmt);
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

#if 0
status_t CuePlayer::setDataSource(const sp<IStreamSource> &source) {
    return INVALID_OPERATION;
}

status_t CuePlayer::initCheck() {
    FMP_DBG("[HMP-CUE]initCheck");
    return NO_ERROR;//INVALID_OPERATION;
}

status_t CuePlayer::setLooping(int loop) {
    FMP_DBG("[HMP-CUE]setLooping");
    mLoop = loop ? true : false;
    return NO_ERROR;
}

status_t CuePlayer::setUID(uid_t uid) {
    FMP_DBG("[HMP-CUE]setUID %d",uid);
    mUid = uid;
    return INVALID_OPERATION;
}

bool CuePlayer::isPlaying() {
    //FMP_DBG("[HMP-CUE]isPlaying");
    return mCurrentState == FF_MEDIA_PLAYER_STARTED || mCurrentState == FF_MEDIA_PLAYER_DECODED;
}

status_t CuePlayer::getVideoWidth(int *w)
{
    FMP_DBG("[HMP-CUE]getVideoWidth %d",mVideoWidth);

    if (mCurrentState < MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    *w = mVideoWidth;
    return NO_ERROR;
}

status_t CuePlayer::getVideoHeight(int *h)
{
    FMP_DBG("[HMP-CUE]getVideoHeight %d",mVideoHeight);

    if (mCurrentState < FF_MEDIA_PLAYER_PREPARED) {
        return INVALID_OPERATION;
    }
    *h = mVideoHeight;
    return NO_ERROR;
}

status_t CuePlayer::setParameter(int key, const Parcel &request) {
    FMP_DBG("[HMP-CUE]setParameter(key=%d)", key);
    return NO_ERROR;
}

status_t CuePlayer::getParameter(int key, Parcel *reply) {
    FMP_DBG("[HMP-CUE]getParameter");
    return NO_ERROR;
}

//status_t CuePlayer::setListener(const sp<MediaPlayerBase> &listener)
status_t CuePlayer::setListener(const wp<MediaPlayerBase> &listener)
{
    FMP_DBG( "[HMP-CUE]setListener");
    mListener = listener;
    return NO_ERROR;
}

void CuePlayer::notify(int msg, int ext1, int ext2)
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

status_t CuePlayer::dump(int fd, const Vector<String16> &args) const {
    return INVALID_OPERATION;
}

#endif

}  // namespace android


