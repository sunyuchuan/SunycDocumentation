
#define LOG_TAG "FF_Func"
#include "ff_func.h"

#include <utils/Log.h>

#define FF_DBG  ALOGI

//Get current system time
int64_t getCurSysTime(){
    struct timeval tv_s;
    gettimeofday(&tv_s, NULL);
    int64_t curSystemClock = ((tv_s.tv_sec) * 1000) +  (tv_s.tv_usec / 1000);
    return curSystemClock; //msec
}

AVFrame *ff_alloc_frame(){
#ifndef FFMPEG_30
    return avcodec_alloc_frame();
#else
    return av_frame_alloc();
#endif
}

void ff_initial() {
    av_register_all();
    avformat_network_init();
}

void ff_deinit(){
    avformat_network_deinit();
}

int ff_open_input_file(AVFormatContext **ic_ptr, const char *filename){
#ifndef FFMPEG_30
    int ret = av_open_input_file(ic_ptr, filename, NULL, 0, NULL);
#else
    int ret = avformat_open_input(ic_ptr, filename, NULL, NULL);
#endif
    if(ret < 0){
        FF_DBG("[HMP] av_open_input_file fail %s",ff_err2str(ret));
    }
    return ret;
}

int ff_find_stream_info(AVFormatContext *ic){
#ifndef FFMPEG_30
    int ret = av_find_stream_info(ic);
#else
    int ret = avformat_find_stream_info(ic,NULL);
#endif
    if(ret < 0){
        FF_DBG("[HMP] av_find_stream_info fail %s",ff_err2str(ret));
    }
    return ret;
}

AVCodec *ff_find_decoder(enum CodecID id){
    AVCodec * codec = avcodec_find_decoder(id);
    if(codec == NULL){
        FF_DBG("[HMP]avcodec_find_decoder fail");
    }else{
        FF_DBG("[HMP]avcodec_find_decoder : name=%s",codec->name);
    }
    return codec;
}

int ff_codec_open(AVCodecContext *avctx, AVCodec *codec)
{
#ifndef FFMPEG_30
    int ret = avcodec_open(avctx, codec);
#else
    int ret = avcodec_open2(avctx, codec, NULL);
#endif
    if(ret < 0){
        FF_DBG("[HMP] avcodec_open fail %s",ff_err2str(ret));
    }
    return ret;
}


int ff_read_frame(AVFormatContext *s, AVPacket *pkt)
{
#ifndef FFMPEG_30
    int ret = read_frame(s, pkt);
#else
    int ret = av_read_frame(s, pkt);
#endif
    if(ret < 0) {
        //FF_DBG("[HMP]read_frame fail %s",ff_err2str(ret));
    }else{
        ///ffplayer_dump_packet(pkt);
    }
    return ret;
}

int ff_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp,int flags)
{
    int ret = av_seek_frame(s, stream_index, timestamp, flags);
    if(ret < 0){
        FF_DBG("[HMP] av_seek_frame fail %s",ff_err2str(ret));
    }
    return ret;
}


#ifndef FFMPEG_30
int ff_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt)
{
    int ret = codec_decode_audio(avctx, samples, frame_size_ptr, avpkt);
#else
int ff_decode_audio(AVCodecContext *avctx, AVFrame *samples, int *frame_size_ptr, AVPacket *avpkt)
{
    int ret = avcodec_decode_audio4(avctx, samples, frame_size_ptr, avpkt);
#endif
    if(ret < 0) {
        FF_DBG("[HMP]codec_decode_audio fail %s",ff_err2str(ret));
    }else{
        //FF_DBG("[HMP]avcodec_decode_audio  (ret=%d)",ret);
        //FF_DBG("[HMP]				       (pktIdx=%d)",avpkt->stream_index);
        //FF_DBG("[HMP]				       (pktSize=%d)",avpkt->size);
        //FF_DBG("[HMP]				       (pktPos=%d)",avpkt->size);
        //FF_DBG("[HMP]				       (pktDur=%d)",avpkt->duration);
        //FF_DBG("[HMP]				       (pktPts=%d)",avpkt->pts);
        //FF_DBG("[HMP]				       (convDur=%d)",avpkt->convergence_duration);
        //FF_DBG("[HMP]				       (frmSize=%d)",*frame_size_ptr);
        //FF_DBG("[HMP]				       (len=%d)",ret);
    }
    return ret;
}

int ff_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *packet)
{
#ifndef FFMPEG_30
    int ret = codec_decode_video(avctx, picture, got_picture_ptr, packet);
#else
    int ret = avcodec_decode_video2(avctx, picture, got_picture_ptr, packet);
#endif
    if(ret < 0) {
        FF_DBG("[HMP]avcodec_decode_video2 fail %s",ff_err2str(ret));
    }
    return ret;
}

int ff_decode_subtitle(AVCodecContext *avctx, AVSubtitle *picture, int *got_picture_ptr, AVPacket *packet)
{
    int ret = -1;
    ret = avcodec_decode_subtitle2(avctx, picture, got_picture_ptr, packet);
    if(ret < 0) {
        FF_DBG("[HMP]avcodec_decode_subtitle2 fail %s",ff_err2str(ret));
    }
    return ret;
}


