#ifndef ANDROID_FF_FUNC_H
#define ANDROID_FF_FUNC_H

#include <assert.h>

#ifndef FFMPEG_30
#include "include/libavcodec/avcodec.h"
#include "include/libavformat/avformat.h"
#include "include/libswscale/swscale.h"
#include "include/libavutil/log.h"
#else
#include "ffmpeg-3.0/libavcodec/avcodec.h"
#include "ffmpeg-3.0/libavformat/avformat.h"
#include "ffmpeg-3.0/libswscale/swscale.h"
#include "ffmpeg-3.0/libavutil/log.h"
#include "ffmpeg-3.0/libswresample/swresample.h"
#include "ffmpeg-3.0/libavutil/opt.h"
#include "ffmpeg-3.0/libavutil/imgutils.h"

#define CodecID AVCodecID
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#define CODEC_TYPE_SUBTITLE AVMEDIA_TYPE_SUBTITLE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

#define ff_free             av_free
#define ff_malloc           av_malloc
#define ff_gettime          av_gettime
#define ff_log_set_callback av_log_set_callback
#define ff_rescale_q        av_rescale_q

//#define FFPLAYER_NEED_DUMP

typedef struct {
    int64_t pos;
    int nblocks;
    int size;
    int skip;
    int64_t pts;
} APEFrame;

typedef struct {
    /* Derived fields */
    uint32_t junklength;
    uint32_t firstframe;
    uint32_t totalsamples;
    int currentframe;
    APEFrame *frames;

    /* Info from Descriptor Block */
    char magic[4];
    int16_t fileversion;
    int16_t padding1;
    uint32_t descriptorlength;
    uint32_t headerlength;
    uint32_t seektablelength;
    uint32_t wavheaderlength;
    uint32_t audiodatalength;
    uint32_t audiodatalength_high;
    uint32_t wavtaillength;
    uint8_t md5[16];

    /* Info from Header Block */
    uint16_t compressiontype;
    uint16_t formatflags;
    uint32_t blocksperframe;
    uint32_t finalframeblocks;
    uint32_t totalframes;
    uint16_t bps;
    uint16_t channels;
    uint32_t samplerate;

    /* Seektable */
    uint32_t *seektable;
} APEContext;

int64_t getCurSysTime();

char *ff_err2str(int errnum);

int ffplayer_dump_audio(const void *buffer, int size);

int ffplayer_dump_packet(AVPacket * pPacket);

void prtVideoContext(AVFormatContext* formatctx);

void prtAudioContext(AVFormatContext* formatctx);

void prtApeInfo(APEContext *ape_ctx);

void prtStrInfo(AVStream* stream);

//*************************************************************************************

void ff_initial();

void ff_deinit();

AVFrame *ff_alloc_frame();

void ff_free_packet(AVPacket *pkt);

int ff_open_input_file(AVFormatContext **ic_ptr, const char *filename);

void ff_close_input_file(AVFormatContext *s);

int ff_find_stream_info(AVFormatContext *ic);

AVCodec *ff_find_decoder(enum CodecID id);

int ff_codec_open(AVCodecContext *avctx, AVCodec *codec);

int ff_read_frame(AVFormatContext *s, AVPacket *pkt);

int ff_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp,int flags);

#ifndef FFMPEG_30
int ff_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt);
#else
int ff_decode_audio(AVCodecContext *avctx, AVFrame *samples, int *frame_size_ptr, AVPacket *avpkt);
#endif

int ff_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *packet);

int ff_decode_subtitle(AVCodecContext *avctx, AVSubtitle *picture, int *got_picture_ptr, AVPacket *packet);

int codec_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt);

int codec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, AVPacket *avpkt);

struct SwsContext *ff_getContext(AVCodecContext *codec);

int ff_picture_fill(AVPicture *picture, uint8_t *ptr, AVCodecContext *codec);

int ff_scale(struct SwsContext *context, AVFrame* frame,AVFrame* dstframe, int srcSliceY, int srcSliceH);

int read_frame(AVFormatContext *s, AVPacket *pkt);

//*************************************************************************************

#endif  //ANDROID_FF_FUNC_H
