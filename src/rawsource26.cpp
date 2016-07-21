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
    int __stdcall SetCacheHints(int hints, int)
    {
        return hints == CACHE_GET_MTMODE ? MT_SERIALIZED : 0;
    }
};


void RawSource::setProcess(const char* pix_type)
{
    using std::make_tuple;
    constexpr int Y = PLANAR_Y, U = PLANAR_U, V = PLANAR_V;

    typedef void (__stdcall *write_frame_t)(
        int, PVideoFrame&, uint8_t*, int*, int, ise_t*);
    typedef std::tuple<int, int, int, int, int, int, write_frame_t> pixel_format_t;

    std::unordered_map<std::string, pixel_format_t> table;

    table["BGR"]       = make_tuple(VideoInfo::CS_BGR24,     0, 1, 2, -1, 1, write_planar);
    table["BGR24"]     = make_tuple(VideoInfo::CS_BGR24,     0, 1, 2, -1, 1, write_planar);
    table["RGB"]       = make_tuple(VideoInfo::CS_BGR24,     2, 1, 0, -1, 3, write_packed_reorder);
    table["RGB24"]     = make_tuple(VideoInfo::CS_BGR24,     2, 1, 0, -1, 3, write_packed_reorder);

    table["BGRA"]      = make_tuple(VideoInfo::CS_BGR32,     0, 1, 2,  3, 1, write_planar);
    table["BGR32"]     = make_tuple(VideoInfo::CS_BGR32,     0, 1, 2,  3, 1, write_planar);
    table["RGBA"]      = make_tuple(VideoInfo::CS_BGR32,     2, 1, 0,  3, 4, write_packed_reorder);
    table["RGB32"]     = make_tuple(VideoInfo::CS_BGR32,     2, 1, 0,  3, 4, write_packed_reorder);
    table["ARGB"]      = make_tuple(VideoInfo::CS_BGR32,     3, 2, 1,  0, 4, write_packed_reorder);
    table["ABGR"]      = make_tuple(VideoInfo::CS_BGR32,     3, 0, 1,  2, 4, write_packed_reorder);

    table["YUY2"]      = make_tuple(VideoInfo::CS_YUY2,      0, 1, 2,  3, 1, write_planar);
    table["YUYV"]      = make_tuple(VideoInfo::CS_YUY2,      0, 1, 2,  3, 1, write_planar);
    table["UYVY"]      = make_tuple(VideoInfo::CS_YUY2,      1, 0, 3,  2, 4, write_packed_reorder);
    table["VYUY"]      = make_tuple(VideoInfo::CS_YUY2,      3, 0, 1,  2, 4, write_packed_reorder);

    table["YV24"]      = make_tuple(VideoInfo::CS_YV24,      Y, V, U, -1, 3, write_planar);
    table["I444"]      = make_tuple(VideoInfo::CS_YV24,      Y, U, V, -1, 3, write_planar);
    table["YUV444P8"]  = make_tuple(VideoInfo::CS_YV24,      Y, U, V, -1, 3, write_planar);
    table["YUV444P16"] = make_tuple(VideoInfo::CS_YUV444P16, Y, U, V, -1, 3, write_planar);
    table["YUV444PS"]  = make_tuple(VideoInfo::CS_YUV444PS,  Y, U, V, -1, 3, write_planar);

    table["YV16"]      = make_tuple(VideoInfo::CS_YV16,      Y, V, U, -1, 3, write_planar);
    table["I422"]      = make_tuple(VideoInfo::CS_YV16,      Y, U, V, -1, 3, write_planar);
    table["YUV422P8"]  = make_tuple(VideoInfo::CS_YV16,      Y, U, V, -1, 3, write_planar);
    table["YUV422P16"] = make_tuple(VideoInfo::CS_YUV422P16, Y, U, V, -1, 3, write_planar);
    table["YUV422PS"]  = make_tuple(VideoInfo::CS_YUV422PS,  Y, U, V, -1, 3, write_planar);
    table["P210"]      = make_tuple(VideoInfo::CS_YUV444P16, Y, U, V, -1, 2, write_packed_chroma_16);
    table["P216"]      = make_tuple(VideoInfo::CS_YUV444P16, Y, U, V, -1, 2, write_packed_chroma_16);

    table["YV411"]     = make_tuple(VideoInfo::CS_YV411,     Y, V, U, -1, 3, write_planar);
    table["Y41B"]      = make_tuple(VideoInfo::CS_YV411,     Y, V, U, -1, 3, write_planar);
    table["I411"]      = make_tuple(VideoInfo::CS_YV411,     Y, U, V, -1, 3, write_planar);
    table["YUV411P8"]  = make_tuple(VideoInfo::CS_YV411,     Y, U, V, -1, 3, write_planar);

    table["I420"]      = make_tuple(VideoInfo::CS_I420,      Y, U, V, -1, 3, write_planar);
    table["IYUV"]      = make_tuple(VideoInfo::CS_I420,      Y, U, V, -1, 3, write_planar);
    table["YV12"]      = make_tuple(VideoInfo::CS_YV12,      Y, V, U, -1, 3, write_planar);
    table["YUV420P8"]  = make_tuple(VideoInfo::CS_I420,      Y, U, V, -1, 3, write_planar);
    table["YUV420P16"] = make_tuple(VideoInfo::CS_YUV420P16, Y, U, V, -1, 3, write_planar);
    table["YUV420PS"]  = make_tuple(VideoInfo::CS_YUV420PS,  Y, U, V, -1, 3, write_planar);

    table["NV12"]      = make_tuple(VideoInfo::CS_I420,      Y, U, V, -1, 2, write_packed_chroma_8);
    table["NV21"]      = make_tuple(VideoInfo::CS_YV12,      Y, V, U, -1, 2, write_packed_chroma_8);
    table["P010"]      = make_tuple(VideoInfo::CS_YUV420P16, Y, U, V, -1, 2, write_packed_chroma_16);
    table["P016"]      = make_tuple(VideoInfo::CS_YUV420P16, Y, U, V, -1, 2, write_packed_chroma_16);

    table["Y8"]        = make_tuple(VideoInfo::CS_Y8,        Y, 0, 0, -1, 1, write_planar);
    table["GRAY"]      = make_tuple(VideoInfo::CS_Y8,        Y, 0, 0, -1, 1, write_planar);
    table["GRAY8"]     = make_tuple(VideoInfo::CS_Y8,        Y, 0, 0, -1, 1, write_planar);
    table["Y16"]       = make_tuple(VideoInfo::CS_Y16,       Y, 0, 0, -1, 1, write_planar);
    table["GRAY16"]    = make_tuple(VideoInfo::CS_Y16,       Y, 0, 0, -1, 1, write_planar);
    table["Y32"]       = make_tuple(VideoInfo::CS_Y32,       Y, 0, 0, -1, 1, write_planar);
    table["GRAYS"]     = make_tuple(VideoInfo::CS_Y32,       Y, 0, 0, -1, 1, write_planar);

    auto key = std::string(pix_type);
    std::transform(key.begin(), key.end(), key.begin(), ::toupper);

    std::tie(vi.pixel_type, order[0], order[1], order[2], order[3], col_count, writeDestFrame) = table[key];
    validate(vi.pixel_type == 0,
             "Invalid pixel type. Supported: RGB, RGBA, BGR, BGRA, ARGB, "
             "ABGR, YV24, I444, YUY2, YUYV, UYVY, YVYU, VYUY, YV16, I422, "
             "YV411, Y41B, I411, YV12, I420, IYUV, NV12, NV21, Y8, GRAY, "
             "YUV444P8, YUV444P16, YUV444PS, YUV422P8, YUV422P16, YUV422PS, "
             "YUV420P8, YUV420P16, YUV420PS, GRAY8, GRAY16, GRAYS, Y16, Y32, "
             "P210, P216, P010, P016");

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

    //create full index and get number of frames.
    index.resize(maxframe + 1);
    vi.num_frames = generate_index(index, rawindex, framesize, fileSize);

    void* b = _aligned_malloc(
        vi.IsPlanar() ? vi.width * vi.height * vi.ComponentSize() : framesize, 64);
    validate(b == nullptr, "failed to allocate read buffer.");
    rawbuf = reinterpret_cast<uint8_t*>(b);

    if (vi.ComponentSize() != 1) {
        show = false;
    }
}


PVideoFrame __stdcall RawSource::GetFrame(int n, ise_t* env)
{
    const i_struct* idx = index.data();
    PVideoFrame dst = env->NewVideoFrame(vi);

    if (_lseeki64(fileHandle, idx[n].index, SEEK_SET) == -1L) {
        // black frame with message
        write_black_frame(dst, vi);
        if (vi.ComponentSize() == 1) {
            env->ApplyMessage(&dst, vi, "failed to seek file!", vi.width,
                              0x00FFFFFF, 0x00FFFFFF, 0);
        }
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
                             index, show);

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

