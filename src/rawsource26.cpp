/*
 RawSource26 - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Pech Ernst)
    for avisynth2.6.
*/

#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <cinttypes>
#include "common.h"



typedef void (*FuncWriteDestFrame)(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count, ise_t* env);

static void WriteNV420(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count, ise_t* env)
{
    int width = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
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
    uint8_t* dstp2 = dst->GetWritePtr(order[2]);
    
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

static void WritePlanar(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count, ise_t* env)
{
    for (int i = 0; i < count; i++) {
        int debug = order[i];
        int width = dst->GetRowSize(order[i]);
        int height = dst->GetHeight(order[i]);
        uint8_t* dstp = dst->GetWritePtr(order[i]);
        int pitch = dst->GetPitch(order[i]);
        int read_size = width * height;
        ZeroMemory(buff, read_size);
        _read(fd, buff, read_size);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }
}

static void WritePacked(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count, ise_t* env)
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    uint8_t* dstp = dst->GetWritePtr();
    int pitch = dst->GetPitch();
    int read_size = width * height;
    ZeroMemory(buff, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);
}

static void WritePackedWithReorder(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count, ise_t* env)
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    uint8_t* dstp = dst->GetWritePtr();
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
    int fileHandle;
    int64_t fileSize;
    int order[4];
    int col_count;
    int level;
    bool show;

    std::vector<uint8_t> rawbuf;

    struct i_struct {
        int64_t index;
        char type; //Key, Delta, Bigdelta
    };
    std::vector<i_struct> index;

    void setProcess(const char* pix_type);
    FuncWriteDestFrame WriteDestFrame;

public:
    RawSource(const char *sourcefile, const int a_width, const int a_height,
              const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
              const char *a_index, const bool a_show, ise_t *env);
    virtual ~RawSource();
    PVideoFrame __stdcall GetFrame(int n, ise_t *env);
    bool __stdcall GetParity(int n);
    void __stdcall GetAudio(void *buf, int64_t start, int64_t count, ise_t* env) {}
    const VideoInfo& __stdcall GetVideoInfo() {return vi;}
    int __stdcall SetCacheHints(int cachehints,int frame_range) { return 0; }
};


void RawSource::setProcess(const char* pix_type)
{
    const struct {
        const char *fmt_name;
        const int avs_pix_type;
        const int order[4];
        const int cnt;
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
        { pix_type, VideoInfo::CS_UNKNOWN, {0, 0, 0, 0}, 0, nullptr }
    };
    int i = 0;
    while (stricmp(pix_type, pixelformats[i].fmt_name))
        i++;
    validate(pixelformats[i].avs_pix_type == VideoInfo::CS_UNKNOWN,
             "Invalid pixel type. Supported: RGB, RGBA, BGR, BGRA, ARGB,"
             " ABGR, YV24, I444, YUY2, YUYV, UYVY, YVYU, VYUY, YV16, I422,"
             " YV411, Y41B, I411, YV12, I420, IYUV, NV12, NV21, Y8, GRAY");

    vi.pixel_type = pixelformats[i].avs_pix_type;
    memcpy(order, pixelformats[i].order, sizeof(int) * 4);
    col_count = pixelformats[i].cnt;
    WriteDestFrame = pixelformats[i].func;
}


