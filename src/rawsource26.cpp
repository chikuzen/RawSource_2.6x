/*
 RawSource26 - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Pech Ernst)
    for avisynth2.6.
*/

#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <windows.h>
#include "avisynth.h"

#pragma warning(disable:4996)
#define MAX_PIXTYPE_LEN 32
#define MAX_Y4M_HEADER 128
#define MIN_RESOLUTION 8
#define MAX_WIDTH 65536
#define MAX_HEIGHT 65536

#define Y4M_STREAM_MAGIC "YUV4MPEG2"
#define Y4M_STREAM_MAGIC_LEN 9
#define Y4M_FRAME_MAGIC "FRAME"
#define Y4M_FRAME_MAGIC_LEN 5

static const AVS_Linkage* AVS_linkage = 0;

typedef void (*FuncWriteDestFrame)(int fd, PVideoFrame& dst, BYTE* buff, int* order, int count, IScriptEnvironment* env);

static void WriteNV420(int fd, PVideoFrame& dst, BYTE* buff, int* order, int count, IScriptEnvironment* env)
{
    int width = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    BYTE* dstp = dst->GetWritePtr(PLANAR_Y);
    int pitch = dst->GetPitch(PLANAR_Y);
    int read_size = width * height;

    ZeroMemory(buff, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);

    read_size >>= 1;
    width >>= 1;
    height >>= 1;
    dstp = dst->GetWritePtr(order[1]);
    pitch = dst->GetPitch(order[1]);
    BYTE* dstp2 = dst->GetWritePtr(order[2]);
    
    ZeroMemory(buff, read_size);
    _read(fd, buff, read_size);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            dstp[j]  = buff[j << 1];
            dstp2[j] = buff[(j << 1) + 1];
        }
        buff += width;
        dstp += pitch;
        dstp2 += pitch;
    }
}

static void WritePlanar(int fd, PVideoFrame& dst, BYTE* buff, int* order, int count, IScriptEnvironment* env)
{
    for (int i = 0; i < count; i++) {
        int debug = order[i];
        int width = dst->GetRowSize(order[i]);
        int height = dst->GetHeight(order[i]);
        BYTE* dstp = dst->GetWritePtr(order[i]);
        int pitch = dst->GetPitch(order[i]);
        int read_size = width * height;
        ZeroMemory(buff, read_size);
        _read(fd, buff, read_size);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }
}

static void WritePacked(int fd, PVideoFrame& dst, BYTE* buff, int* order, int count, IScriptEnvironment* env)
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    BYTE* dstp = dst->GetWritePtr();
    int pitch = dst->GetPitch();
    int read_size = width * height;
    ZeroMemory(buff, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);
}

static void WritePackedWithReorder(int fd, PVideoFrame& dst, BYTE* buff, int* order, int count, IScriptEnvironment* env)
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    BYTE* dstp = dst->GetWritePtr();
    int pitch = dst->GetPitch();
    int read_size = width * height;
    ZeroMemory(buff, width * height);
    _read(fd, buff, read_size);

    for (int i = 0; i < height; i++) {
        for (int j = 0, time = width / count; j < time; j++) {
            for (int k = 0; k < count; k++) {
                dstp[j * count + k] = buff[j * count + order[k]];
            }
        }
        buff += width;
        dstp += pitch;
    }
}

class RawSource : public IClip {

    VideoInfo vi;
    int h_rawfile;
    __int64 filelen;
    __int64 headeroffset;
    int y4m_headerlen;
    char y4m_headerbuf[MAX_Y4M_HEADER];
    char pix_type[MAX_PIXTYPE_LEN];
    int order[4];
    int col_count;
    int ret;
    int level;
    bool show;
    BYTE *rawbuf;

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
    FuncWriteDestFrame WriteDestFrame;

public:
    RawSource(const char *sourcefile, const int a_width, const int a_height,
              const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
              const char *a_index, const bool a_show, IScriptEnvironment *env);
    virtual ~RawSource();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
    bool __stdcall GetParity(int n);
    void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment* env) {}
    const VideoInfo& __stdcall GetVideoInfo() {return vi;}
    int __stdcall SetCacheHints(int cachehints,int frame_range) { return 0; }
};

