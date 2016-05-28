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

constexpr unsigned MIN_WIDTH = 8;
constexpr unsigned MIN_HEIGHT = 8;
constexpr unsigned MAX_WIDTH = 65536;
constexpr unsigned MAX_HEIGHT = 65536;


bool parse_y4m(std::vector<char>& header, VideoInfo& vi,
               int64_t& header_offset, int64_t& frame_offset);


static inline void validate(bool cond, const char* msg)
{
    if (cond) throw std::runtime_error(msg);
}

#endif //RAWSOURCE_COMMON_H
