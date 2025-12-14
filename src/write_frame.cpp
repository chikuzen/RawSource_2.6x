/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/

#include <type_traits>
#include <immintrin.h>
#include "common.h"

template < bool IS_9BITS, bool BIG_ENDIAN, bool IS_PACKED_BGR >
static AVS_FORCEINLINE void
write_planar_base(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    const __m256i be2le_idx
        = _mm256_load_si256(reinterpret_cast<const __m256i*>(be2le));

    for (int i = 0; i < count; ++i) {
        int plane = order[i];
        int rowsize = dst->GetRowSize(plane);
        int height = dst->GetHeight(plane);
        uint8_t* dstp = dst->GetWritePtr(plane);
        int pitch = dst->GetPitch(plane);
        size_t read_size = rowsize * height;
        if constexpr (IS_PACKED_BGR) {
            fread(buff + bufoffset, 1, read_size - bufoffset, file);
            dstp += pitch * (height - 1);
            pitch = -pitch;
            env->BitBlt(dstp, pitch, buff, rowsize, rowsize, height);
        } else {
            if (rowsize == pitch) {
                memcpy(dstp, buff, bufoffset);
                fread(dstp + bufoffset, 1, read_size - bufoffset, file);
            } else {
                fread(buff + bufoffset, 1, read_size - bufoffset, file);
                env->BitBlt(dstp, pitch, buff, rowsize, rowsize, height);
            }
        }
        bufoffset = 0;
        if constexpr (BIG_ENDIAN) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < rowsize; x += 32) {
                    __m256i* d = reinterpret_cast<__m256i*>(dstp + x);
                    __m256i be = _mm256_load_si256(d);
                    __m256i le = _mm256_shuffle_epi8(be, be2le_idx);
                    if (IS_9BITS) {
                        le = _mm256_slli_epi16(le, 1);
                    }
                    _mm256_store_si256(d, le);
                }
                dstp += pitch;
            }
        } else if constexpr (IS_9BITS) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < rowsize; x += 32) {
                    __m256i* sd = reinterpret_cast<__m256i*>(dstp + x);
                    __m256i s = _mm256_load_si256(sd);
                    _mm256_store_si256(sd, _mm256_slli_epi16(s, 1));
                }
                dstp += pitch;
            }
        }
    }
}


