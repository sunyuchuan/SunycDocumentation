
//---------------------------------------------------------------------------
#include "ff_func.h"

#include <utils/Log.h>

#define HMP_DBG  ALOGI


//---------------------------------------------------------------------------
#ifdef FFPLAYER_NEED_DUMP
    static FILE *file_hmp_handle = NULL;
    static char file_hmp_path[] = "/data/user/ffplayer_pcm.pcm";
    static int dump_cnt = 0;
    static int dump_req= 1000000;
int ffplayer_dump_audio(const void *buffer, int size)
{
        if(dump_cnt >= dump_req)
            return -1;

        if(file_hmp_handle == NULL)
        {
            file_hmp_handle = fopen(file_hmp_path, "wb+");
            if(file_hmp_handle == NULL)
            {
                ALOGE("pcm write dump file fail %s ", file_hmp_path);
                return -1;
            }
            else
            {
                ALOGI("pcm write dump file done %d\n", file_hmp_handle);
            }
        }

        int ret = fwrite(buffer, size,1, file_hmp_handle);
        if(ret <= 0)
        {
            ALOGE("pcm write dump data fail");
        }
        dump_cnt ++;
        if(dump_cnt == dump_req){
            ALOGI("HeliosPlayer has dumped [%d] buffer, going to close!",dump_req);
            fclose(file_hmp_handle);
        }

        ///ALOGI("dump pcm data %d	(cnt = %d)", size,dump_cnt);

        return ret;
}

    static FILE *file_pkt_handle = NULL;
    static char file_pkt_path[] = "/data/user/ffplayer_pkt.pcm";

int ffplayer_dump_packet(AVPacket * pPacket){
        if(file_pkt_handle == NULL)
        {
            file_pkt_handle = fopen(file_pkt_path, "wb+");
            if(file_pkt_handle == NULL)
            {
                ALOGE("pcm write dump file fail %s ", file_pkt_path);
                return -1;
            }
            else
            {
                ALOGI("pcm write dump file done %d\n", file_pkt_handle);
            }
        }

        int ret = fwrite(pPacket->data, pPacket->size,1, file_pkt_handle);
        if(ret <= 0)
        {
            ALOGE("pcm write dump data fail");
        }

        ///ALOGI("dump pcm data %d	(cnt = %d)", size,dump_cnt);

        return ret;
}

#endif
//---------------------------------------------------------------------------
#define AV_ERROR_MAX_STRING_SIZE 64

char *ff_err2str(int errnum){
    char *errbuf = (char[AV_ERROR_MAX_STRING_SIZE]){0};

    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);

    return errbuf;
}


//---------------------------------------------------------------------------

void prtVideoContext(AVFormatContext* formatctx)
{
    if(formatctx == NULL)
        return;

    HMP_DBG("[HMP]------video_codec_id:%d",formatctx->video_codec_id);
    HMP_DBG("[HMP]------bit_rate:%d",formatctx->bit_rate);
    HMP_DBG("[HMP]------nb_streams:%d",formatctx->nb_streams);
    //HMP_DBG("[HMP]------author:%s",formatctx->author);
    HMP_DBG("[HMP]------duration:%d",formatctx->duration);
}

void prtAudioContext(AVFormatContext* formatctx)
{
    if(formatctx == NULL)
        return;

    HMP_DBG("[HMP]------audio_codec_id:%d",formatctx->audio_codec_id);
    HMP_DBG("[HMP]------bit_rate:%d",formatctx->bit_rate);
    HMP_DBG("[HMP]------nb_streams:%d",formatctx->nb_streams);
    //HMP_DBG("[HMP]------author:%s",formatctx->author);
    HMP_DBG("[HMP]------duration:%d",formatctx->duration);
}


