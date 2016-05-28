/*
RawSource26 - reads raw video data files

Author: Oka Motofumi (chikuzen.mo at gmail dot com)

This program is rewriting of RawSource.dll(original author is Ernst Pech)
for avisynth2.6x/Avisynth+.
*/


#ifndef RAWSOURCE_COMMON_H
#define RAWSOURCE_COMMON_H


#include <cstring>
#include <vector>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <avisynth.h>

#pragma warning(disable: 4996)


typedef IScriptEnvironment ise_t;

constexpr unsigned MIN_WIDTH = 8;
constexpr unsigned MIN_HEIGHT = 8;
constexpr unsigned MAX_WIDTH = 65536;
constexpr unsigned MAX_HEIGHT = 65536;


struct rindex {
    int number;
    int64_t bytepos;
    rindex(int x, int64_t y) : number(x), bytepos(y) {}
};

struct i_struct {
    int64_t index;
    char type; //Key, Delta, Bigdelta
    i_struct() : index(0), type(0) {}
};


bool parse_y4m(std::vector<char>& header, VideoInfo& vi,
               int64_t& header_offset, int64_t& frame_offset);

void set_rawindex(std::vector<rindex>& r, const char* index,
                  int64_t header_offset, int64_t frame_offset,
                  size_t framesize);

int generate_index(std::vector<i_struct>& index, std::vector<rindex>& rawindex,
                   size_t framesize, int64_t filesize);

void __stdcall
write_NV420(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
            ise_t* env) noexcept;

void __stdcall
write_planar(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
             ise_t* env) noexcept;

void __stdcall
write_packed(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
             ise_t* env) noexcept;

void __stdcall
write_packed_reorder(int fd, PVideoFrame& dst, uint8_t* buff, int* order,
                     int count, ise_t* env) noexcept;

void __stdcall write_black_frame(PVideoFrame& dst, const VideoInfo& vi) noexcept;


static inline void validate(bool cond, const char* msg)
{
    if (cond) throw std::runtime_error(msg);
}

#endif //RAWSOURCE_COMMON_H