RawSource::RawSource (const char *sourcefile, const int a_width, const int a_height,
                      const char *a_pix_type, const int a_fpsnum, const int a_fpsden,
                      const char *a_index, const bool a_show, ise_t *env)
{
    fileHandle = _open(sourcefile, _O_BINARY | _O_RDONLY);
    validate(fileHandle == -1, "Cannot open videofile.");

    fileSize = _filelengthi64(fileHandle);
    validate(fileSize == -1L, "Cannot get videofile length.");

    memset(&vi, 0, sizeof(VideoInfo));
    vi.width = a_width;
    vi.height = a_height;
    vi.SetFPS(a_fpsnum, a_fpsden);
    vi.SetFieldBased(false);

    char pix_type[16] = {};
    strcpy(pix_type, a_pix_type);

    level = 0;
    show = a_show;

    int64_t header_offset = 0;
    int64_t frame_offset = 0;

    if (strlen(a_index) == 0) {    //use header if valid else width, height, pixel_type from AVS are used
        std::vector<char> read_buff(256, 0);
        char* data = read_buff.data();
        _read(fileHandle, data, read_buff.size());    //read some bytes and test on header
        bool ret = parse_y4m(read_buff, vi, header_offset, frame_offset);

        if (vi.width > MAX_WIDTH || vi.height > MAX_HEIGHT) {
            const char* msg = "Resolution too big(%d x %d)."
                              " Maximum acceptable resolution is %u x %u.";
            sprintf(data, msg, vi.width, vi.height, MAX_WIDTH, MAX_HEIGHT);
            throw std::runtime_error(data);
        }

        if (ret) {
            strcpy(pix_type, data);
        }
    }

    setProcess(pix_type);


    size_t framesize = vi.width * vi.height * vi.BitsPerPixel() / 8;

    int maxframe = static_cast<int>(fileSize / framesize);    //1 = one frame

    validate(maxframe < 1, "File too small for even one frame.");

    index.resize(maxframe + 1);

    rawbuf.resize(vi.IsPlanar() ? vi.width * vi.height : framesize);

    //index build using string descriptor
    struct rindex {
        int number;
        int64_t bytepos;
        rindex(int x, int64_t y) : number(x), bytepos(y) {}
    };

    std::vector<rindex> rawindex;
    rawindex.reserve(2);

    if (!strcmp(a_index, "")) {
        rawindex.emplace_back(0, header_offset);
        rawindex.emplace_back(1, header_offset + frame_offset + framesize);
    } else {
        std::vector<char> read_buff;
        const char * pos = strchr(a_index, '.');
        if (pos != nullptr) { //assume indexstring is a filename
            FILE* indexfile = fopen(a_index, "r");
            validate(!indexfile, "Cannot open indexfile.");
            fseek(indexfile, 0, SEEK_END);
            read_buff.resize(ftell(indexfile) + 1, 0);
            fseek(indexfile, 0, SEEK_SET);
            fread(read_buff.data(), 1, read_buff.size() - 1, indexfile);
            fclose(indexfile);
        } else {
            read_buff.resize(strlen(a_index) + 1, 0);
            strcpy(read_buff.data(), a_index);
        }
        //read all framenr:bytepos pairs
        const char* seps = " \n";
        for (char* token = strtok(read_buff.data(), seps);
                token != nullptr;
                token = strtok(nullptr, seps)) {
            int num1 = -1;
            int64_t num2 = -1;
            char* p_del = strchr(token, ':');
            if (!p_del)
                break;
            sscanf(token, "%d", &num1);
            sscanf(p_del + 1, "%" SCNi64, &num2);

            if ((num1 < 0) || (num2 < 0))
                break;
            rawindex.emplace_back(num1, num2);
        }
    }

    validate(rawindex.size() == 0 || rawindex[0].number != 0,
             "When using an index: frame 0 is mandatory");    //at least an entries for frame0

//fill up missing bytepos (create full index)
    int frame = 0;          //framenumber
    int p_ri = 0;           //pointer to raw index
    int delta = framesize;  //delta between 1 frame
    int big_delta = 0;      //delta between many e.g. 25 frames
    int big_steps = 0;      //how many big deltas have occured
    int big_frame_step = 0; //how many frames is big_delta for?

    int rimax = rawindex.size() - 1;

    //rawindex[1].bytepos - rawindex[0].bytepos;    //current bytepos delta
    int64_t bytepos = rawindex[0].bytepos;
    index[frame].type = 'K';
    while ((frame < maxframe) && ((bytepos + framesize) <= fileSize)) {    //next frame must be readable
        index[frame].index = bytepos;

        if ((p_ri < rimax) && (rawindex[p_ri].number <= frame)) {
            p_ri++;
            big_steps = 1;
        }
        frame++;

        if ((p_ri > 0) && (rawindex[p_ri - 1].number + big_steps * big_frame_step == frame)) {
            bytepos = rawindex[p_ri - 1].bytepos + big_delta * big_steps;
            big_steps++;
            index[frame].type = 'B';
        } else {
            if (rawindex[p_ri].number == frame) {
                bytepos = rawindex[p_ri].bytepos;    //sync if framenumber is given in raw index
                index[frame].type = 'K';
            } else {
                bytepos = bytepos + delta;
                index[frame].type = 'D';
            }
        }

//check for new delta and big_delta
        if ((p_ri > 0) && (rawindex[p_ri].number == rawindex[p_ri-1].number + 1)) {
            delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
        } else if (p_ri > 1) {
//if more than 1 frame difference and
//2 successive equal distances then remember as big_delta
//if second delta < first delta then reset
            if (rawindex[p_ri].number - rawindex[p_ri - 1].number == rawindex[p_ri - 1].number - rawindex[p_ri - 2].number) {
                big_frame_step = rawindex[p_ri].number - rawindex[p_ri - 1].number;
                big_delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
            } else {
                if ((rawindex[p_ri].number - rawindex[p_ri - 1].number) < (rawindex[p_ri - 1].number - rawindex[p_ri - 2].number)) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
                if (frame >= rawindex[p_ri].number) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
            }
        }
    }
    vi.num_frames = frame;
}