void prtApeInfo(APEContext *ape_ctx)
{

    ///APEContext *ape_ctx = s->priv_data;

    int i;

    HMP_DBG("Descriptor Block:\n\n");
    HMP_DBG("magic                = \"%c%c%c%c\"\n", ape_ctx->magic[0], ape_ctx->magic[1], ape_ctx->magic[2], ape_ctx->magic[3]);
    HMP_DBG("fileversion          = %"PRId16"\n", ape_ctx->fileversion);
    HMP_DBG("descriptorlength     = %"PRIu32"\n", ape_ctx->descriptorlength);
    HMP_DBG("headerlength         = %"PRIu32"\n", ape_ctx->headerlength);
    HMP_DBG("seektablelength      = %"PRIu32"\n", ape_ctx->seektablelength);
    HMP_DBG("wavheaderlength      = %"PRIu32"\n", ape_ctx->wavheaderlength);
    HMP_DBG("audiodatalength      = %"PRIu32"\n", ape_ctx->audiodatalength);
    HMP_DBG("audiodatalength_high = %"PRIu32"\n", ape_ctx->audiodatalength_high);
    HMP_DBG("wavtaillength        = %"PRIu32"\n", ape_ctx->wavtaillength);
    HMP_DBG("md5                  = ");
    HMP_DBG("1~8:  %02x | %02x | %02x | %02x | %02x | %02x | %02x | %02x \n",
         ape_ctx->md5[0],ape_ctx->md5[1],ape_ctx->md5[2],ape_ctx->md5[3],ape_ctx->md5[4],ape_ctx->md5[5],ape_ctx->md5[6],ape_ctx->md5[7]);
    HMP_DBG("9~16: %02x | %02x | %02x | %02x | %02x | %02x | %02x | %02x \n",
         ape_ctx->md5[8],ape_ctx->md5[9],ape_ctx->md5[10],ape_ctx->md5[11],ape_ctx->md5[12],ape_ctx->md5[13],ape_ctx->md5[14],ape_ctx->md5[15]);
    HMP_DBG("\n");

    HMP_DBG("\nHeader Block:\n\n");

    HMP_DBG("compressiontype      = %"PRIu16"\n", ape_ctx->compressiontype);
    HMP_DBG("formatflags          = %"PRIu16"\n", ape_ctx->formatflags);
    HMP_DBG("blocksperframe       = %"PRIu32"\n", ape_ctx->blocksperframe);
    HMP_DBG("finalframeblocks     = %"PRIu32"\n", ape_ctx->finalframeblocks);
    HMP_DBG("totalframes          = %"PRIu32"\n", ape_ctx->totalframes);
    HMP_DBG("bps                  = %"PRIu16"\n", ape_ctx->bps);
    HMP_DBG("channels             = %"PRIu16"\n", ape_ctx->channels);
    HMP_DBG("samplerate           = %"PRIu32"\n", ape_ctx->samplerate);

    HMP_DBG("\nSeektable\n\n");
    if ((ape_ctx->seektablelength / sizeof(uint32_t)) != ape_ctx->totalframes) {
        HMP_DBG("No seektable\n");
    } else {
        for (i = 0; i < ape_ctx->seektablelength / sizeof(uint32_t); i++) {
            if (i < ape_ctx->totalframes - 1) {
                HMP_DBG("seektable:%8d   %d (%d bytes)\n", i, ape_ctx->seektable[i], ape_ctx->seektable[i + 1] - ape_ctx->seektable[i]);
            } else {
                HMP_DBG("%8d   %d\n", i, ape_ctx->seektable[i]);
            }
        }
    }

    HMP_DBG("\nFrames\n\n");
    for (i = 0; i < ape_ctx->totalframes; i++){
        float sec = 0.0;
        if(ape_ctx->samplerate)
            sec = (float)(ape_ctx->frames[i].nblocks/ape_ctx->samplerate);
        HMP_DBG("%8d   pos:%8"PRId64" framesize:%8d (%d samples  %f seconds)\n", i,
               ape_ctx->frames[i].pos, ape_ctx->frames[i].size,
               ape_ctx->frames[i].nblocks, sec);
    }

    HMP_DBG("\nCalculated information:\n\n");
    HMP_DBG("junklength           = %"PRIu32"\n", ape_ctx->junklength);
    HMP_DBG("firstframe           = %"PRIu32"\n", ape_ctx->firstframe);
    HMP_DBG("totalsamples         = %"PRIu32"\n", ape_ctx->totalsamples);
    if(ape_ctx->samplerate){
        int duration = ape_ctx->totalsamples / ape_ctx->samplerate;
        HMP_DBG("duration	    	  = %"PRIu32"\n", duration);
    }
}


void prtStrInfo(AVStream* stream){
    HMP_DBG("+++++++++id:         %d",stream->id);
    HMP_DBG("+++++++++duration:   %ld",stream->duration);
    HMP_DBG("+++++++++frame_num:  %ld",stream->nb_frames);
    HMP_DBG("+++++++++frm_num(c): %d",stream->codec_info_nb_frames);
    HMP_DBG("+++++++++frm_rate:   %d",stream->avg_frame_rate);
    HMP_DBG("+++++++++idx:        %d",stream->index);
    HMP_DBG("+++++++++codec name: %s",stream->codec->codec_name);
    HMP_DBG("+++++++++codec id:   %d",stream->codec->codec_id);
    HMP_DBG("+++++++++bitrate:    %ld",stream->codec->bit_rate);
}


