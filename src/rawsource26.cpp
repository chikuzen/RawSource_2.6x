/*
 RawSource26 - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Pech Ernst)
    for avisynth2.6.
*/

#include <io.h>
#include <fcntl.h>
#include <cinttypes>
#include <malloc.h>
#include "common.h"



class RawSource : public IClip {

    VideoInfo vi;
    int fileHandle;
    int64_t fileSize;
    int order[4];
    int col_count;
    bool show;

    uint8_t* rawbuf;
    std::vector<i_struct> index;

    void setProcess(const char* pix_type);

    void(__stdcall *writeDestFrame)(
        int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
        ise_t* env);

public:
    RawSource(const char* source, const int width, const int height,
              const char* pix_type, const int fpsnum, const int fpsden,
              const char* index, const bool show);
    PVideoFrame __stdcall GetFrame(int n, ise_t *env);

    ~RawSource() { _close(fileHandle); _aligned_free(rawbuf); }
    bool __stdcall GetParity(int n) { return vi.image_type == VideoInfo::IT_TFF; }
    void __stdcall GetAudio(void *buf, int64_t start, int64_t count, ise_t* env) {}
    const VideoInfo& __stdcall GetVideoInfo() { return vi; }
    int __stdcall SetCacheHints(int cachehints,int frame_range) { return 0; }
};


void RawSource::setProcess(const char* pix_type)
{
    typedef void (__stdcall *write_frame_t)(
        int, PVideoFrame&, uint8_t*, int*, int, ise_t*);

    const struct {
        const char *fmt_name;
        const int avs_pix_type;
        const int order[4];
        const int cnt;
        write_frame_t func;
    } pixelformats[] = {
        {"BGR",   VideoInfo::CS_BGR24, {       0,        1,        2, 9}, 3, write_packed        },
        {"BGR24", VideoInfo::CS_BGR24, {       0,        1,        2, 9}, 3, write_packed        },
        {"RGB",   VideoInfo::CS_BGR24, {       2,        1,        0, 9}, 3, write_packed_reorder},
        {"RGB24", VideoInfo::CS_BGR24, {       2,        1,        0, 9}, 3, write_packed_reorder},
        {"BGRA",  VideoInfo::CS_BGR32, {       0,        1,        2, 3}, 4, write_packed        },
        {"BGR32", VideoInfo::CS_BGR32, {       0,        1,        2, 3}, 4, write_packed        },
        {"RGBA",  VideoInfo::CS_BGR32, {       2,        1,        0, 3}, 4, write_packed_reorder},
        {"RGB32", VideoInfo::CS_BGR32, {       2,        1,        0, 3}, 4, write_packed_reorder},
        {"ARGB",  VideoInfo::CS_BGR32, {       3,        2,        1, 0}, 4, write_packed_reorder},
        {"ABGR",  VideoInfo::CS_BGR32, {       3,        0,        1, 2}, 4, write_packed_reorder},
        {"YUY2",  VideoInfo::CS_YUY2,  {       0,        1,        2, 3}, 4, write_packed        },
        {"YUYV",  VideoInfo::CS_YUY2,  {       0,        1,        2, 3}, 4, write_packed        },
        {"UYVY",  VideoInfo::CS_YUY2,  {       1,        0,        3, 2}, 4, write_packed_reorder},
        {"VYUY",  VideoInfo::CS_YUY2,  {       3,        0,        1, 2}, 4, write_packed_reorder},
        {"YV24",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, write_planar        },
        {"I444",  VideoInfo::CS_YV24,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, write_planar        },
        {"YV16",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, write_planar        },
        {"I422",  VideoInfo::CS_YV16,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, write_planar        },
        {"YV411", VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, write_planar        },
        {"Y41B",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, write_planar        },
        {"I411",  VideoInfo::CS_YV411, {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, write_planar        },
        {"I420",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, write_planar        },
        {"IYUV",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 3, write_planar        },
        {"YV12",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 3, write_planar        },
        {"NV12",  VideoInfo::CS_I420,  {PLANAR_Y, PLANAR_U, PLANAR_V, 0}, 2, write_NV420         },
        {"NV21",  VideoInfo::CS_YV12,  {PLANAR_Y, PLANAR_V, PLANAR_U, 0}, 2, write_NV420         },
        {"Y8",    VideoInfo::CS_Y8,    {PLANAR_Y,        0,        0, 0}, 1, write_planar        },
        {"GRAY",  VideoInfo::CS_Y8,    {PLANAR_Y,        0,        0, 0}, 1, write_planar        },
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
    writeDestFrame = pixelformats[i].func;
}


RawSource::RawSource (const char *source, const int width, const int height,
                      const char *ptype, const int fpsnum, const int fpsden,
                      const char *a_index, const bool s) : show(s)
{
    fileHandle = _open(source, _O_BINARY | _O_RDONLY);
    validate(fileHandle == -1, "Cannot open videofile.");

    fileSize = _filelengthi64(fileHandle);
    validate(fileSize == -1L, "Cannot get videofile length.");

    memset(&vi, 0, sizeof(VideoInfo));
    vi.width = width;
    vi.height = height;
    vi.SetFPS(fpsnum, fpsden);
    vi.SetFieldBased(false);

    char pix_type[16] = {};
    strcpy(pix_type, ptype);

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

    rawbuf = reinterpret_cast<uint8_t*>(
        _aligned_malloc(vi.IsPlanar() ? vi.width * vi.height : framesize, 16));
    validate(rawbuf == nullptr, "failed to allocate read buffer.");

    //index build using string descriptor
    std::vector<rindex> rawindex;
    set_rawindex(rawindex, a_index, header_offset, frame_offset, framesize);

    //create full index and get number of frames.
    vi.num_frames = generate_index(index, rawindex, framesize, fileSize);
}


PVideoFrame __stdcall RawSource::GetFrame(int n, ise_t* env)
{
    const i_struct* idx = index.data();
    PVideoFrame dst = env->NewVideoFrame(vi);

    if (_lseeki64(fileHandle, idx[n].index, SEEK_SET) == -1L) {
        // black frame with message
        write_black_frame(dst, vi);
        env->ApplyMessage(&dst, vi, "failed to seek file!", vi.width,
                          0x00FFFFFF, 0x00FFFFFF, 0);
        return dst;
    }

    writeDestFrame(fileHandle, dst, rawbuf, order, col_count, env);

    if (show) { //output debug info
        char info[64];
        sprintf(info, "%d : %" PRIi64 " %c", n, idx[n].index, idx[n].type);
        env->ApplyMessage(&dst, vi, info, vi.width / 2, 0x00FFFFFF, 0, 0);
    }

    return dst;
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
                             index, show);

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
