
#define LOG_TAG "FF_Decoder"

#include "ff_func.h"

#include <utils/Log.h>

#define HAVE_PTHREADS 1

#ifndef FFMPEG_30
int codec_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt)
{
    int ret;

    avctx->pkt = avpkt;

    if (!avpkt->data && avpkt->size) {
        ALOGE("invalid packet: NULL data, size != 0\n");
        return AVERROR(EINVAL);
    }

    if((avctx->codec->capabilities & CODEC_CAP_DELAY) || avpkt->size){
        //FIXME remove the check below _after_ ensuring that all audio check that the available space is enough
        if(*frame_size_ptr < AVCODEC_MAX_AUDIO_FRAME_SIZE){
            ALOGE("buffer smaller than AVCODEC_MAX_AUDIO_FRAME_SIZE\n");
            return -1;
        }
        if(*frame_size_ptr < FF_MIN_BUFFER_SIZE ||
        *frame_size_ptr < avctx->channels * avctx->frame_size * sizeof(int16_t)){
            ALOGE("buffer %d too small\n", *frame_size_ptr);
            return -1;
        }

        ret = avctx->codec->decode(avctx, samples, frame_size_ptr, avpkt);
        avctx->frame_number++;
    }else{
        ret= 0;
        *frame_size_ptr=0;
    }
    return ret;
}


static int64_t guess_correct_pts(AVCodecContext *ctx,
                                 int64_t reordered_pts, int64_t dts)
{
    int64_t pts = AV_NOPTS_VALUE;

    if (dts != AV_NOPTS_VALUE) {
        ctx->pts_correction_num_faulty_dts += dts <= ctx->pts_correction_last_dts;
        ctx->pts_correction_last_dts = dts;
    }
    if (reordered_pts != AV_NOPTS_VALUE) {
        ctx->pts_correction_num_faulty_pts += reordered_pts <= ctx->pts_correction_last_pts;
        ctx->pts_correction_last_pts = reordered_pts;
    }
    if ((ctx->pts_correction_num_faulty_pts<=ctx->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE)
       && reordered_pts != AV_NOPTS_VALUE)
        pts = reordered_pts;
    else
        pts = dts;

    return pts;
}

#if 0
static av_always_inline void emms_c(void)
{
    if(av_get_cpu_flags() & AV_CPU_FLAG_MMX)
        __asm__ volatile ("emms" ::: "memory");
}
#endif
int codec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *avpkt)
{
    int ret;

    *got_picture_ptr= 0;
    if((avctx->coded_width||avctx->coded_height) && av_image_check_size(avctx->coded_width, avctx->coded_height, 0, avctx))
        return -1;

    if((avctx->codec->capabilities & CODEC_CAP_DELAY) || avpkt->size || (avctx->active_thread_type&FF_THREAD_FRAME)){
        avctx->pkt = avpkt;
        if (HAVE_PTHREADS && avctx->active_thread_type&FF_THREAD_FRAME)
             ret = ff_thread_decode_frame(avctx, picture, got_picture_ptr, avpkt);
        else {
            ret = avctx->codec->decode(avctx, picture, got_picture_ptr, avpkt);
            picture->pkt_dts= avpkt->dts;

            if(!avctx->has_b_frames){
                picture->pkt_pos= avpkt->pos;
            }
            //FIXME these should be under if(!avctx->has_b_frames)
            if (!picture->sample_aspect_ratio.num)
                picture->sample_aspect_ratio = avctx->sample_aspect_ratio;
            if (!picture->width)
                picture->width = avctx->width;
            if (!picture->height)
                picture->height = avctx->height;
            if (picture->format == PIX_FMT_NONE)
                picture->format = avctx->pix_fmt;
        }

        ///emms_c(); //needed to avoid an emms_c() call before every return;  //Fixme: Aladdin

        if (*got_picture_ptr){
            avctx->frame_number++;
            picture->best_effort_timestamp = guess_correct_pts(avctx, picture->pkt_pts, picture->pkt_dts);
        }
    }else
        ret= 0;

    return ret;
}