RawSource::~RawSource() {
    _close(fileHandle);
}

PVideoFrame __stdcall RawSource::GetFrame(int n, ise_t* env)
{
    const i_struct* idx = index.data();

    if (show && !level) {    //output debug info - call Subtitle
        char message[255];
        sprintf(message, "%d : %I64d %c", n, idx[n].index, idx[n].type);
        const char* arg_names[11] = {0, 0, "x", "y", "font", "size", "text_color", "halo_color"};
        AVSValue args[8] = {this, AVSValue(message), 4, 12, "Arial", 15, 0xFFFFFF, 0x000000};
        level = 1;
        PClip resultClip = (env->Invoke("Subtitle", AVSValue(args, 8), arg_names )).AsClip();
        PVideoFrame src1 = resultClip->GetFrame(n, env);
        level = 0;
        return src1;
    }

    PVideoFrame dst = env->NewVideoFrame(vi);
    if (_lseeki64(fileHandle, idx[n].index, SEEK_SET) == -1L) {
        return dst;    //error. do nothing
    }
    WriteDestFrame(fileHandle, dst, rawbuf.data(), order, col_count, env);
    return dst;
}


bool __stdcall RawSource::GetParity(int n)
{
    return vi.image_type == VideoInfo::IT_TFF;
}



AVSValue __cdecl CreateRawSource(AVSValue args, void* user_data, ise_t* env)
{
    char buff[128] = {};

    try {
        validate(!args[0].Defined(), "No source specified");

        const char *source = args[0].AsString();
        const int width = args[1].AsInt(720);
        const int height = args[2].AsInt(576);
        const char *pix_type = args[3].AsString("YUY2");
        const int fpsnum = args[4].AsInt(25);
        const int fpsden = args[5].AsInt(1);
        const char *index = args[6].AsString("");
        const bool show = args[7].AsBool(false);

        if (width < MIN_WIDTH || height < MIN_HEIGHT) {
            sprintf(buff, "width and height need to be %u x %u or lower.",
                    MIN_WIDTH, MIN_HEIGHT);
            throw std::runtime_error(buff);
        }
        if (width >= MAX_WIDTH || height >= MAX_HEIGHT) {
            sprintf(buff, "width and height need to be lower than %u x %u.",
                    MAX_WIDTH, MAX_HEIGHT);
            throw std::runtime_error(buff);
        }
        validate(strlen(pix_type) > 15, "pixel_type is too long.");
        validate(fpsnum < 1 || fpsden < 1,
                 "fpsnum and fpsden need to be 1 or higher.");

        return new RawSource(source, width, height, pix_type, fpsnum, fpsden,
                             index, show, env);

    } catch (std::runtime_error& e) {
        env->ThrowError("RawSource: %s", e.what());
    }
    return 0;
}


const AVS_Linkage* AVS_linkage = nullptr;


extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(ise_t* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;
    env->AddFunction("RawSource",
                     "[file]s[width]i[height]i[pixel_type]s[fpsnum]i[fpsden]i[index]s[show]b",
                     CreateRawSource, 0);

    if (env->FunctionExists("SetFilterMTMode")) {
        static_cast<IScriptEnvironment2*>(
            env)->SetFilterMTMode("RawSource", MT_SERIALIZED, true);
    }

    return "RawSource for AviSynth2.6x/Avisynth+.";
}
