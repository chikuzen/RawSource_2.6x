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

void set_rawindex(std::vector<rindex>& r, const char* index,
                  int64_t header_offset, int64_t frame_offset,
                  size_t framesize);


bool parse_y4m(std::vector<char>& header, VideoInfo& vi,
               int64_t& header_offset, int64_t& frame_offset);


static inline void validate(bool cond, const char* msg)
{
    if (cond) throw std::runtime_error(msg);
}

#endif //RAWSOURCE_COMMON_H
