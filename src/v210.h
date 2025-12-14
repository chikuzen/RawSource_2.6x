/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <cstdint>

struct Block_t {
    // Data is stored in blocks of 32 bit values in little - endian.
    // Each such block contains 3 components, one each in bits 0 - 9, 10 - 19
    // and 20 - 29, the remaining two bits are unused.
    // (from https://wiki.multimedia.cx/index.php/V210)

    uint32_t data0 : 10;
    uint32_t data1 : 10;
    uint32_t data2 : 10;
    uint32_t gabage : 2;
};

class V210_t {
    // Since UYVY consists of 4 components that represent two pixels, which of
    // those bits correspond to which component makes a pattern that repeats
    // every 4 32-bit blocks:
    // (from https://wiki.multimedia.cx/index.php/V210)

    Block_t b0; //U Y V
    Block_t b1; //Y U Y
    Block_t b2; //V Y U
    Block_t b3; //Y V Y
public:
    void setY(uint16_t* dstY) noexcept
    {
        dstY[0] = b0.data1;
        dstY[1] = b1.data0;
        dstY[2] = b1.data2;
        dstY[3] = b2.data1;
        dstY[4] = b3.data0;
        dstY[5] = b3.data2;
    }
    void setU(uint16_t* dstU) noexcept
    {
        dstU[0] = b0.data0;
        dstU[1] = b1.data1;
        dstU[2] = b2.data2;
    }
    void setV(uint16_t* dstV) noexcept
    {
        dstV[0] = b0.data2;
        dstV[1] = b2.data0;
        dstV[2] = b3.data1;
    }
    static size_t getStrideBytes(size_t width) noexcept
    {
        // In addition the start of each line is aligned to a multiple of
        // 128 bytes, where unused blocks are padded with 0.
        // Unused parts of a partial(i.e.last) block do not seem to contain
        // any specific value.
        // (from https://wiki.multimedia.cx/index.php/V210)

        return (((width + 5) / 6 * sizeof(V210_t) + 127) & ~127);
    }
};