void write_planar(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_planar_base<false, false, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_packed_bgr(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_planar_base<false, false, true>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}


void write_planar_9(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_planar_base<true, false, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_planar_9be(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_planar_base<true, true, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_planar_16be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_planar_base<false, true, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_packed_bgr_16be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_planar_base<false, true, true>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}


static AVS_FORCEINLINE void
convert_be2le_all(uint8_t* buff, size_t buffsize, const uint8_t* be2le) noexcept
{
    const __m256i be2le_idx =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(be2le));
    const uint8_t* end = buff + buffsize;
    while (buff < end) {
        __m256i be = _mm256_load_si256(reinterpret_cast<const __m256i*>(buff));
        __m256i le = _mm256_shuffle_epi8(be, be2le_idx);
        _mm256_store_si256(reinterpret_cast<__m256i*>(buff), le);
        buff += 32;
    }
}

template < typename T >
static AVS_FORCEINLINE void
proc_chroma(__m128i& s0, __m128i& s1, __m128i* d0, __m128i* d1)
{
    if constexpr (std::is_same_v<T, uint8_t>) {
        __m128i t0 = _mm_unpacklo_epi8(s0, s1); //u0 u8 v0 v8 u1 u9 v1 v9 u2 ua v2 va u3 ub v3 vb
        __m128i t1 = _mm_unpackhi_epi8(s0, s1); //u4 uc v4 vc u5 ud v5 vd u6 ue v6 ve u7 uf v7 vf
        s0 = _mm_unpacklo_epi8(t0, t1); // u0 u4 u8 uc v0 v4 v8 vc u1 u5 u9 ud v1 v5 v9 vd
        s1 = _mm_unpackhi_epi8(t0, t1); // u2 u6 ua ue v2 v6 va ve u3 u7 ub uf v3 v7 vb vf
        t0 = _mm_unpacklo_epi8(s0, s1); // u0 u2 u4 u6 u8 ua uc ue v0 v2 v4 v6 v8 va vc ve
        t1 = _mm_unpackhi_epi8(s0, s1); // u1 u3 u5 u7 u9 ub ud uf v1 v3 v5 v7 v9 vb vd vf
        s0 = _mm_unpacklo_epi8(t0, t1); // u0 u1 u2 u3 u4 u5 u6 u7 u8 u9 ua ub uc ud ue uf
        s1 = _mm_unpackhi_epi8(t0, t1); // v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 va vb vc vd ve vf
        _mm_store_si128(d0, s0);
        _mm_store_si128(d1, s1);
    } else if constexpr (std::is_same_v<T, uint16_t>) {
        __m128i t0 = _mm_unpacklo_epi16(s0, s1); //u0 u4 v0 v4 u1 u5 v1 v5
        __m128i t1 = _mm_unpackhi_epi16(s0, s1); //u2 u6 v2 v6 u3 u7 v3 v7
        s0 = _mm_unpacklo_epi16(t0, t1); // u0 u2 u4 u6 v0 v2 v4 v6
        s1 = _mm_unpackhi_epi16(t0, t1); // u1 u3 u5 u7 v1 v3 v5 v7
        t0 = _mm_unpacklo_epi16(s0, s1); // u0 u1 u2 u3 u4 u5 u6 u7
        t1 = _mm_unpackhi_epi16(s0, s1); // v0 v1 v2 v3 v4 v5 v6 v7
        _mm_store_si128(d0, t0);
        _mm_store_si128(d1, t1);
    }
}


template < typename T, bool BIG_ENDIAN >
static AVS_FORCEINLINE void
write_packed_chroma(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    int rowsize = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
    int pitch = dst->GetPitch(PLANAR_Y);
    size_t read_bytes = rowsize * height;

    if (rowsize == pitch) {
        memcpy(dstp, buff, bufoffset);
        fread(dstp + bufoffset, 1, read_bytes - bufoffset, file);
        if (BIG_ENDIAN) {
            convert_be2le_all(dstp, read_bytes, be2le);
        }
    } else {
        fread(buff + bufoffset, 1, read_bytes - bufoffset, file);
        if constexpr (BIG_ENDIAN) {
            convert_be2le_all(buff, read_bytes, be2le);
        }
        env->BitBlt(dstp, pitch, buff, rowsize, rowsize, height);
    }

    rowsize = dst->GetRowSize(PLANAR_U);
    height = dst->GetHeight(PLANAR_U);
    pitch = dst->GetPitch(PLANAR_U);
    read_bytes = rowsize * 2 * height;
    uint8_t* dstp0 = dst->GetWritePtr(order[1]);
    uint8_t* dstp1 = dst->GetWritePtr(order[2]);

    fread(buff, 1, read_bytes, file);

    const __m128i be2le_idx
        = _mm_load_si128(reinterpret_cast<const __m128i*>(be2le));
    const uint8_t* srcp = buff;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < rowsize; x += 16) {
            __m128i s0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x));
            __m128i s1 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x + 16));
            __m128i* d0 = reinterpret_cast<__m128i*>(dstp0 + x);
            __m128i* d1 = reinterpret_cast<__m128i*>(dstp1 + x);
            if constexpr (BIG_ENDIAN) {
                s0 = _mm_shuffle_epi8(s0, be2le_idx);
                s1 = _mm_shuffle_epi8(s1, be2le_idx);
            }
            proc_chroma<T>(s0, s1, d0, d1);
        }
        srcp += rowsize * 2;
        dstp0 += pitch;
        dstp1 += pitch;
    }
}

