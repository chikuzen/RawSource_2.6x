/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <io.h>
#include <fcntl.h>
#include <cstdint>
#include "common.h"


void __stdcall
write_planar(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
             ise_t* env) noexcept
{
    for (int i = 0; i < count; i++) {
        int plane = order[i];
        int width = dst->GetRowSize(plane);
        int height = dst->GetHeight(plane);
        uint8_t* dstp = dst->GetWritePtr(plane);
        int pitch = dst->GetPitch(plane);
        unsigned read_size = width * height;
        memset(buff, 0, read_size);
        _read(fd, buff, read_size);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }
}


template <typename T>
static inline void __stdcall
write_packed_chroma(int fd, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env) noexcept
{
    int width = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
    int pitch = dst->GetPitch(PLANAR_Y);
    unsigned read_size = width * height;

    memset(buff, 0, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);

    width = dst->GetRowSize(PLANAR_U) / sizeof(T);
    height = dst->GetHeight(PLANAR_U);
    pitch = dst->GetPitch(PLANAR_U) / sizeof(T);
    read_size = width * 2 * height * sizeof(T);
    T* dstp1 = reinterpret_cast<T*>(dst->GetWritePtr(order[1]));
    T* dstp2 = reinterpret_cast<T*>(dst->GetWritePtr(order[2]));
    T* buff_uv = reinterpret_cast<T*>(buff);

    memset(buff, 0, read_size);
    _read(fd, buff_uv, read_size);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            dstp1[x] = buff_uv[2 * x];
            dstp2[x] = buff_uv[2 * x + 1];
        }
        buff_uv += width * 2;
        dstp1 += pitch;
        dstp2 += pitch;
    }
}

void __stdcall
write_packed_chroma_8(int fd, PVideoFrame& dst, uint8_t* buff, int* order,
                      int count, ise_t* env) noexcept
{
    write_packed_chroma<uint8_t>(fd, dst, buff, order, count, env);
}

void __stdcall
write_packed_chroma_16(int fd, PVideoFrame& dst, uint8_t* buff, int* order,
                       int count, ise_t* env) noexcept
{
    write_packed_chroma<uint16_t>(fd, dst, buff, order, count, env);
}


void __stdcall
write_packed_reorder(int fd, PVideoFrame& dst, uint8_t* buff, int* order,
                     int count, ise_t* env) noexcept
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    uint8_t* dstp = dst->GetWritePtr();
    int pitch = dst->GetPitch();
    int read_size = width * height;
    memset(buff, 0, width * height);
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


void __stdcall
write_black_frame(PVideoFrame& dst, const VideoInfo& vi) noexcept
{
    uint8_t* dstp = dst->GetWritePtr();
    size_t size = dst->GetPitch() * dst->GetHeight();

    if (vi.IsYUY2()) {
        uint16_t* d = reinterpret_cast<uint16_t*>(dstp);
        std::fill(d, d + size / sizeof(uint16_t), 0x8000);
        return;
    }

    memset(dstp, 0, size);
    if (vi.pixel_type & VideoInfo::CS_INTERLEAVED) {
        return;
    }

    size = dst->GetPitch(PLANAR_U) * dst->GetHeight(PLANAR_U);
    dstp = dst->GetWritePtr(PLANAR_U);
    uint8_t* dstv = dst->GetWritePtr(PLANAR_V);

    if (vi.ComponentSize() == 1) {
        memset(dstp, 0x80, size);
        memset(dstv, 0x80, size);
        return;
    }
    if (vi.ComponentSize() == 2) {
        uint16_t* d = reinterpret_cast<uint16_t*>(dstp);
        size /= sizeof(uint16_t);
        std::fill(d, d + size, 0x8000);
        d = reinterpret_cast<uint16_t*>(dstv);
        std::fill(d, d + size, 0x8000);
        return;
    }

    float* d = reinterpret_cast<float*>(dstp);
    size /= sizeof(float);
    std::fill(d, d + size, 0.5f);
    d = reinterpret_cast<float*>(dstv);
    std::fill(d, d + size, 0.5f);
}