/*******************************************************/

static AVPacket *add_to_pktbuf(AVPacketList **packet_buffer, AVPacket *pkt,
                               AVPacketList **plast_pktl){
    AVPacketList *pktl = av_mallocz(sizeof(AVPacketList));
    if (!pktl)
        return NULL;

    if (*packet_buffer)
        (*plast_pktl)->next = pktl;
    else
        *packet_buffer = pktl;

    /* add the packet in the buffered packet list */
    *plast_pktl = pktl;
    pktl->pkt= *pkt;
    return &pktl->pkt;
}


static int is_intra_only(AVCodecContext *enc){
    if(enc->codec_type == AVMEDIA_TYPE_AUDIO){
        return 1;
    }else if(enc->codec_type == AVMEDIA_TYPE_VIDEO){
        switch(enc->codec_id){
        case CODEC_ID_MJPEG:
        case CODEC_ID_MJPEGB:
        case CODEC_ID_LJPEG:
        case CODEC_ID_RAWVIDEO:
        case CODEC_ID_DVVIDEO:
        case CODEC_ID_HUFFYUV:
        case CODEC_ID_FFVHUFF:
        case CODEC_ID_ASV1:
        case CODEC_ID_ASV2:
        case CODEC_ID_VCR1:
        case CODEC_ID_DNXHD:
        case CODEC_ID_JPEG2000:
            return 1;
        default: break;
        }
    }
    return 0;
}


/**
 * Get the number of samples of an audio frame. Return -1 on error.
 */
static int get_audio_frame_size(AVCodecContext *enc, int size)
{
    int frame_size;

    if(enc->codec_id == CODEC_ID_VORBIS)
        return -1;

    if (enc->frame_size <= 1) {
        int bits_per_sample = av_get_bits_per_sample(enc->codec_id);

        if (bits_per_sample) {
            if (enc->channels == 0)
                return -1;
            frame_size = (size << 3) / (bits_per_sample * enc->channels);
        } else {
            /* used for example by ADPCM codecs */
            if (enc->bit_rate == 0)
                return -1;
            frame_size = ((int64_t)size * 8 * enc->sample_rate) / enc->bit_rate;
        }
    } else {
        frame_size = enc->frame_size;
    }
    return frame_size;
}

static void update_initial_durations(AVFormatContext *s, AVStream *st, AVPacket *pkt)
{
    AVPacketList *pktl= s->packet_buffer;
    int64_t cur_dts= 0;

    if(st->first_dts != AV_NOPTS_VALUE){
        cur_dts= st->first_dts;
        for(; pktl; pktl= pktl->next){
            if(pktl->pkt.stream_index == pkt->stream_index){
                if(pktl->pkt.pts != pktl->pkt.dts || pktl->pkt.dts != AV_NOPTS_VALUE || pktl->pkt.duration)
                    break;
                cur_dts -= pkt->duration;
            }
        }
        pktl= s->packet_buffer;
        st->first_dts = cur_dts;
    }else if(st->cur_dts)
        return;

    for(; pktl; pktl= pktl->next){
        if(pktl->pkt.stream_index != pkt->stream_index)
            continue;
        if(pktl->pkt.pts == pktl->pkt.dts && pktl->pkt.dts == AV_NOPTS_VALUE
           && !pktl->pkt.duration){
            pktl->pkt.dts= cur_dts;
            if(!st->codec->has_b_frames)
                pktl->pkt.pts= cur_dts;
            cur_dts += pkt->duration;
            pktl->pkt.duration= pkt->duration;
        }else
            break;
    }
    if(st->first_dts == AV_NOPTS_VALUE)
        st->cur_dts= cur_dts;
}


