#ifndef RAWSOURCE_H
#define RAWSOURCE_H

#include <stdio.h>
#include <io.h>
#include "FCNTL.H"
#include "windows.h"
#include "avisynth26.h"

#pragma warning(disable:4996)

#define MAX_PIXTYPE_LEN 32
#define MAX_Y4M_HEADER 128
#define MIN_RESOLUTION 8
#define MAX_WIDTH 4096

#define Y4M_STREAM_MAGIC "YUV4MPEG2"
#define Y4M_STREAM_MAGIC_LEN 9
#define Y4M_FRAME_MAGIC "FRAME"
#define Y4M_FRAME_MAGIC_LEN 5

class RawSource: public IClip {

    VideoInfo vi;
    int h_rawfile;
    __int64 filelen;
    __int64 headeroffset;
    int y4m_headerlen;
    char y4m_headerbuf[MAX_Y4M_HEADER];
    char pix_type[MAX_PIXTYPE_LEN];
    int mapping[4];
    int mapcnt;
    int ret;
    int level;
    bool show;
    unsigned char *rawbuf;


    struct ri_struct {
        int framenr;
        __int64 bytepos;
    };
    struct i_struct {
        __int64 index;
        char type; //Key, Delta, Bigdelta
    };

    ri_struct * rawindex;
    i_struct * index;

    int ParseHeader();

public:
    RawSource (const char *sourcefile, const int a_width, const int a_height,
               const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
               const char *a_index, const bool a_show, IScriptEnvironment *env);
    virtual ~RawSource();

// avisynth virtual functions
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
    bool __stdcall GetParity(int n);
    void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment* env) {}
    const VideoInfo& __stdcall GetVideoInfo() {return vi;}
    void __stdcall SetCacheHints(int cachehints,int frame_range) {}
};
#endif //RAWSOURCE_H
