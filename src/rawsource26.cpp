/*
 RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <algorithm>
#include <cinttypes>
#include <string>
#include <tuple>
#include <unordered_map>
#include "common.h"



class RawSource : public IClip {

    VideoInfo vi;
    int fileHandle;
    int64_t fileSize;
    int order[4];
    int col_count;
    bool show;

    uint8_t* rawbuf;
    i_struct* index;

    void setProcess(const char* pix_type);

    void(__stdcall *writeDestFrame)(
        int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
        ise_t* env);

public:
    RawSource(const char* source, const int width, const int height,
              const char* pix_type, const int fpsnum, const int fpsden,
              const char* index, const bool show, ise_t* env);
    PVideoFrame __stdcall GetFrame(int n, ise_t *env);

    ~RawSource() { _close(fileHandle); }
    bool __stdcall GetParity(int n) { return vi.image_type == VideoInfo::IT_TFF; }
    void __stdcall GetAudio(void *buf, int64_t start, int64_t count, ise_t* env) {}
    const VideoInfo& __stdcall GetVideoInfo() { return vi; }
    int __stdcall SetCacheHints(int hints, int)
    {
        return hints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
    }
};


void RawSource::setProcess(const char* pix_type)
{
    using std::make_tuple;
    constexpr int Y = PLANAR_Y, U = PLANAR_U, V = PLANAR_V;
    constexpr int G = PLANAR_G, B = PLANAR_B, R = PLANAR_R, A = PLANAR_A;
    constexpr int X = 99999999;

    typedef void (__stdcall *write_frame_t)(
        int, PVideoFrame&, uint8_t*, int*, int, ise_t*);
    typedef std::tuple<int, int, int, int, int, int, write_frame_t> pixel_format_t;

    std::unordered_map<std::string, pixel_format_t> table;

    table["BGR"]        = make_tuple(VideoInfo::CS_BGR24,       0, 1, 2, X, 1, write_planar);
    table["BGR24"]      = make_tuple(VideoInfo::CS_BGR24,       0, 1, 2, X, 1, write_planar);
    table["RGB"]        = make_tuple(VideoInfo::CS_BGR24,       2, 1, 0, X, 3, write_packed_reorder_8);
    table["RGB24"]      = make_tuple(VideoInfo::CS_BGR24,       2, 1, 0, X, 3, write_packed_reorder_8);

    table["BGR48"]      = make_tuple(VideoInfo::CS_BGR48,       0, 1, 2, X, 1, write_planar);
    table["RGB48"]      = make_tuple(VideoInfo::CS_BGR48,       2, 1, 0, X, 3, write_packed_reorder_16);

    table["BGRA"]       = make_tuple(VideoInfo::CS_BGR32,       0, 1, 2, 3, 1, write_planar);
    table["BGR32"]      = make_tuple(VideoInfo::CS_BGR32,       0, 1, 2, 3, 1, write_planar);
    table["RGBA"]       = make_tuple(VideoInfo::CS_BGR32,       2, 1, 0, 3, 4, write_packed_reorder_8);
    table["RGB32"]      = make_tuple(VideoInfo::CS_BGR32,       2, 1, 0, 3, 4, write_packed_reorder_8);
    table["ARGB"]       = make_tuple(VideoInfo::CS_BGR32,       3, 2, 1, 0, 4, write_packed_reorder_8);
    table["ARGB32"]     = make_tuple(VideoInfo::CS_BGR32,       3, 2, 1, 0, 4, write_packed_reorder_8);
    table["ABGR"]       = make_tuple(VideoInfo::CS_BGR32,       3, 0, 1, 2, 4, write_packed_reorder_8);
    table["ABGR32"]     = make_tuple(VideoInfo::CS_BGR32,       3, 0, 1, 2, 4, write_packed_reorder_8);

    table["BGR64"]      = make_tuple(VideoInfo::CS_BGR64,       0, 1, 2, 3, 1, write_planar);
    table["BGRA64"]     = make_tuple(VideoInfo::CS_BGR64,       0, 1, 2, 3, 1, write_planar);
    table["RGB64"]      = make_tuple(VideoInfo::CS_BGR64,       2, 1, 0, 3, 4, write_packed_reorder_16);
    table["RGBA64"]     = make_tuple(VideoInfo::CS_BGR64,       2, 1, 0, 3, 4, write_packed_reorder_16);
    table["ARGB64"]     = make_tuple(VideoInfo::CS_BGR64,       3, 2, 1, 0, 4, write_packed_reorder_16);
    table["ABGR64"]     = make_tuple(VideoInfo::CS_BGR64,       3, 0, 1, 2, 4, write_packed_reorder_16);

    table["YUY2"]       = make_tuple(VideoInfo::CS_YUY2,        0, 1, 2, 3, 1, write_planar);
    table["YUYV"]       = make_tuple(VideoInfo::CS_YUY2,        0, 1, 2, 3, 1, write_planar);
    table["UYVY"]       = make_tuple(VideoInfo::CS_YUY2,        1, 0, 3, 2, 4, write_packed_reorder_8);
    table["VYUY"]       = make_tuple(VideoInfo::CS_YUY2,        3, 0, 1, 2, 4, write_packed_reorder_8);

    table["GBRP"]       = make_tuple(VideoInfo::CS_RGBP,        G, B, R, X, 3, write_planar);
    table["GBRP10"]     = make_tuple(VideoInfo::CS_RGBP10,      G, B, R, X, 3, write_planar);
    table["GBRP12"]     = make_tuple(VideoInfo::CS_RGBP12,      G, B, R, X, 3, write_planar);
    table["GBRP14"]     = make_tuple(VideoInfo::CS_RGBP14,      G, B, R, X, 3, write_planar);
    table["GBRP16"]     = make_tuple(VideoInfo::CS_RGBP16,      G, B, R, X, 3, write_planar);
    table["GBRPS"]      = make_tuple(VideoInfo::CS_RGBPS,       G, B, R, X, 3, write_planar);

    table["GBRAP"]      = make_tuple(VideoInfo::CS_RGBAP,       G, B, R, A, 4, write_planar);
    table["GBRAP10"]    = make_tuple(VideoInfo::CS_RGBAP10,     G, B, R, A, 4, write_planar);
    table["GBRAP12"]    = make_tuple(VideoInfo::CS_RGBAP12,     G, B, R, A, 4, write_planar);
    table["GBRAP14"]    = make_tuple(VideoInfo::CS_RGBAP14,     G, B, R, A, 4, write_planar);
    table["GBRAP16"]    = make_tuple(VideoInfo::CS_RGBAP16,     G, B, R, A, 4, write_planar);
    table["GBRAPS"]     = make_tuple(VideoInfo::CS_RGBAPS,      G, B, R, A, 4, write_planar);

    table["YV24"]       = make_tuple(VideoInfo::CS_YV24,        Y, V, U, X, 3, write_planar);
    table["YUV444P8"]   = make_tuple(VideoInfo::CS_YV24,        Y, U, V, X, 3, write_planar);
    table["YUV444P9"]   = make_tuple(VideoInfo::CS_YUV444P10,   Y, U, V, X, 3, write_planar_9);
    table["YUV444P10"]  = make_tuple(VideoInfo::CS_YUV444P10,   Y, U, V, X, 3, write_planar);
    table["YUV444P12"]  = make_tuple(VideoInfo::CS_YUV444P12,   Y, U, V, X, 3, write_planar);
    table["YUV444P14"]  = make_tuple(VideoInfo::CS_YUV444P14,   Y, U, V, X, 3, write_planar);
    table["YUV444P16"]  = make_tuple(VideoInfo::CS_YUV444P16,   Y, U, V, X, 3, write_planar);
    table["YUV444PS"]   = make_tuple(VideoInfo::CS_YUV444PS,    Y, U, V, X, 3, write_planar);

    table["YUVA444P8"]  = make_tuple(VideoInfo::CS_YUVA444,     Y, U, V, A, 4, write_planar);
    table["YUVA444P10"] = make_tuple(VideoInfo::CS_YUV444P10,   Y, U, V, A, 4, write_planar);
    table["YUVA444P12"] = make_tuple(VideoInfo::CS_YUV444P12,   Y, U, V, A, 4, write_planar);
    table["YUVA444P14"] = make_tuple(VideoInfo::CS_YUV444P14,   Y, U, V, A, 4, write_planar);
    table["YUVA444P16"] = make_tuple(VideoInfo::CS_YUV444P16,   Y, U, V, A, 4, write_planar);
    table["YUVA444PS"]  = make_tuple(VideoInfo::CS_YUV444PS,    Y, U, V, A, 4, write_planar);

    table["YV16"]       = make_tuple(VideoInfo::CS_YV16,        Y, V, U, X, 3, write_planar);
    table["YUV422P8"]   = make_tuple(VideoInfo::CS_YV16,        Y, U, V, X, 3, write_planar);
    table["YUV422P9"]   = make_tuple(VideoInfo::CS_YUV444P10,   Y, U, V, X, 3, write_planar_9);
    table["YUV422P10"]  = make_tuple(VideoInfo::CS_YUV422P10,   Y, U, V, X, 3, write_planar);
    table["YUV422P12"]  = make_tuple(VideoInfo::CS_YUV422P12,   Y, U, V, X, 3, write_planar);
    table["YUV422P14"]  = make_tuple(VideoInfo::CS_YUV422P14,   Y, U, V, X, 3, write_planar);
    table["YUV422P16"]  = make_tuple(VideoInfo::CS_YUV422P16,   Y, U, V, X, 3, write_planar);
    table["YUV422PS"]   = make_tuple(VideoInfo::CS_YUV422PS,    Y, U, V, X, 3, write_planar);

    table["YUVA422P8"]  = make_tuple(VideoInfo::CS_YUVA422,     Y, U, V, A, 4, write_planar);
    table["YUVA422P10"] = make_tuple(VideoInfo::CS_YUVA422P10,  Y, U, V, A, 4, write_planar);
    table["YUVA422P12"] = make_tuple(VideoInfo::CS_YUVA422P12,  Y, U, V, A, 4, write_planar);
    table["YUVA422P14"] = make_tuple(VideoInfo::CS_YUVA422P14,  Y, U, V, A, 4, write_planar);
    table["YUVA422P16"] = make_tuple(VideoInfo::CS_YUVA422P16,  Y, U, V, A, 4, write_planar);
    table["YUVA422PS"]  = make_tuple(VideoInfo::CS_YUVA422PS,   Y, U, V, A, 4, write_planar);

    table["P210"]       = make_tuple(VideoInfo::CS_YUV422P16,   Y, U, V, X, 2, write_packed_chroma_16);
    table["P216"]       = make_tuple(VideoInfo::CS_YUV422P16,   Y, U, V, X, 2, write_packed_chroma_16);

    table["YV411"]      = make_tuple(VideoInfo::CS_YV411,       Y, V, U, X, 3, write_planar);
    table["Y41B"]       = make_tuple(VideoInfo::CS_YV411,       Y, V, U, X, 3, write_planar);
    table["YUV411P8"]   = make_tuple(VideoInfo::CS_YV411,       Y, U, V, X, 3, write_planar);

    table["YV12"]       = make_tuple(VideoInfo::CS_YV12,        Y, V, U, X, 3, write_planar);
    table["I420"]       = make_tuple(VideoInfo::CS_I420,        Y, U, V, X, 3, write_planar);
    table["IYUV"]       = make_tuple(VideoInfo::CS_I420,        Y, U, V, X, 3, write_planar);
    table["YUV420P8"]   = make_tuple(VideoInfo::CS_I420,        Y, U, V, X, 3, write_planar);
    table["YUV420P9"]   = make_tuple(VideoInfo::CS_YUV420P10,   Y, U, V, X, 3, write_planar_9);
    table["YUV420P10"]  = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 3, write_planar);
    table["YUV420P12"]  = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 3, write_planar);
    table["YUV420P14"]  = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 3, write_planar);
    table["YUV420P16"]  = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 3, write_planar);
    table["YUV420PS"]   = make_tuple(VideoInfo::CS_YUV420PS,    Y, U, V, X, 3, write_planar);

    table["YUVA420P8"]  = make_tuple(VideoInfo::CS_YUVA420,     Y, U, V, A, 4, write_planar);
    table["YUVA420P10"] = make_tuple(VideoInfo::CS_YUVA420P16,  Y, U, V, A, 4, write_planar);
    table["YUVA420P12"] = make_tuple(VideoInfo::CS_YUVA420P16,  Y, U, V, A, 4, write_planar);
    table["YUVA420P14"] = make_tuple(VideoInfo::CS_YUVA420P16,  Y, U, V, A, 4, write_planar);
    table["YUVA420P16"] = make_tuple(VideoInfo::CS_YUVA420P16,  Y, U, V, A, 4, write_planar);
    table["YUVA420PS"]  = make_tuple(VideoInfo::CS_YUVA420PS,   Y, U, V, A, 4, write_planar);

    table["NV12"]       = make_tuple(VideoInfo::CS_I420,        Y, U, V, X, 2, write_packed_chroma_8);
    table["NV21"]       = make_tuple(VideoInfo::CS_YV12,        Y, V, U, X, 2, write_packed_chroma_8);
    table["P010"]       = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 2, write_packed_chroma_16);
    table["P016"]       = make_tuple(VideoInfo::CS_YUV420P16,   Y, U, V, X, 2, write_packed_chroma_16);

    table["Y8"]         = make_tuple(VideoInfo::CS_Y8,          Y, X, X, X, 1, write_planar);
    table["GREY8"]      = make_tuple(VideoInfo::CS_Y8,          Y, X, X, X, 1, write_planar);
    table["Y10"]        = make_tuple(VideoInfo::CS_Y10,         Y, X, X, X, 1, write_planar);
    table["Y12"]        = make_tuple(VideoInfo::CS_Y12,         Y, X, X, X, 1, write_planar);
    table["Y14"]        = make_tuple(VideoInfo::CS_Y14,         Y, X, X, X, 1, write_planar);
    table["Y16"]        = make_tuple(VideoInfo::CS_Y16,         Y, X, X, X, 1, write_planar);
    table["GREY16"]     = make_tuple(VideoInfo::CS_Y16,         Y, X, X, X, 1, write_planar);
    table["Y32"]        = make_tuple(VideoInfo::CS_Y32,         Y, X, X, X, 1, write_planar);
    table["GREYS"]      = make_tuple(VideoInfo::CS_Y32,         Y, X, X, X, 1, write_planar);

    auto key = std::string(pix_type);
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);

    std::tie(vi.pixel_type, order[0], order[1], order[2], order[3], col_count, writeDestFrame) = table[key];

    validate(vi.pixel_type == 0, "Invalid pixel type. Supported types are bellows.:\n"
        "RGB, RGBA, BGR, BGRA, ARGB, ABGR, RGB48, BGR48, RGBA64, BGRA64, ARGB64, ABGR64,\n"
        "GBRP, GBRP10, GBRP12, GBRP14, GBRP16, GBRPS, GBRAP, GBRAP10, GBRAP12, GBRAP14, GBRAP16, GBRAPS,\n"
        "Y8, GREY8, Y10, Y12, Y14, Y16, GREY16, Y32, GREYS, YUY2, YUYV, UYVY, YVYU, VYUY,\n"
        "YV24, I444, YUV444P8, YUV444P9, YUV444P10, YUV444P12, YUV444P14, YUV444P16, YUV444PS,\n"
        "YUVA444P8, YUVA444P10, YUVA444P12, YUVA444P14, YUVA444P16, YUVA444PS,\n"
        "YV16, I422, YUV422P8, YUV422P9, YUV422P10, YUV422P12, YUV422P14, YUV422P16, YUV422PS,\n"
        "YUVA422P8, YUVA422P10, YUVA422P12, YUVA422P14, YUVA422P16, YUVA422PS,\n"
        "YV12, I420, IYUV, YUV420P8, YUV420P9, YUV420P10, YUV420P12, YUV420P14, YUV420P16, YUV420PS,\n"
        "YUVA420P8, YUVA420P10, YUVA420P12, YUVA420P14, YUVA420P16, YUVA420PS,\n"
        "YV411, Y41B, NV12, NV21, P210, P216, P010, P016");

}


RawSource::RawSource(const char *source, const int width, const int height,
                     const char *ptype, const int fpsnum, const int fpsden,
                     const char *a_index, const bool s, ise_t* env) : show(s)
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

    if (strlen(a_index) == 0) { //use header if valid else width, height, pixel_type from AVS are used
        std::vector<char> read_buff(256, 0);
        char* data = read_buff.data();
        _read(fileHandle, data, read_buff.size()); //read some bytes and test on header
        if (parse_y4m(read_buff, vi, header_offset, frame_offset)) {
            strcpy(pix_type, data);
        }
    }

    setProcess(pix_type);

    size_t framesize = vi.width * vi.height * vi.BitsPerPixel() / 8;

    int maxframe = static_cast<int>(fileSize / framesize);    //1 = one frame

    validate(maxframe < 1, "File too small for even one frame.");

    //index build using string descriptor
    std::vector<rindex> rawindex;
    set_rawindex(rawindex, a_index, header_offset, frame_offset, framesize);

    auto env2 = static_cast<IScriptEnvironment2*>(env);
    auto free_buffer = [](void* p, ise_t* e) {
        static_cast<IScriptEnvironment2*>(e)->Free(p);
        p = nullptr;
    };

    //create full index and get number of frames.
    void* b = env2->Allocate((maxframe + 1) * sizeof(i_struct), 8, AVS_NORMAL_ALLOC);
    validate(!b, "failed to allocate index array.");
    env->AtExit(free_buffer, b);
    index = reinterpret_cast<i_struct*>(b);
    vi.num_frames = generate_index(index, rawindex, framesize, fileSize);

    b = env2->Allocate(vi.BytesFromPixels(vi.width * vi.height), 64, AVS_NORMAL_ALLOC);
    validate(!b, "failed to allocate read buffer.");
    env->AtExit(free_buffer, b);
    rawbuf = reinterpret_cast<uint8_t*>(b);

}


PVideoFrame __stdcall RawSource::GetFrame(int n, ise_t* env)
{
    auto dst = env->NewVideoFrame(vi);

    if (_lseeki64(fileHandle, index[n].index, SEEK_SET) == -1L) {
        // black frame with message
        write_black_frame(dst, vi);
        env->ApplyMessage(&dst, vi, "failed to seek file!", vi.width,
                          0xFFFFFF, 0xFFFFFF, 0);
        return dst;
    }

    writeDestFrame(fileHandle, dst, rawbuf, order, col_count, env);

    if (show) { //output debug info
        char info[64];
        sprintf(info, "%d : %" PRIi64 " %c", n, index[n].index, index[n].type);
        env->ApplyMessage(&dst, vi, info, vi.width / 2, 0xFFFFFF, 0, 0);
    }

    return dst;
}


AVSValue __cdecl create_rawsource(AVSValue args, void* user_data, ise_t* env)
{
    char buff[128] = {};

    try {
        validate(!args[0].Defined(), "No source specified");

        const char *source = args[0].AsString();
        const int width = args[1].AsInt(720);
        const int height = args[2].AsInt(576);
        const char *pix_type = args[3].AsString("YUV420P8");
        const int fpsnum = args[4].AsInt(25);
        const int fpsden = args[5].AsInt(1);
        const char *index = args[6].AsString("");
        const bool show = args[7].AsBool(false);

        if (width < MIN_WIDTH || height < MIN_HEIGHT) {
            snprintf(buff, 127, "width and height need to be %u x %u or higher.",
                     MIN_WIDTH, MIN_HEIGHT);
            throw std::runtime_error(buff);
        }

        validate(strlen(pix_type) > 15, "pixel_type is too long.");
        validate(fpsnum < 1 || fpsden < 1,
                 "fpsnum and fpsden need to be 1 or higher.");

        return new RawSource(source, width, height, pix_type, fpsnum, fpsden,
                             index, show, env);

    } catch (std::runtime_error& e) {
        env->ThrowError("RawSourcePlus: %s", e.what());
    }
    return 0;
}


const AVS_Linkage* AVS_linkage = nullptr;


extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(ise_t* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    const char* args =
        "[file]s"
        "[width]i"
        "[height]i"
        "[pixel_type]s"
        "[fpsnum]i"
        "[fpsden]i"
        "[index]s"
        "[show]b";

    env->AddFunction("RawSourcePlus", args, create_rawsource, nullptr);

    return "RawSource for Avisynth+.";
}