static void update_initial_timestamps(AVFormatContext *s, int stream_index, int64_t dts, int64_t pts)
{
    AVStream *st= s->streams[stream_index];
    AVPacketList *pktl= s->packet_buffer;

    if(st->first_dts != AV_NOPTS_VALUE || dts == AV_NOPTS_VALUE || st->cur_dts == AV_NOPTS_VALUE)
        return;

    st->first_dts= dts - st->cur_dts;
    st->cur_dts= dts;

    for(; pktl; pktl= pktl->next){
        if(pktl->pkt.stream_index != stream_index)
            continue;
        //FIXME think more about this check
        if(pktl->pkt.pts != AV_NOPTS_VALUE && pktl->pkt.pts == pktl->pkt.dts)
            pktl->pkt.pts += st->first_dts;

        if(pktl->pkt.dts != AV_NOPTS_VALUE)
            pktl->pkt.dts += st->first_dts;

        if(st->start_time == AV_NOPTS_VALUE && pktl->pkt.pts != AV_NOPTS_VALUE)
            st->start_time= pktl->pkt.pts;
    }
    if (st->start_time == AV_NOPTS_VALUE)
        st->start_time = pts;
}


/**
 * Return the frame duration in seconds. Return 0 if not available.
 */
static void compute_frame_duration(int *pnum, int *pden, AVStream *st, AVCodecParserContext *pc, AVPacket *pkt)
{
    int frame_size;
    *pnum = 0;
    *pden = 0;
    switch(st->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        if(st->time_base.num*1000LL > st->time_base.den){
            *pnum = st->time_base.num;
            *pden = st->time_base.den;
        }else if(st->codec->time_base.num*1000LL > st->codec->time_base.den){
            *pnum = st->codec->time_base.num;
            *pden = st->codec->time_base.den;
            if (pc && pc->repeat_pict) {
                if (*pnum > INT_MAX / (1 + pc->repeat_pict))
                    *pden /= 1 + pc->repeat_pict;
                else
                    *pnum *= 1 + pc->repeat_pict;
            }
            //If this codec can be interlaced or progressive then we need a parser to compute duration of a packet
            //Thus if we have no parser in such case leave duration undefined.
            if(st->codec->ticks_per_frame>1 && !pc){
                *pnum = *pden = 0;
            }
        }
        break;
    case AVMEDIA_TYPE_AUDIO:
        frame_size = get_audio_frame_size(st->codec, pkt->size);
        if (frame_size <= 0 || st->codec->sample_rate <= 0)
            break;
        *pnum = frame_size;
        *pden = st->codec->sample_rate;
        break;
    default:
        break;
    }
}

