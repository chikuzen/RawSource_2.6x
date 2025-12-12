/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#ifndef RAWSOURCE_COMMON_H
#define RAWSOURCE_COMMON_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define NOMINMAX
    #define NOGDI
    #include <windows.h>
#else
    #define _FILE_OFFSET_BITS   64;
    #define _fseeki64   fseek
    #define _ftelli64   ftell
#endif
#include <avisynth.h>

#pragma warning(disable: 4996)


struct rawindex_t {
    int number;
    int64_t bytepos;
    rawindex_t() : number(0), bytepos(0) {}
    rawindex_t(int x, int64_t y) : number(x), bytepos(y) {}
};

struct i_struct {
    int64_t index;
    char type; //Key, Delta, Bigdelta
};

using ise_t = IScriptEnvironment;

using write_frame_t = void (*)(FILE*, PVideoFrame&, uint8_t*, int*, int,
    ise_t*, const uint8_t* be2le) noexcept;

constexpr unsigned MIN_WIDTH = 16;
constexpr unsigned MIN_HEIGHT = 16;

#define Y4M_STREAM_MAGIC "YUV4MPEG2";
#define Y4M_FRAME_MAGIC "FRAME";


void parse_y4m(std::string& header, VideoInfo& vi, std::string& pix_type);

void set_rawindex(std::vector<rawindex_t>& r, const std::string& index,
    int64_t header_offset, int64_t frame_offset, int64_t framesize);

int generate_index(i_struct* index, std::vector<rawindex_t>& rawindex,
    int64_t framesize, int64_t filesize);

void write_planar(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_planar_9(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_packed_chroma_8(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_packed_chroma_16(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_packed_reorder_8(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_packed_reorder_16(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept;

void write_black_frame(PVideoFrame& dst, const VideoInfo& vi) noexcept;

template <typename T>
static inline void validate(bool cond, T msg)
{
    if (cond) throw std::runtime_error(msg);
}

char* fgetsRLF(char* buf, int mc, FILE* f);

void split(const std::string& str, std::vector<std::string>& v,
    const char* separator) noexcept;

#endif //RAWSOURCE_COMMON_H
