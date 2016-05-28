#include "common.h"


bool parse_y4m(std::vector<char>& header, VideoInfo& vi,
               int64_t& header_offset, int64_t& frame_offset)
{
    const char* header_err = "YUV4MPEG2 header error.";
    const char* unsupported = "This file's YUV4MPEG2 HEADER is unsupported.";

    const char* Y4M_STREAM_MAGIC = "YUV4MPEG2";
    const char* Y4M_FRAME_MAGIC = "FRAME";
    constexpr size_t st_magic_len = 9;
    constexpr size_t fr_magic_len = 5;

    char* buff = header.data();
    const size_t buffsize = header.size();

    if (strncmp(buff, Y4M_STREAM_MAGIC, st_magic_len)) {
        return false;
    }

    vi.height = 0;
    vi.width = 0;
    strcpy(buff, "i420");

    unsigned int numerator = 0;
    unsigned int denominator = 0;
    char ctag[9] = {0};
    int64_t i;
    for (i = st_magic_len; i < buffsize && buff[i] != '\n'; ++i) {
        if (!strncmp(buff + i, " W", 2)) {
            i += 2;
            sscanf(buff + i, "%d", &vi.width);
        }

        if (!strncmp(buff + i, " H", 2)) {
            i += 2;
            sscanf(buff + i, "%d", &vi.height);
        }

        if (!strncmp(buff + i, " I", 2)) {
            i += 2;
            validate(buff[i] == 'm', unsupported);
            if (buff[i] == 't') {
                vi.image_type = VideoInfo::IT_TFF;
            } else if (buff[i] == 'b') {
                vi.image_type = VideoInfo::IT_BFF;
            }
        }

        if (!strncmp(buff + i, " F", 2)) {
            i += 2;
            sscanf(buff + i, "%u:%u", &numerator, &denominator);
            validate(numerator == 0 || denominator == 0, header_err);
            vi.SetFPS(numerator, denominator);
        }

        if (!strncmp(buff + i, " C", 2)) {
            i += 2;
            sscanf(buff + i, "%s", ctag);
            if (!strncmp(ctag, "444alpha", 8)) {
                strcpy(buff, "AYUV");
            } else if (!strncmp(ctag, "444", 3)) {
                strcpy(buff, "I444");
            } else if (!strncmp(ctag, "422", 3)) {
                strcpy(buff, "I422");
            } else if (!strncmp(ctag, "411", 3)) {
                strcpy(buff, "I411");
            } else if (!strncmp(ctag, "420", 3)) {
                strcpy(buff, "I420");
            } else if (!strncmp(ctag, "mono", 4)) {
                strcpy(buff, "GRAY");
            } else {
                throw std::runtime_error(header_err);
            }
        }
    }

    validate(!numerator || !denominator || !vi.width || !vi.height, header_err);

    ++i;

    validate(strncmp(buff + i, Y4M_FRAME_MAGIC, fr_magic_len) != 0, header_err);

    header_offset = i;

    i += fr_magic_len;

    while (i < buffsize && buff[i] != '\n') ++i;

    frame_offset = i - header_offset + 1;
    header_offset += frame_offset;

    return true;
}