static void compute_pkt_fields(AVFormatContext *s, AVStream *st, AVCodecParserContext *pc, AVPacket *pkt)
{
    int num, den, presentation_delayed, delay, i;
    int64_t offset;

    if (s->flags & AVFMT_FLAG_NOFILLIN)
        return;

    if((s->flags & AVFMT_FLAG_IGNDTS) && pkt->pts != AV_NOPTS_VALUE)
        pkt->dts= AV_NOPTS_VALUE;

    if (st->codec->codec_id != CODEC_ID_H264 && pc && pc->pict_type == AV_PICTURE_TYPE_B)
        //FIXME Set low_delay = 0 when has_b_frames = 1
        st->codec->has_b_frames = 1;

    /* do we have a video B-frame ? */
    delay= st->codec->has_b_frames;
    presentation_delayed = 0;

    // ignore delay caused by frame threading so that the mpeg2-without-dts warning will not trigger
    if (delay && st->codec->active_thread_type&FF_THREAD_FRAME)
        delay -= st->codec->thread_count-1;

    /* XXX: need has_b_frame, but cannot get it if the codec is
        not initialized */
    if (delay && pc && pc->pict_type != AV_PICTURE_TYPE_B)
        presentation_delayed = 1;

    if(pkt->pts != AV_NOPTS_VALUE && pkt->dts != AV_NOPTS_VALUE && pkt->dts > pkt->pts && st->pts_wrap_bits<63 ){
        pkt->dts -= 1LL<<st->pts_wrap_bits;
    }

    // some mpeg2 in mpeg-ps lack dts (issue171 / input_file.mpg)
    // we take the conservative approach and discard both
    // Note, if this is misbehaving for a H.264 file then possibly presentation_delayed is not set correctly.
    if(delay==1 && pkt->dts == pkt->pts && pkt->dts != AV_NOPTS_VALUE && presentation_delayed){
        ALOGI("invalid dts/pts combination %Ld\n", pkt->dts);
        pkt->dts= pkt->pts= AV_NOPTS_VALUE;
    }

    if (pkt->duration == 0) {
        compute_frame_duration(&num, &den, st, pc, pkt);
        if (den && num) {
            pkt->duration = av_rescale_rnd(1, num * (int64_t)st->time_base.den, den * (int64_t)st->time_base.num, AV_ROUND_DOWN);

            if(pkt->duration != 0 && s->packet_buffer)
                update_initial_durations(s, st, pkt);
        }
    }

    /* correct timestamps with byte offset if demuxers only have timestamps on packet boundaries */
    if(pc && st->need_parsing == AVSTREAM_PARSE_TIMESTAMPS && pkt->size){
        /* this will estimate bitrate based on this frame's duration and size */
        offset = av_rescale(pc->offset, pkt->duration, pkt->size);
        if(pkt->pts != AV_NOPTS_VALUE)
            pkt->pts += offset;
        if(pkt->dts != AV_NOPTS_VALUE)
            pkt->dts += offset;
    }

    if (pc && pc->dts_sync_point >= 0) {
        // we have synchronization info from the parser
        int64_t den = st->codec->time_base.den * (int64_t) st->time_base.num;
        if (den > 0) {
            int64_t num = st->codec->time_base.num * (int64_t) st->time_base.den;
            if (pkt->dts != AV_NOPTS_VALUE) {
                // got DTS from the stream, update reference timestamp
                st->reference_dts = pkt->dts - pc->dts_ref_dts_delta * num / den;
                pkt->pts = pkt->dts + pc->pts_dts_delta * num / den;
            } else if (st->reference_dts != AV_NOPTS_VALUE) {
                // compute DTS based on reference timestamp
                pkt->dts = st->reference_dts + pc->dts_ref_dts_delta * num / den;
                pkt->pts = pkt->dts + pc->pts_dts_delta * num / den;
            }
            if (pc->dts_sync_point > 0)
                st->reference_dts = pkt->dts; // new reference
        }
    }

    /* This may be redundant, but it should not hurt. */
    if(pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE && pkt->pts > pkt->dts)
        presentation_delayed = 1;

    //av_log(NULL, AV_LOG_DEBUG, "IN delayed:%d pts:%"PRId64", dts:%"PRId64" cur_dts:%"PRId64" st:%d pc:%p\n", presentation_delayed, pkt->pts, pkt->dts, st->cur_dts, pkt->stream_index, pc);
    /* interpolate PTS and DTS if they are not present */
    //We skip H264 currently because delay and has_b_frames are not reliably set
    if((delay==0 || (delay==1 && pc)) && st->codec->codec_id != CODEC_ID_H264){
        if (presentation_delayed) {
            /* DTS = decompression timestamp */
            /* PTS = presentation timestamp */
            if (pkt->dts == AV_NOPTS_VALUE)
                pkt->dts = st->last_IP_pts;
            update_initial_timestamps(s, pkt->stream_index, pkt->dts, pkt->pts);
            if (pkt->dts == AV_NOPTS_VALUE)
                pkt->dts = st->cur_dts;

            /* this is tricky: the dts must be incremented by the duration of the frame we are displaying, i.e. the last I- or P-frame */
            if (st->last_IP_duration == 0)
                st->last_IP_duration = pkt->duration;
            if(pkt->dts != AV_NOPTS_VALUE)
                st->cur_dts = pkt->dts + st->last_IP_duration;
            st->last_IP_duration  = pkt->duration;
            st->last_IP_pts= pkt->pts;
            /* cannot compute PTS if not present (we can compute it only by knowing the future */
        } else if(pkt->pts != AV_NOPTS_VALUE || pkt->dts != AV_NOPTS_VALUE || pkt->duration){
            if(pkt->pts != AV_NOPTS_VALUE && pkt->duration){
                int64_t old_diff= FFABS(st->cur_dts - pkt->duration - pkt->pts);
                int64_t new_diff= FFABS(st->cur_dts - pkt->pts);
                if(old_diff < new_diff && old_diff < (pkt->duration>>3)){
                    pkt->pts += pkt->duration;
                    //av_log(NULL, AV_LOG_DEBUG, "id:%d old:%"PRId64" new:%"PRId64" dur:%d cur:%"PRId64" size:%d\n", pkt->stream_index, old_diff, new_diff, pkt->duration, st->cur_dts, pkt->size);
                }
            }

            /* presentation is not delayed : PTS and DTS are the same */
            if(pkt->pts == AV_NOPTS_VALUE)
                pkt->pts = pkt->dts;
            update_initial_timestamps(s, pkt->stream_index, pkt->pts, pkt->pts);
            if(pkt->pts == AV_NOPTS_VALUE)
                pkt->pts = st->cur_dts;
            pkt->dts = pkt->pts;
            if(pkt->pts != AV_NOPTS_VALUE)
                st->cur_dts = pkt->pts + pkt->duration;
        }
    }

    if(pkt->pts != AV_NOPTS_VALUE && delay <= MAX_REORDER_DELAY){
        st->pts_buffer[0]= pkt->pts;
        for(i=0; i<delay && st->pts_buffer[i] > st->pts_buffer[i+1]; i++)
            FFSWAP(int64_t, st->pts_buffer[i], st->pts_buffer[i+1]);
        if(pkt->dts == AV_NOPTS_VALUE)
            pkt->dts= st->pts_buffer[0];
        if(st->codec->codec_id == CODEC_ID_H264){ //we skiped it above so we try here
            update_initial_timestamps(s, pkt->stream_index, pkt->dts, pkt->pts); // this should happen on the first packet
        }
        if(pkt->dts > st->cur_dts)
            st->cur_dts = pkt->dts;
    }

    //av_log(NULL, AV_LOG_ERROR, "OUTdelayed:%d/%d pts:%"PRId64", dts:%"PRId64" cur_dts:%"PRId64"\n", presentation_delayed, delay, pkt->pts, pkt->dts, st->cur_dts);

    /* update flags */
    if(is_intra_only(st->codec))
        pkt->flags |= AV_PKT_FLAG_KEY;
    else if (pc) {
        pkt->flags = 0;
        /* keyframe computation */
        if (pc->key_frame == 1)
            pkt->flags |= AV_PKT_FLAG_KEY;
        else if (pc->key_frame == -1 && pc->pict_type == AV_PICTURE_TYPE_I)
            pkt->flags |= AV_PKT_FLAG_KEY;
    }
    if (pc)
        pkt->convergence_duration = pc->convergence_duration;
}


