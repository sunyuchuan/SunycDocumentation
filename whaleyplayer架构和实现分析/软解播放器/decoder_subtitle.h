#ifndef FFMPEG_DECODER_SUBTITLE_H
#define FFMPEG_DECODER_SUBTITLE_H

#include "decoder.h"
#include <media/MediaPlayerInterface.h>
#include "clock.h"

namespace android {

enum {
    // These keys must be in sync with the keys in TimedText.java
    KEY_DISPLAY_FLAGS                 = 1, // int
    KEY_STYLE_FLAGS                   = 2, // int
    KEY_BACKGROUND_COLOR_RGBA         = 3, // int
    KEY_HIGHLIGHT_COLOR_RGBA          = 4, // int
    KEY_SCROLL_DELAY                  = 5, // int
    KEY_WRAP_TEXT                     = 6, // int
    KEY_START_TIME                    = 7, // int
    KEY_STRUCT_BLINKING_TEXT_LIST     = 8, // List<CharPos>
    KEY_STRUCT_FONT_LIST              = 9, // List<Font>
    KEY_STRUCT_HIGHLIGHT_LIST         = 10, // List<CharPos>
    KEY_STRUCT_HYPER_TEXT_LIST        = 11, // List<HyperText>
    KEY_STRUCT_KARAOKE_LIST           = 12, // List<Karaoke>
    KEY_STRUCT_STYLE_LIST             = 13, // List<Style>
    KEY_STRUCT_TEXT_POS               = 14, // TextPos
    KEY_STRUCT_JUSTIFICATION          = 15, // Justification
    KEY_STRUCT_TEXT                   = 16, // Text

    KEY_GLOBAL_SETTING                = 101,
    KEY_LOCAL_SETTING                 = 102,
    KEY_START_CHAR                    = 103,
    KEY_END_CHAR                      = 104,
    KEY_FONT_ID                       = 105,
    KEY_FONT_SIZE                     = 106,
    KEY_TEXT_COLOR_RGBA               = 107,
};

class DecoderSubtitle : public IDecoder
{
public:
    DecoderSubtitle(AVStream*, int, int,wp<MediaPlayerBase>);
    ~DecoderSubtitle();
    void *                      mPlayerAddr;
    wp<MediaPlayerBase>         mListener;
    FFclock*                    mClock;
    void notify_p(int, int, int, Parcel *);
    void _notify_p(char* ,unsigned long);
    void assParse(char *, char *);
    void syncAndShow(char *strSubtitle);
    virtual void stop();

private:
    bool                        prepare();
    bool                        decode(void* ptr);
    bool                        process(AVPacket *packet);
    AVSubtitle                  *mSubtitle;
    AVSubtitleRect              **mSubrects;
    int                         mVideoWidth;
    int                         mVideoHeight;
    double                      mStartTime;
    double                      mEndTime;
    bool                        mAbortRequest;
};

}
#endif