RawSource::RawSource (const char *sourcefile, const int a_width, const int a_height,
                      const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
                      const char *a_index, const bool a_show, IScriptEnvironment *env)
{
    if ((h_rawfile = _open(sourcefile, _O_BINARY | _O_RDONLY)) == -1)
        env->ThrowError("Cannot open videofile.");

    if ((filelen = _filelengthi64(h_rawfile)) == -1L)
        env->ThrowError("Cannot get videofile length.");

    ZeroMemory(&vi, sizeof(VideoInfo));
    vi.width = a_width;
    vi.height = a_height;
    vi.fps_numerator = a_fpsnum;
    vi.fps_denominator = a_fpsden;
    vi.SetFieldBased(false);

    strcpy(pix_type, a_pix_type);

    level = 0;
    show = a_show;

    y4m_headerlen = 0;
    headeroffset = 0;

    if (!strcmp(a_index, "")) {    //use header if valid else width, height, pixel_type from AVS are used
        ret = _read(h_rawfile, y4m_headerbuf, MAX_Y4M_HEADER);    //read some bytes and test on header
        ret = ParseHeader();
        if (vi.width > MAX_WIDTH)
            env->ThrowError("Width too big(%d). Maximum acceptable width is %d.", vi.width, MAX_WIDTH);
        if (vi.height > MAX_HEIGHT)
            env->ThrowError("Height too big(%d). Maximum acceptable height is %d.", vi.height, MAX_HEIGHT);
        switch (ret) {
        case -1:
            env->ThrowError("YUV4MPEG2 header error.");
            break;
        case -2:
            env->ThrowError("This file's YUV4MPEG2 HEADER is unsupported.");
        default:
            break;
        }
    }

    struct {
        char *fmt_name;
        int avs_pix_type;
        int order[4];
        int cnt;
        FuncWriteDestFrame func;
    } pixelformats[] = {
        {"BGR",   VideoInfo::CS_BGR24, {       0,        1,        2, 9}, 3, WritePacked           },
        {"BGR24", VideoInfo::CS_BGR24, {       0,        1,        2, 9}, 3, WritePacked           },
        {"RGB",   VideoInfo::CS_BGR24, {       2,        1,        0, 9}, 3, WritePackedWithReorder},
        {"RGB24", VideoInfo::CS_BGR24, {       2,        1,        0, 9}, 3, WritePackedWithReorder},
        {"BGRA",  VideoInfo::CS_BGR32, {       0,        1,        2, 3}, 4, WritePacked           },
        {"BGR32", VideoInfo::CS_BGR32, {       0,        1,        2, 3}, 4, WritePacked           },
        {"RGBA",  VideoInfo::CS_BGR32, {       2,        1,        0, 3}, 4, WritePackedWithReorder},
        {"RGB32", VideoInfo::CS_BGR32, {       2,        1,        0, 3}, 4, WritePackedWithReorder},
        {"ARGB",  VideoInfo::CS_BGR32, {       3,        2,        1, 0}, 4, WritePackedWithReorder},
        {"ABGR",  VideoInfo::CS_BGR32, {       3,        0,        1, 2}, 4, WritePackedWithReorder},
        {"YUY2",  VideoInfo::CS_YUY2,  {       0,        1,        2, 3}, 4, WritePacked           },
        {"YUYV",  VideoInfo::CS_YUY2,  {       0,        1,        2, 3}, 4, WritePacked           },
        {"UYVY",  VideoInfo::CS_YUY2,  {       1,        0,        3, 2}, 4, WritePackedWithReorder},
        {"VYUY",  VideoInfo::CS_YUY2,  {       3,        0,        1, 2}, 4, WritePackedWithReorder},
        {"YV24",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, WritePlanar           },
        {"I444",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, WritePlanar           },
        {"YV16",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, WritePlanar           },
        {"I422",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, WritePlanar           },
        {"YV411", VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, WritePlanar           },
        {"Y41B",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, WritePlanar           },
        {"I411",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, WritePlanar           },
        {"I420",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, WritePlanar           },
        {"IYUV",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, WritePlanar           },
        {"YV12",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, WritePlanar           },
        {"NV12",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 2, WriteNV420            },
        {"NV21",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 2, WriteNV420            },
        {"Y8",    VideoInfo::CS_Y8,    {PLANAR_Y,        0,        0, 0}, 1, WritePlanar           },
        {"GRAY",  VideoInfo::CS_Y8,    {PLANAR_Y,        0,        0, 0}, 1, WritePlanar           },
        {pix_type, VideoInfo::CS_UNKNOWN, NULL}
    };
    int i = 0;
    while (stricmp(pix_type, pixelformats[i].fmt_name))
        i++;
    if (pixelformats[i].avs_pix_type == VideoInfo::CS_UNKNOWN) {
        env->ThrowError("Invalid pixel type. Supported: RGB, RGBA, BGR, BGRA, ARGB,"
                        " ABGR, YV24, I444, YUY2, YUYV, UYVY, YVYU, VYUY, YV16, I422,"
                        " YV411, Y41B, I411, YV12, I420, IYUV, NV12, NV21, Y8, GRAY");
    }

    vi.pixel_type = pixelformats[i].avs_pix_type;
    memcpy(order, pixelformats[i].order, sizeof(pixelformats[i].order));
    col_count = pixelformats[i].cnt;
    WriteDestFrame = pixelformats[i].func;

    int framesize = (vi.width * vi.height * vi.BitsPerPixel()) >> 3;

    int maxframe = (int)(filelen / (__int64)framesize);    //1 = one frame

    if (maxframe < 1)
        env->ThrowError("File too small for even one frame.");

    index = new i_struct[maxframe + 1];
    rawindex = new ri_struct[maxframe + 1];
    rawbuf = new unsigned char[vi.IsPlanar() ? vi.width * vi.height: vi.width * vi.height * (vi.BitsPerPixel() >> 3)];

//index build using string descriptor
    char * indstr;
    FILE * indexfile;
    char seps[] = " \n";
    char * token;
    int frame;
    __int64 bytepos;
    int p_ri = 0;
    int rimax;
    char * p_del;
    int num1, num2;
    int delta;    //delta between 1 frame
    int big_delta;    //delta between many e.g. 25 frames
    int big_frame_step;    //how many frames is big_delta for?
    int big_steps;    //how many big deltas have occured

//read all framenr:bytepos pairs
    if (!strcmp(a_index, "")) {
        rawindex[0].framenr = 0;
        rawindex[0].bytepos = headeroffset;
        rawindex[1].framenr = 1;
        rawindex[1].bytepos = headeroffset + y4m_headerlen + framesize;
        rimax = 1;
    } else {
        const char * pos = strchr(a_index, '.');
        if (pos != NULL) {    //assume indexstring is a filename
            indexfile = fopen(a_index, "r");
            if (indexfile == 0)
                env->ThrowError("Cannot open indexfile.");
            fseek(indexfile, 0, SEEK_END);
            ret = ftell(indexfile);
            indstr = new char[ret + 1];
            fseek(indexfile, 0, SEEK_SET);
            fread(indstr, 1, ret, indexfile);
            fclose(indexfile);
        } else {
            indstr = new char[strlen(a_index) + 1];
            strcpy(indstr, a_index);
        }
        token = strtok(indstr, seps);
        while (token) {
            num1 = -1;
            num2 = -1;
            p_del = strchr(token, ':');
            if (!p_del)
                break;
            ret = sscanf(token, "%d", &num1);
            ret = sscanf(p_del + 1, "%d", &num2);

            if ((num1 < 0) || (num2 < 0))
                break;
            rawindex[p_ri].framenr = num1;
            rawindex[p_ri].bytepos = num2;
            p_ri++;
            token = strtok(0, seps);
        }
        rimax = p_ri - 1;
        delete indstr;
    }

    if ((rimax < 0) || rawindex[0].framenr)
        env->ThrowError("When using an index: frame 0 is mandatory");    //at least an entries for frame0

//fill up missing bytepos (create full index)
    frame = 0;    //framenumber
    p_ri = 0;    //pointer to raw index
    big_frame_step = 0;
    big_steps = 0;
    big_delta = 0;

    delta = framesize;
    //rawindex[1].bytepos - rawindex[0].bytepos;    //current bytepos delta
    bytepos = rawindex[0].bytepos;
    index[frame].type = 'K';
    while ((frame < maxframe) && ((bytepos + framesize) <= filelen)) {    //next frame must be readable
        index[frame].index = bytepos;

        if ((p_ri < rimax) && (rawindex[p_ri].framenr <= frame)) {
            p_ri++;
            big_steps = 1;
        }
        frame++;

        if ((p_ri > 0) && (rawindex[p_ri - 1].framenr + big_steps * big_frame_step == frame)) {
            bytepos = rawindex[p_ri - 1].bytepos + big_delta * big_steps;
            big_steps++;
            index[frame].type = 'B';
        } else {
            if (rawindex[p_ri].framenr == frame) {
                bytepos = rawindex[p_ri].bytepos;    //sync if framenumber is given in raw index
                index[frame].type = 'K';
            } else {
                bytepos = bytepos + delta;
                index[frame].type = 'D';
            }
        }

//check for new delta and big_delta
        if ((p_ri > 0) && (rawindex[p_ri].framenr == rawindex[p_ri-1].framenr + 1)) {
            delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
        } else if (p_ri > 1) {
//if more than 1 frame difference and
//2 successive equal distances then remember as big_delta
//if second delta < first delta then reset
            if (rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr == rawindex[p_ri - 1].framenr - rawindex[p_ri - 2].framenr) {
                big_frame_step = rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr;
                big_delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
            } else {
                if ((rawindex[p_ri].framenr - rawindex[p_ri - 1].framenr) < (rawindex[p_ri - 1].framenr - rawindex[p_ri - 2].framenr)) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
                if (frame >= rawindex[p_ri].framenr) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
            }
        }
        frame = frame;
    }
    vi.num_frames = frame;
}