void write_packed_chroma_8(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_chroma<uint8_t, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_packed_chroma_16(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_chroma<uint16_t, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_packed_chroma_16be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_chroma<uint16_t, true>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

template < typename T, bool BIG_ENDIAN >
static AVS_FORCEINLINE void
write_packed_rgb_reorder(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    int rowsize = dst->GetRowSize();
    int height = dst->GetHeight();
    size_t read_bytes = rowsize * height;

    fread(buff + bufoffset, 1, read_bytes - bufoffset, file);

    if constexpr (BIG_ENDIAN) {
        //At first, convert all big endian samples to little endian.
        convert_be2le_all(buff, read_bytes, be2le);
    }

    T* buffx = reinterpret_cast<T*>(buff);
    int pitch = dst->GetPitch() / sizeof(T);
    T* dstp = reinterpret_cast<T*>(dst->GetWritePtr()) + pitch * (height - 1);
    rowsize /= sizeof(T);

    for (int i = 0; i < height; ++i) {
        for (int j = 0, width = rowsize / count; j < width; ++j) {
            for (int k = 0; k < count; ++k) {
                dstp[j * count + k] = buffx[j * count + order[k]];
            }
        }
        buffx += rowsize;
        dstp -= pitch;
    }
}


void write_rgb24(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_rgb_reorder<uint8_t, false>(file, dst, buff, order, count,
        env, be2le, bufoffset);
}


void write_rgb48(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_rgb_reorder<uint16_t, false>(file, dst, buff, order, count,
        env, be2le, bufoffset);
}

void write_rgb48_be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_rgb_reorder<uint16_t, true>(file, dst, buff, order, count,
        env, be2le, bufoffset);
}

template < bool IS_RGB, bool BIG_ENDIAN >
static AVS_FORCEINLINE void
write_packed_reorder_base(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    int rowsize = dst->GetRowSize();
    int height = dst->GetHeight();
    int pitch = dst->GetPitch();
    uint8_t* dstp = dst->GetWritePtr();
    size_t read_bytes = rowsize * height;

    fread(buff + bufoffset, 1, read_bytes - bufoffset, file);

    const __m256i be2le_idx
        = _mm256_load_si256(reinterpret_cast<const __m256i*>(be2le));
    const __m256i reorder_idx
        = _mm256_load_si256(reinterpret_cast<const __m256i*>(order));
    const uint8_t* srcp = buff;

    if constexpr (IS_RGB) {
        dstp += pitch * (height - 1);
        pitch = -pitch;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < rowsize; x += 32) {
            __m256i s = _mm256_loadu_si256(
                reinterpret_cast<const __m256i*>(srcp + x));
            s = _mm256_shuffle_epi8(s, reorder_idx);
            if (BIG_ENDIAN) {
                s = _mm256_shuffle_epi8(s, be2le_idx);
            }
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x), s);
        }
        srcp += rowsize;
        dstp += pitch;
    }
}

