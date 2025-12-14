/*
 RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/

#include <format>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <filesystem>
#include <system_error>
#include <array>
#if defined(_WIN32)
    #include <io.h>
    #include <fcntl.h>
    #include <avs/alignment.h>
#else
    #include <avisynth/avs/alignment.h>
#endif
#include "common.h"




class RawSource : public IClip {
    VideoInfo vi;
    FILE* file;
    int64_t fileSize;
    int order[4];
    int colCount;
    bool show;
    size_t frameOffset;
    props_t props;

    uint8_t* rawbuf;
    i_struct* index;
    uint8_t* shuffleIndex;
    uint8_t* be2le;

    void openFile(const std::string& fname, std::filesystem::path& fpath);
    void setProcess(std::string& pix_type);
    void generateShuffleIndex(bool is_16bit);
    void generateBE2LEIndex();
    void parseFileName(std::filesystem::path& fpath, std::string& pix_type);

    write_frame_t writeDestFrame;

public:
    RawSource(const std::string& source, const int width, const int height,
        const std::string& pix_type, const int fpsnum, const int fpsden,
        const std::string& index, const bool show, const int sarnum,
        const int sarden, const int frames, ise_t* env);
    ~RawSource() {
        fclose(file);
        avs_free(rawbuf); rawbuf = nullptr;
        avs_free(index); index = nullptr;
        avs_free(shuffleIndex); shuffleIndex = nullptr;
        avs_free(be2le); be2le = nullptr;
    }

    PVideoFrame __stdcall GetFrame(int n, ise_t *env);
    bool __stdcall GetParity(int n) { return vi.image_type == VideoInfo::IT_TFF; }
    void __stdcall GetAudio(void *buf, int64_t start, int64_t count, ise_t* env) {}
    const VideoInfo& __stdcall GetVideoInfo() { return vi; }
    int __stdcall SetCacheHints(int hints, int)
    {
        return hints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
    }
};


void RawSource::openFile(const std::string& fname, std::filesystem::path& fpath)
{
    if (fname == "-") {
#if defined(_WIN32)
        validate(_setmode(_fileno(stdin), _O_BINARY) == -1,
            "failed to set binary mode to stdin.");
#endif
        file = stdin;
        fileSize = -1;
        return;
    }

    std::error_code ec;
#if defined(_WIN32)
    wchar_t tmp[MAX_PATH * 4] = { 0 };
    MultiByteToWideChar(CP_UTF8, 0, fname.c_str(), -1, tmp, MAX_PATH * 4);
    fpath = tmp;
    if (!std::filesystem::exists(fpath, ec)) {
        MultiByteToWideChar(CP_ACP, 0, fname.c_str(), -1, tmp, MAX_PATH * 4);
        fpath = tmp;
        validate(!std::filesystem::exists(fpath, ec),
            std::format("{} is not exists.", fname));
    }
    file = _wfopen(tmp, L"rb");
#else
    fpath = fname;
    validate(!fs::exists(fpath, ec), std::format("{} is not exists.", fname));
    file = fopen(tmp, "rb");
#endif
    validate(!file, std::format("failed to open {}.", fname));
    fileSize = static_cast<int64_t>(std::filesystem::file_size(fpath));
}

void RawSource::setProcess(std::string& pix_type)
{
    constexpr int Y = PLANAR_Y, U = PLANAR_U, V = PLANAR_V;
    constexpr int G = PLANAR_G, B = PLANAR_B, R = PLANAR_R, A = PLANAR_A;
    constexpr int X = 99999999;

    using pixel_format_t
        = std::tuple<int, int, int, int, int, int, write_frame_t> ;

    std::unordered_map<std::string, pixel_format_t> t;

    using VI = VideoInfo;
    auto mt = [](int cs, int o0, int o1, int o2, int o3, int c, write_frame_t f) {
        return std::make_tuple(cs, o0, o1, o2, o3, c, f);
    };

    t["BGR"]    = mt(VI::CS_BGR24, 0, 1, 2, X, 1, write_packed_bgr);
    t["BGR24"]  = mt(VI::CS_BGR24, 0, 1, 2, X, 1, write_packed_bgr);
    t["RGB"]    = mt(VI::CS_BGR24, 2, 1, 0, X, 3, write_rgb24);
    t["RGB24"]  = mt(VI::CS_BGR24, 2, 1, 0, X, 3, write_rgb24);

    t["BGRA"]   = mt(VI::CS_BGR32, 0, 1, 2, 3, 1, write_packed_bgr);
    t["BGR32"]  = mt(VI::CS_BGR32, 0, 1, 2, 3, 1, write_packed_bgr);
    t["BGR0"]   = mt(VI::CS_BGR32, 0, 1, 2, 3, 1, write_packed_bgr);
    t["RGBA"]   = mt(VI::CS_BGR32, 2, 1, 0, 3, 4, write_rgba32);
    t["RGB32"]  = mt(VI::CS_BGR32, 2, 1, 0, 3, 4, write_rgba32);
    t["RGB0"]   = mt(VI::CS_BGR32, 2, 1, 0, 3, 4, write_rgba32);
    t["ARGB"]   = mt(VI::CS_BGR32, 3, 2, 1, 0, 4, write_rgba32);
    t["ARGB32"] = mt(VI::CS_BGR32, 3, 2, 1, 0, 4, write_rgba32);
    t["ABGR"]   = mt(VI::CS_BGR32, 1, 2, 3, 0, 4, write_rgba32);
    t["ABGR32"] = mt(VI::CS_BGR32, 1, 2, 3, 0, 4, write_rgba32);
    t["0BGR"]   = mt(VI::CS_BGR32, 1, 2, 3, 0, 4, write_rgba32);
    t["0RGB"]   = mt(VI::CS_BGR32, 3, 2, 1, 0, 4, write_rgba32);

    t["BGR48"]   = mt(VI::CS_BGR48, 0, 1, 2, X, 1, write_packed_bgr);
    t["RGB48"]   = mt(VI::CS_BGR48, 2, 1, 0, X, 3, write_rgb48);
    t["BGR48BE"] = mt(VI::CS_BGR48, 0, 1, 2, X, 1, write_packed_bgr_16be);
    t["RGB48BE"] = mt(VI::CS_BGR48, 2, 1, 0, X, 3, write_rgb48_be);

    t["BGR64"]    = mt(VI::CS_BGR64, 0, 1, 2, 3, 1, write_packed_bgr);
    t["BGRA64"]   = mt(VI::CS_BGR64, 0, 1, 2, 3, 1, write_packed_bgr);
    t["RGB64"]    = mt(VI::CS_BGR64, 2, 1, 0, 3, 4, write_rgba64);
    t["RGBA64"]   = mt(VI::CS_BGR64, 2, 1, 0, 3, 4, write_rgba64);
    t["BGR64BE"]  = mt(VI::CS_BGR64, 0, 1, 2, 3, 1, write_packed_bgr_16be);
    t["BGRA64BE"] = mt(VI::CS_BGR64, 0, 1, 2, 3, 1, write_packed_bgr_16be);
    t["RGB64BE"]  = mt(VI::CS_BGR64, 2, 1, 0, 3, 4, write_rgba64_be);
    t["RGBA64BE"] = mt(VI::CS_BGR64, 2, 1, 0, 3, 4, write_rgba64_be);

    t["YUY2"]     = mt(VI::CS_YUY2,       0, 1, 2, 3, 1, write_planar);
    t["YUYV"]     = mt(VI::CS_YUY2,       0, 1, 2, 3, 1, write_planar);
    t["YVYU"]     = mt(VI::CS_YUY2,       0, 3, 2, 1, 4, write_packed_reorder);
    t["UYVY"]     = mt(VI::CS_YUY2,       1, 0, 3, 2, 4, write_packed_reorder);
    t["VYUY"]     = mt(VI::CS_YUY2,       1, 2, 3, 0, 4, write_packed_reorder);
    t["AYUV"]     = mt(VI::CS_YUVA444,    A, Y, U, V, 4, write_ayuv);
    t["VUYA"]     = mt(VI::CS_YUVA444,    V, U, Y, A, 4, write_ayuv);
    t["VUYX"]     = mt(VI::CS_YUVA444,    V, U, Y, A, 4, write_ayuv);
    t["UYVA"]     = mt(VI::CS_YUVA444,    U, Y, V, A, 4, write_ayuv);
    t["AYUV64"]   = mt(VI::CS_YUVA444P16, A, Y, U, V, 4, write_ayuv_16);
    t["AYUV64BE"] = mt(VI::CS_YUVA444P16, A, Y, U, V, 4, write_ayuv_16be);

    // Although, Y210 do not have 16-bit sample precision,
    // the data itself has been left-shifted to make it 16-bit.
    t["Y210"]   = mt(VI::CS_YUV422P16, Y, U, Y, V, 3, write_y21x);
    t["Y216"]   = mt(VI::CS_YUV422P16, Y, U, Y, V, 3, write_y21x);

    // I've never seen a UYVY10/12/14/16 file, and ffmpeg doesn't support them,
    // but apparently there are people out there who need them.
    // It probably works fine, but I don't care if it doesn't.
    // If there's a sample I might be interested.
    t["UYVY10"]   = mt(VI::CS_YUV422P10, U, Y, V, Y, 3, write_uyvy_16);
    t["UYVY12"]   = mt(VI::CS_YUV422P12, U, Y, V, Y, 3, write_uyvy_16);
    t["UYVY14"]   = mt(VI::CS_YUV422P14, U, Y, V, Y, 3, write_uyvy_16);
    t["UYVY16"]   = mt(VI::CS_YUV422P16, U, Y, V, Y, 3, write_uyvy_16);
    t["UYVY10BE"] = mt(VI::CS_YUV422P10, U, Y, V, Y, 3, write_uyvy_16be);
    t["UYVY12BE"] = mt(VI::CS_YUV422P12, U, Y, V, Y, 3, write_uyvy_16be);
    t["UYVY14BE"] = mt(VI::CS_YUV422P14, U, Y, V, Y, 3, write_uyvy_16be);
    t["UYVY16BE"] = mt(VI::CS_YUV422P16, U, Y, V, Y, 3, write_uyvy_16be);

    t["GBRP"]      = mt(VI::CS_RGBP,    G, B, R, X, 3, write_planar);
    t["GBRP9"]     = mt(VI::CS_RGBP10,  G, B, R, X, 3, write_planar_9);
    t["GBRP10"]    = mt(VI::CS_RGBP10,  G, B, R, X, 3, write_planar);
    t["GBRP12"]    = mt(VI::CS_RGBP12,  G, B, R, X, 3, write_planar);
    t["GBRP14"]    = mt(VI::CS_RGBP14,  G, B, R, X, 3, write_planar);
    t["GBRP16"]    = mt(VI::CS_RGBP16,  G, B, R, X, 3, write_planar);
    t["GBRP9BE"]   = mt(VI::CS_RGBP10,  G, B, R, X, 3, write_planar_9be);
    t["GBRP10BE"]  = mt(VI::CS_RGBP10,  G, B, R, X, 3, write_planar_16be);
    t["GBRP12BE"]  = mt(VI::CS_RGBP12,  G, B, R, X, 3, write_planar_16be);
    t["GBRP14BE"]  = mt(VI::CS_RGBP14,  G, B, R, X, 3, write_planar_16be);
    t["GBRP16BE"]  = mt(VI::CS_RGBP16,  G, B, R, X, 3, write_planar_16be);
    t["GBRPS"]     = mt(VI::CS_RGBPS,   G, B, R, X, 3, write_planar);

    t["GBRAP"]     = mt(VI::CS_RGBAP,   G, B, R, A, 4, write_planar);
    t["GBRAP10"]   = mt(VI::CS_RGBAP10, G, B, R, A, 4, write_planar);
    t["GBRAP12"]   = mt(VI::CS_RGBAP12, G, B, R, A, 4, write_planar);
    t["GBRAP14"]   = mt(VI::CS_RGBAP14, G, B, R, A, 4, write_planar);
    t["GBRAP16"]   = mt(VI::CS_RGBAP16, G, B, R, A, 4, write_planar);
    t["GBRAP10BE"] = mt(VI::CS_RGBAP10, G, B, R, A, 4, write_planar_16be);
    t["GBRAP12BE"] = mt(VI::CS_RGBAP12, G, B, R, A, 4, write_planar_16be);
    t["GBRAP14BE"] = mt(VI::CS_RGBAP14, G, B, R, A, 4, write_planar_16be);
    t["GBRAP16BE"] = mt(VI::CS_RGBAP16, G, B, R, A, 4, write_planar_16be);
    t["GBRAPS"]    = mt(VI::CS_RGBAPS,  G, B, R, A, 4, write_planar);

    t["YV24"]        = mt(VI::CS_YV24,       Y, V, U, X, 3, write_planar);
    t["YUV444P8"]    = mt(VI::CS_YV24,       Y, U, V, X, 3, write_planar);
    t["YUV444P9"]    = mt(VI::CS_YUV444P10,  Y, U, V, X, 3, write_planar_9);
    t["YUV444P10"]   = mt(VI::CS_YUV444P10,  Y, U, V, X, 3, write_planar);
    t["YUV444P12"]   = mt(VI::CS_YUV444P12,  Y, U, V, X, 3, write_planar);
    t["YUV444P14"]   = mt(VI::CS_YUV444P14,  Y, U, V, X, 3, write_planar);
    t["YUV444P16"]   = mt(VI::CS_YUV444P16,  Y, U, V, X, 3, write_planar);
    t["YUV444P9BE"]  = mt(VI::CS_YUV444P10, Y, U, V, X, 3, write_planar_9be);
    t["YUV444P10BE"] = mt(VI::CS_YUV444P10, Y, U, V, X, 3, write_planar_16be);
    t["YUV444P12BE"] = mt(VI::CS_YUV444P12, Y, U, V, X, 3, write_planar_16be);
    t["YUV444P14BE"] = mt(VI::CS_YUV444P14, Y, U, V, X, 3, write_planar_16be);
    t["YUV444P16BE"] = mt(VI::CS_YUV444P16, Y, U, V, X, 3, write_planar_16be);

    t["YUVA444"]    = mt(VI::CS_YUVA444,    Y, U, V, A, 4, write_planar);
    t["YUVA444P9"]    = mt(VI::CS_YUVA444P10, Y, U, V, A, 4, write_planar_9);
    t["YUVA444P10"]   = mt(VI::CS_YUVA444P10, Y, U, V, A, 4, write_planar);
    t["YUVA444P12"]   = mt(VI::CS_YUVA444P12, Y, U, V, A, 4, write_planar);
    t["YUVA444P14"]   = mt(VI::CS_YUVA444P14, Y, U, V, A, 4, write_planar);
    t["YUVA444P16"]   = mt(VI::CS_YUVA444P16, Y, U, V, A, 4, write_planar);
    t["YUVA444P9BE"]  = mt(VI::CS_YUVA444P10, Y, U, V, A, 4, write_planar_9be);
    t["YUVA444P10BE"] = mt(VI::CS_YUVA444P10, Y, U, V, A, 4, write_planar_16be);
    t["YUVA444P12BE"] = mt(VI::CS_YUVA444P12, Y, U, V, A, 4, write_planar_16be);
    t["YUVA444P14BE"] = mt(VI::CS_YUVA444P14, Y, U, V, A, 4, write_planar_16be);
    t["YUVA444P16BE"] = mt(VI::CS_YUVA444P16, Y, U, V, A, 4, write_planar_16be);

    t["YV16"]        = mt(VI::CS_YV16,      Y, V, U, X, 3, write_planar);
    t["YUV422P8"]    = mt(VI::CS_YV16,      Y, U, V, X, 3, write_planar);
    t["YUV422P9"]    = mt(VI::CS_YUV422P10, Y, U, V, X, 3, write_planar_9);
    t["YUV422P10"]   = mt(VI::CS_YUV422P10, Y, U, V, X, 3, write_planar);
    t["YUV422P12"]   = mt(VI::CS_YUV422P12, Y, U, V, X, 3, write_planar);
    t["YUV422P14"]   = mt(VI::CS_YUV422P14, Y, U, V, X, 3, write_planar);
    t["YUV422P16"]   = mt(VI::CS_YUV422P16, Y, U, V, X, 3, write_planar);
    t["YUV422P9BE"]  = mt(VI::CS_YUV422P10, Y, U, V, X, 3, write_planar_9be);
    t["YUV422P10BE"] = mt(VI::CS_YUV422P10, Y, U, V, X, 3, write_planar_16be);
    t["YUV422P12BE"] = mt(VI::CS_YUV422P12, Y, U, V, X, 3, write_planar_16be);
    t["YUV422P14BE"] = mt(VI::CS_YUV422P14, Y, U, V, X, 3, write_planar_16be);
    t["YUV422P16BE"] = mt(VI::CS_YUV422P16, Y, U, V, X, 3, write_planar_16be);

    t["YUVA422"]    = mt(VI::CS_YUVA422,    Y, U, V, A, 4, write_planar);
    t["YUVA422P9"]    = mt(VI::CS_YUVA422P10, Y, U, V, A, 4, write_planar_9);
    t["YUVA422P10"]   = mt(VI::CS_YUVA422P10, Y, U, V, A, 4, write_planar);
    t["YUVA422P12"]   = mt(VI::CS_YUVA422P12, Y, U, V, A, 4, write_planar);
    t["YUVA422P14"]   = mt(VI::CS_YUVA422P14, Y, U, V, A, 4, write_planar);
    t["YUVA422P16"]   = mt(VI::CS_YUVA422P16, Y, U, V, A, 4, write_planar);
    t["YUVA422P9BE"]  = mt(VI::CS_YUVA422P10, Y, U, V, A, 4, write_planar_9be);
    t["YUVA422P10BE"] = mt(VI::CS_YUVA422P10, Y, U, V, A, 4, write_planar_16be);
    t["YUVA422P12BE"] = mt(VI::CS_YUVA422P12, Y, U, V, A, 4, write_planar_16be);
    t["YUVA422P14BE"] = mt(VI::CS_YUVA422P14, Y, U, V, A, 4, write_planar_16be);
    t["YUVA422P16BE"] = mt(VI::CS_YUVA422P16, Y, U, V, A, 4, write_planar_16be);

    t["YV12"]        = mt(VI::CS_YV12,      Y, V, U, X, 3, write_planar);
    t["I420"]        = mt(VI::CS_I420,      Y, U, V, X, 3, write_planar);
    t["IYUV"]        = mt(VI::CS_I420,      Y, U, V, X, 3, write_planar);
    t["YUV420P9"]    = mt(VI::CS_YUV420P10, Y, U, V, X, 3, write_planar_9);
    t["YUV420P10"]   = mt(VI::CS_YUV420P10, Y, U, V, X, 3, write_planar);
    t["YUV420P12"]   = mt(VI::CS_YUV420P12, Y, U, V, X, 3, write_planar);
    t["YUV420P14"]   = mt(VI::CS_YUV420P14, Y, U, V, X, 3, write_planar);
    t["YUV420P16"]   = mt(VI::CS_YUV420P16, Y, U, V, X, 3, write_planar);
    t["YUV420P9BE"]  = mt(VI::CS_YUV420P10, Y, U, V, X, 3, write_planar_9be);
    t["YUV420P10BE"] = mt(VI::CS_YUV420P10, Y, U, V, X, 3, write_planar_16be);
    t["YUV420P12BE"] = mt(VI::CS_YUV420P12, Y, U, V, X, 3, write_planar_16be);
    t["YUV420P14BE"] = mt(VI::CS_YUV420P14, Y, U, V, X, 3, write_planar_16be);
    t["YUV420P16BE"] = mt(VI::CS_YUV420P16, Y, U, V, X, 3, write_planar_16be);

    t["YUVA420"]    = mt(VI::CS_YUVA420,    Y, U, V, A, 4, write_planar);
    t["YUVA420P9"]    = mt(VI::CS_YUVA420P10, Y, U, V, A, 4, write_planar_9);
    t["YUVA420P10"]   = mt(VI::CS_YUVA420P10, Y, U, V, A, 4, write_planar);
    t["YUVA420P12"]   = mt(VI::CS_YUVA420P12, Y, U, V, A, 4, write_planar);
    t["YUVA420P14"]   = mt(VI::CS_YUVA420P14, Y, U, V, A, 4, write_planar);
    t["YUVA420P16"]   = mt(VI::CS_YUVA420P16, Y, U, V, A, 4, write_planar);
    t["YUVA420P9BE"]  = mt(VI::CS_YUVA420P10, Y, U, V, A, 4, write_planar_9be);
    t["YUVA420P10BE"] = mt(VI::CS_YUVA420P10, Y, U, V, A, 4, write_planar_16be);
    t["YUVA420P12BE"] = mt(VI::CS_YUVA420P12, Y, U, V, A, 4, write_planar_16be);
    t["YUVA420P14BE"] = mt(VI::CS_YUVA420P14, Y, U, V, A, 4, write_planar_16be);
    t["YUVA420P16BE"] = mt(VI::CS_YUVA420P16, Y, U, V, A, 4, write_planar_16be);

    t["YV411"]    = mt(VI::CS_YV411, Y, V, U, X, 3, write_planar);
    t["Y41B"]     = mt(VI::CS_YV411, Y, V, U, X, 3, write_planar);
    t["YUV411P8"] = mt(VI::CS_YV411, Y, U, V, X, 3, write_planar);

    t["NV12"]   = mt(VI::CS_I420,      Y, U, V, X, 2, write_packed_chroma_8);
    t["NV21"]   = mt(VI::CS_YV12,      Y, V, U, X, 2, write_packed_chroma_8);
    t["NV16"]   = mt(VI::CS_YV16,      Y, U, V, X, 2, write_packed_chroma_8);
    t["NV24"]   = mt(VI::CS_YV24,      Y, U, V, X, 2, write_packed_chroma_8);
    t["NV42"]   = mt(VI::CS_YV24,      Y, V, U, X, 2, write_packed_chroma_8);
    t["NV20"]   = mt(VI::CS_YUV422P10, Y, U, V, X, 2, write_packed_chroma_16);
    t["NV20BE"] = mt(VI::CS_YUV422P10, Y, U, V, X, 2, write_packed_chroma_16be);

    // Although, PX10 and PX12 do not have 16-bit sample precision,
    // the data itself has been left-shifted to make it 16-bit.
    // According to the specs, these should only be little endian,
    // but for some reason ffmpeg also supports big endian.
    t["P010"]   = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P012"]   = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P016"]   = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P210"]   = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P212"]   = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P216"]   = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P410"]   = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P412"]   = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P416"]   = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16);
    t["P010BE"] = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P012BE"] = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P016BE"] = mt(VI::CS_YUV420P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P210BE"] = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P212BE"] = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P216BE"] = mt(VI::CS_YUV422P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P410BE"] = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P412BE"] = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16be);
    t["P416BE"] = mt(VI::CS_YUV444P16, Y, U, V, X, 2, write_packed_chroma_16be);

    t["Y8"]       = mt(VI::CS_Y8,  Y, X, X, X, 1, write_planar);
    t["Y10"]      = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar);
    t["Y12"]      = mt(VI::CS_Y12, Y, X, X, X, 1, write_planar);
    t["Y14"]      = mt(VI::CS_Y14, Y, X, X, X, 1, write_planar);
    t["Y16"]      = mt(VI::CS_Y16, Y, X, X, X, 1, write_planar);
    t["Y32"]      = mt(VI::CS_Y32, Y, X, X, X, 1, write_planar);
    t["GREY8"]    = mt(VI::CS_Y8,  Y, X, X, X, 1, write_planar);
    t["GREY9"]    = mt(VI::CS_Y10,  Y, X, X, X, 1, write_planar_9);
    t["GREY10"]   = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar);
    t["GREY12"]   = mt(VI::CS_Y12, Y, X, X, X, 1, write_planar);
    t["GREY14"]   = mt(VI::CS_Y14, Y, X, X, X, 1, write_planar);
    t["GREY16"]   = mt(VI::CS_Y16, Y, X, X, X, 1, write_planar);
    t["GREYS"]    = mt(VI::CS_Y32, Y, X, X, X, 1, write_planar);
    t["GREY9BE"]  = mt(VI::CS_Y10,  Y, X, X, X, 1, write_planar_9be);
    t["GREY10BE"] = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar_16be);
    t["GREY12BE"] = mt(VI::CS_Y12, Y, X, X, X, 1, write_planar_16be);
    t["GREY14BE"] = mt(VI::CS_Y14, Y, X, X, X, 1, write_planar_16be);
    t["GREY16BE"] = mt(VI::CS_Y16, Y, X, X, X, 1, write_planar_16be);
    t["GRAY8"]    = mt(VI::CS_Y8, Y, X, X, X, 1, write_planar);
    t["GRAY9"]    = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar_9);
    t["GRAY10"]   = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar);
    t["GRAY12"]   = mt(VI::CS_Y12, Y, X, X, X, 1, write_planar);
    t["GRAY14"]   = mt(VI::CS_Y14, Y, X, X, X, 1, write_planar);
    t["GRAY16"]   = mt(VI::CS_Y16, Y, X, X, X, 1, write_planar);
    t["GRAYS"]    = mt(VI::CS_Y32, Y, X, X, X, 1, write_planar);
    t["GRAY9BE"]  = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar_9be);
    t["GRAY10BE"] = mt(VI::CS_Y10, Y, X, X, X, 1, write_planar_16be);
    t["GRAY12BE"] = mt(VI::CS_Y12, Y, X, X, X, 1, write_planar_16be);
    t["GRAY14BE"] = mt(VI::CS_Y14, Y, X, X, X, 1, write_planar_16be);
    t["GRAY16BE"] = mt(VI::CS_Y16, Y, X, X, X, 1, write_planar_16be);
    t["GRAYS"]    = mt(VI::CS_Y32, Y, X, X, X, 1, write_planar);

    try {
        auto val = t.at(pix_type);
        vi.pixel_type = std::get<0>(val);
        order[0] = std::get<1>(val);
        order[1] = std::get<2>(val);
        order[2] = std::get<3>(val);
        order[3] = std::get<4>(val);
        colCount = std::get<5>(val);
        writeDestFrame = std::get<6>(val);
    } catch (std::out_of_range& e) {
        throw std::runtime_error(
            std::format("'{}' is unsupported pixel type.", pix_type));
    }
}


void RawSource::generateShuffleIndex(bool is_16bit)
{
    shuffleIndex = reinterpret_cast<uint8_t*>(avs_malloc(32, 32));
    validate(!shuffleIndex, "failed to allocate yuv order array.");
#if defined(_MSC_VER)
    //VS2022 continues to issue warnings, unaware of the NULL check two lines before.
    if (!shuffleIndex) return;
#endif

    if (is_16bit) {
        for (auto i = 0; i < 32; i += 8) {
            shuffleIndex[i + 0] = order[0] * 2 + i;
            shuffleIndex[i + 1] = order[0] * 2 + i + 1;
            shuffleIndex[i + 2] = order[1] * 2 + i;
            shuffleIndex[i + 3] = order[1] * 2 + i + 1;
            shuffleIndex[i + 4] = order[2] * 2 + i;
            shuffleIndex[i + 5] = order[2] * 2 + i + 1;
            shuffleIndex[i + 6] = order[3] * 2 + i;
            shuffleIndex[i + 7] = order[3] * 2 + i + 1;
        }
    } else {
        for (auto i = 0; i < 32; i += 4) {
            shuffleIndex[i + 0] = order[0] + i;
            shuffleIndex[i + 1] = order[1] + i;
            shuffleIndex[i + 2] = order[2] + i;
            shuffleIndex[i + 3] = order[3] + i;
        }
    }
}


void RawSource::generateBE2LEIndex()
{
    be2le = reinterpret_cast<uint8_t*>(avs_malloc(32, 32));
    validate(!be2le, "failed to allocate array for convert endian.");
#if defined(_MSC_VER)
    //VS2022 continues to issue warnings, unaware of the NULL check two lines before.
    if (!be2le) return;
#endif
    for (uint8_t i = 0; i < 32; i += 2) {
        be2le[i + 1] = i;
        be2le[i] = i + 1;
    }
}


void
RawSource::parseFileName(std::filesystem::path& fpath, std::string& pix_type)
{
    auto stem = fpath.stem();
    std::vector<std::string> v;
#if defined(_WIN32)
    std::wstring wstem(stem.wstring());
    std::string fname(MAX_PATH * 4, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstem.c_str(), -1, &fname[0], MAX_PATH * 4,
        nullptr, nullptr);
#else
    std::string fname(stem.string());
#endif
    split(fname, v, "_");
    for (const auto& n : v) {
        if (n.find("w=") == 0) {
            int w  = std::stoi(n.substr(2));
            if (w > 0) vi.width = w;
            continue;
        }
        if (n.find("h=") == 0) {
            int h = std::stoi(n.substr(2));
            if (h > 0) vi.height = h;
            continue;
        }
        if (n.find("pt=") == 0) {
            pix_type = n.substr(3);
            continue;
        }
        if (n.find("fn=") == 0) {
            int fn = std::stoi(n.substr(3));
            if (fn > 0) vi.fps_numerator = fn;
            continue;
        }
        if (n.find("fd=") == 0) {
            int fd = std::stoi(n.substr(3));
            if (fd > 0) vi.fps_denominator = fd;
        }
        if (n.find("sn=") == 0) {
            int sn = std::stoi(n.substr(3));
            if (sn > -1) props.sarNum = sn;
            continue;
        }
        if (n.find("sd=") == 0) {
            int sd = std::stoi(n.substr(3));
            if (sd > -1) props.sarDen = sd;
            continue;
        }
        if (n.find("fr=") == 0) {
            int fr = std::stoi(n.substr(3));
            if (fr > 0) vi.num_frames = fr;
        }
    }
}

RawSource::RawSource(const std::string& source, const int width, const int height,
    const std::string& ptype, const int fpsnum, const int fpsden,
    const std::string& a_index, const bool s, const int sarnum, const int sarden,
    const int frames, ise_t* env)
    : show(s), rawbuf(nullptr), index(nullptr), shuffleIndex(nullptr),
    be2le(nullptr), props(props_t())
{
    std::filesystem::path fpath;
    openFile(source, fpath);

    std::memset(&vi, 0, sizeof(VideoInfo));
    vi.width = width;
    vi.height = height;
    vi.SetFPS(fpsnum, fpsden);
    vi.SetFieldBased(false);
    vi.num_frames = frames;

    int64_t header_offset = 0;
    frameOffset = 0;
    std::string pix_type(ptype);

    if (pix_type == "" && source != "-") {
        parseFileName(fpath, pix_type);
    }

    if (a_index.length() == 0 && pix_type == "") { //use header if valid else width, height, pixel_type from AVS are used
        char buf[256] = { 0 };
        fread(buf, 1, 10, file);
        std::string header(buf);

        if (header == "YUV4MPEG2 ") {
            header = fgetsRLF(buf, 256, file);
            validate(header.length() > 254, "too large Y4M header.");
            parse_y4m(header, vi, pix_type, props);

            header = fgetsRLF(buf, 256, file);
            validate(header != "FRAME",
                std::format("unsupported frame header. {}", header));
            frameOffset = 6;
            header_offset = _ftelli64(file);
        }
    }

    if (pix_type == "") pix_type = "YUV420P8";
    std::transform(pix_type.begin(), pix_type.end(), pix_type.begin(), ::toupper);
    setProcess(pix_type);

    generateBE2LEIndex();

    std::array<write_frame_t, 4> arr = {
        write_rgba32,
        write_rgba64,
        write_rgba64_be,
        write_packed_reorder
    };
    if (std::find(arr.begin(), arr.end(), writeDestFrame) != arr.end()) {
        generateShuffleIndex(vi.IsRGB64());
    }

    int64_t framesize = vi.width * vi.height * vi.BitsPerPixel() / 8;
    rawbuf = reinterpret_cast<uint8_t*>(avs_malloc(framesize, 64));
    validate(!rawbuf, "failed to allocate read buffer.");

    if (fileSize < 1) {
        return;
    }

    int64_t maxframe = fileSize / framesize;    //1 = one frame

    validate(maxframe < 1, "File too small for even one frame.");

    //index build using string descriptor
    std::vector<rawindex_t> rawindex;
    set_rawindex(rawindex, a_index, header_offset, frameOffset, framesize);

    //create full index and get number of frames.
    index = reinterpret_cast<i_struct*>(avs_malloc((maxframe + 1) * sizeof(i_struct), 8));
    validate(!index, "failed to allocate index array.");
    vi.num_frames = generate_index(index, rawindex, framesize, fileSize);
}


PVideoFrame __stdcall RawSource::GetFrame(int n, ise_t* env)
{
    auto dst = env->NewVideoFrame(vi);

    if (fileSize > 0 && _fseeki64(file, index[n].index, SEEK_SET) != 0) {
        // black frame with message
        write_black_frame(dst, vi);
        env->ApplyMessage(&dst, vi, "failed to seek file!", vi.width / 2,
            0xFFFFFF, 0, 0);
        return dst;
    }

    size_t bufoffset = 0;
    if (fileSize < 0) {
        size_t read = frameOffset;
        if (read == 0) {
            read = 64;
            bufoffset = 64;
        }
        if (fread(rawbuf, 1, read, file) != read) {
            write_black_frame(dst, vi);
            env->ApplyMessage(&dst, vi,
                " \n \nstdin is empty.\n"
                "Please abort and terminate\n"
                "the application",
                vi.width * 2 / 5, 0xFFFFFF, 0, 0);
            return dst;
        }
    }

    int* o = shuffleIndex ? reinterpret_cast<int*>(shuffleIndex) : order;
    writeDestFrame(file, dst, rawbuf, o, colCount, env, be2le, bufoffset);

    if (fileSize > 0 && show) { //output debug info
        auto info = std::format("{} : {} {}", n, index[n].index, index[n].type);
        env->ApplyMessage(&dst, vi, info.c_str(), vi.width / 2, 0xFFFFFF, 0, 0);
    }

    auto map = env->getFramePropsRW(dst);
    auto at = static_cast<double>(vi.fps_denominator) / vi.fps_numerator * n;
    env->propSetFloat(map, "_AbsoluteTime", at, 0);
    env->propSetInt(map, "_DurationNum", vi.fps_numerator, 0);
    env->propSetInt(map, "_DurationDen", vi.fps_denominator, 0);
    if (props.sarNum > -1 && props.sarDen > -1) {
        env->propSetInt(map, "_SARNum", props.sarNum, 0);
        env->propSetInt(map, "_SARDen", props.sarDen, 0);
    }
    env->propSetInt(map, "_FieldBased", vi.image_type, 0);
    if (props.colRange > -1) {
        env->propSetInt(map, "_ColorRange", props.colRange, 0);
    }
    if (props.chromaLoc > -1) {
        env->propSetInt(map, "_ChromaLocation", props.chromaLoc, 0);
    }
    if (vi.IsYUV()) {
        env->propSetInt(map, "_Primaries", props.colPrim, 0);
        env->propSetInt(map, "_Transfer", props.transfer, 0);
        env->propSetInt(map, "_Matrix", props.colMat, 0);
    }

    return dst;
}


AVSValue __cdecl create_rawsource(AVSValue args, void* user_data, ise_t* env)
{
    try {
        validate(!args[0].Defined(), "No source specified");

        std::string source(args[0].AsString());
        const int width = args[1].AsInt(720);
        const int height = args[2].AsInt(576);
        std::string pix_type(args[3].AsString(""));
        const int fpsnum = args[4].AsInt(25);
        const int fpsden = args[5].AsInt(1);
        std::string index(args[6].AsString(""));
        const bool show = args[7].AsBool(false);
        const int sarnum = args[8].AsInt(0);
        const int sarden = args[9].AsInt(0);
        const int frames = args[10].AsInt(INT_MAX);

        validate(width < MIN_WIDTH || height < MIN_HEIGHT,
            std::format("width and height need to be {} x {} or higher.",
                MIN_WIDTH, MIN_HEIGHT));
        validate(pix_type.length() > 15, "pixel_type is too long.");
        validate(fpsnum < 1 || fpsden < 1,
            "fpsnum and fpsden need to be 1 or higher.");
        validate(sarnum < 0 || sarden < 0,
            "sarnum and sarden need to be 0 or higher.");
        validate(frames < 1, "frames need to be 1 or higher.");

        return new RawSource(source, width, height, pix_type, fpsnum, fpsden,
            index, show, sarnum, sarden, frames, env);

    } catch (std::exception& e) {
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
        "[show]b"
        "[sarnum]i"
        "[sarden]i"
        "[frames]i";

    env->AddFunction("RawSourcePlus", args, create_rawsource, nullptr);

    return "RawSource for Avisynth+.";
}