static int set_codec_from_probe_data(AVFormatContext *s, AVStream *st, AVProbeData *pd)
{
    static const struct {
        const char *name; enum CodecID id; enum AVMediaType type;
    } fmt_id_type[] = {
        { "aac"      , CODEC_ID_AAC       , AVMEDIA_TYPE_AUDIO },
        { "ac3"      , CODEC_ID_AC3       , AVMEDIA_TYPE_AUDIO },
        { "dts"      , CODEC_ID_DTS       , AVMEDIA_TYPE_AUDIO },
        { "eac3"     , CODEC_ID_EAC3      , AVMEDIA_TYPE_AUDIO },
        { "h264"     , CODEC_ID_H264      , AVMEDIA_TYPE_VIDEO },
        { "m4v"      , CODEC_ID_MPEG4     , AVMEDIA_TYPE_VIDEO },
        { "mp3"      , CODEC_ID_MP3       , AVMEDIA_TYPE_AUDIO },
        { "ape" 	 , CODEC_ID_APE 	  , AVMEDIA_TYPE_AUDIO },  //Patch
        { "mpegvideo", CODEC_ID_MPEG2VIDEO, AVMEDIA_TYPE_VIDEO },
        { 0 }
    };
    int score;
    AVInputFormat *fmt = av_probe_input_format3(pd, 1, &score);

    if (fmt) {
        int i;
        ALOGI("Probe with size=%d, packets=%d detected %s with score=%d\n",
               pd->buf_size, MAX_PROBE_PACKETS - st->probe_packets, fmt->name, score);
        for (i = 0; fmt_id_type[i].name; i++) {
            if (!strcmp(fmt->name, fmt_id_type[i].name)) {
                st->codec->codec_id   = fmt_id_type[i].id;
                st->codec->codec_type = fmt_id_type[i].type;
                break;
            }
        }
    }
    return score;
}