void write_packed_reorder(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_reorder_base<false, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_rgba32(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_reorder_base<true, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_rgba64(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_reorder_base<true, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_rgba64_be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_packed_reorder_base<true, true>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

template < bool IS_YUYV, bool BIG_ENDIAN >
static AVS_FORCEINLINE void
write_planar_from_packed(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    int rowsize = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    int pitchY = dst->GetPitch(PLANAR_Y);
    int pitchUV = dst->GetPitch(PLANAR_U);
    uint8_t* dstpY = dst->GetWritePtr(PLANAR_Y);
    uint8_t* dstpU = dst->GetWritePtr(PLANAR_U);
    uint8_t* dstpV = dst->GetWritePtr(PLANAR_V);
    size_t read_bytes = rowsize * 2 * height;

    fread(buff + bufoffset, 1, read_bytes - bufoffset, file);

    const uint8_t* srcp = buff;
    const __m128i be2le_idx
        = _mm_load_si128(reinterpret_cast<const __m128i*>(be2le));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < rowsize ; x += 32) {
            __m128i s0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x));
            __m128i s1 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x + 16));
            __m128i s2 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x + 32));
            __m128i s3 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(srcp + 2 * x + 48));
            if constexpr (BIG_ENDIAN) {
                s0 = _mm_shuffle_epi8(s0, be2le_idx);//Y0 U0 Y1 V0 Y2 U1 Y3 V1
                s1 = _mm_shuffle_epi8(s1, be2le_idx);//Y4 U2 Y5 V2 Y6 U3 Y7 V3
                s2 = _mm_shuffle_epi8(s2, be2le_idx);//Y8 U4 Y9 V4 Ya U5 Yb V5
                s3 = _mm_shuffle_epi8(s3, be2le_idx);//Yc U6 Yd V6 Ye U7 Yf V7
            }
            __m128i t0 = _mm_unpacklo_epi16(s0, s2);//Y0 Y8 U0 U4 Y1 Y9 V0 V4
            __m128i t1 = _mm_unpackhi_epi16(s0, s2);//Y2 Ya U1 U5 Y3 Yb V1 V5
            __m128i t2 = _mm_unpacklo_epi16(s1, s3);//Y4 Yc U2 U6 Y5 Yd V2 V6
            __m128i t3 = _mm_unpackhi_epi16(s1, s3);//Y6 Ye U3 U7 Y7 Yf V3 V7

            s0 = _mm_unpacklo_epi16(t0, t2);//YYYYUUUU
            s1 = _mm_unpackhi_epi16(t0, t2);//YYYYVVVV
            s2 = _mm_unpacklo_epi16(t1, t3);//YYYYUUUU
            s3 = _mm_unpackhi_epi16(t1, t3);//YYYYVVVV

            t0 = _mm_unpacklo_epi16(s0, s2);//YYYYYYYY
            t1 = _mm_unpackhi_epi16(s0, s2);//UUUUUUUU
            t2 = _mm_unpacklo_epi16(s1, s3);//YYYYYYYY
            t3 = _mm_unpackhi_epi16(s1, s3);//VVVVVVVV

            if constexpr (IS_YUYV) {
                s0 = _mm_unpacklo_epi16(t0, t2);
                s1 = _mm_unpackhi_epi16(t0, t2);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpY + x), s0);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpY + x + 16), s1);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpU + x / 2), t1);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpV + x / 2), t3);
            } else {
                s0 = _mm_unpacklo_epi16(t1, t3);
                s1 = _mm_unpackhi_epi16(t1, t3);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpY + x), s0);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpY + x + 16), s1);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpU + x / 2), t0);
                _mm_store_si128(reinterpret_cast<__m128i*>(dstpV + x / 2), t2);
            }
        }
        srcp += rowsize * 2;
        dstpY += pitchY;
        dstpU += pitchUV;
        dstpV += pitchUV;
    }
}