void ff_free_packet(AVPacket *pkt)
{
#ifndef FFMPEG_30
    if (pkt) {
        if (pkt->destruct){
            //pkt->destruct(pkt);
        }
        if(pkt->data){
            av_free(pkt->data);
            pkt->data = NULL;
        }
        pkt->size = 0;
    }
#else
    av_packet_unref(pkt);
#endif
}


//*************************************************************************************************************
/* XXX: suppress the packet queue */
#ifndef FFMPEG_30
static void flush_packet_queue(AVFormatContext *s)
{
    AVPacketList *pktl;

    for(;;) {
        pktl = s->packet_buffer;
        if (!pktl)
            break;
        s->packet_buffer = pktl->next;
        ff_free_packet(&pktl->pkt);
        av_free(pktl);
    }
    while(s->raw_packet_buffer){
        pktl = s->raw_packet_buffer;
        s->raw_packet_buffer = pktl->next;
        ff_free_packet(&pktl->pkt);
        av_free(pktl);
    }
    s->packet_buffer_end=
    s->raw_packet_buffer_end= NULL;
    s->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;
}

void ff_free_context(AVFormatContext *s)
{
    int i;
    AVStream *st;

    av_opt_free(s);

    if (s->iformat && s->iformat->priv_class && s->priv_data)
        av_opt_free(s->priv_data);

    for(i=0;i<s->nb_streams;i++) {
        /* free all data in a stream component */
        st = s->streams[i];
        if (st->parser) {
            av_parser_close(st->parser);
            ff_free_packet(&st->cur_pkt);
        }
        av_dict_free(&st->metadata);
        av_freep(&st->index_entries);
        av_freep(&st->codec->extradata);
        av_freep(&st->codec->subtitle_header);
        av_freep(&st->codec);
#if FF_API_OLD_METADATA
        av_freep(&st->filename);
#endif
        av_freep(&st->priv_data);
        av_freep(&st->info);
        av_freep(&st);
    }

    for(i=s->nb_programs-1; i>=0; i--) {
#if FF_API_OLD_METADATA
        av_freep(&s->programs[i]->provider_name);
        av_freep(&s->programs[i]->name);
#endif
        av_metadata_free(&s->programs[i]->metadata);
        av_freep(&s->programs[i]->stream_index);
        av_freep(&s->programs[i]);
    }

    av_freep(&s->programs);
    av_freep(&s->priv_data);
    while(s->nb_chapters--) {
#if FF_API_OLD_METADATA
        av_free(s->chapters[s->nb_chapters]->title);
#endif
        av_dict_free(&s->chapters[s->nb_chapters]->metadata);
        av_freep(&s->chapters[s->nb_chapters]);
    }
    av_freep(&s->chapters);
    av_metadata_free(&s->metadata);
    //av_freep(&s->key);
    av_free(s);
}


int ff_url_closep(URLContext **hh)
{
    URLContext *h= *hh;
    int ret = 0;
    if (!h)
        return 0;     /* can happen when ffurl_open fails */

    if (h->is_connected && h->prot->url_close)
        ret = h->prot->url_close(h);
#if CONFIG_NETWORK
    if (h->prot->flags & URL_PROTOCOL_FLAG_NETWORK)
        ff_network_close();
#endif
    if (h->prot->priv_data_size) {
        if (h->prot->priv_data_class)
            av_opt_free(h->priv_data);
        av_freep(&h->priv_data);
    }
    av_freep(hh);
    return ret;
}

int ff_io_close(AVIOContext *s)
{
    if (!s) return 0;

    avio_flush(s);
    URLContext *h = s->opaque;
    av_freep(&s->buffer);
    av_free(s);
    return ff_url_closep(&h);
}
#endif

void ff_close_input_file(AVFormatContext *s)
{
#ifndef FFMPEG_30
    /********av_close_input_stream***********/
    flush_packet_queue(s);
    if (s->iformat->read_close) s->iformat->read_close(s);
    ff_free_context(s);
    /*************************************/
    AVIOContext *pb = (s->iformat->flags & AVFMT_NOFILE) || (s->flags & AVFMT_FLAG_CUSTOM_IO) ? NULL : s->pb;
    if(pb) ff_io_close(pb);//avio_close
#else
    avformat_close_input(&s);
#endif
}


struct SwsContext *ff_getContext(AVCodecContext *codec){

    return sws_getContext(codec->width,codec->height,codec->pix_fmt,codec->width,codec->height,
#ifndef FFMPEG_30
								 PIX_FMT_RGB565, SWS_BICUBIC, NULL, NULL, NULL);
#else
								 AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
#endif
}

int ff_picture_fill(AVPicture *picture, uint8_t *ptr, AVCodecContext *codec)
{
#ifndef FFMPEG_30
    return avpicture_fill(picture,ptr, PIX_FMT_RGB565, codec->width,codec->height);
#else
    return avpicture_fill(picture,ptr, AV_PIX_FMT_YUV420P, codec->width,codec->height);
#endif
}

int ff_scale(struct SwsContext *context, AVFrame* frame,AVFrame* dstframe, int srcSliceY, int srcSliceH)
{
    return sws_scale(context, frame->data,frame->linesize, srcSliceY, srcSliceH, dstframe->data, dstframe->linesize);
}