int read_packet(AVFormatContext *s, AVPacket *pkt)
{
    int ret, i;
    AVStream *st;

    for(;;){
        AVPacketList *pktl = s->raw_packet_buffer;

        if (pktl) {
            *pkt = pktl->pkt;
            if(s->streams[pkt->stream_index]->request_probe <= 0){
                s->raw_packet_buffer = pktl->next;
                s->raw_packet_buffer_remaining_size += pkt->size;
                av_free(pktl);
                return 0;
            }
        }

        av_init_packet(pkt);

        ret= s->iformat->read_packet(s, pkt);
        if (ret < 0) {
            if (!pktl || ret == AVERROR(EAGAIN)){
                ///ALOGE("**ape read packet   %s",ff_err2str(ret));
                return ret;
            }
            for (i = 0; i < s->nb_streams; i++)
                if(s->streams[i]->request_probe > 0)
                    s->streams[i]->request_probe = -1;
            continue;
        }

        st= s->streams[pkt->stream_index];

        switch(st->codec->codec_type){
        case AVMEDIA_TYPE_VIDEO:
            if(s->video_codec_id)   st->codec->codec_id= s->video_codec_id;
            break;
        case AVMEDIA_TYPE_AUDIO:
            if(s->audio_codec_id)   st->codec->codec_id= s->audio_codec_id;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            if(s->subtitle_codec_id)st->codec->codec_id= s->subtitle_codec_id;
            break;
        }

        if(!pktl && st->request_probe <= 0){
            //ALOGE("**read_packet pktl is not exist & request_probe < 0   %s",ff_err2str(ret));
            return ret;
        }

        add_to_pktbuf(&s->raw_packet_buffer, pkt, &s->raw_packet_buffer_end);
        s->raw_packet_buffer_remaining_size -= pkt->size;

        if(st->request_probe>0){
            AVProbeData *pd = &st->probe_data;
            int end;
            av_log(s, AV_LOG_DEBUG, "probing stream %d pp:%d\n", st->index, st->probe_packets);
            --st->probe_packets;

            pd->buf = av_realloc(pd->buf, pd->buf_size+pkt->size+AVPROBE_PADDING_SIZE);
            memcpy(pd->buf+pd->buf_size, pkt->data, pkt->size);
            pd->buf_size += pkt->size;
            memset(pd->buf+pd->buf_size, 0, AVPROBE_PADDING_SIZE);

            end=    s->raw_packet_buffer_remaining_size <= 0
                 || st->probe_packets<=0;

            if(end || av_log2(pd->buf_size) != av_log2(pd->buf_size - pkt->size)){
                int score= set_codec_from_probe_data(s, st, pd);
                if( (st->codec->codec_id != CODEC_ID_NONE && score > AVPROBE_SCORE_MAX/4) || end){
                    pd->buf_size=0;
                    av_freep(&pd->buf);
                    st->request_probe= -1;
                    if(st->codec->codec_id != CODEC_ID_NONE){
                        av_log(s, AV_LOG_DEBUG, "probed stream %d\n", st->index);
                    }else
                        av_log(s, AV_LOG_WARNING, "probed stream %d failed\n", st->index);
                }
            }
        }
    }
}