RawSource::~RawSource() {
    _close(h_rawfile);
    delete [] rawbuf;
    delete [] index;
    delete [] rawindex;
}

PVideoFrame __stdcall RawSource::GetFrame(int n, IScriptEnvironment* env)
{
    if (show && !level) {    //output debug info - call Subtitle
        char message[255];
        sprintf(message, "%d : %I64d %c", n, index[n].index, index[n].type);
        const char* arg_names[11] = {0, 0, "x", "y", "font", "size", "text_color", "halo_color"};
        AVSValue args[8] = {this, AVSValue(message), 4, 12, "Arial", 15, 0xFFFFFF, 0x000000};
        level = 1;
        PClip resultClip = (env->Invoke("Subtitle", AVSValue(args, 8), arg_names )).AsClip();
        PVideoFrame src1 = resultClip->GetFrame(n, env);
        level = 0;
        return src1;
    }

    PVideoFrame dst = env->NewVideoFrame(vi);
    if ((ret = (int)_lseeki64(h_rawfile, index[n].index, SEEK_SET)) == -1L) {
        return dst;    //error. do nothing
    }
    WriteDestFrame(h_rawfile, dst, rawbuf, order, col_count, env);
    return dst;
}

int RawSource::ParseHeader()
{
    if (strncmp(y4m_headerbuf, Y4M_STREAM_MAGIC, Y4M_STREAM_MAGIC_LEN))
        return 0;

    vi.height = 0;
    vi.width = 0;
    strcpy(pix_type, "i420");

    unsigned int numerator = 0;
    unsigned int denominator = 0;
    char ctag[9] = {0};
    int i;
    for (i = Y4M_STREAM_MAGIC_LEN; (i < MAX_Y4M_HEADER) && (y4m_headerbuf[i] != '\n'); i++) {
        if (!strncmp(y4m_headerbuf + i, " W", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%d", &vi.width);
        }

        if (!strncmp(y4m_headerbuf + i, " H", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%d", &vi.height);
        }

        if (!strncmp(y4m_headerbuf + i, " I", 2)) {
            i += 2;
            if (y4m_headerbuf[i] == 'm')
                return -2;
            else if (y4m_headerbuf[i] == 't')
                vi.image_type = VideoInfo::IT_TFF;
            else if (y4m_headerbuf[i] == 'b')
                vi.image_type = VideoInfo::IT_BFF;
        }

        if (!strncmp(y4m_headerbuf + i, " F", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%u:%u", &numerator, &denominator);
            if (numerator && denominator)
                vi.SetFPS(numerator, denominator);
            else
                return -1;
        }

        if (!strncmp(y4m_headerbuf + i, " C", 2)) {
            i += 2;
            sscanf(y4m_headerbuf + i, "%s", ctag);
            if (!strncmp(ctag, "444alpha", 8)) {
                strcpy(pix_type, "AYUV");
            } else if (!strncmp(ctag, "444", 3)) {
                strcpy(pix_type, "I444");
            } else if (!strncmp(ctag, "422", 3)) {
                strcpy(pix_type, "I422");
            } else if (!strncmp(ctag, "411", 3)) {
                strcpy(pix_type, "I411");
            } else if (!strncmp(ctag, "420", 3)) {
                strcpy(pix_type, "I420");
            } else if (!strncmp(ctag, "mono", 4)) {
                strcpy(pix_type, "GRAY");
            } else {
                return -1;
            }
        }
    }

    if (!numerator || !denominator || !vi.width || !vi.height) {
        return -1;
    }

    i++;

    if (strncmp(y4m_headerbuf + i, Y4M_FRAME_MAGIC, Y4M_FRAME_MAGIC_LEN)) {
        return -1;
    }

    headeroffset = i;

    i += Y4M_FRAME_MAGIC_LEN;

    while ((i < 128) && (y4m_headerbuf[i] != '\n'))
        i++;

    y4m_headerlen = i - (int)headeroffset + 1;
    headeroffset = headeroffset + y4m_headerlen;

    return 0;
}

bool __stdcall RawSource::GetParity(int n)
{
    return vi.image_type == VideoInfo::IT_TFF;
}

AVSValue __cdecl CreateRawSource(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    if (!args[0].Defined())
        env->ThrowError("RawSource: No source specified");

    const char *source = args[0].AsString();
    const int width = args[1].AsInt(720);
    const int height = args[2].AsInt(576);
    const char *pix_type = args[3].AsString("YUY2");
    const int fpsnum = args[4].AsInt(25);
    const int fpsden = args[5].AsInt(1);
    const char *index = args[6].AsString("");
    const bool show = args[7].AsBool(false);

    if (width < MIN_RESOLUTION || height < MIN_RESOLUTION)
        env->ThrowError("RawSource: width and height need to be %d or higher.", MIN_RESOLUTION);
    if (width > MAX_WIDTH)
        env->ThrowError("RawSource: width needs to be %d or lower.", MAX_WIDTH);
    if (height > MAX_HEIGHT)
        env->ThrowError("RawSource: height needs to be %d or lower.", MAX_HEIGHT);
    if (strlen(pix_type) > MAX_PIXTYPE_LEN - 1)
        env->ThrowError("RawSource: pixel_type needs to be %d chars or shorter.", MAX_PIXTYPE_LEN - 1);
    if (fpsnum < 1 || fpsden < 1)
        env->ThrowError("RawSource: fpsnum and fpsden need to be 1 or higher.");

    return new RawSource(source, width, height, pix_type, fpsnum, fpsden, index, show, env);
}

extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;
    env->AddFunction("RawSource",
                     "[file]s[width]i[height]i[pixel_type]s[fpsnum]i[fpsden]i[index]s[show]b",
                     CreateRawSource, 0);
  return "RawSource for AviSynth2.6x";
}