void write_y21x(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_planar_from_packed<true, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_uyvy_16(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_planar_from_packed<false, false>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

void write_uyvy_16be(FILE* file, PVideoFrame& dst, uint8_t* buff,
    int* order, int count, ise_t* env, const uint8_t* be2le,
    size_t bufoffset) noexcept
{
    write_planar_from_packed<false, true>(file, dst, buff, order, count, env,
        be2le, bufoffset);
}

template < typename T > static AVS_FORCEINLINE void
proc_ayuv(__m128i& s0, __m128i& s1, __m128i& s2, __m128i& s3, uint8_t* d0,
    uint8_t* d1, uint8_t* d2, uint8_t* d3) noexcept
{
    if constexpr (std::is_same_v<T, uint8_t>) {
        __m128i t0 = _mm_unpacklo_epi8(s0, s2);//a0 a8 y0 y8 u0 u8 v0 v8 a1 a9 y1 y9 u1 u9 v1 v9
        __m128i t1 = _mm_unpackhi_epi8(s0, s2);//a2 aa y2 ya u2 ua v2 va a3 ab y3 yb u3 ub v3 vb
        __m128i t2 = _mm_unpacklo_epi8(s1, s3);//a4 ac y4 yc u4 uc v4 vc a5 ad y5 yd u5 ud v5 vd
        __m128i t3 = _mm_unpackhi_epi8(s1, s3);//a6 ae y6 ye u6 ue v6 ve a7 af y7 yf u7 uf v7 vf

        s0 = _mm_unpacklo_epi8(t0, t2);//a0 a4 a8 ac y0 y4 y8 yc u0 u4 u8 uc v0 v4 v8 vc
        s1 = _mm_unpackhi_epi8(t0, t2);//a1 a5 a9 ad y1 y5 y9 yd u1 u5 u9 ud v1 v5 v9 vd
        s2 = _mm_unpacklo_epi8(t1, t3);//a2 a6 aa ae y2 y6 ya ye u2 u6 ua ue v2 v6 va ve
        s3 = _mm_unpackhi_epi8(t1, t3);//a3 a7 ab af y3 y7 yb yf u3 u7 ub uf v3 v7 vb vf

        t0 = _mm_unpacklo_epi8(s0, s2);//a0 a2 a4 a6 a8 aa ac ae y0 y2 y4 y6 y8 ya yc ye
        t1 = _mm_unpackhi_epi8(s0, s2);//u0 u2 u4 u6 u8 ua uc ue v0 v2 v4 v6 v8 va vc ve
        t2 = _mm_unpacklo_epi8(s1, s3);//a1 a3 a5 a7 a9 ab ad af y1 y3 y5 y7 y9 yb yd yf
        t3 = _mm_unpackhi_epi8(s1, s3);//u1 u3 u5 u7 u9 ub ud uf v1 v3 v5 v7 v9 vb vd vf

        s0 = _mm_unpacklo_epi8(t0, t2);//a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af
        s1 = _mm_unpackhi_epi8(t0, t2);//y0 y1 y2 y3 y4 y5 y6 y7 y8 y9 ya yb yc yd ye yf
        s2 = _mm_unpacklo_epi8(t1, t3);//u0 u1 u2 u3 u4 u5 u6 u7 u8 u9 ua ub uc ud ue uf
        s3 = _mm_unpackhi_epi8(t1, t3);//v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 va vb vc vd ve vf

        _mm_store_si128(reinterpret_cast<__m128i*>(d0), s0);
        _mm_store_si128(reinterpret_cast<__m128i*>(d1), s1);
        _mm_store_si128(reinterpret_cast<__m128i*>(d2), s2);
        _mm_store_si128(reinterpret_cast<__m128i*>(d3), s3);

    } else if constexpr (std::is_same_v<T, uint16_t>) {
        __m128i t0 = _mm_unpacklo_epi16(s0, s2);//A0 A4 Y0 Y3 U0 U4 V0 V4
        __m128i t1 = _mm_unpackhi_epi16(s0, s2);//A1 A5 Y1 Y5 U1 U5 V1 V5
        __m128i t2 = _mm_unpacklo_epi16(s1, s3);//A2 A6 Y2 Y6 U2 U6 V2 V6
        __m128i t3 = _mm_unpackhi_epi16(s1, s3);//A3 A7 Y3 Y7 U3 U7 V3 V7

        s0 = _mm_unpacklo_epi16(t0, t2);//A0 A2 A4 A6 Y0 Y2 Y4 Y6
        s1 = _mm_unpackhi_epi16(t0, t2);//U0 U2 U4 U6 V0 V2 V4 V6
        s2 = _mm_unpacklo_epi16(t1, t3);//A1 A3 A5 A7 Y1 Y3 Y5 Y7
        s3 = _mm_unpackhi_epi16(t1, t3);//U1 U3 U5 U7 V1 V3 V5 V7

        t0 = _mm_unpacklo_epi16(s0, s2);//A0 A1 A2 A3 A4 A5 A6 A7
        t1 = _mm_unpackhi_epi16(s0, s2);//Y0 Y1 Y2 Y3 Y4 Y5 Y6 Y7
        t2 = _mm_unpacklo_epi16(s1, s3);//U0 U1 U2 U3 U4 U5 U6 U7
        t3 = _mm_unpackhi_epi16(s1, s3);//V0 V1 V2 V3 V4 V5 V6 V7

        _mm_store_si128(reinterpret_cast<__m128i*>(d0), t0);
        _mm_store_si128(reinterpret_cast<__m128i*>(d1), t1);
        _mm_store_si128(reinterpret_cast<__m128i*>(d2), t2);
        _mm_store_si128(reinterpret_cast<__m128i*>(d3), t3);
    }
}


template < typename T, bool BIG_ENDIAN >
static AVS_FORCEINLINE void
write_ayuv_base(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    int rowsize = dst->GetRowSize(PLANAR_Y);
    int height = dst->GetHeight(PLANAR_Y);
    int pitch0 = dst->GetPitch(order[0]);
    int pitch1 = dst->GetPitch(order[1]);
    int pitch2 = dst->GetPitch(order[2]);
    int pitch3 = dst->GetPitch(order[3]);
    uint8_t* dstp0 = dst->GetWritePtr(order[0]);
    uint8_t* dstp1 = dst->GetWritePtr(order[1]);
    uint8_t* dstp2 = dst->GetWritePtr(order[2]);
    uint8_t* dstp3 = dst->GetWritePtr(order[3]);
    size_t read_bytes = rowsize * height * 4;

    const __m128i be2le_idx
        = _mm_load_si128(reinterpret_cast<const __m128i*>(be2le));

    fread(buff + bufoffset, 1, read_bytes - bufoffset, file);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < rowsize; x += 16) {
            __m128i s0 = _mm_loadu_si128(
                reinterpret_cast<__m128i*>(buff + 4 * x +  0));
            __m128i s1 = _mm_loadu_si128(
                reinterpret_cast<__m128i*>(buff + 4 * x + 16));
            __m128i s2 = _mm_loadu_si128(
                reinterpret_cast<__m128i*>(buff + 4 * x + 32));
            __m128i s3 = _mm_loadu_si128(
                reinterpret_cast<__m128i*>(buff + 4 * x + 48));
            if constexpr (BIG_ENDIAN) {
                s0 = _mm_shuffle_epi8(s0, be2le_idx);
                s1 = _mm_shuffle_epi8(s1, be2le_idx);
                s2 = _mm_shuffle_epi8(s2, be2le_idx);
                s3 = _mm_shuffle_epi8(s3, be2le_idx);
            }
            proc_ayuv<T>(s0, s1, s2, s3, dstp0 + x, dstp1 + x, dstp2 + x,
                dstp3 + x);
        }
        buff += rowsize * 4;
        dstp0 += pitch0;
        dstp1 += pitch1;
        dstp2 += pitch2;
        dstp3 += pitch3;
    }
}

void write_ayuv(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_ayuv_base<uint8_t, false>(file, dst, buff, order, count, env, be2le,
        bufoffset);
}

void write_ayuv_16(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_ayuv_base<uint16_t, false>(file, dst, buff, order, count, env, be2le,
        bufoffset);
}

void write_ayuv_16be(FILE* file, PVideoFrame& dst, uint8_t* buff, int* order,
    int count, ise_t* env, const uint8_t* be2le, size_t bufoffset) noexcept
{
    write_ayuv_base<uint16_t, true>(file, dst, buff, order, count, env, be2le,
        bufoffset);
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
        std::fill_n(reinterpret_cast<uint16_t*>(dst->GetWritePtr(planes[0])),
            size, val);
        std::fill_n(reinterpret_cast<uint16_t*>(dst->GetWritePtr(planes[1])),
            size, val);
    } else {
        size /= sizeof(float);
        float val = vi.IsYUV() ? 0.5f : 0.0f;
        float* d = reinterpret_cast<float*>(dstp);
        std::fill_n(reinterpret_cast<float*>(dst->GetWritePtr(planes[0])),
            size, val);
        std::fill_n(reinterpret_cast<float*>(dst->GetWritePtr(planes[1])),
            size, val);
    }
}

