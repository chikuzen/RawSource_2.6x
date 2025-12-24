/*
RawSourcePlus - reads raw video data files

    Author: Oka Motofumi (chikuzen.mo at gmail dot com)

    This program is rewriting of RawSource.dll(original author is Ernst Pech)
    for Avisynth+.
*/


#include <algorithm>
#include <unordered_map>
#include <tuple>
#include <cstring>
#include <format>
#include "common.h"


char* fgetsRLF(char* buf, int mc, FILE* f)
{
    if (!fgets(buf, mc, f)) {
        return nullptr;
    }
    size_t len = strlen(buf);
    if (buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    return buf;
}

void split(const std::string& str, std::vector<std::string>& dst,
    const char* separator) noexcept
{
    size_t offset = 0;
    std::string sep(separator);
    size_t length = sep.length();
    while (true) {
        auto pos = str.find(sep, offset);
        if (pos == std::string::npos) {
            auto t = str.substr(offset);
            if (t != "") {
                dst.push_back(t);
            }
            break;
        }
        auto t = str.substr(offset, pos - offset);
        if (t != "") {
            dst.push_back(t);
        }
        offset = pos + length;
    }
}


static void
createY4MFormatMap(std::unordered_map<std::string, std::string>& fmtMap) noexcept
{
    fmtMap["420JPEG"]  = "I420";
    fmtMap["420MPEG2"] = "I420";
    fmtMap["420PALDV"] = "I420";
    fmtMap["420P9"]    = "YUV420P9";
    fmtMap["420P10"]   = "YUV420P10";
    fmtMap["420P12"]   = "YUV420P12";
    fmtMap["420P14"]   = "YUV420P14";
    fmtMap["420P16"]   = "YUV420P16";
    fmtMap["422P9"]    = "YUV422P9";
    fmtMap["422P10"]   = "YUV422P10";
    fmtMap["422P12"]   = "YUV422P12";
    fmtMap["422P14"]   = "YUV422P14";
    fmtMap["422P16"]   = "YUV422P16";
    fmtMap["444P9"]    = "YUV444P9";
    fmtMap["444P10"]   = "YUV444P10";
    fmtMap["444P12"]   = "YUV444P12";
    fmtMap["444P14"]   = "YUV444P14";
    fmtMap["444P16"]   = "YUV444P16";
    fmtMap["420"]      = "I420";
    fmtMap["422"]      = "YUV422P8";
    fmtMap["444"]      = "YUV444P8";
    fmtMap["444ALPHA"] = "YUVA444";
    fmtMap["411"]      = "YUV411";
    fmtMap["MONO"]     = "Y8";
    fmtMap["MONO9"]    = "GREY9";
    fmtMap["MONO10"]   = "Y10";
    fmtMap["MONO12"]   = "Y12";
    fmtMap["MONO14"]   = "Y14";
    fmtMap["MONO16"]   = "Y16";
}


void parse_y4m(std::string& header, VideoInfo& vi, std::string& pix_type,
    props_t& props)
{
    const char* header_err = "YUV4MPEG2 header error.";
    const char* unsupported = "This file's YUV4MPEG2 HEADER is unsupported.";

    constexpr size_t fr_magic_len = 5;
    std::unordered_map<std::string, std::string> fmtMap;
    createY4MFormatMap(fmtMap);

    vi.height = 0;
    vi.width = 0;
    std::vector<std::string> params;
    split(header, params, " ");
    std::string altcolor;
    std::vector<std::string> v;

    for (const auto& p : params) {
        if (p[0] == 'W') {
            vi.width = std::stoi(p.substr(1));
            continue;

        } else if (p[0] == 'H') {
            vi.height = std::stoi(p.substr(1));
            continue;

        } else if (p[0] == 'I') {
            validate(p[1] == 'm', unsupported);
            if (p[1] == 't') {
                vi.image_type = VideoInfo::IT_TFF;
            } else if (p[1] == 'b') {
                vi.image_type = VideoInfo::IT_BFF;
            }
            continue;

        } else if (p[0] == 'F') {
            split(p.substr(1), v, ":");
            int num = std::stoi(v[0]);
            int den = std::stoi(v[1]);
            validate(num < 1 || den < 1, header_err);
            vi.SetFPS(num, den);
            v.clear();
            continue;

        } else if (p[0] == 'A') {
            split(p.substr(1), v, ":");
            int num = std::stoi(v[0]);
            int den = std::stoi(v[1]);
            validate(num < 0 || den < 0, header_err);
            props.sarNum = num;
            props.sarDen = den;
            v.clear();
            continue;

        } else if (p[0] == 'C') {
            auto key = p.substr(1);
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            pix_type = fmtMap.at(key);
            if (key == "420JPEG") {
                props.chromaLoc = 1;
            } else if (key == "420MPEG2") {
                props.chromaLoc = 0;
            } else if (key == "420PALDV") {
                props.chromaLoc = 2;
            }
            continue;

        } else if (p[0] == 'X') {
            if (p.find("YSCSS=") == 1) {
                auto key = p.substr(7);
                std::transform(key.begin(), key.end(), key.begin(), ::toupper);
                altcolor = fmtMap.at(key);
                if (pix_type == "") {
                    pix_type = altcolor;
                    if (key == "420JPEG") {
                        props.chromaLoc = 1;
                    } else if (key == "420MPEG2") {
                        props.chromaLoc = 0;
                    } else if (key == "420PALDV") {
                        props.chromaLoc = 2;
                    }
                }
            } else if (p.find("COLORRANGE=") == 1) {
                auto range = p.substr(12);
                if (range == "FULL") {
                    props.colRange = 0;
                } else if (range == "LIMITED") {
                    props.colRange = 1;
                }
            } else if (p.find("COLORPRIMARIES=") == 1) {
                int cprim = std::stoi(p.substr(16));
                props.colPrim = cprim;
            } else if (p.find("COLORMATRIX=") == 1) {
                int cmat = std::stoi(p.substr(13));
                props.colMat = cmat;
            } else if (p.find("TRANSFER=") == 1) {
                int tr = std::stoi(p.substr(10));
                props.transfer = tr;
            }
        }
    }

    validate(!vi.fps_numerator || !vi.fps_denominator || !vi.width
        || !vi.height, header_err);

    if (pix_type == "") {
        pix_type = "I420";
    }
}



void set_rawindex(std::vector<rawindex_t>& rawindex, const std::string& index,
                  int64_t header_offset, int64_t frame_offset, int64_t framesize)
{
    rawindex.reserve(2);

    if (index.length() == 0) {
        rawindex.emplace_back(0, header_offset);
        rawindex.emplace_back(1, header_offset + frame_offset + framesize);
        return;
    }

    std::vector<std::string> index_list;
    std::vector<std::string> tmp;

    if (index.find(".") != std::string::npos) { //assume indexstring is a filename
        FILE* fp = fopen(index.c_str(), "r");
        validate(!fp, std::format("failed to open index file {}.", index));
        char buf[1024];
        std::string str;
        while (fgetsRLF(buf, 1024, fp)) {
            str = buf;
            split(str, index_list, " ");
        }
        if (fp) fclose(fp);
    } else {
        if (index.find("\n") != std::string::npos) {
            split(index, tmp, "\n");
        } else {
            tmp.push_back(index);
        }
        for (const auto& i : tmp) {
            split(i, index_list, " ");
        }
        tmp.clear();
    }

    for (const auto& idx : index_list) {
        split(idx, tmp, ":");
        int number = std::stoi(tmp[0]);
        int64_t bytepos = std::stoll(tmp[1]);
        if (number < 0 || bytepos < 0) break;
        rawindex.emplace_back(number, bytepos);
    }

    validate(rawindex.size() == 0 || rawindex[0].number != 0,
             "When using an index: frame 0 is mandatory"); //at least an entries for frame0

}


int generate_index(i_struct* index, std::vector<rawindex_t>& rawindex,
                   int64_t framesize, int64_t filesize)
{
    int frame = 0;          //framenumber
    int p_ri = 0;           //pointer to raw index
    int64_t delta = framesize;  //delta between 1 frame
    int64_t big_delta = 0;      //delta between many e.g. 25 frames
    int big_steps = 0;      //how many big deltas have occured
    int big_frame_step = 0; //how many frames is big_delta for?
    int rimax = rawindex.size() - 1;
    int64_t maxframe = filesize / framesize;

    //rawindex[1].bytepos - rawindex[0].bytepos;    //current bytepos delta
    int64_t bytepos = rawindex[0].bytepos;
    index[frame].type = 'K';

    while ((frame < maxframe) && ((bytepos + framesize) <= filesize)) { //next frame must be readable
        index[frame].index = bytepos;

        if ((p_ri < rimax) && (rawindex[p_ri].number <= frame)) {
            ++p_ri;
            big_steps = 1;
        }
        ++frame;

        if ((p_ri > 0) && (rawindex[p_ri - 1].number + big_steps * big_frame_step == frame)) {
            bytepos = rawindex[p_ri - 1].bytepos + big_delta * big_steps;
            ++big_steps;
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
