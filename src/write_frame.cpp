#include <io.h>
#include <fcntl.h>
#include <cstdint>
#include "common.h"


void __stdcall
write_NV420(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
            ise_t* env) noexcept
{
    int width = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
    int pitch = dst->GetPitch(PLANAR_Y);
    int read_size = width * height;

    memset(buff, 0, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);

    read_size /= 2;
    width /= 2;
    height /= 2;
    dstp = dst->GetWritePtr(order[1]);
    pitch = dst->GetPitch(order[1]);
    uint8_t* dstp2 = dst->GetWritePtr(order[2]);

    memset(buff, 0, read_size);
    _read(fd, buff, read_size);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            dstp[j]  = buff[j * 2];
            dstp2[j] = buff[j * 2 + 1];
        }
        buff += width;
        dstp += pitch;
        dstp2 += pitch;
    }
}


void __stdcall
write_planar(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
             ise_t* env) noexcept
{
    for (int i = 0; i < count; i++) {
        int debug = order[i];
        int width = dst->GetRowSize(order[i]);
        int height = dst->GetHeight(order[i]);
        uint8_t* dstp = dst->GetWritePtr(order[i]);
        int pitch = dst->GetPitch(order[i]);
        int read_size = width * height;
        memset(buff, 0, read_size);
        _read(fd, buff, read_size);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }
}


void __stdcall
write_packed(int fd, PVideoFrame& dst, uint8_t* buff, int* order, int count,
             ise_t* env) noexcept
{
    int width = dst->GetRowSize();
    int height = dst->GetHeight();
    uint8_t* dstp = dst->GetWritePtr();
    int pitch = dst->GetPitch();
    int read_size = width * height;
    memset(buff, 0, read_size);
    _read(fd, buff, read_size);
    env->BitBlt(dstp, pitch, buff, width, width, height);
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
        uint32_t* d = reinterpret_cast<uint32_t*>(dstp);
        std::fill(d, d + size / sizeof(uint32_t), 0x80008000);
        return;
    }

    memset(dstp, 0, size);
    if (vi.IsRGB() || vi.IsY8()) {
        return;
    }

    size = dst->GetPitch(PLANAR_U) * dst->GetHeight(PLANAR_U);
    memset(dst->GetWritePtr(PLANAR_U), 0x80, size);
    memset(dst->GetWritePtr(PLANAR_V), 0x80, size);
}