static int read_frame_internal(AVFormatContext *s, AVPacket *pkt)
{
    AVStream *st;
    int len, ret, i;

    av_init_packet(pkt);

    for(;;) {
        /* select current input stream component */
        st = s->cur_st;
        if (st) {
            if (!st->need_parsing || !st->parser) {
                /* no parsing needed: we just output the packet as is raw data support */
                *pkt = st->cur_pkt;
                st->cur_pkt.data= NULL;
                compute_pkt_fields(s, st, NULL, pkt);
                s->cur_st = NULL;
                if ((s->iformat->flags & AVFMT_GENERIC_INDEX) &&
                    (pkt->flags & AV_PKT_FLAG_KEY) && pkt->dts != AV_NOPTS_VALUE) {
                    ff_reduce_index(s, st->index);
                    av_add_index_entry(st, pkt->pos, pkt->dts, 0, 0, AVINDEX_KEYFRAME);
                }
                break;
            } else if (st->cur_len > 0 && st->discard < AVDISCARD_ALL) {
                len = av_parser_parse2(st->parser, st->codec, &pkt->data, &pkt->size,
                                       st->cur_ptr, st->cur_len,
                                       st->cur_pkt.pts, st->cur_pkt.dts,
                                       st->cur_pkt.pos);
                st->cur_pkt.pts = AV_NOPTS_VALUE;
                st->cur_pkt.dts = AV_NOPTS_VALUE;
                /* increment read pointer */
                st->cur_ptr += len;
                st->cur_len -= len;

                /* return packet if any */
                if (pkt->size) {
                got_packet:
                    pkt->duration = 0;
                    pkt->stream_index = st->index;
                    pkt->pts = st->parser->pts;
                    pkt->dts = st->parser->dts;
                    pkt->pos = st->parser->pos;
                    if(pkt->data == st->cur_pkt.data && pkt->size == st->cur_pkt.size){
                        s->cur_st = NULL;
                        pkt->destruct= st->cur_pkt.destruct;
                        st->cur_pkt.destruct= NULL;
                        st->cur_pkt.data    = NULL;
                        assert(st->cur_len == 0);
                    }else{
                        pkt->destruct = NULL;
                    }
                    compute_pkt_fields(s, st, st->parser, pkt);

                    if((s->iformat->flags & AVFMT_GENERIC_INDEX) && pkt->flags & AV_PKT_FLAG_KEY){
                        int64_t pos= (st->parser->flags & PARSER_FLAG_COMPLETE_FRAMES) ? pkt->pos : st->parser->frame_offset;
                        ff_reduce_index(s, st->index);
                        av_add_index_entry(st, pos, pkt->dts, 0, 0, AVINDEX_KEYFRAME);
                    }

                    break;
                }
            } else {
                /* free packet */
                ff_free_packet(&st->cur_pkt);
                s->cur_st = NULL;
            }
        } else {
            AVPacket cur_pkt;
            /* read next packet */
            ret = read_packet(s, &cur_pkt);
            if (ret < 0) {
	            ALOGE("******read frame: read_packet fail %s",ff_err2str(ret));
                if (ret == AVERROR(EAGAIN)){
                    return ret;
                }
                /* return the last frames, if any */
                for(i = 0; i < s->nb_streams; i++) {
                    st = s->streams[i];
                    if (st->parser && st->need_parsing) {
                        av_parser_parse2(st->parser, st->codec, &pkt->data, &pkt->size,
                                        NULL, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);
                        if (pkt->size)
                            goto got_packet;
                    }
                }
                /* no more packets: really terminate parsing */
                return ret;
            }
            st = s->streams[cur_pkt.stream_index];
            st->cur_pkt= cur_pkt;

            if(st->cur_pkt.pts != AV_NOPTS_VALUE &&
               st->cur_pkt.dts != AV_NOPTS_VALUE &&
               st->cur_pkt.pts < st->cur_pkt.dts){
                ALOGW("Invalid timestamps stream=%d, pts=%"PRId64", dts=%"PRId64", size=%d\n",
                    st->cur_pkt.stream_index,
                    st->cur_pkt.pts,
                    st->cur_pkt.dts,
                    st->cur_pkt.size);
                //ff_free_packet(&st->cur_pkt);
                //return -1;
            }

            if(s->debug & FF_FDEBUG_TS)
                ALOGD("av_read_packet stream=%d, pts=%"PRId64", dts=%"PRId64", size=%d, duration=%d, flags=%d\n",
                    st->cur_pkt.stream_index,
                    st->cur_pkt.pts,
                    st->cur_pkt.dts,
                    st->cur_pkt.size,
                    st->cur_pkt.duration,
                    st->cur_pkt.flags);

            s->cur_st = st;
            st->cur_ptr = st->cur_pkt.data;
            st->cur_len = st->cur_pkt.size;
            if (st->need_parsing && !st->parser && !(s->flags & AVFMT_FLAG_NOPARSE)) {
                st->parser = av_parser_init(st->codec->codec_id);
                if (!st->parser) {
                    /* no parser available: just output the raw packets */
                    st->need_parsing = AVSTREAM_PARSE_NONE;
                }else if(st->need_parsing == AVSTREAM_PARSE_HEADERS){
                    st->parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
                }else if(st->need_parsing == AVSTREAM_PARSE_FULL_ONCE){
                    st->parser->flags |= PARSER_FLAG_ONCE;
                }
            }
        }
    }
    if(s->debug & FF_FDEBUG_TS)
        ALOGD("av_read_frame_internal stream=%d, pts=%"PRId64", dts=%"PRId64", size=%d, duration=%d, flags=%d\n",
            pkt->stream_index,
            pkt->pts,
            pkt->dts,
            pkt->size,
            pkt->duration,
            pkt->flags);

    return 0;
}

