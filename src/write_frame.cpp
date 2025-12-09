/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <emmintrin.h>
#include "common.h"


void write_planar(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    for (int i = 0; i < count; i++) {
        int plane = order[i];
        int width = dst->GetRowSize(plane);
        int height = dst->GetHeight(plane);
        uint8_t* dstp = dst->GetWritePtr(plane);
        int pitch = dst->GetPitch(plane);
        size_t read_size = width * height;
        if (width == pitch) {
            fread(dstp, 1, read_size, file);
            continue;
        }
        memset(buff, 0, read_size);
        fread(buff, 1, read_size, file);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }
}

void write_planar_9(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    write_planar(file, dst, buff, order, count, env);

    // convert 9bit planar YUV to 10bit
    const int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    for (int p = 0; p < count; ++p) {
        const int plane = planes[p];
        const int height = dst->GetHeight(plane);
        const int rowsize = dst->GetRowSize(plane);
        const int pitch = dst->GetPitch(plane);
        uint8_t* dstp = dst->GetWritePtr(plane);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < rowsize; x += 16) {
                __m128i* d = reinterpret_cast<__m128i*>(dstp + x);
                _mm_store_si128(d, _mm_slli_epi16(_mm_load_si128(d), 1));
            }
            dstp += pitch;
        }
    }
}


template <typename T>
static inline void
write_packed_chroma(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    int width = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
    int pitch = dst->GetPitch(PLANAR_Y);
    size_t read_size = width * height;

    if (width == pitch) {
        fread(dstp, 1, read_size, file);
    } else {
        std::memset(buff, 0, read_size);
        fread(buff, 1, read_size, file);
        env->BitBlt(dstp, pitch, buff, width, width, height);
    }

    width = dst->GetRowSize(PLANAR_U) / sizeof(T);
    height = dst->GetHeight(PLANAR_U);
    pitch = dst->GetPitch(PLANAR_U) / sizeof(T);
    read_size = width * 2 * height * sizeof(T);
    T* dstp1 = reinterpret_cast<T*>(dst->GetWritePtr(order[1]));
    T* dstp2 = reinterpret_cast<T*>(dst->GetWritePtr(order[2]));
    T* buff_uv = reinterpret_cast<T*>(buff);

    memset(buff, 0, read_size);
    fread(buff, 1, read_size, file);
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
write_packed_chroma_8(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
                      int count, ise_t* env) noexcept
{
    write_packed_chroma<uint8_t>(file, dst, buff, order, count, env);
}

void __stdcall
write_packed_chroma_16(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
                       int count, ise_t* env) noexcept
{
    write_packed_chroma<uint16_t>(file, dst, buff, order, count, env);
}


template <typename T>
static inline void
write_packed_reorder(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    int rowsize = dst->GetRowSize();
    int height = dst->GetHeight();
    size_t read_bytes = rowsize * height;

    T* buffx = reinterpret_cast<T*>(buff);
    T* dstp = reinterpret_cast<T*>(dst->GetWritePtr());
    int pitch = dst->GetPitch() / sizeof(T);
    rowsize /= sizeof(T);

    memset(buff, 0, read_bytes);
    fread(buff, 1, read_bytes, file);

    for (int i = 0; i < height; i++) {
        for (int j = 0, width = rowsize / count; j < width; j++) {
            for (int k = 0; k < count; k++) {
                dstp[j * count + k] = buffx[j * count + order[k]];
            }
        }
        buffx += rowsize;
        dstp += pitch;
    }
}

void write_packed_reorder_8(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    write_packed_reorder<uint8_t>(file, dst, buff, order, count, env);
}

void write_packed_reorder_16(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env) noexcept
{
    write_packed_reorder<uint16_t>(file, dst, buff, order, count, env);
}


void write_black_frame(PVideoFrame& dst, const VideoInfo& vi) noexcept
{
    uint8_t* dstp = dst->GetWritePtr();
    size_t size = dst->GetPitch() * dst->GetHeight();

    if (vi.IsYUY2()) {
        uint16_t* d = reinterpret_cast<uint16_t*>(dstp);
        std::fill_n(d, size / sizeof(uint16_t), 0x8000);
        return;
    }

    memset(dstp, 0, size);
    if (vi.pixel_type & VideoInfo::CS_INTERLEAVED) {
        return;
    }

    if (vi.pixel_type & (VideoInfo::CS_RGBA_TYPE | VideoInfo::CS_YUVA)) {
        memset(dst->GetWritePtr(PLANAR_A), 0, size);
    }

    const int planes[] = {
        vi.IsYUV() ? PLANAR_U : PLANAR_B,
        vi.IsYUV() ? PLANAR_V : PLANAR_R,
    };

    size = dst->GetPitch(planes[0]) * dst->GetHeight(planes[1]);

    if (vi.ComponentSize() == 1) {
        uint8_t val = vi.IsYUV() ? 0x80 : 0;
        memset(dst->GetWritePtr(planes[0]), val, size);
        memset(dst->GetWritePtr(planes[1]), val, size);
    } else if (vi.ComponentSize() == 2) {
        size /= sizeof(uint16_t);
        uint16_t val = vi.IsYUV() ? 0x8000 : 0;
        std::fill_n(reinterpret_cast<uint16_t*>(dst->GetWritePtr(planes[0])), size, val);
        std::fill_n(reinterpret_cast<uint16_t*>(dst->GetWritePtr(planes[1])), size, val);
    } else {
        size /= sizeof(float);
        float val = vi.IsYUV() ? 0.5f : 0.0f;
        float* d = reinterpret_cast<float*>(dstp);
        std::fill_n(reinterpret_cast<float*>(dst->GetWritePtr(planes[0])), size, val);
        std::fill_n(reinterpret_cast<float*>(dst->GetWritePtr(planes[1])), size, val);
    }
}

