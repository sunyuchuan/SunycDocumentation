#define LOG_TAG "FFSubtitleDec"
#include <utils/Log.h>
#include "decoder_subtitle.h"
#include <time.h>

namespace android {

DecoderSubtitle::DecoderSubtitle(AVStream* stream,
    int width,int height,wp<MediaPlayerBase> listener):
    IDecoder(stream),
    mVideoWidth(width),
    mVideoHeight(height),
    mListener(listener)
{
    SLOGD("constructor");
    mSubtitle = NULL;
    mAbortRequest = false;
}

DecoderSubtitle::~DecoderSubtitle()
{
    SLOGD("destructor");
    if(mSubtitle)
    {
        avsubtitle_free(mSubtitle);
        mSubtitle = NULL;
    }
}

bool DecoderSubtitle::prepare()
{
    SLOGD("prepare");
    mSubtitle = (AVSubtitle*)av_mallocz(sizeof(AVSubtitle));
    if(NULL == mSubtitle)
    {
        ALOGE("av_malloc AVSubtitle fail");
        return false;
    }
    return true;
}

void DecoderSubtitle::stop()
{
    if(mClock != NULL) {
        mClock->stop();
    }

    mQueue->abort();
    mAbortRequest = true;
    ALOGI("waiting on end of DecoderSubtitle thread");
    int ret = -1;
    if((ret = wait()) != 0) {
        ALOGI("Couldn't cancel DecoderSubtitle: %i", ret);
        return;
    }
}

void DecoderSubtitle::syncAndShow(char *strSubtitle)
{
    if((mClock->getAudioClock() - mStartTime) < -AS_SYNC_THRESHOLD
        && !mAbortRequest)
    {
        mClock->subWait();
    }
    if((mClock->getAudioClock() - mStartTime) >= -AS_SYNC_THRESHOLD
        && (mClock->getAudioClock() - mEndTime) <= 0
        && !mAbortRequest)
    {
        _notify_p(strSubtitle,mStartTime);
        mClock->subWait();
    }
    if((mClock->getAudioClock() - mEndTime) > 0)
    {
        _notify_p(NULL,0);
    }
}

static double timeStr2sec(char *str,int len)
{
    struct tm tb;

    while(str[--len] != ':');
    double secs = atof(str+len+1);

    char fmt[] = "%H:%M:%S";
    if(strptime(str,fmt,&tb) == NULL)
        return -1;
    return secs+tb.tm_min*60+tb.tm_hour*60*60;
}

static void getTimeStr(char time[][125],char *src)
{
    int count = 0,len0 = 0,len1 = 0,index = 0;
    int len = strlen(src);
    for(int i=0; i<len; i++)
    {
        if(src[i] == ',')
        {
            count ++;
            index = i;
            continue;
        }
        switch(count)
        {
            case 1:
                time[0][i-index-1] = src[i];
                len0 = i-index;
                break;
            case 2:
                time[1][i-index-1] = src[i];
                len1 = i-index;
                break;
            case 3:
                time[0][len0] = '\0';
                time[1][len1] = '\0';
                goto end;
            default:
                break;
        }
    }
end:
    ALOGI("time0 %s, time1 %s",time[0],time[1]);
}

void DecoderSubtitle::assParse(char *dst, char *src)
{
    int index = strlen(src),strlen_dst = 0;
    char time[2][125] = {0};
    bool flag = false;

    while(src[--index] != '}')
    {
        if(src[index] == ',' && src[index-1] == ',')
            break;
    }
    strncpy(dst,src+index+1,strlen(src)-index-1);

    strlen_dst = strlen(dst);
    for(int j=0; j<strlen_dst; j++)
    {
        if(dst[j] == 92)//the char of '\' is 92
        {
            if(dst[j+1] == 'N' || dst[j+1] == 'n')
            {
                flag = true;
                dst[j] = '\n';
                continue;
            }
        }
        if(flag == true)
        {
            dst[j] = dst[j+1];
        }
    }

    getTimeStr(time,src);
    mStartTime = timeStr2sec(time[0],strlen(time[0]));
    mEndTime = timeStr2sec(time[1],strlen(time[1]));
    ALOGI("mStartTime %f, mEndTime %f",mStartTime,mEndTime);
}

void DecoderSubtitle::notify_p(int msg, int ext1, int ext2, Parcel *obj)
{
    if (mListener != NULL)
    {
        sp<MediaPlayerBase> listener = mListener.promote();
        if (listener != NULL)
            {listener->sendEvent(msg, ext1, ext2, obj);}
    }
}

void DecoderSubtitle::_notify_p(char* data,unsigned long startTime)
{
    if(data == NULL)
    {
        ALOGI("_notify clear timed text");
        notify_p(MEDIA_TIMED_TEXT,0,0,NULL);
        return;
    }

    ALOGI("_notify %s",data);
    Parcel parcel;
    parcel.writeInt32(KEY_LOCAL_SETTING);
    parcel.writeInt32(KEY_START_TIME);
    parcel.writeInt32(startTime);
    parcel.writeInt32(KEY_STRUCT_TEXT);
    // write the size of the text sample
    parcel.writeInt32(strlen(data)+1);
    // write the text sample as a byte array
    parcel.writeInt32(strlen(data)+1);
    parcel.write(data, strlen(data)+1);
    notify_p(MEDIA_TIMED_TEXT,0,0,&parcel);
}

bool DecoderSubtitle::process(AVPacket *packet)
{
    int completed = 0;
    bool flag = false;
    double pts = 0;
    char *strSubtitle = NULL;
    ALOGI("process");

    if(mPause)
        waitOnNotify();

    if(packet->pts != AV_NOPTS_VALUE)
        pts = av_q2d(mStream->time_base) * packet->pts;

    ff_decode_subtitle(mStream->codec, mSubtitle, &completed, packet);
    if(completed)
    {
        for(int i=0;i<mSubtitle->num_rects;i++)
        {
            if(SUBTITLE_ASS ==mSubtitle->rects[i]->type )
            {
                ALOGI("This is a ass format subtitle, value is %s",mSubtitle->rects[i]->ass);
                char *temp = mSubtitle->rects[i]->ass;
                if(!strSubtitle)
                    strSubtitle = (char *)malloc((strlen(temp)+1)*sizeof(char));
                if(strSubtitle)
                {
                    memset(strSubtitle,0,strlen(temp)+1);
                    assParse(strSubtitle,temp);
                    mClock->setSubClock(mStartTime,mEndTime);
                }
            }
            if(SUBTITLE_TEXT == mSubtitle->rects[i]->type)
            {
                ALOGI("This is a text format subtitle, value is %s",mSubtitle->rects[i]->text);
            }
            if( &mSubtitle->rects[i]->pict != NULL)
            {
                ALOGI("and it contains picts here !!!!");
            }
        }
    }

    if(strSubtitle)
    {
        syncAndShow(strSubtitle);
        free(strSubtitle);
        strSubtitle = NULL;
    }
    return true;
}

bool DecoderSubtitle::decode(void* ptr)
{
    AVPacket        pPacket;
    ALOGV("decoding subtitle");

    while(mRunning)
    {
        ALOGV("decoding subtitle while");
        if(mQueue->get(&pPacket, true) < 0)
        {
            ALOGI("[HMP]mQueue can't get queue & return!");
            mRunning = false;
            return false;
        }
        if(!process(&pPacket))
        {
            mRunning = false;
            return false;
        }
        // Free the packet that was allocated by av_read_frame
        ff_free_packet(&pPacket);
    }
    ALOGV("decoding subtitle ended");
    return true;
}

}