int read_frame(AVFormatContext *s, AVPacket *pkt)
{
    AVPacketList *pktl;
    int eof=0;
    const int genpts= s->flags & AVFMT_FLAG_GENPTS;

    for(;;){
        pktl = s->packet_buffer;
        if (pktl) {
            AVPacket *next_pkt= &pktl->pkt;

            if(genpts && next_pkt->dts != AV_NOPTS_VALUE){
                int wrap_bits = s->streams[next_pkt->stream_index]->pts_wrap_bits;
                while(pktl && next_pkt->pts == AV_NOPTS_VALUE){
                    if(   pktl->pkt.stream_index == next_pkt->stream_index
                       && (0 > av_compare_mod(next_pkt->dts, pktl->pkt.dts, 2LL << (wrap_bits - 1)))
                       && av_compare_mod(pktl->pkt.pts, pktl->pkt.dts, 2LL << (wrap_bits - 1))) { //not b frame
                        next_pkt->pts= pktl->pkt.dts;
                    }
                    pktl= pktl->next;
                }
                pktl = s->packet_buffer;
            }

            if( next_pkt->pts != AV_NOPTS_VALUE || next_pkt->dts == AV_NOPTS_VALUE || !genpts || eof)
            {
                /* read packet from packet buffer, if there is data */
                *pkt = *next_pkt;
                s->packet_buffer = pktl->next;
                av_free(pktl);
                return 0;
            }
        }

        if(genpts){
            int ret= read_frame_internal(s, pkt);
            if(ret<0){
                if(pktl && ret != AVERROR(EAGAIN)){
                    eof=1;
                    continue;
                }else
                    return ret;
            }

            if(av_dup_packet(add_to_pktbuf(&s->packet_buffer, pkt, &s->packet_buffer_end)) < 0)
                return AVERROR(ENOMEM);
        }else{
            assert(!s->packet_buffer);
            return read_frame_internal(s, pkt);
        }
    }
}
#endif

