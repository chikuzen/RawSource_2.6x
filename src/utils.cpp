/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <cstdio>
#include <cinttypes>
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
    strcpy(buff, "YUV420P8");

    unsigned int numerator = 0;
    unsigned int denominator = 0;
    char ctag[16] = {0};

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
                strcpy(buff, "YUVA444");
            } else if (!strncmp(ctag, "444p16", 6)) {
                strcpy(buff, "YUV444P16");
            } else if (!strncmp(ctag, "444p14", 6)) {
                strcpy(buff, "YUV444P14");
            } else if (!strncmp(ctag, "444p12", 6)) {
                strcpy(buff, "YUV444P12");
            } else if (!strncmp(ctag, "444p10", 6)) {
                strcpy(buff, "YUV444P10");
            } else if (!strncmp(ctag, "444p9", 5)) {
                strcpy(buff, "YUV444P9");
            } else if (!strncmp(ctag, "444", 3)) {
                strcpy(buff, "YUV444P8");
            } else if (!strncmp(ctag, "422p16", 6)) {
                strcpy(buff, "YUV422P16");
            } else if (!strncmp(ctag, "422p14", 6)) {
                strcpy(buff, "YUV422P14");
            } else if (!strncmp(ctag, "422p12", 6)) {
                strcpy(buff, "YUV422P12");
            } else if (!strncmp(ctag, "422p10", 6)) {
                strcpy(buff, "YUV422P10");
            } else if (!strncmp(ctag, "422p9", 5)) {
                strcpy(buff, "YUV422P9");
            } else if (!strncmp(ctag, "422", 3)) {
                strcpy(buff, "YUV422P8");
            } else if (!strncmp(ctag, "411", 3)) {
                strcpy(buff, "YUV411P8");
            } else if (!strncmp(ctag, "420p16", 6)) {
                strcpy(buff, "YUV420P16");
            } else if (!strncmp(ctag, "420p14", 6)) {
                strcpy(buff, "YUV420P14");
            } else if (!strncmp(ctag, "420p12", 6)) {
                strcpy(buff, "YUV420P12");
            } else if (!strncmp(ctag, "420p10", 6)) {
                strcpy(buff, "YUV420P10");
            } else if (!strncmp(ctag, "420p9", 5)) {
                strcpy(buff, "YUV420P9");
            } else if (!strncmp(ctag, "420", 3)) {
                strcpy(buff, "YUV420P8");
            } else if (!strncmp(ctag, "mono16", 6)) {
                strcpy(buff, "GREY16");
            } else if (!strncmp(ctag, "mono", 4)) {
                strcpy(buff, "GREY8");
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



void set_rawindex(std::vector<rindex>& rawindex, const char* index,
                  int64_t header_offset, int64_t frame_offset, size_t framesize)
{
    rawindex.reserve(2);

    if (strlen(index) == 0) {
        rawindex.emplace_back(0, header_offset);
        rawindex.emplace_back(1, header_offset + frame_offset + framesize);
        return;
    }

    std::vector<char> read_buff;
    const char * pos = strchr(index, '.');
    if (pos != nullptr) { //assume indexstring is a filename
        FILE* indexfile = fopen(index, "r");
        validate(!indexfile, "Cannot open indexfile.");
        fseek(indexfile, 0, SEEK_END);
        read_buff.resize(ftell(indexfile) + 1, 0);
        fseek(indexfile, 0, SEEK_SET);
        fread(read_buff.data(), 1, read_buff.size() - 1, indexfile);
        fclose(indexfile);
    } else {
        read_buff.resize(strlen(index) + 1, 0);
        strcpy(read_buff.data(), index);
    }

    //read all framenr:bytepos pairs
    const char* seps = " \n";
    for (char* token = strtok(read_buff.data(), seps);
            token != nullptr;
            token = strtok(nullptr, seps)) {
        int num1 = -1;
        int64_t num2 = -1;
        char* p_del = strchr(token, ':');
        if (!p_del)
            break;
        sscanf(token, "%d", &num1);
        sscanf(p_del + 1, "%" SCNi64, &num2);

        if ((num1 < 0) || (num2 < 0))
            break;
        rawindex.emplace_back(num1, num2);
    }

    validate(rawindex.size() == 0 || rawindex[0].number != 0,
             "When using an index: frame 0 is mandatory"); //at least an entries for frame0

}


int generate_index(i_struct* index, std::vector<rindex>& rawindex,
                   size_t framesize, int64_t filesize)
{
    int frame = 0;          //framenumber
    int p_ri = 0;           //pointer to raw index
    int delta = framesize;  //delta between 1 frame
    int big_delta = 0;      //delta between many e.g. 25 frames
    int big_steps = 0;      //how many big deltas have occured
    int big_frame_step = 0; //how many frames is big_delta for?
    int rimax = rawindex.size() - 1;
    int maxframe = static_cast<int>(filesize / framesize);

    //rawindex[1].bytepos - rawindex[0].bytepos;    //current bytepos delta
    int64_t bytepos = rawindex[0].bytepos;
    index[frame].type = 'K';

    while ((frame < maxframe) && ((bytepos + framesize) <= filesize)) { //next frame must be readable
        index[frame].index = bytepos;

        if ((p_ri < rimax) && (rawindex[p_ri].number <= frame)) {
            p_ri++;
            big_steps = 1;
        }
        frame++;

        if ((p_ri > 0) && (rawindex[p_ri - 1].number + big_steps * big_frame_step == frame)) {
            bytepos = rawindex[p_ri - 1].bytepos + big_delta * big_steps;
            big_steps++;
            index[frame].type = 'B';
        } else {
            if (rawindex[p_ri].number == frame) {
                bytepos = rawindex[p_ri].bytepos; //sync if framenumber is given in raw index
                index[frame].type = 'K';
            } else {
                bytepos = bytepos + delta;
                index[frame].type = 'D';
            }
        }

        //check for new delta and big_delta
        if ((p_ri > 0) && (rawindex[p_ri].number == rawindex[p_ri-1].number + 1)) {
            delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
        } else if (p_ri > 1) {
            //if more than 1 frame difference and
            //2 successive equal distances then remember as big_delta
            //if second delta < first delta then reset
            if (rawindex[p_ri].number - rawindex[p_ri - 1].number == rawindex[p_ri - 1].number - rawindex[p_ri - 2].number) {
                big_frame_step = rawindex[p_ri].number - rawindex[p_ri - 1].number;
                big_delta = (int)(rawindex[p_ri].bytepos - rawindex[p_ri - 1].bytepos);
            } else {
                if ((rawindex[p_ri].number - rawindex[p_ri - 1].number) < (rawindex[p_ri - 1].number - rawindex[p_ri - 2].number)) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
                if (frame >= rawindex[p_ri].number) {
                    big_delta = 0;
                    big_frame_step = 0;
                }
            }
        }
    }
    return frame;
}

